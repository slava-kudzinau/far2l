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
#include "markdown/MarkdownRender.hpp"
#include "Colorspace.h"   // iRGB, toIRGB: the fail-path split below
#include "cfg/config.hpp"
#include "mix/pathmix.hpp"
#include "farcolors.hpp"
#include "markdown/MdPalette.hpp"
#include "ViewerPrinter.hpp"
#include "interf.hpp"
#include "scrbuf.hpp"

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
	L"  const s = \"\\lit\\n\" + 42 * true;",
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
// foreground, its 4-bit foreground, its style word.  The look columns are the
// active scheme's row: `fg`/`style` are far2l.hrd's (--rgb; the console
// scheme's rows carry no style), `fg4` console default.hrd's (no --rgb).
struct V2Want
{
	int line;
	int col;
	const wchar_t* what;
	uint64_t fg;   // the far2l.hrd row (--rgb)
	uint64_t fg4;  // the console scheme's 4-bit row (no --rgb)
	uint64_t style; // far2l.hrd's style word (the console rows carry none)
};

// One expectation, by a row of the table below: the last cell covering the
// row's column of a one-line call carries the row's 24-bit foreground, the
// scheme's 4-bit foreground, the row's style word, and three zeros.  The
// active scheme decides which side is measured: --rgb runs far2l.hrd (the
// row's 24-bit and style columns), without it console default.hrd (the row's
// 4-bit column).  The companions are the same scheme through the palette's
// quantize: far2l.hrd's 24-bit value under the 16-color scheme, the console
// row expanded back to 24 bits under the rgb scheme, both indexed by row.
bool Expect(const V2Out& out, int idx, const V2Want& want)
{
	static const uint64_t s_rgb4[] = {15, 9, 15, 15, 14, 10, 15, 13, 9};
	static const uint64_t s_console24[] = {
		0xffffff, 0x55ffff, 0xffffff, 0xffff55, 0x00aaaa, 0x55ff55, 0xffffff, 0x55ffff,
		0x00aaaa,
	};
	const uint64_t want_fg = g_rgb ? want.fg : s_console24[idx];
	const uint64_t want_fg4 = g_rgb ? s_rgb4[idx] : want.fg4;
	const uint64_t want_style = g_rgb ? want.style : 0;
	const uint64_t* cell = out.Cover(want.col);
	if (cell == nullptr) {
		printf("FAILED tokens_v2: %S: no cell covers col %d\n", want.what, want.col);
		for (int i = 0; i < out.Count(); ++i) {
			const uint64_t* c = out.At(i);
			printf("  cell %d: [%llu,%llu] 0x%llX/%llu style %llu\n", i,
				(unsigned long long)c[0], (unsigned long long)c[1],
				(unsigned long long)c[2], (unsigned long long)c[3],
				(unsigned long long)c[4]);
		}
		return false;
	}
	if (cell[2] != want_fg || cell[3] != want_fg4 || cell[4] != want_style
		|| cell[5] != 0 || cell[6] != 0 || cell[7] != 0) {
		printf("FAILED tokens_v2: %S col %d is [0x%llX/%llu style %llu %llu,%llu,%llu], want [0x%llX/%llu style %llu 0,0,0]\n",
			want.what, want.col,
			(unsigned long long)cell[2], (unsigned long long)cell[3],
			(unsigned long long)cell[4], (unsigned long long)cell[5],
			(unsigned long long)cell[6], (unsigned long long)cell[7],
			(unsigned long long)want_fg, (unsigned long long)want_fg4,
			(unsigned long long)want_style);
		return false;
	}
	printf("probe %S: [0x%llX/%llu style %llu] under %s\n", want.what,
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
	// leaves the fore unset, the F4 chain falls to def:Text.  The string's
	// escape (`\n` in the literal) is the one row with a style word:
	// far2l.hrd's def:StringContent writes style='1'.
	static const V2Want s_want[] = {
		{0, 4,  L"keyword",    0xffffff, 15, 0}, // def:Keyword
		{0, 12, L"function",   0x6060f0, 11, 0}, // def:Function
		{1, 5,  L"keyword2",   0xffffff, 15, 0}, // def:Keyword (const)
		{1, 15, L"string",     0x88ee88, 14, 0}, // def:String (the whole literal)
		{1, 18, L"escape",     0x999911,  3, 1}, // def:StringContent (\n)
		{1, 24, L"number",     0x00e020, 10, 0}, // def:NumberDec -> def:Number (#A)
		{1, 27, L"operator",   0xe0e0c0, 15, 0}, // def:Symbol (*)
		{1, 30, L"constant",   0xa030ff, 11, 0}, // def:BooleanConstant
		{2, 15, L"comment",    0x696989,  8, 0}, // def:Comment
	};
	for (int w = 0; w < 9; ++w) {
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
		if (!Expect(out, w, want)) {
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
		if (!Expect(tail, 8, s_want[8])) {
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
	// The tail identifier keeps no class (`  tailSum` in cells [0,9); `;` at 9 is SymbolStrong).
	int tail_emits = 0;
	for (const Emit& e : *sink) {
		if (e.line == 2 && e.start < 9) {
			++tail_emits; // `  tailSum` carries no class
		}
	}
	if (tail_emits != 0) {
		printf("FAILED tokens_v2_v1_callers: the tail gave %d V1 emits, want 0\n", tail_emits);
		return 1;
	}
	// V2 opens the fence with the function scheme's cell (the name's
	// region, V1's second class-1 emit), then the keyword's leaf - the
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

// UTF-8 to wide: the hostile fixture carries non-ASCII (CJK, emoji, the
// Gothic surrogate pair); the viewer reads its file as wide text.
std::wstring Utf8ToWide(const std::string &s)
{
	std::wstring out;
	out.reserve(s.size());
	size_t i = 0;
	while (i < s.size()) {
		const unsigned char c = static_cast<unsigned char>(s[i]);
		uint32_t cp = c;
		int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
		if (extra != 0 && i + extra < s.size()) {
			cp = c & (0xFF >> (extra + 1));
			for (int k = 1; k <= extra; ++k) {
				cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
			}
		} else {
			extra = 0;
		}
		i += static_cast<size_t>(extra) + 1;
		if (cp >= 0x10000) {
			cp -= 0x10000;
			out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
			out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
		} else {
			out.push_back(static_cast<wchar_t>(cp));
		}
	}
	return out;
}

// A counting shell over the real plugin's fence source: the doc path's fence
// cache, probed - one fetch per (language, hash, generation).
class CountingFenceSource : public MdTokenSource
{
public:
	MdTokenSource *m_real = nullptr;
	int m_fetches = 0;
	bool Tokenize(const std::string &lang, const std::vector<std::string> &lines,
		int budget_ms, std::vector<MdToken> &out) override
	{
		return m_real->Tokenize(lang, lines, budget_ms, out);
	}
	bool TokenizeV2(const std::string &lang, const std::vector<std::string> &lines,
		int budget_ms, std::vector<MdTokenRange> &out,
		unsigned long long &generation) override
	{
		++m_fetches;
		return m_real->TokenizeV2(lang, lines, budget_ms, out, generation);
	}
	bool Ready() override { return m_real->Ready(); }
};

// T4: the fences' cost through the real plugin.  Six 20k-line fences, each
// salted unique, so every fence renders exactly once - the cache's cold key
// (language, text hash, generation), one fetch per fence, counted.  A fence's
// render is the doc's Render: the join, the parse, the fetch, the printer.
// The deadline is the viewer's 100 ms per fence (spec, Runtime).
int tokens_v2_cost()
{
	std::string why;
	if (!LoadModules(why) || !CreateEditorSet(why)) {
		printf("FAILED tokens_v2_cost: %s\n", why.c_str());
		return 1;
	}
	auto src = std::make_unique<CountingFenceSource>();
	CountingFenceSource *count = src.get();
	count->m_real = &MdGetTokenSource();
	MdSetTokenSourceForTest(std::move(src));
	constexpr int kFences = 6;
	constexpr int kFenceLines = 20000;
	constexpr int kBudgetMs = 100;
	double total_ms = 0, worst_ms = 0;
	// One warm render, not counted: the first Render pays the cold pages of
	// the doc and the plugin, the story's number is the steady cost.
	{
		std::wstring warm = L"```javascript\n";
		for (int i = 0; i < 200; ++i) {
			warm += kJsFence[i % 3];
			warm += L" // warm\n";
		}
		warm += L"```\n";
		MarkdownDoc doc;
		doc.Render(warm, 80);
	}
	const int base = count->m_fetches;
	for (int f = 0; f < kFences; ++f) {
		std::wstring text = L"```javascript\n";
		for (int i = 0; i < kFenceLines; ++i) {
			text += kJsFence[i % 3];
			text += L" // f";
			text += std::to_wstring(f);
			text += L'-';
			text += std::to_wstring(i);
			text += L'\n';
		}
		text += L"```\n";
		MarkdownDoc doc;
		const auto t0 = std::chrono::steady_clock::now();
		doc.Render(text, 80);
		const double ms = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - t0).count() / 1000.0;
		bool any_range = false;
		for (const MdTokenGroup &g : doc.TokenGroups()) {
			if (!g.ranges.empty()) {
				any_range = true;
				break;
			}
		}
		if (!any_range) {
			printf("FAILED tokens_v2_cost: fence %d rendered no scheme ranges\n", f);
			MdSetTokenSourceForTest(nullptr);
			return 1;
		}
		if (ms > kBudgetMs) {
			printf("FAILED tokens_v2_cost: fence %d rendered in %.1f ms, want under %d\n",
				f, ms, kBudgetMs);
			MdSetTokenSourceForTest(nullptr);
			return 1;
		}
		total_ms += ms;
		if (ms > worst_ms) {
			worst_ms = ms;
		}
		printf("fence %d: %d javascript lines in %.1f ms\n", f, kFenceLines, ms);
	}
	if (count->m_fetches != base + kFences) {
		printf("FAILED tokens_v2_cost: the fences fetched %d, want one per fence\n",
			count->m_fetches - base);
		MdSetTokenSourceForTest(nullptr);
		return 1;
	}
	printf("tokens_v2_cost: %d fences x %d lines (javascript), mean %.1f ms, worst %.1f ms, deadline %d ms\n",
		kFences, kFenceLines, total_ms / kFences, worst_ms, kBudgetMs);

	// The hostile fixture: 1 MB of python - long lines, nested regions,
	// unterminated strings, unicode - through the same fence path.  The first
	// render fetches the fence; the second render of the same fence rides the
	// cache, one fetch per (language, hash, generation).  look-driver.sh
	// exports FAR2L_TESTING_DIR; the fallback serves a run from the root.
	const char *dir = getenv("FAR2L_TESTING_DIR");
	const std::string path = (dir != nullptr && *dir != '\0')
		? std::string(dir) + "/fixture.py" : "testing/markdown/fixture.py";
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		printf("FAILED tokens_v2_cost: no fixture at %s\n", path.c_str());
		MdSetTokenSourceForTest(nullptr);
		return 1;
	}
	const std::string bytes((std::istreambuf_iterator<char>(in)),
		std::istreambuf_iterator<char>());
	if (bytes.size() < 1048576) {
		printf("FAILED tokens_v2_cost: fixture is %zu bytes, want 1 MB\n", bytes.size());
		MdSetTokenSourceForTest(nullptr);
		return 1;
	}
	const std::wstring md = L"```python\n" + Utf8ToWide(bytes) + L"```\n";
	MarkdownDoc hostile;
	const int hostile_base = count->m_fetches;
	const auto t1 = std::chrono::steady_clock::now();
	hostile.Render(md, 80);
	const double ms1 = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - t1).count() / 1000.0;
	const auto t2 = std::chrono::steady_clock::now();
	hostile.Render(md, 80); // the same fence again: the cache answers
	const double ms2 = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - t2).count() / 1000.0;
	bool hostile_range = false;
	for (const MdTokenGroup &g : hostile.TokenGroups()) {
		if (!g.ranges.empty()) {
			hostile_range = true;
			break;
		}
	}
	if (!hostile_range) {
		printf("FAILED tokens_v2_cost: the hostile fence rendered no scheme ranges\n");
		MdSetTokenSourceForTest(nullptr);
		return 1;
	}
	if (ms1 > kBudgetMs) {
		printf("FAILED tokens_v2_cost: the hostile fence rendered in %.1f ms, want under %d\n",
			ms1, kBudgetMs);
		MdSetTokenSourceForTest(nullptr);
		return 1;
	}
	if (count->m_fetches != hostile_base + 1) {
		printf("FAILED tokens_v2_cost: the hostile fence fetched %d, want one (the cache refetched)\n",
			count->m_fetches - hostile_base);
		MdSetTokenSourceForTest(nullptr);
		return 1;
	}
	printf("tokens_v2_cost: hostile fixture.py %zu bytes, first %.1f ms, cached %.1f ms, deadline %d ms\n",
		bytes.size(), ms1, ms2, kBudgetMs);
	printf("ok tokens_v2_cost: the 20k-line fences and the 1 MB fixture inside the deadline, fetches=%d\n",
		count->m_fetches);
	MdSetTokenSourceForTest(nullptr);
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

// The viewer background of the contrast rule: the resolver reads it from the background
// bits of COL_VIEWERTEXT, so a truecolor entry of the color table sets it.
uint64_t SaveBackground()
{
	return FarColors::setcolors[COL_VIEWERTEXT];
}

// The rgb is a written #RRGGBB color packed the way the app packs a
// background (COMPOSE_RGB, low byte red), so a round can use the scheme's
// real rows on the real navy background.
void SetBackground(uint32_t rgb)
{
	const uint32_t bgr = ((rgb & 0xff) << 16) | (rgb & 0xff00) | ((rgb >> 16) & 0xff);
	FarColors::setcolors[COL_VIEWERTEXT] = TrueFore(0xffffff) | (uint64_t(bgr) << 40)
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

	// The real decode (the byte-order regression, 2026-11-07): the rows are the
	// far2l.hrd parents of the markdown regions, written #RRGGBB in an attribute;
	// a decoder that reads the high byte as red fails EmText and drops the whole
	// look.  Every row must clear 3:1 on the navy viewer background.
	static const unsigned int kScheme[10] = {0x99eeee, 0x8888ff, 0x8888ff, 0x40a000, 0xff20a0,
		0xffe977, 0x6699ff, 0xffffff, 0xffffff, 0x99eeee};
	FakeScheme(34, kScheme, kStyle);
	SetBackground(0x000080);
	for (uint64_t role : s_scheme_roles) {
		if (!CheckRole("look_contrast", role, WantScheme(kScheme[SchemeRoleIndex(role)], 0))) {
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
	return 0;
}

// ---- T1 case (story markdown-code-tint): the field, the blend, the verdict ----

// The 24-bit background of a far attribute: the truecolor bits when set, else the
// palette's background slot (the base of MdCodeTint, the IndexedBackgroundRgb of
// far2l/src/filediff.cpp).
uint32_t AttrBack(uint64_t attr)
{
	if (attr & BACKGROUND_TRUECOLOR)
		return (uint32_t)((attr >> 40) & 0xffffff);
	return Palette::FARPalette[(attr & B_MASK) >> 4] & 0x00ffffff;
}

// The 24-bit foreground of a far attribute: the truecolor bits, else the palette's
// foreground slot (extractColor reads a 4-bit index from slot 16).
uint32_t AttrFore(uint64_t attr)
{
	if (attr & FOREGROUND_TRUECOLOR)
		return (uint32_t)((attr >> 16) & 0xffffff);
	return Palette::FARPalette[16 + (attr & 0x0f)] & 0x00ffffff;
}

// The palette's blend, the driver's copy: one eighth of `mix` into `base`, both
// 0xBBGGRR (the BlendRgb pattern of far2l/src/filediff.cpp, alpha 32/256).
uint32_t BlendEighth(uint32_t base, uint32_t mix)
{
	constexpr int alpha = 32;
	constexpr int inv = 256 - alpha;
	const uint32_t r = (uint32_t)(((base & 0xff) * inv + (mix & 0xff) * alpha) >> 8);
	const uint32_t g = (uint32_t)((((base >> 8) & 0xff) * inv + ((mix >> 8) & 0xff) * alpha) >> 8);
	const uint32_t b = (uint32_t)((((base >> 16) & 0xff) * inv + ((mix >> 16) & 0xff) * alpha) >> 8);
	return r | (g << 8) | (b << 16);
}

// The code block tint (story markdown-code-tint, plan T1): a fence's code rows carry the
// tint field, rows outside the block are plain; the
// row's background is the blend of one eighth of the resolved code foreground into the
// viewer background; a low-contrast pair (code foreground, tint) draws no tint; a
// viewer background change is a new pair and answers a new blend (the cache key of
// MdCodeTint - no time cache).
int tint()
{
	EnsureTrueColor();
	if (WriteOverrides("[markdown]\n", 81) != 0) {
		return 1;
	}
	// All ten rows bright against a dark background: the Colorer look wins (story
	// markdown-colorer-look, spec, Cache), so the code role is the scheme's
	// markdown:Code fore, a color the driver owns.
	static const unsigned int kFore[10] = {0xd0d0d0, 0xd8d8d8, 0xe0e0e0, 0xe8e8e8, 0xf0f0f0,
		0xf8f8f8, 0xd0d0ff, 0xffd0d0, 0xd0ffd0, 0xffffd0};
	static const unsigned int kStyle[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	const uint64_t saved = SaveBackground();
	FakeScheme(61, kFore, kStyle);
	SetBackground(0x101010);

	// 1. The field: the fence's rows carry the tint, rows outside the block are plain.
	MarkdownDoc doc;
	doc.Render(L"para\n\n```cpp\nint x;\n```\n\nafter\n", 80);
	{
		const std::vector<MdLine> &lines = doc.Lines();
		int code_rows = 0, plain = 0;
		for (const MdLine &l : lines) {
			bool is_code = false;
			for (const MdRun &r : l.runs) {
				if (r.role == MD_ROLE_CODE)
					is_code = true;
			}
			if (is_code && l.tint < 0) {
				printf("FAILED tint: a fence row has no tint\n");
				return 1;
			}
			if (!is_code && l.tint >= 0) {
				printf("FAILED tint: a row outside the block carries a tint\n");
				return 1;
			}
			is_code ? ++code_rows : ++plain;
		}
		if (code_rows == 0 || plain == 0) {
			printf("FAILED tint: the fixture has no fence row or no plain row\n");
			return 1;
		}
		printf("ok tint: %d fence rows carry the tint, %d rows outside are plain\n",
			code_rows, plain);
	}

	// The code rows of an undrawn mermaid fence carry the tint too (spec, Row tint).
	MarkdownDoc mer;
	mer.Render(L"```mermaid\ngraph XX\nA\n```\n", 80);
	{
		const std::vector<MdLine> &lines = mer.Lines();
		if (lines.size() != 2) {
			printf("FAILED tint: the mermaid fixture made %zu rows, want the two code rows\n",
				lines.size());
			return 1;
		}
		for (size_t i = 0; i < lines.size(); ++i) {
			if (lines[i].tint < 0) {
				printf("FAILED tint: the undrawn fence row %zu lost the tint\n", i);
				return 1;
			}
		}
		printf("ok tint: the undrawn fence rows carry the tint\n");
	}

	// 2. The blend: the row background equals the blend of the same inputs - the live
	// viewer background and the resolved code color, mixed by the driver's copy of the
	// palette's BlendRgb.
	const uint64_t bg_attr = FarColorToReal(COL_VIEWERTEXT);
	const uint64_t code_attr = MdRunColor(MD_ROLE_CODE);
	const uint32_t want = BlendEighth(AttrBack(bg_attr), AttrFore(code_attr));
	const int64_t got = MdCodeTint(bg_attr);
	if (got != (int64_t)want) {
		printf("FAILED tint: the code row draws 0x%06llx, want 0x%06x (code 0x%06x on bg 0x%06x)\n",
			(unsigned long long)got, want, (unsigned)AttrFore(code_attr), (unsigned)AttrBack(bg_attr));
		return 1;
	}
	printf("tint: code 0x%06x on bg 0x%06x draws 0x%06x\n", (unsigned)AttrFore(code_attr),
		(unsigned)AttrBack(bg_attr), want);
	if (MdCodeTint(bg_attr) != got) {   // the same pair answers the same verdict
		printf("FAILED tint: the same pair answered a second value\n");
		return 1;
	}

	// 3. A low-contrast pair draws no tint: the code foreground equals the viewer
	// background, so the blend is the background and the pair sits at 1:1.
	if (WriteOverrides("[markdown]\ncode = #808080\n", 82) != 0) {
		SetBackground((uint32_t)((saved >> 40) & 0xffffff));
		return 1;
	}
	SetBackground(0x808080);
	if (MdCodeTint(FarColorToReal(COL_VIEWERTEXT)) != -1) {
		printf("FAILED tint: a low-contrast pair drew a tint\n");
		return 1;
	}
	printf("ok tint: 0x808080 code on a 0x808080 background draws no tint\n");

	// 4. A viewer background change: the same code color, a new pair, a new blend.
	SetBackground(0x202020);
	const uint64_t bg2 = FarColorToReal(COL_VIEWERTEXT);
	const uint32_t want2 = BlendEighth(AttrBack(bg2), AttrFore(MdRunColor(MD_ROLE_CODE)));
	const int64_t got2 = MdCodeTint(bg2);
	if (got2 != (int64_t)want2) {
		printf("FAILED tint: the new background draws 0x%06llx, want 0x%06x\n",
			(unsigned long long)got2, want2);
		return 1;
	}
	printf("ok tint: a background change redraws 0x%06x on the new pair\n", want2);

	// 5. The runs ride the tint (review): a printed code row is one uniform tint
	// background.  The cells the runs paint - a cell under a token, and the cells
	// past the end of the text - carry the same 24-bit background: the driver's own
	// BlendEighth of the code foreground into the viewer background, not the
	// product's output.  The token slice keeps its foreground.
	if (WriteOverrides("[markdown]\n", 83) != 0) {
		return 1;
	}
	SetBackground(0x101010);
	const uint64_t bg3 = FarColorToReal(COL_VIEWERTEXT);
	const uint32_t want3 = BlendEighth(AttrBack(bg3), AttrFore(MdRunColor(MD_ROLE_CODE)));
	// The virtual screen behind Text/GetText exists only after far2l's console
	// init; the headless driver allocates the standard 80x25 here (idempotent)
	// and sets the screen extents the console calls clip against.
	ScrBuf.AllocBuf(80, 25);
	COORD con_size{};
	GetVideoMode(con_size);
	{
		size_t row = 0;
		while (row < doc.Lines().size() && doc.Lines()[row].tint < 0)
			++row;
		if (row >= doc.Lines().size()) {
			printf("FAILED tint: the fixture has no code row to print\n");
			return 1;
		}
		const MdLine &code_line = doc.Lines()[row];
		int text_end = 0;   // one past the last text cell (the runs' total width)
		for (const MdRun &run : code_line.runs)
			text_end += (int)run.text.size();
		// A cell under a token: the group's first range on the tokenized path, the
		// first cell of the code text (the 4-cell indent counted) on the plain one.
		// V2 ranges carry the scheme's own color, so the expected foreground is
		// the range's resolved color, not a role alias (ADR-010).
		int token_col = 4;
		uint64_t want_fore = MdRunColor(MD_ROLE_CODE) & ~kBack;
		if (code_line.tokens >= 0
			&& (size_t)code_line.tokens < doc.TokenGroups().size()
			&& !doc.TokenGroups()[code_line.tokens].ranges.empty()) {
			const MdTokenRange &rng = doc.TokenGroups()[code_line.tokens].ranges[0];
			token_col = (int)rng.start;
			want_fore = MdRangeColor(rng.fg, rng.fg4, rng.underline) & ~kBack;
		}
		MarkdownPrinter printer(FarColorToReal(COL_VIEWERSELECTEDTEXT),
			&doc.Lines(), &doc.TokenGroups());
		for (size_t i = 0; i <= row; ++i) {
			GotoXY(0, 3 + (int)i);
			printer.Print(0, 80, L"");
		}
		// The runs land in the virtual screen (ScrBuf), the fill straight on the
		// console buffer; the flush merges them, the way ShowScreen does.
		ScrBuf.Flush();
		std::vector<CHAR_INFO> cells(80);
		GetText(0, 3 + (int)row, 79, 3 + (int)row, cells.data(),
			80 * (int)sizeof(CHAR_INFO));
		static const int s_cols[] = {token_col, text_end, 79};
		for (int col : s_cols) {
			const CHAR_INFO &c = cells[col];
			if (!(c.Attributes & BACKGROUND_TRUECOLOR)
				|| (uint32_t)((c.Attributes >> 40) & 0xffffff) != want3) {
				printf("FAILED tint: the cell at column %d draws back 0x%06llx (tc=%d), want 0x%06x\n",
					col, (unsigned long long)((c.Attributes >> 40) & 0xffffff),
					(c.Attributes & BACKGROUND_TRUECOLOR) ? 1 : 0, want3);
				SetBackground((uint32_t)((saved >> 40) & 0xffffff));
				return 1;
			}
		}
		// The slice under a token keeps its role's true-color foreground; the tint
		// rides under it as the row background, one does not erase the other: the
		// slice's color side must equal the role color the driver's own palette copy
		// computes (want_fore, kBack's background side dropped), and the background
		// loop above pins the slice, the cells past the text and the far corner to
		// the tint.
		if ((cells[token_col].Attributes & FOREGROUND_TRUECOLOR) == 0
			|| ((cells[token_col].Attributes >> 16) & 0xffffff) == 0
			|| (cells[token_col].Attributes & ~kBack) != (want_fore & ~kBack)) {
			printf("FAILED tint: the cell under the token is 0x%llx, want the role "
				"foreground on the tint (want 0x%llx)\n",
				(unsigned long long)cells[token_col].Attributes,
				(unsigned long long)want_fore);
			SetBackground((uint32_t)((saved >> 40) & 0xffffff));
			return 1;
		}
		printf("ok tint: the code row (y=%d): the cell under the token (%d) and the cells "
			"past the text (%d, 79) draw the tint 0x%06x; the token keeps its foreground\n",
			3 + (int)row, token_col, text_end, want3);
	}

	SetBackground((uint32_t)((saved >> 40) & 0xffffff));
	return 0;
}

// T2: the render's token cache keyed on the scheme generation (spec.md,
// Cache; story markdown-colorer-fences).  A fake look answers two
// generations, a fake token source answers ranges; the second render
// refetches every fence once, a resize does not.  The report prints the
// fetch count.
class FenceLookSource : public MdLookSource
{
public:
	unsigned long long m_generation = 52;
	unsigned long long Generation() override { return m_generation; }
	unsigned long long Fetch(const char *const *names, int count, MdRegionLook *out) override
	{
		(void)names; (void)count; (void)out;
		return m_generation;
	}
};

// One range over each fence line, colored by the case's current fg.  The
// generation rides the answer unchanged: the key of the entry equals the
// generation the source answers (the real plugin answers its scheme's), so
// the render's key-update path stays warm across a resize.
class FenceTokenSource : public MdTokenSource
{
public:
	int m_fetches = 0;
	uint32_t m_fg = 0x00ff00;
	unsigned long long m_generation = 52;   // the scheme the colors come from
	bool Tokenize(const std::string &lang, const std::vector<std::string> &lines,
		int budget_ms, std::vector<MdToken> &out) override
	{
		(void)lang; (void)budget_ms;
		for (size_t i = 0; i < lines.size(); ++i) {
			out.push_back(MdToken{0, 3, 1, static_cast<uint32_t>(i)});
		}
		return true;
	}
	bool TokenizeV2(const std::string &lang, const std::vector<std::string> &lines,
		int budget_ms, std::vector<MdTokenRange> &out,
		unsigned long long &generation) override
	{
		(void)lang; (void)budget_ms;
		++m_fetches;
		generation = m_generation;   // the key's generation: the scheme of the colors
		for (size_t i = 0; i < lines.size(); ++i) {
			out.push_back(MdTokenRange{0, 3, m_fg, 7, false, static_cast<uint32_t>(i)});
		}
		return true;
	}
};

// Two fences, one language: the render fetches each once (the shared 40 ms
// budget answers within a millisecond of work).
int look_fence_generation()
{
	auto look = std::make_unique<FenceLookSource>();
	FenceLookSource *probe = look.get();
	MdSetLookSourceForTest(std::move(look));
	auto src = std::make_unique<FenceTokenSource>();
	FenceTokenSource *fake = src.get();
	MdSetTokenSourceForTest(std::move(src));
	const wchar_t *const md = L"```js\nvar x;\n```\n\n```js\nlet y;\n```\n";

	MarkdownDoc doc;
	doc.Render(md, 80);
	if (fake->m_fetches != 2) {
		printf("FAILED look_fence_generation: first render fetched %d, want 2\n",
			fake->m_fetches);
		MdSetTokenSourceForTest(nullptr);
		MdSetLookSourceForTest(nullptr);
		return 1;
	}

	doc.Invalidate(72); // a resize: the cache survives, no refetch (R30); the
	// fake answers the look's generation, so the key the first render stored
	// (the render updates it from the answer) re-hits: the fetches stay 2,
	// pinning the key-update path.
	if (fake->m_fetches != 2) {
		printf("FAILED look_fence_generation: resize refetched (%d fetches)\n",
			fake->m_fetches);
		MdSetTokenSourceForTest(nullptr);
		MdSetLookSourceForTest(nullptr);
		return 1;
	}

	probe->m_generation = 0x12345678ULL; // a scheme change
	fake->m_generation = 0x12345678ULL;   // the new scheme answers its generation
	fake->m_fg = 0x00ffff;               // the new scheme's rows
	doc.Render(md, 80);
	if (fake->m_fetches != 4) {
		printf("FAILED look_fence_generation: scheme change fetched %d total, want 4\n",
			fake->m_fetches);
		MdSetTokenSourceForTest(nullptr);
		MdSetLookSourceForTest(nullptr);
		return 1;
	}
	if (doc.Lines().size() < 3) {
		printf("FAILED look_fence_generation: %zu lines\n", doc.Lines().size());
		MdSetTokenSourceForTest(nullptr);
		MdSetLookSourceForTest(nullptr);
		return 1;
	}
	if (doc.Lines()[0].tokens < 0 || doc.Lines()[2].tokens < 0) {
		printf("FAILED look_fence_generation: a fence line carries no group\n");
		MdSetTokenSourceForTest(nullptr);
		MdSetLookSourceForTest(nullptr);
		return 1;
	}
	const MdTokenGroup &g0 = doc.TokenGroups()[doc.Lines()[0].tokens];
	const MdTokenGroup &g1 = doc.TokenGroups()[doc.Lines()[2].tokens];
	if (g0.ranges.size() != 1 || g0.ranges[0].start != 4 || g0.ranges[0].end != 7
		|| g0.ranges[0].fg != 0x00ffff || g0.ranges[0].fg4 != 7
		|| g1.ranges.size() != 1 || g1.ranges[0].fg != 0x00ffff) {
		printf("FAILED look_fence_generation: refetch kept the old color or lost the range\n");
		MdSetTokenSourceForTest(nullptr);
		MdSetLookSourceForTest(nullptr);
		return 1;
	}

	// Nested spans and indented fences (regression: outer PairedBrackets span
	// covering `{ ... }` must not overwrite inner keyword/string/comment spans;
	// a fence inside a list or blockquote must shift its token cells by the
	// row's lead width).
	const int fetches = fake->m_fetches;
	{
		class NestedTokenSource : public MdTokenSource
		{
		public:
			bool Tokenize(const std::string &, const std::vector<std::string> &,
				int, std::vector<MdToken> &) override { return true; }
			bool TokenizeV2(const std::string &, const std::vector<std::string> &,
				int, std::vector<MdTokenRange> &out, unsigned long long &gen) override
			{
				gen = 99;
				// Outer block [0, 12) in plain fg, inner string [2, 7) in green,
				// inner escape [4, 6) in yellow.
				out.push_back(MdTokenRange{0, 12, 0x99eeee, 7, false, 0});
				out.push_back(MdTokenRange{2, 7, 0x88ee88, 10, false, 0});
				out.push_back(MdTokenRange{4, 6, 0x999911, 14, true, 0});
				return true;
			}
		};
		MdSetTokenSourceForTest(std::make_unique<NestedTokenSource>());
		MarkdownDoc ndoc;
		ndoc.Render(L"1. item\n\n   ```js\n   ['\\n']\n   ```\n", 80);
		size_t frow = 0;
		while (frow < ndoc.Lines().size() && ndoc.Lines()[frow].tokens < 0)
			++frow;
		if (frow >= ndoc.Lines().size()) {
			printf("FAILED look_fence_generation: list-indented fence has no token group\n");
			MdSetTokenSourceForTest(nullptr);
			MdSetLookSourceForTest(nullptr);
			return 1;
		}
		const MdTokenGroup &ng = ndoc.TokenGroups()[ndoc.Lines()[frow].tokens];
		// List indent is 2 cells + CODE_INDENT 4 cells = 6 cells offset.
		// Disjoint slices: [6,8) outer, [8,10) string, [10,12) escape, [12,13) string, [13,18) outer.
		if (ng.ranges.size() != 5
			|| ng.ranges[0].start != 6 || ng.ranges[0].end != 8 || ng.ranges[0].fg != 0x99eeee
			|| ng.ranges[1].start != 8 || ng.ranges[1].end != 10 || ng.ranges[1].fg != 0x88ee88
			|| ng.ranges[2].start != 10 || ng.ranges[2].end != 12 || ng.ranges[2].fg != 0x999911 || !ng.ranges[2].underline
			|| ng.ranges[3].start != 12 || ng.ranges[3].end != 13 || ng.ranges[3].fg != 0x88ee88
			|| ng.ranges[4].start != 13 || ng.ranges[4].end != 18 || ng.ranges[4].fg != 0x99eeee) {
			printf("FAILED look_fence_generation: nested/indented ranges wrong (count=%zu)\n",
				ng.ranges.size());
			for (size_t i = 0; i < ng.ranges.size(); ++i) {
				printf("  range %zu: [%u,%u) fg=0x%06x ul=%d\n", i,
					ng.ranges[i].start, ng.ranges[i].end, ng.ranges[i].fg,
					ng.ranges[i].underline ? 1 : 0);
			}
			MdSetTokenSourceForTest(nullptr);
			MdSetLookSourceForTest(nullptr);
			return 1;
		}
	}

	printf("ok look_fence_generation: generation-keyed cache, nested/indented ranges, fetches=%d\n",
		fetches);
	MdSetTokenSourceForTest(nullptr);
	MdSetLookSourceForTest(nullptr);
	return 0;
}

// T3: the one view-wide contrast verdict of spec.md, Contrast (story
// markdown-colorer-fences).  The look's batch verdict, keyed (viewer
// background, scheme generation), decides for the whole view: a scheme whose
// foregrounds fail 3:1 sends the fences to the plain code role and the prose
// to the fork table - the same verdict both paths consult, printed once per
// round.  A passing scheme sends the scheme colors to the fences: the
// range's 24-bit foreground, the scheme's 4-bit row as the companion, the
// underline bit on the far attribute (the printer's span pass re-applies
// it).  A 4-bit scheme answers the 4-bit path: the scheme's fg4, no 3:1
// check.
int look_fence_contrast()
{
	EnsureTrueColor();
	static const unsigned int kLight[10] = {0xd0d0d0, 0xd8d8d8, 0xe0e0e0, 0xe8e8e8, 0xf0f0f0,
		0xf8f8f8, 0xd0d0ff, 0xffd0d0, 0xd0ffd0, 0xffffd0};
	static const unsigned int kFail[10] = {0xd0d0d0, 0xd8d8d8, 0xe0e0e0, 0xe8e8e8, 0xf0f0f0,
		0xf8f8f8, 0xd0d0ff, 0xffd0d0, 0x303030, 0xffffd0};
	static const unsigned int kStyle[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	if (WriteOverrides("[markdown]\n", 9) != 0) {
		return 1;
	}
	const uint64_t saved = SaveBackground();
	SetBackground(0x101010);
	const uint32_t fg = 0x123456;   // the region's row a fence range carries
	const uint32_t fg4 = 9;         // the scheme's 4-bit row of the same region

	// A passing scheme: the range paints the 24-bit foreground over the
	// viewer background, the scheme's 4-bit row rides the low bits (a
	// terminal that ignores RGB shows it), and the underline bit rides the
	// far attribute for FinishRow's span pass.
	FakeScheme(91, kLight, kStyle);
	const uint64_t pass_range = MdRangeColor(fg, fg4, false);
	if (pass_range != WithBack(TrueFore(fg) | fg4)) {
		printf("FAILED look_fence_contrast: a passing range is 0x%llx, want 0x%llx\n",
			(unsigned long long)pass_range,
			(unsigned long long)WithBack(TrueFore(fg) | fg4));
		SetBackground((uint32_t)((saved >> 40) & 0xffffff));
		return 1;
	}
	if (MdRangeColor(fg, fg4, true) != (pass_range | kUnderline)) {
		printf("FAILED look_fence_contrast: an underlined range is 0x%llx, want 0x%llx\n",
			(unsigned long long)MdRangeColor(fg, fg4, true),
			(unsigned long long)(pass_range | kUnderline));
		SetBackground((uint32_t)((saved >> 40) & 0xffffff));
		return 1;
	}
	printf("probe look_fence_contrast: verdict pass, fences 0x%llx, prose 0x%llx\n",
		(unsigned long long)pass_range, (unsigned long long)MdRunColor(MD_ROLE_CODE));

	// A scheme that fails 3:1 (row 8 at 0x303030), a new generation: the one
	// verdict sends the fences to the plain code role and the prose to the
	// fork table, and the fence's color is the code role's color - the same
	// verdict, one recompute.
	FakeScheme(92, kFail, kStyle);
	const uint64_t plain = MdRunColor(MD_ROLE_CODE);
	const uint64_t fail_range = MdRangeColor(fg, fg4, false);
	if (fail_range != plain || plain != WantRgb(MD_ROLE_CODE)) {
		printf("FAILED look_fence_contrast: a failing scheme gives fences 0x%llx prose 0x%llx, want the fork table 0x%llx\n",
			(unsigned long long)fail_range, (unsigned long long)plain,
			(unsigned long long)WantRgb(MD_ROLE_CODE));
		SetBackground((uint32_t)((saved >> 40) & 0xffffff));
		return 1;
	}
	if (MdRangeColor(fg, fg4, true) != (plain | kUnderline)) {
		printf("FAILED look_fence_contrast: a failing underlined range is 0x%llx, want 0x%llx\n",
			(unsigned long long)MdRangeColor(fg, fg4, true),
			(unsigned long long)(plain | kUnderline));
		SetBackground((uint32_t)((saved >> 40) & 0xffffff));
		return 1;
	}
	printf("probe look_fence_contrast: verdict fail, fences 0x%llx = prose 0x%llx\n",
		(unsigned long long)fail_range, (unsigned long long)plain);

	// A 4-bit scheme (the same failing rows): the 4-bit path answers - the
	// scheme's fg4, no 3:1 check.
	EnsureNoTrueColor();
	if (MdRangeColor(fg, fg4, false) != WithBack(fg4)) {
		printf("FAILED look_fence_contrast: the 4-bit path gives 0x%llx, want 0x%llx\n",
			(unsigned long long)MdRangeColor(fg, fg4, false),
			(unsigned long long)WithBack(fg4));
		SetBackground((uint32_t)((saved >> 40) & 0xffffff));
		EnsureTrueColor();
		return 1;
	}
	// The fail path's companion bits: the range carries the code role's RGB
	// row in the truecolor bits and the code role's fallback16 (the 0x0d row)
	// in the low nibble.  An fg/fg4 swap on the fail path splits to the other
	// pair, where the equality with the prose color cannot see it.
	RGB split_fg, split_bg;
	extractColor(fail_range, split_fg, split_bg);
	const iRGB split_rgb = toIRGB(split_fg);
	if (split_rgb.r != 0xff || split_rgb.g != 0x87 || split_rgb.b != 0xff
		|| (fail_range & 0x0f) != 13) {
		printf("FAILED look_fence_contrast: the fail range splits to 0x%02X%02X%02X/%llu, want 0xFF87FF/13\n",
			split_rgb.r, split_rgb.g, split_rgb.b,
			(unsigned long long)(fail_range & 0x0f));
		SetBackground((uint32_t)((saved >> 40) & 0xffffff));
		EnsureTrueColor();
		return 1;
	}
	printf("probe look_fence_contrast: the fail range is the code row 0xFF87FF with the 0x0d companion\n");
	EnsureTrueColor();
	SetBackground((uint32_t)((saved >> 40) & 0xffffff));
	printf("ok look_fence_contrast: one view-wide verdict, fences with the prose\n");
	return 0;
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
	{"tokens_v2_cost", tokens_v2_cost},
	{"look_fence_generation", look_fence_generation},
	{"look_fence_contrast", look_fence_contrast},
	{"tint", tint},
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
