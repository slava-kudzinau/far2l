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
};

// The export's emit context: tokens land in the caller's vector.
struct CollectCtx
{
	std::vector<MdToken> *out;
};

void Collect(void *ctx, int, int start, int end, int cls)
{
	CollectCtx *c = static_cast<CollectCtx *>(ctx);
	c->out->push_back(MdToken{static_cast<uint32_t>(start), static_cast<uint32_t>(end),
		static_cast<uint8_t>(cls)});
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

std::string MdNormalizeLangName(const std::string &lang)
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

// The token source that calls the plugin export.  The module is located once
// per process; any failure sets one "no colors" flag for good.
class PluginTokenSource : public MdTokenSource
{
	TokenizeFn m_tokenize = nullptr;
	bool m_broken = false;

	// Finds the loaded plugin module and its export, once.
	bool Open()
	{
		if (CtrlObject == nullptr) {
			return false;
		}
		Plugin *plugin = CtrlObject->Plugins.FindPlugin(SYSID_COLORER);
		if (plugin == nullptr) {
			return false;
		}
		plugin->Load();
		const std::string module = wide_to_utf8(plugin->GetModuleName());
		if (module.empty()) {
			return false;
		}
		// The exact path far2l passed to its own dlopen, asked back with
		// RTLD_NOLOAD: the module the process already loaded answers.
		void *handle = dlopen(module.c_str(), RTLD_LAZY | RTLD_NOLOAD);
		if (handle == nullptr) {
			return false;
		}
		m_tokenize = reinterpret_cast<TokenizeFn>(dlsym(handle, "ColorerTokenizeV1"));
		return m_tokenize != nullptr;
	}

public:
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
		if (rc < 0) { // -1 no editorSet, -2 unknown language, -3 a Colorer exception
			m_broken = true;
			return false;
		}
		return true; // 0 all lines, 1 stopped at the deadline (tokens so far stay)
	}
};

std::unique_ptr<MdTokenSource> g_test_source;

} // namespace

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
