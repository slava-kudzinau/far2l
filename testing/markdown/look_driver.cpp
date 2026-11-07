// Standalone driver for the Colorer look of the F3 Markdown view (story
// markdown-colorer-look).  Built and run by testing/markdown/look-driver.sh,
// which needs the full build: it dlopen()s the colorer plugin module next to
// the far2l binary, the same harness shape as highlight_driver.cpp.
//
// R1 harness shape: one main, a table of {name, fn} cases.  `--case <name>`
// selects one case; no --case runs all and returns 1 on the first failure.
// Prints "ok <case>" / "FAILED <case>: <why>".
//
// Env: FAR2L_APP is the far2l binary, FAR2L_PLUGIN the colorer module.
// --rgb attaches a truecolor backend, so the plugin's ReloadBase reads the
// rgb scheme (far2l.hrd) instead of the 16-color console.hrd.

#include <farplug-wide.h>

#include "headers.hpp"
#include "ctrlobj.hpp"
#include "cfg/language.hpp"
#include "global.hpp"
#include "ConsoleOutput.h"
#include "markdown/MdColorer.hpp"
#include "cfg/config.hpp"
#include "mix/pathmix.hpp"
#include "farcolors.hpp"
#include "markdown/MdPalette.hpp"

#include <utils.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <vector>

namespace
{
int g_failures = 0;

// ---- plugin entry points, loaded from the module ----

using ModuleOpenFn = void (*)(const char*);
using StartupInfoFn = void (*)(const PluginStartupInfo*);
using ViewerEventFn = int (*)(int, void*);
using TokenizeFn = int (*)(const wchar_t* lang, const wchar_t* const* lines, int count,
	void (*emit)(void* ctx, int line, int start, int end, int token_class),
	void* ctx, int deadline_ms);
using LookFn = int (*)(int abi, const wchar_t* const* names, int count,
	unsigned int* look, int capacity, unsigned int* generation);
// V2 (story markdown-colorer-fences): the region's real look per token, in
// 8-word cells; the return is the number of cells written, codes -1..-3.
using TokenizeV2Fn = int (*)(int abi, const wchar_t* language, const wchar_t* const* lines,
	int nLines, int budget_ms, uint64_t* cells, int capacity, uint64_t* generation);

ModuleOpenFn g_module_open = nullptr;
StartupInfoFn g_set_startup_info = nullptr;
ViewerEventFn g_viewer_event = nullptr;
TokenizeFn g_tokenize = nullptr;
LookFn g_look = nullptr;
TokenizeV2Fn g_tokenize_v2 = nullptr;
// --rgb ran: the plugin's ReloadBase picked the rgb scheme (far2l.hrd);
// without it, the 16-color scheme (console/default.hrd, `fore="#B"` rows).
bool g_rgb = false;
ViewerInfo g_vi = {};
// The plugin manager the core bridge asks. The driver owns it here:
// `new ControlObject` scans and registers the plugins, as at startup.
ControlObject *CtrlObject = nullptr;
// The plugin's Info callbacks run headless here: the catalog path needs
// SaveScreen/Message/RestoreScreen/GetMsg; ReadSettings needs the Cfg trio.
intptr_t StubMessage(INT_PTR, DWORD, const wchar_t*, const wchar_t* const*, int, int) { return 1; }

const wchar_t* StubGetMsg(INT_PTR, FarLangMsgID) { return L"probe"; }
HANDLE StubSaveScreen(int, int, int, int) { return (HANDLE)1; }
void StubRestoreScreen(HANDLE) {}
INT_PTR StubAdvControl(INT_PTR, int, void*, void*) { return 1; }
int StubEditorControl(int, void*) { return 1; }
int StubViewerControl(int command, void* param)
{
	if (command == VCTL_GETINFO) {
		*static_cast<ViewerInfo*>(param) = g_vi;
	}
	return 1;
}

// A truecolor console for the plugin: with no backend the headless console
// reports the 4-color palette, and ReloadBase picks console.hrd, not the rgb
// far2l.hrd the spec table pins.
class RgbBackend : public IConsoleOutputBackend
{
public:
	void OnConsoleOutputUpdated(const SMALL_RECT*, size_t) override {}
	void OnConsoleOutputResized() override {}
	void OnConsoleOutputTitleChanged() override {}
	void OnConsoleOutputWindowMoved(bool, COORD) override {}
	COORD OnConsoleGetLargestWindowSize() override { return COORD{80, 25}; }
	void OnConsoleAdhocQuickEdit() override {}
	DWORD64 OnConsoleSetTweaks(DWORD64) override { return 0; }
	void OnConsoleChangeFont() override {}
	void OnConsoleSaveWindowState() override {}
	void OnConsoleSetMaximized(bool) override {}
	void OnConsoleExit() override {}
	bool OnConsoleIsActive() override { return true; }
	void OnConsoleDisplayNotification(const wchar_t*, const wchar_t*) override {}
	bool OnConsoleBackgroundMode(bool) override { return false; }
	bool OnConsoleSetFKeyTitles(const char**) override { return false; }
	BYTE OnConsoleGetColorPalette() override { return 24; }
	void OnConsoleGetBasePalette(void*) override {}
	bool OnConsoleSetBasePalette(void*) override { return false; }
	void OnConsoleOverrideColor(DWORD, DWORD*, DWORD*) override {}
	void OnConsoleSetCursorBlinkTime(DWORD) override {}
	void OnConsoleOutputFlushDrawing() override {}
	void OnGetConsoleImageCaps(WinportGraphicsInfo *wgi) override { *wgi = WinportGraphicsInfo{}; }
	bool OnSetConsoleImage(const char*, DWORD64, const SMALL_RECT*, DWORD, DWORD, const void*) override
	{
		return false;
	}
	bool OnTransformConsoleImage(const char*, const SMALL_RECT*, uint16_t) override { return false; }
	bool OnDeleteConsoleImage(const char*) override { return false; }
	const char* OnConsoleBackendInfo(int) override { return "rgb"; }
};

// The ten region names of the region map, in the order of Global Constraints.
const wchar_t* s_names[10] = {
	L"def:Text", L"markdown:HeadText", L"markdown:LinkText", L"markdown:StrongText",
	L"markdown:EmText", L"markdown:Code", L"markdown:QuoteText", L"markdown:ListMark",
	L"markdown:Rule", L"markdown:Text",
};

// ---- load the modules the way the core bridge (T2) will ----

void* g_plugin_handle = nullptr;

bool LoadModules(std::string& why)
{
	const char* plugin = getenv("FAR2L_PLUGIN");
	if (plugin == nullptr) {
		why = "FAR2L_PLUGIN must name the colorer module";
		return false;
	}
	g_plugin_handle = dlopen(plugin, RTLD_LAZY | RTLD_NOW);
	if (g_plugin_handle == nullptr) {
		const char* err = dlerror();
		why = std::string("dlopen plugin: ") + (err ? err : "unknown");
		return false;
	}
	// T2's lookup: the module the process already loaded answers.
	void* noload = dlopen(plugin, RTLD_LAZY | RTLD_NOLOAD);
	if (noload != g_plugin_handle) {
		why = "dlopen(RTLD_LAZY | RTLD_NOLOAD) did not return the loaded module";
		return false;
	}
	g_module_open = (ModuleOpenFn)dlsym(g_plugin_handle, "PluginModuleOpen");
	g_set_startup_info = (StartupInfoFn)dlsym(g_plugin_handle, "SetStartupInfoW");
	g_viewer_event = (ViewerEventFn)dlsym(g_plugin_handle, "ProcessViewerEventW");
	g_tokenize = (TokenizeFn)dlsym(g_plugin_handle, "ColorerTokenizeV1");
	g_look = (LookFn)dlsym(g_plugin_handle, "ColorerRegionLookV1");
	g_tokenize_v2 = (TokenizeV2Fn)dlsym(g_plugin_handle, "ColorerTokenizeV2");
	if (g_module_open == nullptr || g_set_startup_info == nullptr || g_viewer_event == nullptr
		|| g_tokenize == nullptr || g_look == nullptr || g_tokenize_v2 == nullptr) {
		const char* err = dlerror();
		why = std::string("dlsym: ") + (err ? err : "a needed export is missing");
		return false;
	}

	g_module_open(plugin);
	PluginStartupInfo psi = {};
	psi.ModuleNumber = 1;
	psi.AdvControl = StubAdvControl;
	psi.GetMsg = StubGetMsg;
	psi.EditorControl = StubEditorControl;
	psi.ViewerControl = StubViewerControl;
	psi.SaveScreen = StubSaveScreen;
	psi.RestoreScreen = StubRestoreScreen;
	psi.Message = StubMessage;
	static FarStandardFunctions sfsf = {};
	psi.FSF = &sfsf;
	g_set_startup_info(&psi);
	if (CtrlObject == nullptr) {
		CtrlObject = new ControlObject;
	}
	if (CtrlObject->Plugins.GetPluginsCount() == 0) {
		// main.cpp:763: the plugin manager scans after the language pack.
		CtrlObject->Plugins.LoadPlugins();
	}
	return true;
}

// Create editorSet the way the app does: a viewer event creates FarEditorSet,
// which loads the catalog (pcolorer.cpp:142 ProcessViewerEventW).
bool CreateEditorSet(std::string& why)
{
	(void)why;
	g_vi = {};
	g_vi.StructSize = sizeof(g_vi);
	g_vi.ViewerID = 1;
	g_viewer_event(0, nullptr);
	return true;
}

// ---- cases ----

// The story's first unknown: the export does not create FarEditorSet, the
// viewer's own ProcessViewerEventW does.  The first call answers code 1 in
// under 2 ms; after the event the same call answers 0 with generation 1
// (ReloadBase runs once in the constructor, so a real generation is never 0).
int look_first_call()
{
	std::string why;
	if (!LoadModules(why)) {
		printf("FAILED look_first_call: %s\n", why.c_str());
		return 1;
	}
	unsigned int rows[20] = {0};
	unsigned int gen = 999;
	const auto t0 = std::chrono::steady_clock::now();
	int rc = g_look(1, s_names, 10, rows, 20, &gen);
	const long long us0 = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - t0).count();
	printf("look_first_call: first call rc=%d in %lld us\n", rc, us0);
	if (rc != 1) {
		printf("FAILED look_first_call: first call rc=%d, want 1 (not loaded)\n", rc);
		return 1;
	}
	if (us0 >= 2000) {
		printf("FAILED look_first_call: first call took %lld us, want under 2 ms\n", us0);
		return 1;
	}
	// The viewer's own event creates FarEditorSet (pcolorer.cpp:142).
	g_viewer_event(0, nullptr);
	gen = 999;
	const auto t1 = std::chrono::steady_clock::now();
	rc = g_look(1, s_names, 10, rows, 20, &gen);
	const long long us1 = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - t1).count();
	printf("look_first_call: after ProcessViewerEventW rc=%d in %lld us, generation=%u\n",
		rc, us1, gen);
	if (rc != 0) {
		printf("FAILED look_first_call: after the event rc=%d, want 0\n", rc);
		return 1;
	}
	if (gen != 1) {
		printf("FAILED look_first_call: generation=%u, want 1 (the constructor's ReloadBase)\n", gen);
		return 1;
	}
	// The count-0 probe: abi checked, no rows, the generation answered.
	gen = 999;
	rc = g_look(1, s_names, 0, nullptr, 0, &gen);
	printf("look_first_call: count-0 probe rc=%d, generation=%u\n", rc, gen);
	if (rc != 0 || gen != 1) {
		printf("FAILED look_first_call: count-0 probe rc=%d gen=%u, want 0/1\n", rc, gen);
		return 1;
	}
	return 0;
}

// The ten names of the region map under far2l.hrd (the default rgb scheme),
// the exact ten pairs of the spec table (ruling R2: the tenth row,
// markdown:Text, is pinned only to a non-zero fore).
int look_real_hrd()
{
	std::string why;
	if (!LoadModules(why) || !CreateEditorSet(why)) {
		printf("FAILED look_real_hrd: %s\n", why.c_str());
		return 1;
	}
	struct Want { const wchar_t* name; unsigned int fore; unsigned int style; bool any_fore; };
	static const Want s_want[10] = {
		{L"def:Text",          0x99EEEE, 0, false},
		{L"markdown:HeadText", 0x8888ff, 4, false},
		{L"markdown:LinkText", 0x8888ff, 4, false},
		{L"markdown:StrongText", 0x40a000, 0, false},
		{L"markdown:EmText",   0xff20a0, 0, false},
		{L"markdown:Code",     0xFFE977, 0, false},
		{L"markdown:QuoteText", 0x6699ff, 0, false},
		{L"markdown:ListMark", 0xFFFFFF, 0, false},
		{L"markdown:Rule",     0xFFFFFF, 0, false},
		{L"markdown:Text",     0, 0, true},
	};
	unsigned int rows[20] = {0};
	unsigned int gen = 0;
	const int rc = g_look(1, s_names, 10, rows, 20, &gen);
	if (rc != 0) {
		printf("FAILED look_real_hrd: rc=%d, want 0\n", rc);
		return 1;
	}
	for (int i = 0; i < 10; ++i) {
		const unsigned int fore = rows[2 * i];
		const unsigned int style = rows[2 * i + 1];
		if (s_want[i].any_fore) {
			if (fore == 0) {
				printf("FAILED look_real_hrd: %S fore=0, want non-zero (present)\n", s_want[i].name);
				return 1;
			}
		} else if (fore != s_want[i].fore || style != s_want[i].style) {
			printf("FAILED look_real_hrd: %S gave 0x%06X/%u, want 0x%06X/%u\n",
				s_want[i].name, fore, style, s_want[i].fore, s_want[i].style);
			return 1;
		}
		printf("probe %S: 0x%06X/%u\n", s_want[i].name, fore, style);
	}
	printf("probe: ten rows under far2l.hrd, generation=%u\n", gen);
	return 0;
}

// The story's third unknown: the cost of one ten-name fetch on a warm
// scheme.  The number goes to closeout.md; the bridge cache in T4 is on
// either way.
int look_cost()
{
	std::string why;
	if (!LoadModules(why) || !CreateEditorSet(why)) {
		printf("FAILED look_cost: %s\n", why.c_str());
		return 1;
	}
	unsigned int rows[20] = {0};
	unsigned int gen = 0;
	if (g_look(1, s_names, 10, rows, 20, &gen) != 0) {
		printf("FAILED look_cost: warm-up call rc!=0\n");
		return 1;
	}
	const auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < 1000; ++i) {
		if (g_look(1, s_names, 10, rows, 20, &gen) != 0) {
			printf("FAILED look_cost: call %d rc!=0\n", i);
			return 1;
		}
	}
	const long long us = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - t0).count();
	printf("look_cost: ten-name fetch mean %lld us over 1000 calls (rc 0, generation %u)\n",
		us / 1000, gen);
	return 0;
}

// ---- T2 cases: the core bridge ----

// A fake look source: records the names and the fetches of the last Fetch, answers the
// rows and the generation of one scheme, cycles codes for look_codes.
class FakeLookSource : public MdLookSource
{
public:
	FakeLookSource()
	{
		for (int i = 0; i < 10; ++i) {
			m_fore[i] = 0x110000u + static_cast<unsigned>(i);
			m_style[i] = 4;
		}
	}

	unsigned long long Generation() override
	{
		++m_probes;
		if (m_codes_left > 0) {
			return 0; // the export answered 1..4: no look
		}
		return m_generation;
	}

	unsigned long long Fetch(const char *const *names, int count,
		MdRegionLook *out) override
	{
		++m_fetches;
		m_last_names.assign(names, names + count);
		if (m_codes_left > 0) {
			return 0; // the export answered 1..4: no look
		}
		for (int i = 0; i < count; ++i) {
			out[i].fore = m_fore[i];
			out[i].style = m_style[i];
		}
		return m_generation;
	}

	// The rows of one scheme generation, in the order of the ten names.
	void SetRows(const unsigned int *fore, const unsigned int *style)
	{
		for (int i = 0; i < 10; ++i) {
			m_fore[i] = fore[i];
			m_style[i] = style[i];
		}
	}

	std::vector<std::string> m_last_names;
	unsigned long long m_generation = 7;
	int m_codes_left = 0;
	int m_fetches = 0;
	int m_probes = 0;

private:
	unsigned int m_fore[10];
	unsigned int m_style[10];
};

// The bridge hands the plugin the ten names of Global Constraints, in order,
// and copies the plugin's rows back through Fetch, with the generation.
int look_bridge()
{
	int count = 0;
	const char *const *names = MdRegionNames(&count);
	if (names == nullptr || count != 10) {
		printf("FAILED look_bridge: MdRegionNames count=%d\n", count);
		return 1;
	}
	static const char *const kWant[10] = {
		"def:Text", "markdown:HeadText", "markdown:LinkText",
		"markdown:StrongText", "markdown:EmText", "markdown:Code",
		"markdown:QuoteText", "markdown:ListMark", "markdown:Rule",
		"markdown:Text",
	};
	for (int i = 0; i < 10; ++i) {
		if (strcmp(names[i], kWant[i]) != 0) {
			printf("FAILED look_bridge: name %d is %s\n", i, names[i]);
			return 1;
		}
	}

	FakeLookSource *record = new FakeLookSource(); // the hook keeps it for the
	MdSetLookSourceForTest(std::unique_ptr<FakeLookSource>(record));
	MdRegionLook rows[10] = {};
	const unsigned long long gen = MdGetLookSource().Fetch(names, count, rows);
	// the hook owns the source for the life of the process; read the record
	// back before clearing it
	if (record->m_last_names.size() != 10 || record->m_last_names[0] != "def:Text"
		|| record->m_last_names[9] != "markdown:Text") {
		printf("FAILED look_bridge: the fake saw %zu names\n",
			record->m_last_names.size());
		return 1;
	}
	if (gen != 7) {
		printf("FAILED look_bridge: generation=%llu\n", gen);
		return 1;
	}
	for (int i = 0; i < 10; ++i) {
		if (rows[i].fore != 0x110000u + static_cast<unsigned>(i)
			|| rows[i].style != 4) {
			printf("FAILED look_bridge: row %d is 0x%06X/%u\n", i,
				rows[i].fore, rows[i].style);
			return 1;
		}
	}
	printf("ok look_bridge: ten names in order, rows copied, generation 7\n");
	return 0;
}

// Codes 1..4 (and an old plugin binary without the symbol) give Fetch 0 and
// Generation 0; the token source keeps working untouched.
int look_codes()
{
	int count = 0;
	const char *const *names = MdRegionNames(&count);
	for (int round = 0; round < 4; ++round) {
		auto fake = std::make_unique<FakeLookSource>();
		fake->m_codes_left = 1; // the next Fetch answers "no look" (code 1..4)
		MdSetLookSourceForTest(std::move(fake));
		MdRegionLook rows[10] = {};
		const unsigned long long gen = MdGetLookSource().Fetch(names, count, rows);
		const unsigned long long probe = MdGetLookSource().Generation();
		MdSetLookSourceForTest(nullptr); // the process keeps the fake alive
		if (gen == 0 && probe == 0) {
			printf("ok look_codes: round %d no look\n", round);
		}
		if (gen != 0 || probe != 0) {
			printf("FAILED look_codes: round %d gave %llu/%llu\n", round, gen,
				probe);
			return 1;
		}
	}
	// dlsym found no ColorerRegionLookV1 (an old plugin binary): the fake
	// answers 0, standing for the missing symbol; the token path is untouched.
	{
		auto fake = std::make_unique<FakeLookSource>();
		fake->m_codes_left = 1;
		MdSetLookSourceForTest(std::move(fake));
		MdRegionLook rows[1] = {};
		const unsigned long long gen = MdGetLookSource().Fetch(names, 1, rows);
		MdSetLookSourceForTest(nullptr);
		if (gen != 0) {
			printf("FAILED look_codes: missing symbol gave %llu\n", gen);
			return 1;
		}
		std::vector<MdToken> tokens;
		if (!MdGetTokenSource().Ready()) {
			printf("FAILED look_codes: the token source is broken\n");
			return 1;
		}
		if (!MdGetTokenSource().Tokenize("cpp", {"int x = 1;"}, 5000, tokens)) {
			printf("FAILED look_codes: Tokenize failed\n");
			return 1;
		}
	}
	printf("ok look_codes: codes 1..4 and the missing symbol give no look\n");
	return 0;
}

// The real bridge through the real plugin: Generation 0 before the first
// viewer event (the code-1 path: dlopen, dlsym, export, mapping), generation
// 1 after, and the ten far2l.hrd rows through Fetch.
int look_bridge_real()
{
	printf("bridge_real: start\n");
	std::string why;
	if (!CreateEditorSet(why)) {
		printf("bridge_real: editor set made\n");
		printf("FAILED look_bridge_real: %s\n", why.c_str());
		return 1;
	}
	printf("bridge_real: gen\n");
	if (MdGetLookSource().Generation() != 1) {
		printf("FAILED look_bridge_real: generation=%llu after the load\n",
			MdGetLookSource().Generation());
		return 1;
	}
	int count = 0;
	const char *const *names = MdRegionNames(&count);
	MdRegionLook rows[10] = {};
	const unsigned long long gen = MdGetLookSource().Fetch(names, count, rows);
	if (gen != 1) {
		printf("FAILED look_bridge_real: fetch generation=%llu\n", gen);
		return 1;
	}
	static const unsigned int kWant[10][2] = {
		{0x99EEEE, 0}, {0x8888FF, 4}, {0x8888FF, 4}, {0x40A000, 0},
		{0xFF20A0, 0}, {0xFFE977, 0}, {0x6699FF, 0}, {0xFFFFFF, 0},
		{0xFFFFFF, 0}, {0x99EEEE, 0},
	};
	static const char *const kNames[10] = {
		"def:Text", "HeadText", "LinkText", "StrongText", "EmText", "Code",
		"QuoteText", "ListMark", "Rule", "Text",
	};
	for (int i = 0; i < 10; ++i) {
		if (rows[i].fore != kWant[i][0] || rows[i].style != kWant[i][1]) {
			printf("FAILED look_bridge_real: markdown:%s is 0x%06X/%u\n",
				kNames[i], rows[i].fore, rows[i].style);
			return 1;
		}
	}
	printf("ok look_bridge_real: generation 1, ten far2l.hrd rows\n");
	return 0;
}

// ---- T1 cases: the V2 export (story markdown-colorer-fences) ----

// The javascript fence: a keyword (`function`), a function name (the regexp
// region of `function name (`), a string, a number, a boolean constant, an
// operator, and a tail: the identifier and the semicolon of the third line,
// regions with no class (plain code role, spec States).
const wchar_t* const kJsFence[] = {
	L"function tailSum(a, b) {",
	L"  const s = \"lit\" + 42 * true;",
	L"  tailSum; // note",
};
constexpr int kJsFenceLines = 3;

// One V2 call: the cells of `n` lines under the active scheme.  The words
// past the count keep the sentinel 0xAB, so an over-write is visible.
struct V2Out
{
	std::vector<uint64_t> w;
	int rc = -99;
	uint64_t gen = 0;
	int Count() const { return rc > 0 ? rc : 0; }
	const uint64_t* At(int i) const { return w.data() + static_cast<size_t>(i) * 8; }
	// The last cell covering the 1-based column (the printer paints the
	// inner region last, R2; single-line calls keep the cells on one line).
	const uint64_t* Cover(int col) const
	{
		const uint64_t* hit = nullptr;
		for (int i = 0; i < Count(); ++i) {
			const uint64_t* c = At(i);
			if (c[0] <= static_cast<uint64_t>(col) && static_cast<uint64_t>(col) <= c[1]) {
				hit = c;
			}
		}
		return hit;
	}
};

V2Out CallV2(const wchar_t* const* lines, int n, int budget_ms = 5000, int capacity = 256,
	int abi = 1, const wchar_t* lang = L"javascript")
{
	V2Out out;
	out.w.assign(static_cast<size_t>(capacity) * 8 + 8, 0xAB);
	out.rc = g_tokenize_v2(abi, lang, lines, n, budget_ms, out.w.data(), capacity, &out.gen);
	return out;
}

// One expectation: a cell covering `col` of the one-line call, its 24-bit
// foreground, its 4-bit foreground, its style word.
struct V2Want
{
	int line;
	int col;
	const wchar_t* what;
	uint64_t fg;   // the far2l.hrd row (--rgb)
	uint64_t fg4;  // the console scheme's 4-bit row (no --rgb)
	uint64_t style;
};

// One expectation, by index into the table below: the last cell covering
// `col` of a one-line call carries the wanted 24-bit foreground, the
// scheme's 4-bit foreground, the style word, and three zeros.  The active
// scheme decides which side is measured: --rgb runs far2l.hrd (the table's
// 24-bit column), without it console default.hrd (the table's 4-bit
// column).  The companions are the same scheme through the palette's
// quantize: far2l.hrd's 24-bit value under the 16-color scheme, the
// console row expanded back to 24 bits under the rgb scheme.
bool Expect(const V2Out& out, int idx, int col, const wchar_t* what)
{
	// 0 keyword, 1 function, 2 keyword2, 3 string, 4 number, 5 operator,
	// 6 constant, 7 comment (far2l.hrd rows, console default.hrd rows).
	static const uint64_t s_rgb[] = {
		0xffffff, 0x6060f0, 0xffffff, 0x88ee88, 0x00e020, 0xe0e0c0, 0xa030ff, 0x696989,
	};
	static const uint64_t s_rgb4[] = {15, 9, 15, 15, 10, 15, 13, 9};
	static const uint64_t s_console4[] = {15, 11, 15, 14, 10, 15, 11, 3};
	static const uint64_t s_console24[] = {
		0xffffff, 0x55ffff, 0xffffff, 0xffff55, 0x55ff55, 0xffffff, 0x55ffff, 0x00aaaa,
	};
	const uint64_t want_fg = g_rgb ? s_rgb[idx] : s_console24[idx];
	const uint64_t want_fg4 = g_rgb ? s_rgb4[idx] : s_console4[idx];
	const uint64_t* cell = out.Cover(col);
	if (cell == nullptr) {
		printf("FAILED tokens_v2: %S: no cell covers col %d\n", what, col);
		for (int i = 0; i < out.Count(); ++i) {
			const uint64_t* c = out.At(i);
			printf("  cell %d: [%llu,%llu] 0x%llX/%llu style %llu\n", i,
				(unsigned long long)c[0], (unsigned long long)c[1],
				(unsigned long long)c[2], (unsigned long long)c[3],
				(unsigned long long)c[4]);
		}
		return false;
	}
	if (cell[2] != want_fg || cell[3] != want_fg4 || cell[4] != 0
		|| cell[5] != 0 || cell[6] != 0 || cell[7] != 0) {
		printf("FAILED tokens_v2: %S col %d is [0x%llX/%llu style %llu %llu,%llu,%llu], want [0x%llX/%llu style 0 0,0,0]\n",
			what, col,
			(unsigned long long)cell[2], (unsigned long long)cell[3],
			(unsigned long long)cell[4], (unsigned long long)cell[5],
			(unsigned long long)cell[6], (unsigned long long)cell[7],
			(unsigned long long)want_fg, (unsigned long long)want_fg4);
		return false;
	}
	printf("probe %S: [0x%llX/%llu style %llu] under %s\n", what,
		(unsigned long long)cell[2], (unsigned long long)cell[3],
		(unsigned long long)cell[4], g_rgb ? "far2l.hrd" : "console default.hrd");
	return true;
}

// The javascript fence through the real plugin, V2: one call per line (the
// cells keep their line), then the whole fence for the tail.  The 24-bit
// values are the far2l.hrd rows (spec Done), the 4-bit values the console
// scheme's rows; an unset color (def:Function on the console scheme) takes
// def:Text, the F4 chain.
int tokens_v2()
{
	std::string why;
	if (!LoadModules(why) || !CreateEditorSet(why)) {
		printf("FAILED tokens_v2: %s\n", why.c_str());
		return 1;
	}
	// The rows the two schemes give (far2l.hrd / console default.hrd).  The
	// function name's console fg is def:Text's #B: def:Function's assign
	// leaves the fore unset, the F4 chain falls to def:Text.
	static const V2Want s_want[] = {
		{0, 4,  L"keyword",    0xffffff, 15, 0}, // def:Keyword
		{0, 12, L"function",   0x6060f0, 11, 0}, // def:Function
		{1, 5,  L"keyword2",   0xffffff, 15, 0}, // def:Keyword (const)
		{1, 15, L"string",     0x88ee88, 14, 0}, // def:String (content)
		{1, 21, L"number",     0x00e020, 10, 0}, // def:NumberDec -> def:Number (#A)
		{1, 24, L"operator",   0xe0e0c0, 15, 0}, // def:Symbol (*)
		{1, 27, L"constant",   0xa030ff, 11, 0}, // def:BooleanConstant
		{2, 15, L"comment",    0x696989,  8, 0}, // def:Comment
	};
	for (int w = 0; w < 8; ++w) {
		const V2Want& want = s_want[w];
		V2Out out = CallV2(&kJsFence[want.line], 1);
		if (out.rc < 1) {
			printf("FAILED tokens_v2: line %d rc=%d, want cells\n", want.line, out.rc);
			return 1;
		}
		if (out.gen == 0) {
			printf("FAILED tokens_v2: generation=0 with a count\n");
			return 1;
		}
		if (!Expect(out, w, want.col, want.what)) {
			return 1;
		}
	}
	// The tail line (`  tailSum; // note`): the identifier carries no cell
	// (plain code role); the semicolon's Symbol match and the comment do.
	{
		V2Out tail = CallV2(&kJsFence[2], 1);
		if (tail.Count() < 1) {
			printf("FAILED tokens_v2: the tail line gave %d cells, want the comment\n",
				tail.Count());
			return 1;
		}
		for (int i = 0; i < tail.Count(); ++i) {
			if (tail.At(i)[0] < 10) {
				printf("FAILED tokens_v2: the tail line cell %d starts at %llu, want 10+\n",
					i, (unsigned long long)tail.At(i)[0]);
				return 1;
			}
		}
		if (!Expect(tail, 7, 15, L"comment")) {
			return 1;
		}
		if (tail.Cover(4) != nullptr) {
			printf("FAILED tokens_v2: a cell covers the tail identifier (col 4)\n");
			return 1;
		}
		printf("probe tail: no cell at col 4, the plain code role (def:Text)\n");
	}
	// The whole fence: the two token lines' cells plus the tail's two
	// (the semicolon's Symbol, the comment block), the 1-based inclusive
	// span of the keyword.
	V2Out fence = CallV2(kJsFence, kJsFenceLines);
	V2Out head2 = CallV2(kJsFence, 2);
	if (fence.rc != head2.rc + 2) {
		printf("FAILED tokens_v2: the fence gave %d cells, the first two lines %d, want two more\n",
			fence.rc, head2.rc);
		return 1;
	}
	// The fence's stream order: the function scheme's cell opens the
	// fence (jsFunctionDef enters at its start keyword, its span is the
	// name), the keyword's leaf writes after it, and the printer paints
	// the leaf over the scheme (R2).  1-based inclusive.
	const uint64_t* fn = fence.At(0);
	if (fn[0] != 10 || fn[1] != 16) {
		printf("FAILED tokens_v2: the fence cell 0 is [%llu,%llu], want [10,16] (the name)\n",
			(unsigned long long)fn[0], (unsigned long long)fn[1]);
		return 1;
	}
	const uint64_t* kw = fence.At(1);
	if (kw[0] != 1 || kw[1] != 8) {
		printf("FAILED tokens_v2: the fence cell 1 is [%llu,%llu], want [1,8] (the keyword)\n",
			(unsigned long long)kw[0], (unsigned long long)kw[1]);
		return 1;
	}
	// abi: the shape's abi is 1; a larger abi answers -2.
	V2Out abi2 = CallV2(kJsFence, kJsFenceLines, 5000, 256, 2);
	if (abi2.rc != -2 || abi2.gen != 0) {
		printf("FAILED tokens_v2: abi 2 gave rc=%d gen=%llu, want -2/0\n",
			abi2.rc, (unsigned long long)abi2.gen);
		return 1;
	}
	printf("ok tokens_v2: fence cells under %s, the tail bare, abi 2 -> -2 (generation %llu)\n",
		g_rgb ? "far2l.hrd" : "console default.hrd", (unsigned long long)fence.gen);
	return 0;
}

// 32 lines, 256 cells, the deadline (spec: same input and limits as V1,
// as look_codes).
int tokens_v2_limits()
{
	std::string why;
	if (!LoadModules(why) || !CreateEditorSet(why)) {
		printf("FAILED tokens_v2_limits: %s\n", why.c_str());
		return 1;
	}
	// The line cap: 34 lines answer as 32 (the extra lines add no cells),
	// under the cell cap so the count shows the line cap at work.
	static const wchar_t* kShort = L"1;";
	std::vector<const wchar_t*> lines(34, kShort);
	V2Out full32 = CallV2(lines.data(), 32);
	V2Out full34 = CallV2(lines.data(), 34);
	if (full32.rc <= 0 || full32.rc >= 256) {
		printf("FAILED tokens_v2_limits: 32 short lines gave rc=%d, want 0 < rc < 256\n",
			full32.rc);
		return 1;
	}
	if (full34.rc != full32.rc) {
		printf("FAILED tokens_v2_limits: 34 lines gave rc=%d, want rc=%d (the 32-line cap)\n",
			full34.rc, full32.rc);
		return 1;
	}
	printf("probe limits: %d cells from %d lines (cap 32 lines)\n", full32.rc, 34);
	// The cell cap: a token-rich fence writes exactly capacity cells, and
	// touches nothing past them (the sentinel word at 256*8 stays).
	static const wchar_t* kRich =
		L"1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1;";
	std::vector<const wchar_t*> rich(120, kRich);
	V2Out cap = CallV2(rich.data(), 120, 5000, 256);
	if (cap.rc != 256) {
		printf("FAILED tokens_v2_limits: a rich fence gave rc=%d, want 256 (the cell cap)\n",
			cap.rc);
		return 1;
	}
	if (cap.w[256 * 8] != 0xAB) {
		printf("FAILED tokens_v2_limits: the word past the cap was written\n");
		return 1;
	}
	// A small capacity answers its own count.
	V2Out ten = CallV2(rich.data(), 120, 5000, 10);
	if (ten.rc != 10 || ten.w[10 * 8] != 0xAB) {
		printf("FAILED tokens_v2_limits: capacity 10 gave rc=%d\n", ten.rc);
		return 1;
	}
	printf("probe limits: 256 cells written, a 10-capacity call stops at 10\n");
	// The deadline: a spent budget answers the cells it wrote before the
	// first check (a small count); a 1 ms budget over a fence with far
	// more tokens than the cap answers a partial count; the viewer's
	// 100 ms writes the full 256.
	std::vector<const wchar_t*> many(32, kRich);
	V2Out spent = CallV2(many.data(), 32, 0);
	V2Out partial = CallV2(many.data(), 32, 1);
	V2Out whole = CallV2(many.data(), 32, 100);
	if (spent.rc <= 0 || spent.rc >= 256) {
		printf("FAILED tokens_v2_limits: a spent budget gave rc=%d, want a small count\n",
			spent.rc);
		return 1;
	}
	if (spent.rc >= partial.rc || partial.rc > whole.rc || whole.rc != 256) {
		printf("FAILED tokens_v2_limits: budgets gave %d/%d/%d, want a rise to 256\n",
			spent.rc, partial.rc, whole.rc);
		return 1;
	}
	if (partial.gen == 0) {
		printf("FAILED tokens_v2_limits: a deadline stop lost the generation\n");
		return 1;
	}
	printf("probe limits: budget 0 -> %d, 1 ms -> %d, 100 ms -> %d (generation %llu)\n",
		spent.rc, partial.rc, whole.rc, (unsigned long long)whole.gen);
	return 0;
}

// V1 keeps its shape beside V2: the four classes through the 4-word emit
// callback, its own stride; V2 answers the same fence in 8-word cells.
int tokens_v2_v1_callers()
{
	std::string why;
	if (!LoadModules(why) || !CreateEditorSet(why)) {
		printf("FAILED tokens_v2_v1_callers: %s\n", why.c_str());
		return 1;
	}
	struct Emit { int line, start, end, cls; };
	std::vector<Emit>* sink = new std::vector<Emit>(); // one case, no teardown (R49)
	const int rc = g_tokenize(L"javascript", kJsFence, kJsFenceLines,
		[](void* ctx, int line, int start, int end, int token_class) {
			static_cast<std::vector<Emit>*>(ctx)->push_back({line, start, end, token_class});
		},
		sink, 5000);
	if (rc != 0) {
		printf("FAILED tokens_v2_v1_callers: V1 rc=%d, want 0\n", rc);
		return 1;
	}
	// The four classes, on their spans (V1's 0-based half-open cells).
	int seen[5] = {0, 0, 0, 0, 0};
	for (const Emit& e : *sink) {
		if (e.cls >= 1 && e.cls <= 4) {
			++seen[e.cls];
		}
		if (e.line == 0 && e.cls == 1 && e.start == 0 && e.end == 8) {
			seen[0] = 1; // `function` on line 0, cells [0,8)
		}
	}
	for (int cls = 1; cls <= 4; ++cls) {
		if (seen[cls] == 0) {
			printf("FAILED tokens_v2_v1_callers: V1 gave no class %d\n", cls);
			return 1;
		}
	}
	if (seen[0] == 0) {
		printf("FAILED tokens_v2_v1_callers: V1's keyword emit is not [0,8) on line 0\n");
		return 1;
	}
	// The tail keeps no class (V1 emits nothing after the comment).
	int tail_emits = 0;
	for (const Emit& e : *sink) {
		if (e.line == 2 && e.start < 10) {
			++tail_emits; // `  tailSum;` carries no class
		}
	}
	if (tail_emits != 0) {
		printf("FAILED tokens_v2_v1_callers: the tail gave %d V1 emits, want 0\n", tail_emits);
		return 1;
	}
	// V2 opens the fence with the function scheme's cell (the name's
	// region, V1's second class-1 emit), then the keyword's leaf — the
	// printer paints the leaf over the scheme, as in V1's R2.
	V2Out v2 = CallV2(kJsFence, kJsFenceLines);
	const uint64_t* fn = v2.At(0);
	const uint64_t* kw = v2.At(1);
	if (v2.rc <= 0 || fn[0] != 10 || fn[1] != 16 || kw[0] != 1 || kw[1] != 8) {
		printf("FAILED tokens_v2_v1_callers: V2's cells are [%llu,%llu],[%llu,%llu], want [10,16],[1,8]\n",
			(unsigned long long)fn[0], (unsigned long long)fn[1],
			(unsigned long long)kw[0], (unsigned long long)kw[1]);
		return 1;
	}
	if (v2.w[static_cast<size_t>(v2.rc) * 8] != 0xAB) {
		printf("FAILED tokens_v2_v1_callers: V2 wrote past its count\n");
		return 1;
	}
	printf("ok tokens_v2_v1_callers: V1's four classes on the 4-word emits, V2's 8-word cells\n");
	return 0;
}

// ---- T5 cases: the resolver, the contrast rule, the user file, the generations ----

// The driver's copies of the palette's private constants: the background bits a
// resolved entry always carries (kBackBits in MdPalette.cpp) and the underline bit of
// the far attribute (COMMON_LVB_UNDERSCORE).
constexpr uint64_t kBack = 0xf0 | BACKGROUND_TRUECOLOR | (uint64_t(0xffffff) << 40);
constexpr uint64_t kUnderline = 0x8000;

// The ten roles the Colorer look paints, in the order of the ten names (spec, Region
// map); body takes the tenth row, markdown:Text (spec, Body).
const uint64_t s_scheme_roles[10] = {MD_ROLE_H1, MD_ROLE_H2, MD_ROLE_H3, MD_ROLE_STRONG,
	MD_ROLE_LINK, MD_ROLE_CODE, MD_ROLE_QUOTE, MD_ROLE_RULE, MD_ROLE_EM, MD_ROLE_LISTMARK};

// A 24-bit foreground in the layout of a far attribute: the copy of TrueForeColor.
uint64_t TrueFore(uint32_t rgb)
{
	const uint64_t bgr = ((rgb & 0xff) << 16) | (rgb & 0xff00) | ((rgb >> 16) & 0xff);
	return (bgr << 16) | FOREGROUND_TRUECOLOR;
}

uint64_t WithBack(uint64_t fore_bits)
{
	const uint64_t body = FarColorToReal(COL_VIEWERTEXT);
	return (fore_bits & ~kBack) | (body & kBack);
}

// Step 1: a user ini value, no contrast check.
uint64_t WantLiteral(uint32_t rgb) { return WithBack(TrueFore(rgb)); }

// Step 2: the scheme row, the style word's bit 4 as the underline bit (spec,
// Underline).
uint64_t WantScheme(uint32_t fore, uint32_t style)
{
	uint64_t bits = TrueFore(fore);
	if (style & 4) {
		bits |= kUnderline;
	}
	return WithBack(bits);
}

// Step 3: the fork RGB default with the 4-bit fallback in the low bits (the exact
// constants of MdPalette.cpp).
uint64_t WantRgb(uint64_t role)
{
	static const uint32_t kRgb[MD_ROLE_COUNT - 1] = {0xffff5f, 0xffaf5f, 0x87afff, 0xffffff,
		0x87ff87, 0xff87ff, 0xafafd7, 0x8787af, 0, 0x87afff, 0x87ff87, 0x8787af, 0xffaf5f,
		0xd7d7af, 0x8787af};
	static const uint64_t k16[MD_ROLE_COUNT - 1] = {F_YELLOW, F_LIGHTRED, F_LIGHTGRAY, F_WHITE,
		F_LIGHTGREEN, F_LIGHTMAGENTA, F_CYAN, F_CYAN, 0, F_LIGHTGRAY, F_LIGHTGREEN, F_CYAN,
		F_LIGHTRED, F_LIGHTMAGENTA, F_CYAN};
	return WithBack(TrueFore(kRgb[role - 1]) | k16[role - 1]);
}

// Step 4: today's look, the palette entry of the fork table (spec, New roles).
uint64_t WantToday(uint64_t role)
{
	static const unsigned int kEntry[MD_ROLE_COUNT] = {COL_VIEWERTEXT, COL_HELPTOPIC,
		COL_HELPTOPIC, COL_HELPTOPIC, COL_HELPTOPIC, COL_HELPTOPIC, COL_HELPHIGHLIGHTTEXT,
		COL_VIEWERTEXT, COL_VIEWERTEXT, COL_PANELHIGHLIGHTTEXT, COL_HELPHIGHLIGHTTEXT,
		COL_HELPHIGHLIGHTTEXT, COL_HELPHIGHLIGHTTEXT, COL_HELPHIGHLIGHTTEXT, COL_VIEWERTEXT,
		COL_VIEWERTEXT};
	return WithBack(FarColorToReal(kEntry[role]));
}

// The region index of a role in the ten-name table (spec, Region map).
int SchemeRoleIndex(uint64_t role)
{
	switch (role) {
		case MD_ROLE_H1:
		case MD_ROLE_H2:
		case MD_ROLE_H3: return 1;
		case MD_ROLE_LINK: return 2;
		case MD_ROLE_STRONG: return 3;
		case MD_ROLE_EM: return 4;
		case MD_ROLE_CODE: return 5;
		case MD_ROLE_QUOTE: return 6;
		case MD_ROLE_LISTMARK: return 7;
		case MD_ROLE_RULE: return 8;
		default: return -1;
	}
}

bool CheckRole(const char *case_name, uint64_t role, uint64_t want)
{
	const uint64_t got = MdRunColor(role);
	if (got != want) {
		printf("FAILED %s: role %llu is 0x%llx, want 0x%llx\n", case_name,
			(unsigned long long)role, (unsigned long long)got, (unsigned long long)want);
		return false;
	}
	return true;
}

// A truecolor backend for the resolver cases: the Colorer step runs at 8 bits of color
// and above.
void EnsureTrueColor()
{
	if (g_winport_con_out != nullptr && WINPORT(GetConsoleColorPalette)(NULL) < 8) {
		static_cast<ConsoleOutput *>(g_winport_con_out)->SetBackend(new RgbBackend);
	}
}

void EnsureNoTrueColor()
{
	if (g_winport_con_out != nullptr && WINPORT(GetConsoleColorPalette)(NULL) >= 8) {
		static_cast<ConsoleOutput *>(g_winport_con_out)->SetBackend(nullptr);
	}
}

// The viewer background of the contrast rule: extractColor takes it from the background
// bits of COL_VIEWERTEXT, so a truecolor entry of the color table sets it.
uint64_t SaveBackground()
{
	return FarColors::setcolors[COL_VIEWERTEXT];
}

void SetBackground(uint32_t rgb)
{
	FarColors::setcolors[COL_VIEWERTEXT] = TrueFore(0xffffff) | (uint64_t(rgb) << 40)
		| BACKGROUND_TRUECOLOR;
}

// settings/markdown-colors.ini.  The palette caches the file on its mtime, so every
// write pins an mtime of its own: a seed, so the cache reloads on every round.
int WriteOverrides(const char *text, int seed)
{
	const std::string path = InMyConfig("settings/markdown-colors.ini");
	std::ofstream out(path, std::ios::trunc);
	if (!out) {
		printf("FAILED: cannot write %s\n", path.c_str());
		return 1;
	}
	out << text;
	out.close();
	std::error_code ec;
	std::filesystem::last_write_time(std::filesystem::path(path),
		std::filesystem::file_time_type(std::chrono::seconds(seed)), ec);
	return ec ? 1 : 0;
}

// A fake scheme of one generation, installed as the look source for the process.
FakeLookSource *FakeScheme(unsigned long long gen, const unsigned int *fore,
	const unsigned int *style)
{
	auto fake = std::make_unique<FakeLookSource>();
	fake->SetRows(fore, style);
	fake->m_generation = gen;
	FakeLookSource *keep = fake.get();
	MdSetLookSourceForTest(std::move(fake));
	return keep;
}

// The resolver order of spec.md over a fake scheme: a user ini value wins for its role,
// the Colorer row wins over the fork default, an absent row (fore 0) takes the fork
// default, and a style word with bit 4 sets the underline bit inside the word.
int look_roles()
{
	EnsureTrueColor();
	if (WriteOverrides("[markdown]\nheading1 = #ff0000\n", 1) != 0) {
		return 1;
	}
	static const unsigned int kFore[10] = {0x010203, 0x040506, 0x070809, 0x0a0b0c, 0x0d0e0f,
		0x101112, 0, 0x131415, 0x161718, 0x191a1b};
	static const unsigned int kStyle[10] = {0, 4, 5, 0, 0, 0, 0, 0, 0, 0};
	const uint64_t saved = SaveBackground();
	SetBackground(0x808080);
	FakeScheme(21, kFore, kStyle);

	static const uint64_t s_roles[] = {MD_ROLE_H1, MD_ROLE_H2, MD_ROLE_H3, MD_ROLE_STRONG,
		MD_ROLE_LINK, MD_ROLE_CODE, MD_ROLE_QUOTE, MD_ROLE_RULE, MD_ROLE_EM, MD_ROLE_LISTMARK,
		MD_ROLE_BODY, MD_ROLE_ART, MD_ROLE_TOKEN_KEYWORD};
	for (uint64_t role : s_roles) {
		uint64_t want;
		if (role == MD_ROLE_H1) {
			want = WantLiteral(0xff0000);            // step 1: the user file
		} else if (role == MD_ROLE_QUOTE) {
			want = WantToday(MD_ROLE_QUOTE);          // the absent row (fore 0)
		} else if (role == MD_ROLE_TOKEN_KEYWORD) {
			want = WantToday(MD_ROLE_TOKEN_KEYWORD);  // the token roles have no region
		} else if (role == MD_ROLE_ART) {
			want = WantToday(MD_ROLE_ART);            // art never takes the scheme
		} else if (role == MD_ROLE_BODY) {
			want = WantScheme(kFore[9], kStyle[9]);   // R2: body takes markdown:Text
		} else {
			const int i = SchemeRoleIndex(role);
			want = WantScheme(kFore[i], kStyle[i]);   // step 2: the Colorer row
		}
		if (!CheckRole("look_roles", role, want)) {
			SetBackground((uint32_t)((saved >> 40) & 0xffffff));
			return 1;
		}
	}
	if (!(MdRunColor(MD_ROLE_H2) & kUnderline)) {
		printf("FAILED look_roles: a style-4 heading has no underline bit\n");
		return 1;
	}
	if (MdRunColor(MD_ROLE_STRONG) & kUnderline) {
		printf("FAILED look_roles: a style-0 strong carries the underline bit\n");
		return 1;
	}
	SetBackground((uint32_t)((saved >> 40) & 0xffffff));
	printf("ok look_roles: ini over scheme over fork default, the style-4 underline\n");
	return 0;
}

// The 3:1 rule of the scheme batch (spec, Cache): all rows pass and the Colorer look
// wins; one row below 3.0 sends every role to the fork table; a new scheme generation
// and a new viewer background both recompute the verdict.
int look_contrast()
{
	EnsureTrueColor();
	static const unsigned int kLight[10] = {0xd0d0d0, 0xd8d8d8, 0xe0e0e0, 0xe8e8e8, 0xf0f0f0,
		0xf8f8f8, 0xd0d0ff, 0xffd0d0, 0xd0ffd0, 0xffffd0};
	static const unsigned int kFail[10] = {0xd0d0d0, 0xd8d8d8, 0xe0e0e0, 0xe8e8e8, 0xf0f0f0,
		0xf8f8f8, 0xd0d0ff, 0xffd0d0, 0x303030, 0xffffd0};
	static const unsigned int kStyle[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	const uint64_t saved = SaveBackground();

	// A clean user file: the resolver cases each own their ini round.
	if (WriteOverrides("[markdown]\n", 7) != 0) {
		return 1;
	}

	// All ten rows at 3:1 or more against a dark background: the scheme wins.
	FakeScheme(31, kLight, kStyle);
	SetBackground(0x101010);
	for (uint64_t role : s_scheme_roles) {
		const int i = SchemeRoleIndex(role);
		if (!CheckRole("look_contrast", role, WantScheme(kLight[i], 0))) {
			return 1;
		}
	}
	// One row below 3.0, in a new generation: every role takes the fork table.
	FakeScheme(32, kFail, kStyle);
	for (uint64_t role : s_scheme_roles) {
		if (!CheckRole("look_contrast", role, WantRgb(role))) {
			return 1;
		}
	}
	// The same rows against a light background: the scheme fails and the fork's own RGB
	// defaults fail the rule too: the whole table moves to today's look.
	SetBackground(0xd0d0d0);
	for (uint64_t role : s_scheme_roles) {
		if (!CheckRole("look_contrast", role, WantToday(role))) {
			return 1;
		}
	}
	// A new generation against the dark background again: the scheme wins it back.
	FakeScheme(33, kLight, kStyle);
	SetBackground(0x101010);
	for (uint64_t role : s_scheme_roles) {
		const int i = SchemeRoleIndex(role);
		if (!CheckRole("look_contrast", role, WantScheme(kLight[i], 0))) {
			return 1;
		}
	}
	SetBackground((uint32_t)((saved >> 40) & 0xffffff));
	printf("ok look_contrast: all or none at 3.0, recomputed on a new scheme and theme\n");
	return 0;
}

// The user file over the Colorer look (spec, Resolution order): a set key wins for its
// role, both as a literal and as a palette name; a key without a value falls through.
int look_ini()
{
	EnsureTrueColor();
	static const unsigned int kFore[10] = {0x010203, 0x040506, 0x070809, 0x0a0b0c, 0x0d0e0f,
		0x101112, 0x131415, 0x161718, 0x191a1b, 0x1c1d1e};
	static const unsigned int kStyle[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	const uint64_t saved = SaveBackground();
	SetBackground(0x808080);
	FakeScheme(41, kFore, kStyle);

	// A literal beats the scheme row for heading1 and code; a name beats it for link;
	// a key without a value leaves quote on its scheme row.
	if (WriteOverrides("[markdown]\nheading1 = #00ff00\ncode = #0000ff\nlink = @Help.Topic\n"
		"quote =\n", 2) != 0) {
		return 1;
	}
	if (!CheckRole("look_ini", MD_ROLE_H1, WantLiteral(0x00ff00)))
		return 1;
	if (!CheckRole("look_ini", MD_ROLE_CODE, WantLiteral(0x0000ff)))
		return 1;
	if (!CheckRole("look_ini", MD_ROLE_LINK, WantToday(MD_ROLE_LINK)))
		return 1;
	if (!CheckRole("look_ini", MD_ROLE_QUOTE, WantScheme(kFore[6], 0)))
		return 1;

	// A key that names a role the scheme does not paint: the fork default of the token
	// roles moves, and only it.  A 4-bit name is a literal too, with no contrast check.
	if (WriteOverrides("[markdown]\ntoken-string = #010203\nemphasis = F_LIGHTCYAN\n", 3) != 0) {
		return 1;
	}
	if (!CheckRole("look_ini", MD_ROLE_TOKEN_STRING, WantLiteral(0x010203)))
		return 1;
	if (!CheckRole("look_ini", MD_ROLE_TOKEN_KEYWORD, WantToday(MD_ROLE_TOKEN_KEYWORD)))
		return 1;
	if (!CheckRole("look_ini", MD_ROLE_EM, WithBack(F_LIGHTCYAN)))
		return 1;

	// An empty file: every role back on the scheme.
	if (WriteOverrides("[markdown]\n", 4) != 0) {
		return 1;
	}
	for (uint64_t role : s_scheme_roles) {
		const int i = SchemeRoleIndex(role);
		if (!CheckRole("look_ini", role, WantScheme(kFore[i], 0))) {
			return 1;
		}
	}
	SetBackground((uint32_t)((saved >> 40) & 0xffffff));
	printf("ok look_ini: the user file beats the Colorer row\n");
	return 0;
}

// The scheme generations of spec.md, Cache: the palette fetches a scheme once, and a new
// generation replaces the cached rows.
int look_generation()
{
	EnsureTrueColor();
	static const unsigned int kFore[10] = {0x010203, 0x040506, 0x070809, 0x0a0b0c, 0x0d0e0f,
		0x101112, 0x131415, 0x161718, 0x191a1b, 0x1c1d1e};
	static const unsigned int kNext[10] = {0x0b0c0d, 0x0e0f10, 0x111213, 0x141516, 0x171819,
		0x1a1b1c, 0x1d1e1f, 0x202122, 0x232425, 0x262728};
	static const unsigned int kStyle[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	const uint64_t saved = SaveBackground();
	SetBackground(0x808080);
	FakeLookSource *keep = FakeScheme(51, kFore, kStyle);
	if (WriteOverrides("[markdown]\n", 5) != 0) {
		return 1;
	}
	if (!CheckRole("look_generation", MD_ROLE_H1, WantScheme(kFore[1], 0))) {
		return 1;
	}
	MdRunColor(MD_ROLE_H1);
	if (keep->m_fetches != 1) {
		printf("FAILED look_generation: generation 51 fetched %d times\n", keep->m_fetches);
		return 1;
	}
	// The plugin reports a new scheme after a ReloadBase: the next draw refetches, and
	// the new rows replace the cached ones.
	keep->SetRows(kNext, kStyle);
	keep->m_generation = 52;
	if (!CheckRole("look_generation", MD_ROLE_H1, WantScheme(kNext[1], 0))) {
		return 1;
	}
	MdRunColor(MD_ROLE_H1);
	if (keep->m_fetches != 2) {
		printf("FAILED look_generation: generation 52 fetched %d times\n", keep->m_fetches);
		return 1;
	}
	SetBackground((uint32_t)((saved >> 40) & 0xffffff));
	printf("ok look_generation: one fetch per generation, the new rows replace the cache\n");
	return 0;
}

// The two new roles on the fork path (spec, New roles): the palette table answers with
// no scheme at all, in the 16-color path, in the 256-color look and under truecolor.
int look_new_roles()
{
	if (WriteOverrides("[markdown]\n", 6) != 0) {
		return 1;
	}
	const uint64_t em256 = MdRoleColor256(MD_ROLE_EM);
	const uint64_t listmark256 = MdRoleColor256(MD_ROLE_LISTMARK);
	if (em256 != (TrueFore(0xd7d7af) | F_LIGHTMAGENTA)) {
		printf("FAILED look_new_roles: MdRoleColor256(emphasis) is 0x%llx\n",
			(unsigned long long)em256);
		return 1;
	}
	if (listmark256 != (TrueFore(0x8787af) | F_CYAN)) {
		printf("FAILED look_new_roles: MdRoleColor256(listmark) is 0x%llx\n",
			(unsigned long long)listmark256);
		return 1;
	}

	// The 16-color path: the 256-color look with the 4-bit index in the low bits (R52).
	EnsureNoTrueColor();
	if (!CheckRole("look_new_roles", MD_ROLE_EM, WithBack(em256)))
		return 1;
	if (!CheckRole("look_new_roles", MD_ROLE_LISTMARK, WithBack(listmark256)))
		return 1;

	// Truecolor, no scheme rows: the fork RGB defaults under a dark background, today's
	// look under a mid-luminance one (spec, Resolution order).
	EnsureTrueColor();
	const uint64_t saved = SaveBackground();
	static const unsigned int kZero[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	FakeScheme(71, kZero, kZero);
	SetBackground(0x101010);
	if (!CheckRole("look_new_roles", MD_ROLE_EM, WantRgb(MD_ROLE_EM)))
		return 1;
	if (!CheckRole("look_new_roles", MD_ROLE_LISTMARK, WantRgb(MD_ROLE_LISTMARK)))
		return 1;
	SetBackground(0x808080);
	if (!CheckRole("look_new_roles", MD_ROLE_EM, WantToday(MD_ROLE_EM)))
		return 1;
	if (!CheckRole("look_new_roles", MD_ROLE_LISTMARK, WantToday(MD_ROLE_LISTMARK)))
		return 1;
	SetBackground((uint32_t)((saved >> 40) & 0xffffff));
	printf("ok look_new_roles: emphasis and listmark resolve on the fork path\n");
}

struct Case
{
	const char* name;
	int (*fn)();
};

const Case s_cases[] = {
	{"look_first_call", look_first_call},
	{"look_real_hrd", look_real_hrd},
	{"look_cost", look_cost},
	{"look_bridge", look_bridge},
	{"look_codes", look_codes},
	{"look_bridge_real", look_bridge_real},
	{"look_roles", look_roles},
	{"look_contrast", look_contrast},
	{"look_ini", look_ini},
	{"look_generation", look_generation},
	{"look_new_roles", look_new_roles},
	{"tokens_v2", tokens_v2},
	{"tokens_v2_limits", tokens_v2_limits},
	{"tokens_v2_v1_callers", tokens_v2_v1_callers},
};
} // namespace

// Headless WinPort reports a 4-color palette; --rgb attaches a truecolor
// backend so the plugin's ReloadBase reads far2l.hrd (the rgb scheme).
void EnsureConsoleOutput(bool rgb)
{
	if (g_winport_con_out == nullptr) {
		ConsoleOutput* co = new ConsoleOutput;
		if (rgb) {
			co->SetBackend(new RgbBackend);
		}
		g_winport_con_out = co;
	}
	// far2l main loads the language pack before the plugin manager; the
	// driver needs it too (ApplySudoConfiguration reads Msg strings).  The
	// .lng files sit in the installed bundle, next to the far2l binary.
	const char* app = getenv("FAR2L_APP");
	std::string dir;
	if (app != nullptr) {
		dir = app;
		size_t slash = dir.rfind('/');
		if (slash != std::string::npos) {
			dir.resize(slash);
		}
	}
	if (!dir.empty()) {
		Lang.Init(FARString(dir.c_str()).CPtr(), true, Msg::NewFileName.ID());
	}
}
int main(int argc, char** argv)
{
	bool rgb = false;
	if (argc >= 2 && strcmp(argv[1], "--rgb") == 0) {
		rgb = true;
		++argv;
		--argc;
	}
	EnsureConsoleOutput(rgb);
	g_rgb = rgb;   // the tokens_v2 rows follow the active scheme
	setvbuf(stdout, nullptr, _IONBF, 0);   // the crash trace keeps the case log
	// Same startup as far2l main (main.cpp:431-437): the plugin manager scans
	// `Plugins` next to the app binary (FAR2L_APP).
	Opt.LoadPlug.MainPluginDir = TRUE;
	Opt.LoadPlug.PluginsPersonal = TRUE;
	Opt.LoadPlug.PluginsCacheOnly = FALSE;
	Opt.LoadPlug.ScanSymlinks = TRUE;   // Plugins is a symlink to the bundle
	// The driver binary sits in the build tree; the app's Plugins tree sits
	// next to the app binary.  Scan the bundle dir (FAR2L_APP, the script sets it).
	const char* app = getenv("FAR2L_APP");
	if (app != nullptr) {
		std::string dir = app;
		size_t slash = dir.rfind('/');
		if (slash != std::string::npos) {
			dir.resize(slash);
			dir += "/";
			g_strFarPath = FARString(dir.c_str());
		}
	}
	// g_strFarPath keeps the trailing slash (main.cpp:440); the plugin
	// manager appends `Plugins` to it (plugins.cpp:362).
	const char* want = nullptr;
	if (argc >= 3 && strcmp(argv[1], "--case") == 0) {
		want = argv[2];
	} else if (argc > 3) {
		fprintf(stderr, "usage: look_driver [--rgb] [--case <name>]\n");
		return 2;
	}

	// A typo in --case must not pass as success.
	if (want != nullptr) {
		bool known = false;
		for (const Case& c : s_cases) {
			if (strcmp(c.name, want) == 0) {
				known = true;
				break;
			}
		}
		if (!known) {
			fprintf(stderr, "unknown case: %s\n", want);
			return 1;
		}
	}

	for (const Case& c : s_cases) {
		if (want != nullptr && strcmp(c.name, want) != 0) {
			continue;
		}
		if (c.fn() != 0) {
			++g_failures;
			break;
		}
		printf("ok %s\n", c.name);
	}
	fflush(stdout);
	fflush(stderr);
	// The Colorer plugin's static teardown throws with the headless console: leave it loaded (R49).
	_exit(g_failures == 0 ? 0 : 1);
}
