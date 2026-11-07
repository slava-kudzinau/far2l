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
#include "cfg/config.hpp"
#include "mix/pathmix.hpp"

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

ModuleOpenFn g_module_open = nullptr;
StartupInfoFn g_set_startup_info = nullptr;
ViewerEventFn g_viewer_event = nullptr;
TokenizeFn g_tokenize = nullptr;
LookFn g_look = nullptr;
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
	if (g_module_open == nullptr || g_set_startup_info == nullptr || g_viewer_event == nullptr
		|| g_tokenize == nullptr || g_look == nullptr) {
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

struct Case
{
	const char* name;
	int (*fn)();
};

const Case s_cases[] = {
	{"look_first_call", look_first_call},
	{"look_real_hrd", look_real_hrd},
	{"look_cost", look_cost},
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
