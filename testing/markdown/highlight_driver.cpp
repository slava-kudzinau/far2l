// Standalone driver for the Colorer fence tokens (story markdown-code-highlight).
// Built and run by testing/markdown/highlight-driver.sh, which needs the full
// build: it dlopen()s the far2l binary and the colorer plugin module next to it.
//
// R1 harness shape: one main, a table of {name, fn} cases.  `--case <name>`
// selects one case; no --case runs all and returns 1 on the first failure.
// Prints "ok <case>" / "FAILED <case>: <why>".
//
// Env: FAR2L_APP is the far2l binary, FAR2L_PLUGIN the colorer module.

#include <farplug-wide.h>

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

ModuleOpenFn g_module_open = nullptr;
StartupInfoFn g_set_startup_info = nullptr;
ViewerEventFn g_viewer_event = nullptr;
TokenizeFn g_tokenize = nullptr;
ViewerInfo g_vi = {};

// Set by the app (far2l/src/global.cpp, C linkage); WinPort's console layer reads it.
extern "C" bool g_far2l_use_vs16 = true;
// The plugin's Info callbacks run headless here: the catalog path needs
// SaveScreen/Message/RestoreScreen/GetMsg; ReadSettings needs the Cfg trio;
// viewerEvent asks ViewerControl for the viewer info.
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

// ---- token collection ----

struct Span
{
	int line;
	int start;
	int end;
	int cls;
};

void Collect(void* ctx, int line, int start, int end, int cls)
{
	static_cast<std::vector<Span>*>(ctx)->push_back(Span{line, start, end, cls});
}

// The classes present in spans, as a sorted "1234"-style string.
std::string ClassesOf(const std::vector<Span>& spans)
{
	bool seen[5] = {false, false, false, false, false};
	for (const Span& s : spans) {
		if (s.cls >= 1 && s.cls <= 4) {
			seen[s.cls] = true;
		}
	}
	std::string out;
	for (int c = 1; c <= 4; c++) {
		if (seen[c]) {
			out += char('0' + c);
		}
	}
	return out;
}

// One snippet per fence name that reaches keyword, string, comment, number
// (spec.md, Language names; json and yaml reach the classes their scheme
// defines, at least string and number).
struct Probe
{
	const wchar_t* lang;
	const wchar_t* snippet[2];
	int n_lines;
	const char* want; // classes expected to be present (substring match)
};

const Probe s_probes[] = {
	{L"js", {L"var s = \"x\"; // c", L"for (var i = 0; i < 3; i++)"}, 2, "1234"},
	{L"ts", {L"var s: string = \"x\"; // c", L"for (var i = 0; i < 3; i++)"}, 2, "1234"},
	{L"python", {L"for i in range(3): print(\"x\")  # c", nullptr}, 1, "1234"},
	{L"bash", {L"for i in 3; do echo \"x\"; done # c", nullptr}, 1, "1234"},
	{L"c", {L"for (int i = 0; i < 3; i++); /* c */", L"const char* s = \"x\";"}, 2, "1234"},
	{L"cpp", {L"for (int i = 0; i < 3; i++); // c", L"const char* s = \"x\";"}, 2, "1234"},
	{L"cpp", {L"for (int i = 0; i < 3; i++); // c", L"const char* s = \"x\";"}, 2, "1234"}, // via c++ alias, below
	{L"go", {L"for i := 0; i < 3; i++ {", L"\t_ = \"x\" // c"}, 2, "1234"},
	{L"rust", {L"for i in 0..3 {", L"\tlet s = \"x\"; // c"}, 2, "1234"},
	{L"java", {L"for (int i = 0; i < 3; i++); // c", L"String s = \"x\";"}, 2, "1234"},
	{L"sql", {L"select 'x' from t where n = 3; -- c", nullptr}, 1, "1234"},
	{L"json", {L"{\"key\": \"x\", \"n\": 3}", nullptr}, 1, "24"},
	{L"yaml", {L"key: \"x\" # c", L"n: 3"}, 2, "24"},
};

// ---- load the modules the way the core bridge (T2) will ----

void* g_app_handle = nullptr;
void* g_plugin_handle = nullptr;

bool LoadModules(std::string& why)
{
	const char* app = getenv("FAR2L_APP");
	const char* plugin = getenv("FAR2L_PLUGIN");
	if (app == nullptr || plugin == nullptr) {
		why = "FAR2L_APP and FAR2L_PLUGIN must name the binary and the plugin module";
		return false;
	}
	g_app_handle = dlopen(app, RTLD_LAZY | RTLD_NOW | RTLD_GLOBAL);
	if (g_app_handle == nullptr) {
		why = std::string("dlopen app: ") + dlerror();
		return false;
	}
	g_plugin_handle = dlopen(plugin, RTLD_LAZY | RTLD_NOW);
	if (g_plugin_handle == nullptr) {
		why = std::string("dlopen plugin: ") + dlerror();
		return false;
	}
	// T2's lookup: the module the process already loaded answers.
	void* noload = dlopen(plugin, RTLD_LAZY | RTLD_NOLOAD);
	if (noload != g_plugin_handle) {
		why = "dlopen(RTLD_LAZY | RTLD_NOLOAD) did not return the loaded module";
		return false;
	}
	printf("probe: dlopen(RTLD_LAZY | RTLD_NOLOAD) returned the module dlopen loaded\n");

	g_module_open = (ModuleOpenFn)dlsym(g_plugin_handle, "PluginModuleOpen");
	g_set_startup_info = (StartupInfoFn)dlsym(g_plugin_handle, "SetStartupInfoW");
	g_viewer_event = (ViewerEventFn)dlsym(g_plugin_handle, "ProcessViewerEventW");
	g_tokenize = (TokenizeFn)dlsym(g_plugin_handle, "ColorerTokenizeV1");
	if (g_module_open == nullptr || g_set_startup_info == nullptr || g_viewer_event == nullptr
		|| g_tokenize == nullptr) {
		why = std::string("dlsym: ") + (dlerror() ? dlerror() : "a needed export is missing");
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
	return true;
}

// Create editorSet the way the app does: a viewer event (VE_READ) creates
// FarEditorSet, which loads the catalog (pcolorer.cpp ProcessViewerEventW).
bool CreateEditorSet(std::string& why)
{
	g_vi = {};
	g_vi.StructSize = sizeof(g_vi);
	g_vi.ViewerID = 1;
	g_vi.FilePos = 0;
	g_vi.CurMode.Hex = 0;
	g_vi.CurMode.Processed = 0;
	g_vi.Options = 0;
	g_viewer_event(VE_READ, &g_vi);
	return true;
}

// ---- cases ----

int probe_real_plugin()
{
	std::string why;
	if (!LoadModules(why)) {
		printf("FAILED probe_real_plugin: %s\n", why.c_str());
		return 1;
	}

	// Unknown language and the no-editorSet return codes, before any event.
	std::vector<Span> spans;
	const wchar_t* lines[] = {L"var s = \"x\"; // c"};
	int rc = g_tokenize(L"js", lines, 1, Collect, &spans, 1000);
	if (rc != -1 || !spans.empty()) {
		printf("FAILED probe_real_plugin: before the first event, rc=%d, %zu events\n", rc, spans.size());
		return 1;
	}

	const auto t_load = std::chrono::steady_clock::now();
	if (!CreateEditorSet(why)) {
		printf("FAILED probe_real_plugin: %s\n", why.c_str());
		return 1;
	}
	printf("probe: FarEditorSet + catalog loaded in %lld ms\n",
		(long long)std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - t_load).count());
	printf("probe: editorSet is not null at the first tokenize call (the event above created it)\n");

	// The js snippet of the task, then the wall time of a first and a later
	// call (plan T1: record both).
	spans.clear();
	const wchar_t* js[] = {L"var s = \"x\"; // c", L"for (var i = 0; i < 3; i++)"};
	const auto t0 = std::chrono::steady_clock::now();
	rc = g_tokenize(L"js", js, 2, Collect, &spans, 1000);
	const long long ms_first = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();
	printf("probe: first export call: %lld ms\n", ms_first);
	if (rc != 0) {
		printf("FAILED probe_real_plugin: js call rc=%d\n", rc);
		return 1;
	}
	const std::string js_classes = ClassesOf(spans);
	printf("probe js: %s\n", js_classes.c_str());
	if (js_classes != "1234") {
		printf("FAILED probe_real_plugin: js reached classes %s, want 1234\n", js_classes.c_str());
		return 1;
	}

	const auto t1 = std::chrono::steady_clock::now();
	rc = g_tokenize(L"js", js, 2, Collect, &spans, 1000);
	printf("probe: later export call: %lld ms\n", (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t1).count());
	if (rc != 0) {
		printf("FAILED probe_real_plugin: second js call rc=%d\n", rc);
		return 1;
	}

	// A two-line block comment: class 3 on both lines; a second call starts
	// clean (state does not leak past the fence).
	spans.clear();
	const wchar_t* comment[] = {L"/* two", L"line */"};
	rc = g_tokenize(L"js", comment, 2, Collect, &spans, 1000);
	if (rc != 0) {
		printf("FAILED probe_real_plugin: comment call rc=%d\n", rc);
		return 1;
	}
	bool line0 = false, line1 = false;
	for (const Span& s : spans) {
		if (s.cls == 3 && s.line == 0) {
			line0 = true;
		}
		if (s.cls == 3 && s.line == 1) {
			line1 = true;
		}
	}
	if (!line0 || !line1) {
		printf("FAILED probe_real_plugin: block comment classes on lines: %s\n", ClassesOf(spans).c_str());
		return 1;
	}
	spans.clear();
	rc = g_tokenize(L"js", comment, 2, Collect, &spans, 1000);
	if (rc != 0) {
		printf("FAILED probe_real_plugin: second comment call rc=%d\n", rc);
		return 1;
	}
	printf("probe: block comment: class 3 on both lines; a second call starts clean\n");

	// The twelve names of spec.md, plus the alias names c++ and zsh.
	for (const Probe& p : s_probes) {
		spans.clear();
		const wchar_t* snippet[2] = {p.snippet[0], p.snippet[1]};
		rc = g_tokenize(p.lang, snippet, p.n_lines, Collect, &spans, 1000);
		const std::string got = ClassesOf(spans);
		printf("probe %ls: %s\n", p.lang, got.c_str());
		if (rc != 0 || strstr(p.want, got.c_str()) == nullptr) {
			printf("FAILED probe_real_plugin: %ls rc=%d reached %s, want %s\n",
				p.lang, rc, got.c_str(), p.want);
			return 1;
		}
	}
	spans.clear();
	const wchar_t* cpp[] = {L"for (int i = 0; i < 3; i++); // c", L"const char* s = \"x\";"};
	rc = g_tokenize(L"c++", cpp, 2, Collect, &spans, 1000);
	printf("probe c++: %s\n", ClassesOf(spans).c_str());
	if (rc != 0 || ClassesOf(spans) != "1234") {
		printf("FAILED probe_real_plugin: c++ alias rc=%d classes %s\n", rc, ClassesOf(spans).c_str());
		return 1;
	}
	spans.clear();
	const wchar_t* zsh[] = {L"for i in 3; do echo \"x\"; done # c"};
	rc = g_tokenize(L"zsh", zsh, 1, Collect, &spans, 1000);
	printf("probe zsh: %s\n", ClassesOf(spans).c_str());
	if (rc != 0 || ClassesOf(spans) != "1234") {
		printf("FAILED probe_real_plugin: zsh alias rc=%d classes %s\n", rc, ClassesOf(spans).c_str());
		return 1;
	}
	spans.clear();
	const wchar_t* tsx[] = {L"var s = \"x\"; // c", L"for (var i = 0; i < 3; i++)"};
	rc = g_tokenize(L"tsx", tsx, 2, Collect, &spans, 1000);
	printf("probe tsx: %s\n", ClassesOf(spans).c_str());
	if (rc != 0) {
		printf("FAILED probe_real_plugin: tsx alias rc=%d\n", rc);
		return 1;
	}
	spans.clear();
	rc = g_tokenize(L"nosuchlang9", lines, 1, Collect, &spans, 1000);
	if (rc != -2 || !spans.empty()) {
		printf("FAILED probe_real_plugin: nosuchlang9 rc=%d, %zu events\n", rc, spans.size());
		return 1;
	}
	return 0;
}

struct Case
{
	const char* name;
	int (*fn)();
};

const Case s_cases[] = {
	{"probe_real_plugin", probe_real_plugin},
};
} // namespace

int main(int argc, char** argv)
{
	const char* want = nullptr;
	if (argc >= 3 && strcmp(argv[1], "--case") == 0) {
		want = argv[2];
	} else if (argc > 3) {
		fprintf(stderr, "usage: highlight_driver [--case <name>]\n");
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
	return g_failures == 0 ? 0 : 1;
}
