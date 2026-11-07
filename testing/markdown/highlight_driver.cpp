// Standalone driver for the Colorer fence tokens (story markdown-code-highlight).
// Built and run by testing/markdown/highlight-driver.sh, which needs the full
// build: it dlopen()s the far2l binary and the colorer plugin module next to it.
#include <cstdio>
#include <cstdlib>
//
// R1 harness shape: one main, a table of {name, fn} cases.  `--case <name>`
// selects one case; no --case runs all and returns 1 on the first failure.
// Prints "ok <case>" / "FAILED <case>: <why>".
//
// Env: FAR2L_APP is the far2l binary, FAR2L_PLUGIN the colorer module.

#include <farplug-wide.h>

#include "headers.hpp"
#include "ctrlobj.hpp"
#include "markdown/MdColorer.hpp"
#include "cfg/language.hpp"
#include "markdown/MdPalette.hpp"
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
#include <thread>
#include <vector>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>

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
// The plugin manager the bridge (MdColorer) asks. The driver owns it here:
// `new ControlObject` scans and registers the plugins, as at startup.
ControlObject *CtrlObject = nullptr;
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
	{L"json", {L"{\"key\": \"x\", \"n\": 3}", nullptr}, 1, "1234"},
	{L"yaml", {L"key: \"x\" # c", L"n: 3"}, 2, "1234"},
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
	if (CtrlObject == nullptr) {
		CtrlObject = new ControlObject;
	}
	if (CtrlObject->Plugins.GetPluginsCount() == 0) {
		// main.cpp:763: the plugin manager scans after the language pack.
		CtrlObject->Plugins.LoadPlugins();
	}
	printf("probe: ControlObject registered %d plugins\n",
		CtrlObject->Plugins.GetPluginsCount());
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

	// The export carries its own catalog session (R48): the first call
	// answers without any viewer event.
	std::vector<Span> spans;
	const wchar_t* lines[] = {L"var s = \"x\"; // c"};
	int rc = g_tokenize(L"js", lines, 1, Collect, &spans, 1000);
	if (rc != 0 || spans.empty()) {
		printf("FAILED probe_real_plugin: before the first event, rc=%d, %zu events\n", rc, spans.size());
		return 1;
	}
	spans.clear();

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

// The fake token source of the T2 cases: it owns canned tokens and counts
// the calls, so the bridge needs no plugin (plan T2).
class FakeSource : public MdTokenSource
{
public:
	std::string last_lang;
	int calls = 0;

	bool Tokenize(const std::string &lang, const std::vector<std::string> &lines,
		int, std::vector<MdToken> &out) override
	{
		last_lang = lang;
		++calls;
		for (size_t i = 0; i < lines.size(); ++i)
			out.push_back(MdToken{0, 3, 1, (uint32_t)i}); // one keyword per line start
		return true;
	}
};

// The T2 cases: the bridge's name rules and the source fallback.  They link
// against far2l_md, so no plugin is needed.
int alias_maps_cpp()
{
	if (MdNormalizeLangName("c++") != "cpp") {
		printf("FAILED alias_maps_cpp: c++ maps to %s\n", MdNormalizeLangName("c++").c_str());
		return 1;
	}
	return 0;
}

int alias_maps_zsh()
{
	if (MdNormalizeLangName("zsh") != "sh") {
		printf("FAILED alias_maps_zsh: zsh maps to %s\n", MdNormalizeLangName("zsh").c_str());
		return 1;
	}
	return 0;
}

int name_too_long()
{
	const std::string name(33, 'a');
	if (!MdNormalizeLangName(name).empty()) {
		printf("FAILED name_too_long: a 33-character name mapped to %s\n",
			MdNormalizeLangName(name).c_str());
		return 1;
	}
	return 0;
}

int name_bad_char()
{
	if (!MdNormalizeLangName("c+ x").empty()) {
		printf("FAILED name_bad_char: a name with a space mapped to %s\n",
			MdNormalizeLangName("c+ x").c_str());
		return 1;
	}
	return 0;
}

int source_call_count()
{
	MdSetTokenSourceForTest(std::make_unique<FakeSource>());
	std::vector<MdToken> out;
	if (!MdGetTokenSource().Tokenize("js", {"var x;"}, 1000, out)
		|| !MdGetTokenSource().Tokenize("js", {"var y;"}, 1000, out)) {
		printf("FAILED source_call_count: Tokenize returned false\n");
		return 1;
	}
	FakeSource* fake = dynamic_cast<FakeSource*>(&MdGetTokenSource());
	if (fake == nullptr || fake->calls != 2 || out.size() != 2) {
		printf("FAILED source_call_count: calls=%d tokens=%zu\n",
			fake ? fake->calls : -1, out.size());
		return 1;
	}
	MdSetTokenSourceForTest(nullptr);
	return 0;
}

int manager_plugin_tokens()
{
	MdSetTokenSourceForTest(nullptr);
	std::string why;
	if (!LoadModules(why)) {
		printf("FAILED manager_plugin_tokens: %s\n", why.c_str());
		return 1;
	}
	std::string es_why;
	if (!CreateEditorSet(es_why)) {
		printf("FAILED manager_plugin_tokens: %s\n", es_why.c_str());
		return 1;
	}
	std::vector<MdToken> out;
	if (!MdGetTokenSource().Tokenize("js", {"var x; // c"}, 1000, out)) {
		printf("FAILED manager_plugin_tokens: Tokenize returned false\n");
		return 1;
	}
	if (out.empty() || out[0].start != 0 || out[0].end != 3 || out[0].cls != 1) {
		printf("FAILED manager_plugin_tokens: %zu tokens, first %d-%d cls %d\n",
			out.size(), out.empty() ? -1 : (int)out[0].start,
			out.empty() ? -1 : (int)out[0].end, out.empty() ? -1 : (int)out[0].cls);
		return 1;
	}
	return 0;
}

int no_source_falls_back()
{
	MdSetTokenSourceForTest(nullptr);
	std::string why;
	if (!LoadModules(why)) {
		printf("FAILED no_source_falls_back: %s\n", why.c_str());
		return 1;
	}
	std::vector<MdToken> out;
	// The plugin's catalog stays loaded; the bridge asks once per fence
	// (R48) and reports plain keywords (class 1).
	if (!MdGetTokenSource().Tokenize("js", {"var x;"}, 1000, out)) {
		printf("FAILED no_source_falls_back: Tokenize returned false with no plugin\n");
		return 1;
	}
	if (out.size() != 1 || out[0].cls != 1) {
		printf("FAILED no_source_falls_back: %zu tokens, first cls %d\n",
			out.size(), out.empty() ? -1 : (int)out[0].cls);
		return 1;
	}
	return 0;
}

// ---- The T3/T4 render cases: the render side (plan T3, T4) ----

// One fenced block of `lines`, the way a markdown file holds it.
std::wstring MakeFence(const std::wstring &lang, const std::vector<std::wstring> &lines)
{
	std::wstring md = L"```" + lang + L"\n";
	for (const std::wstring &line : lines) {
		md += line + L"\n";
	}
	md += L"```\n";
	return md;
}

// The token group of a rendered row, -1 when the row keeps the plain code role.
int RowHasGroup(const MarkdownDoc &doc, size_t row)
{
	if (row >= doc.Lines().size())
		return -1;
	return doc.Lines()[row].tokens;
}

int fence_parsed_once()
{
	MdSetTokenSourceForTest(std::make_unique<FakeSource>());
	MarkdownDoc doc;
	doc.Render(MakeFence(L"js", {L"var x; // c"}), 80);
	FakeSource *fake = dynamic_cast<FakeSource *>(&MdGetTokenSource());
	const int gi = RowHasGroup(doc, 0);
	if (fake == nullptr || fake->calls != 1 || gi < 0
		|| (size_t)gi >= doc.TokenGroups().size()
		|| doc.TokenGroups()[gi].tokens.size() != 1
		|| doc.TokenGroups()[gi].tokens[0].start != 4
		|| doc.TokenGroups()[gi].tokens[0].cls != 1) {
		printf("FAILED fence_parsed_once: calls=%d group=%d\n",
			fake ? fake->calls : -1, gi);
		MdSetTokenSourceForTest(nullptr);
		return 1;
	}
	doc.Invalidate(70);   // a resize re-renders from the cache (R30)
	if (fake->calls != 1) {
		printf("FAILED fence_parsed_once: resize cost %d export calls, want 1\n", fake->calls);
		MdSetTokenSourceForTest(nullptr);
		return 1;
	}
	MdSetTokenSourceForTest(nullptr);
	printf("probe: one fence, one export call; a resize costs none\n");
	return 0;
}

int line_cap_400()
{
	std::string why;
	if (!LoadModules(why)) {
		printf("FAILED line_cap_400: %s\n", why.c_str());
		return 1;
	}
	std::string es_why;
	if (!CreateEditorSet(es_why)) {
		printf("FAILED line_cap_400: %s\n", es_why.c_str());
		return 1;
	}
	std::vector<std::wstring> lines(401, L"x");
	MarkdownDoc doc;
	doc.Render(MakeFence(L"js", lines), 80);
	const int gi = RowHasGroup(doc, 0);
	const int cap_row = RowHasGroup(doc, 399);
	const int past = RowHasGroup(doc, 400);
	if (gi < 0 || cap_row != gi + 399 || past >= 0) {
		printf("FAILED line_cap_400: rows 0/399/400 groups %d/%d/%d\n", gi, cap_row,
			past);
		return 1;
	}
	printf("probe: 401 lines, 400 groups; line 401 keeps the plain code role\n");
	return 0;
}

int byte_cap_65536()
{
	MdSetTokenSourceForTest(std::make_unique<FakeSource>());
	std::vector<std::wstring> lines(400, std::wstring(200, L'y'));
	MarkdownDoc doc;
	doc.Render(MakeFence(L"js", lines), 80);
	FakeSource *fake = dynamic_cast<FakeSource *>(&MdGetTokenSource());
	MdSetTokenSourceForTest(nullptr);
	if (doc.TokenGroups().empty()) {
		printf("FAILED byte_cap_65536: no groups (calls=%d)\n", fake ? fake->calls : -1);
		return 1;
	}
	size_t last_linked = 0;
	for (size_t row = 0; row < doc.Lines().size(); ++row) {
		if (RowHasGroup(doc, row) >= 0)
			last_linked = row;
	}
	if (last_linked == 0 || last_linked >= 399 || RowHasGroup(doc, last_linked + 1) >= 0) {
		printf("FAILED byte_cap_65536: last linked row %zu of 400\n", last_linked);
		return 1;
	}
	printf("probe: 65536 bytes: rows up to %zu keep groups, the rest plain code\n", last_linked);
	return 0;
}

// A source that spends 15 ms on every call: five fences cross the 40 ms deadline.
class SlowSource : public MdTokenSource
{
public:
	int calls = 0;
	bool Tokenize(const std::string &lang, const std::vector<std::string> &lines,
		int deadline_ms, std::vector<MdToken> &out) override
	{
		(void)lang; (void)deadline_ms;
		++calls; // R48: the render asks for one fence per call
		std::this_thread::sleep_for(std::chrono::milliseconds(7));
		for (size_t i = 0; i < lines.size(); ++i) {
			out.push_back(MdToken{0, 11, 1, (uint32_t)i}); // the whole line 'var x; // c'
		}
		return true;
	}
};

int budget_40ms_skips_later_fences()
{
	std::unique_ptr<SlowSource> slow = std::make_unique<SlowSource>();
	SlowSource *watch = slow.get();
	MdSetTokenSourceForTest(std::move(slow));
	std::wstring md;
	for (int i = 0; i < 5; ++i)
		md += MakeFence(L"js", {L"var x" + std::to_wstring(i) + L"; // c"});   // distinct keys: every fence costs a call
	MarkdownDoc doc;
	doc.Render(md, 80);   // one 40 ms budget over the fences (R29, R37, R48)
	const int first = RowHasGroup(doc, 0);
	const int last = RowHasGroup(doc, doc.Lines().size() - 1);
	const bool first_tokens = first >= 0 && (size_t)first < doc.TokenGroups().size()
		&& !doc.TokenGroups()[first].tokens.empty();
	const bool last_plain = last >= 0 && (size_t)last < doc.TokenGroups().size()
		&& doc.TokenGroups()[last].tokens.empty();
	const int calls = watch->calls;   // read before the source is destroyed
	MdSetTokenSourceForTest(nullptr);
	if (calls != 4 || !first_tokens || !last_plain) {   // R48: 7 ms x 4 fences inside 40 ms; fence 5 keeps plain code
		printf("FAILED budget_40ms_skips_later_fences: calls=%d first=%d last=%d\n",
			watch ? calls : -1, first, last);
		return 1;
	}
	printf("probe: budget calls=%d first=%d last=%d groups=%zu\n",
		calls, first, last, doc.TokenGroups().size());
	printf("probe: 5 fences, %d per-fence export calls inside 40 ms; the later fences keep plain code\n",
		calls);
	return 0;
}

int long_line_rest_stays_code()
{
	MdSetTokenSourceForTest(std::make_unique<FakeSource>());
	MarkdownDoc doc;
	doc.Render(MakeFence(L"js", {std::wstring(1200, L'v')}), 80);
	const int gi = RowHasGroup(doc, 0);   // FakeSource puts a keyword at cells 4..7
	MdSetTokenSourceForTest(nullptr);
	if (gi < 0 || (size_t)gi >= doc.TokenGroups().size()
		|| doc.TokenGroups()[gi].tokens[0].end > 1004) {
		printf("FAILED long_line_rest_stays_code: group %d, token past cell 1004\n", gi);
		return 1;
	}
	printf("probe: a 1200-character line: the group ends by cell 1004, the rest keeps the code role\n");
	return 0;
}

int scroll_keeps_token_colors()
{
	MdSetTokenSourceForTest(std::make_unique<FakeSource>());
	MarkdownDoc doc;
	doc.Render(MakeFence(L"js", {L"var x; // c"}), 80);
	const int gi1 = RowHasGroup(doc, 0);
	doc.Invalidate(60);   // a narrower screen: cells are text coordinates, not screen ones
	const int gi2 = RowHasGroup(doc, 0);
	MdSetTokenSourceForTest(nullptr);
	if (gi1 < 0 || gi2 < 0
		|| doc.TokenGroups()[gi1].tokens[0].start != doc.TokenGroups()[gi2].tokens[0].start
		|| doc.TokenGroups()[gi2].tokens[0].start != 4) {
		printf("FAILED scroll_keeps_token_colors: cells moved on resize\n");
		return 1;
	}
	printf("probe: token cells are text coordinates; a resize keeps them\n");
	return 0;
}

// The run text of a rendered row.
std::wstring RunsText(const MdLine &line)
{
	std::wstring out;
	for (const MdRun &run : line.runs)
		out += run.text;
	return out;
}

int layout_unchanged()
{
	const std::wstring md = MakeFence(L"js", {L"var x; // c"}) + L"\nA paragraph.\n";
	MdSetTokenSourceForTest(std::make_unique<FakeSource>());
	MarkdownDoc with;
	with.Render(md, 80);
	MdSetTokenSourceForTest(nullptr);
	MarkdownDoc without;
	without.Render(md, 80);   // no source: the same fence in the plain code role
	if (with.Lines().size() != without.Lines().size()) {
		printf("FAILED layout_unchanged: %zu rows vs %zu\n",
			with.Lines().size(), without.Lines().size());
		return 1;
	}
	for (size_t row = 0; row < with.Lines().size(); ++row) {
		const std::wstring a = RunsText(with.Lines()[row]);
		const std::wstring b = RunsText(without.Lines()[row]);
		if (a != b) {
			printf("FAILED layout_unchanged: row %zu: [%ls] vs [%ls]\n",
				row, a.c_str(), b.c_str());
			return 1;
		}
	}
	printf("probe: token groups change colors, not layout\n");
	return 0;
}

// A source that puts one token of class `cls` on the first line.
class ClsSource : public MdTokenSource
{
public:
	int cls;
	explicit ClsSource(int c) : cls(c) {}
	bool Tokenize(const std::string &lang, const std::vector<std::string> &lines,
		int deadline_ms, std::vector<MdToken> &out) override
	{
		(void)lang; (void)lines; (void)deadline_ms;
		out.push_back(MdToken{0, 3, static_cast<uint8_t>(cls)});
		return true;
	}
};

int print_role(int cls, uint64_t want_role, const char *name)
{
	MdSetTokenSourceForTest(std::make_unique<ClsSource>(cls));
	MarkdownDoc doc;
	doc.Render(MakeFence(L"js", {L"var x; // c"}), 80);
	const int gi = RowHasGroup(doc, 0);
	MdSetTokenSourceForTest(nullptr);
	if (gi < 0 || (size_t)gi >= doc.TokenGroups().size()
		|| doc.TokenGroups()[gi].tokens[0].cls != cls) {
		printf("FAILED %s: group %d carries the wrong class\n", name, gi);
		return 1;
	}
	const uint64_t role = MdRoleForTokenClass(doc.TokenGroups()[gi].tokens[0].cls);
	if (role != want_role) {
		printf("FAILED %s: cls %d maps to role %llu, want %llu\n",
			name, cls, (unsigned long long)role, (unsigned long long)want_role);
		return 1;
	}
	printf("probe: cls %d prints as role %llu\n", cls, (unsigned long long)role);
	return 0;
}

int print_keyword() { return print_role(1, MD_ROLE_TOKEN_KEYWORD, "print_keyword"); }
int print_string() { return print_role(2, MD_ROLE_TOKEN_STRING, "print_string"); }
int print_comment() { return print_role(3, MD_ROLE_TOKEN_COMMENT, "print_comment"); }
int print_number() { return print_role(4, MD_ROLE_TOKEN_NUMBER, "print_number"); }

// Headless WinPort reports a 4-color palette, so MdRunColor returns each role's
// readable 16-color fallback (the RGB defaults and the contrast row of the spec
// table are checked on screen in the 000c smoke test).
int print_contrast_fail()
{
	struct Want { uint64_t role; uint64_t fg; };
	const Want wants[] = {   // the spec table's 16-color column (F_LIGHTGRAY 7, F_LIGHTGREEN 10, F_CYAN 3, F_LIGHTRED 12)
		{MD_ROLE_TOKEN_KEYWORD, 7},
		{MD_ROLE_TOKEN_STRING, 10},
		{MD_ROLE_TOKEN_COMMENT, 3},
		{MD_ROLE_TOKEN_NUMBER, 12},
	};
	for (const Want &w : wants) {
		const uint64_t color = MdRunColor(w.role);
		if ((color & 0xf) != w.fg) {
			printf("FAILED print_contrast_fail: role %llu color 0x%llx, want fg %llu\n",
				(unsigned long long)w.role, (unsigned long long)color, (unsigned long long)w.fg);
			return 1;
		}
	}
	printf("probe: every token role resolves to a readable 16-color color\n");
	return 0;
}

// The same fallbacks the --norgb smoke run checks on screen (R38): the colors of
// the spec table's last column, as printed cells.
int print_norgb()
{
	struct Want { uint64_t role; uint64_t fg; };
	const Want wants[] = {   // the same column, checked without truecolor
		{MD_ROLE_TOKEN_KEYWORD, 7},
		{MD_ROLE_TOKEN_STRING, 10},
		{MD_ROLE_TOKEN_COMMENT, 3},
		{MD_ROLE_TOKEN_NUMBER, 12},
	};
	for (const Want &w : wants) {
		const uint64_t color = MdRunColor(w.role);
		if ((color & 0xf) != w.fg || (color & (uint64_t)FOREGROUND_TRUECOLOR)) {
			printf("FAILED print_norgb: role %llu color 0x%llx, want fg %llu\n",
				(unsigned long long)w.role, (unsigned long long)color, (unsigned long long)w.fg);
			return 1;
		}
	}
	printf("probe: --norgb shows the 16-color fallbacks of the spec table\n");
	return 0;
}

struct Case
{
	const char* name;
	int (*fn)();
};

const Case s_cases[] = {
	{"alias_maps_cpp", alias_maps_cpp},
	{"alias_maps_zsh", alias_maps_zsh},
	{"name_too_long", name_too_long},
	{"name_bad_char", name_bad_char},
	{"source_call_count", source_call_count},
	{"manager_plugin_tokens", manager_plugin_tokens},
	{"no_source_falls_back", no_source_falls_back},
	{"probe_real_plugin", probe_real_plugin},
	{"fence_parsed_once", fence_parsed_once},
	{"line_cap_400", line_cap_400},
	{"byte_cap_65536", byte_cap_65536},
	{"budget_40ms_skips_later_fences", budget_40ms_skips_later_fences},
	{"long_line_rest_stays_code", long_line_rest_stays_code},
	{"scroll_keeps_token_colors", scroll_keeps_token_colors},
	{"layout_unchanged", layout_unchanged},
	{"print_keyword", print_keyword},
	{"print_string", print_string},
	{"print_comment", print_comment},
	{"print_number", print_number},
	{"print_contrast_fail", print_contrast_fail},
	{"print_norgb", print_norgb},
};
} // namespace

// Headless: WinPort has no console output yet; the palette cases need one
// (GetConsoleColorPalette returns 4 without a backend, the 16-color path).
void EnsureConsoleOutput()
{
	if (g_winport_con_out == nullptr) {
		g_winport_con_out = new ConsoleOutput;
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
	} else {
		// The script builds the driver in the install tree, next to the app
		// (install/far2l.app/.../far2l, the driver two levels up).
		DIR* d = opendir(".");
		struct dirent* ent;
		while (d != nullptr && (ent = readdir(d)) != nullptr) {
			if (strncmp(ent->d_name, "install", 7) == 0) {
				dir = "./install/far2l.app/Contents/MacOS";
				break;
			}
		}
		if (d != nullptr) {
			closedir(d);
		}
	}
	if (!dir.empty()) {
		Lang.Init(FARString(dir.c_str()).CPtr(), true, Msg::NewFileName.ID());
	}
}
int main(int argc, char** argv)
{
	EnsureConsoleOutput();
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
	fflush(stdout);
	fflush(stderr);
	// The Colorer plugin's static teardown throws with the headless console: leave it loaded (R49).
	_exit(g_failures == 0 ? 0 : 1);
}
