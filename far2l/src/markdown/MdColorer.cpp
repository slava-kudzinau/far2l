// The bridge to the Colorer plugin export (story markdown-code-highlight,
// ADR-008).  All knowledge of the plugin lives here: the render asks for
// token classes, never colors.  The plugin is found the way far2l finds it,
// so the same module answers: PluginManager, then the module name far2l
// handed to its own dlopen, asked back with RTLD_NOLOAD.

#include "MdColorer.hpp"

#include "headers.hpp"

#include "ctrlobj.hpp"
#include "plug/plclass.hpp"
#include "plug/plugins.hpp"


#include <dlfcn.h>

#include <cctype>

#include "WinPort.h"

namespace
{

// UTF-8 <-> wide, the shape MarkdownRender.cpp uses (the fork has no shared
// Str2Wide pair; WINPORT maps to the platform conversion).
std::wstring utf8_to_wide(const std::string &text)
{
	if (text.empty()) {
		return std::wstring();
	}
	int need = WINPORT(MultiByteToWideChar)(CP_UTF8, 0, text.c_str(),
		static_cast<int>(text.size()), nullptr, 0);
	std::wstring out;
	if (need > 0) {
		out.resize(static_cast<size_t>(need));
		WINPORT(MultiByteToWideChar)(CP_UTF8, 0, text.c_str(),
			static_cast<int>(text.size()), &out[0], need);
	}
	return out;
}

std::string wide_to_utf8(const wchar_t *text)
{
	if (text == nullptr) {
		return std::string();
	}
	int need = WINPORT(WideCharToMultiByte)(CP_UTF8, 0, text, -1, nullptr, 0,
		nullptr, nullptr);
	std::string out;
	if (need > 0) {
		out.resize(static_cast<size_t>(need) - 1); // -1: drop the terminator
		WINPORT(WideCharToMultiByte)(CP_UTF8, 0, text, -1, &out[0], need,
			nullptr, nullptr);
	}
	return out;
}

using TokenizeFn = int (*)(const wchar_t *lang, const wchar_t *const *lines, int count,
	void (*emit)(void *ctx, int line, int start, int end, int token_class), void *ctx,
	int deadline_ms);

// A fence name Colorer's file-name lookup misses gets the name Colorer knows
// (plan T1, R4; the same table the export applies, so it works without it).
struct LangAlias
{
	const char *fence;
	const char *type;
};

const LangAlias s_aliases[] = {
	{"c++", "cpp"},
	{"tsx", "jsx"},
	{"zsh", "sh"},
	{"python", "py"},
	{"bash", "sh"},
	{"shell", "sh"},
	{"rust", "rs"},
	{"javascript", "js"},
	{"typescript", "ts"},
	{"csharp", "cs"},
	{"golang", "go"},
	{"kotlin", "kt"},
	{"powershell", "ps1"},
	{"docker", "dockerfile"},
	{"latex", "tex"},
	{"plaintext", "text"},
	{"perl", "pl"},
	{"elixir", "ex"},
	{"erlang", "erl"},
	{"haskell", "hs"},
	{"julia", "jl"},
	{"objectivec", "m"},
	{"objc", "m"},
	{"obj-c", "m"},
	{"fsharp", "fs"},
	{"vbnet", "vb"},
	{"assembly", "asm"},
	{"pascal", "pas"},
	{"fortran", "f"},
	{"make", "makefile"},
	{"gitignore", "gitignore"},
	{"apache", "conf"},
	{"apacheconf", "apache"},
	{"blade", "php"},
	{"handlebars", "twig"},
	{"brainfuck", "lisp"},
	{"graphviz", "dot"},
	{"terraform", "hcl"},
};

// The export's emit context: tokens land in the caller's vector.
struct CollectCtx
{
	std::vector<MdToken> *out;
};

void Collect(void *ctx, int line, int start, int end, int cls)
{
	CollectCtx *c = static_cast<CollectCtx *>(ctx);
	c->out->push_back(MdToken{static_cast<uint32_t>(start), static_cast<uint32_t>(end),
		static_cast<uint8_t>(cls), static_cast<uint32_t>(line)});
}

// A fence name: at most 32 characters of [A-Za-z0-9+#_-], non-empty
// (spec.md, Language names).  Fills `lower` with the lower-cased name.
bool ValidName(const std::string &lang, std::string &lower)
{
	if (lang.empty() || lang.size() > 32) {
		return false;
	}
	for (const char ch : lang) {
		if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '+' || ch == '#'
			|| ch == '_' || ch == '-') {
			lower += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		} else {
			return false;
		}
	}
	return true;
}

// The plugin module, located once per process: the module name far2l handed
// to its own dlopen, asked back with RTLD_NOLOAD.  The token and the look
// source share it: one load, two symbols (spec.md, Bridge).
void *PluginModuleHandle()
{
	static void *handle = [] {
		if (CtrlObject == nullptr) {
			return static_cast<void *>(nullptr);
		}
		Plugin *plugin = CtrlObject->Plugins.FindPlugin(SYSID_COLORER);
		if (plugin == nullptr) {
			return static_cast<void *>(nullptr);
		}
		plugin->Load();
		const std::string module = wide_to_utf8(plugin->GetModuleName());
		if (module.empty()) {
			return static_cast<void *>(nullptr);
		}
		// The exact path far2l passed to its own dlopen, asked back with
		// RTLD_NOLOAD: the module the process already loaded answers.
		return dlopen(module.c_str(), RTLD_LAZY | RTLD_NOLOAD);
	}();
	return handle;
}

// The token source that calls the plugin export.  The module is located once
// per process; any failure sets one "no colors" flag for good.
class PluginTokenSource : public MdTokenSource
{
	TokenizeFn m_tokenize = nullptr;
	bool m_broken = false;

	// Finds the loaded plugin module and its export, once.
	bool Open()
	{
		void *handle = PluginModuleHandle();
		if (handle == nullptr) {
			return false;
		}
		m_tokenize = reinterpret_cast<TokenizeFn>(dlsym(handle, "ColorerTokenizeV1"));
		return m_tokenize != nullptr;
	}

public:
	// Loads the plugin module and its export, once, before the render budget starts (R42).
	bool Ready() override
	{
		if (m_broken) {
			return false;
		}
		if (m_tokenize == nullptr && !Open()) {
			m_broken = true;
			return false;
		}
		if (m_tokenize != nullptr) {
			// The first parse of a language pays the scheme file load (tens of
			// ms); pay it for the spec's languages here, outside the 40 ms
			// render budget (R43).  A language not listed pays its load inside
			// the budget of its first fence and keeps plain code when it expires.
			static const wchar_t *const kWarmLangs[] = {
				L"cpp", L"js", L"jsx", L"ts", L"py", L"sh", L"rs", L"java", L"css", L"go", L"json"
			};
			std::vector<MdToken> warm;
			CollectCtx ctx{&warm};
			static const wchar_t *const one[] = {L"x = 1;"};
			for (const wchar_t *lang : kWarmLangs) {
				m_tokenize(lang, one, 1, Collect, &ctx, 5000);
			}
			return true;
		}
		return false;
	}

	bool Tokenize(const std::string &lang, const std::vector<std::string> &lines,
		int budget_ms, std::vector<MdToken> &out) override
	{
		if (m_broken) {
			return false;
		}
		if (m_tokenize == nullptr && !Open()) {
			m_broken = true;
			return false;
		}
		const std::string name = MdNormalizeLangName(lang);
		if (name.empty()) {
			return true; // a name the catalog cannot map: plain code role
		}

		const std::wstring wide_lang = utf8_to_wide(name);
		std::vector<std::wstring> wide_lines;
		wide_lines.reserve(lines.size());
		for (const std::string &line : lines) {
			wide_lines.push_back(utf8_to_wide(line));
		}
		std::vector<const wchar_t *> pointers;
		pointers.reserve(wide_lines.size());
		for (const std::wstring &line : wide_lines) {
			pointers.push_back(line.c_str());
		}
		CollectCtx ctx{&out};
		const int rc = m_tokenize(wide_lang.c_str(), pointers.data(),
			static_cast<int>(pointers.size()), Collect, &ctx, budget_ms);
		if (rc == -2) { // unknown language: the fence keeps the plain code role (R44)
			return false;
		}
		if (rc < 0) { // -1 no editorSet, -3 a Colorer exception
			m_broken = true;
			return false;
		}
		return true; // 0 all lines, 1 stopped at the deadline (tokens so far stay)
	}
};

std::unique_ptr<MdTokenSource> g_test_source;

} // namespace

MD_COLORER_API extern std::string MdNormalizeLangName(const std::string &lang)
{
	std::string name;
	if (!ValidName(lang, name)) {
		return std::string();
	}
	for (const LangAlias &alias : s_aliases) {
		if (name == alias.fence) {
			return alias.type;
		}
	}
	return name;
}

MdTokenSource &MdGetTokenSource()
{
	if (g_test_source) {
		return *g_test_source;
	}
	static PluginTokenSource s_plugin_source;
	return s_plugin_source;
}

void MdSetTokenSourceForTest(std::unique_ptr<MdTokenSource> src)
{
	g_test_source = std::move(src);
}

namespace
{

// The export for the look of a list of region names (spec.md, Export).
using RegionLookFn = int (*)(int abi, const wchar_t *const *names, int count,
	unsigned int *look, int capacity, unsigned int *generation);

// The look source that calls the plugin export through the shared module.
// Codes 1 to 4 give generation 0 with no "broken for good" flag: a later
// ReloadBase (a scheme change) can answer.
class PluginLookSource : public MdLookSource
{
	RegionLookFn m_look = nullptr;

	// Finds the export on the shared module, once per process; an old plugin
	// binary without the symbol keeps answering 0.
	RegionLookFn Open()
	{
		if (m_look == nullptr) {
			m_look = reinterpret_cast<RegionLookFn>(
				dlsym(PluginModuleHandle(), "ColorerRegionLookV1"));
		}
		return m_look;
	}

public:
	unsigned long long Generation() override
	{
		RegionLookFn look = Open();
		if (look == nullptr) {
			return 0;
		}
		unsigned int generation = 0;
		if (look(1, nullptr, 0, nullptr, 0, &generation) != 0) {
			return 0;
		}
		return generation;
	}

	unsigned long long Fetch(const char *const *names, int count,
		MdRegionLook *out) override
	{
		RegionLookFn look = Open();
		if (look == nullptr || names == nullptr || out == nullptr || count <= 0) {
			return 0;
		}
		std::vector<std::wstring> wide_names;
		wide_names.reserve(static_cast<size_t>(count));
		for (int i = 0; i < count; ++i) {
			wide_names.push_back(names[i] != nullptr ? utf8_to_wide(names[i])
				: std::wstring());
		}
		std::vector<const wchar_t *> pointers;
		pointers.reserve(static_cast<size_t>(count));
		for (const std::wstring &name : wide_names) {
			pointers.push_back(name.c_str());
		}
		std::vector<unsigned int> words(static_cast<size_t>(2) * count, 0);
		unsigned int generation = 0;
		if (look(1, pointers.data(), count, words.data(), 2 * count, &generation) != 0) {
			return 0;
		}
		for (int i = 0; i < count; ++i) {
			out[i].fore = words[static_cast<size_t>(2) * i];
			out[i].style = words[static_cast<size_t>(2) * i + 1];
		}
		return generation;
	}
};

// The ten region names of the Region map, in the fixed order (spec.md,
// Region map; plan Global Constraints).  MdPalette.cpp maps a role to index.
const char *const s_region_names[] = {
	"def:Text",
	"markdown:HeadText",
	"markdown:LinkText",
	"markdown:StrongText",
	"markdown:EmText",
	"markdown:Code",
	"markdown:QuoteText",
	"markdown:ListMark",
	"markdown:Rule",
	"markdown:Text",
};

std::unique_ptr<MdLookSource> g_test_look_source;

} // namespace

MD_COLORER_API MdLookSource &MdGetLookSource()
{
	if (g_test_look_source) {
		return *g_test_look_source;
	}
	static PluginLookSource s_plugin_look_source;
	return s_plugin_look_source;
}

void MdSetLookSourceForTest(std::unique_ptr<MdLookSource> src)
{
	g_test_look_source = std::move(src);
}

MD_COLORER_API const char *const *MdRegionNames(int *count)
{
	if (count != nullptr) {
		*count = static_cast<int>(sizeof(s_region_names) / sizeof(s_region_names[0]));
	}
	return s_region_names;
}
