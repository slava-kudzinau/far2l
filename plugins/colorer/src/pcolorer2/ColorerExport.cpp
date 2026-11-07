// One small export that turns a fence's lines into token spans, for the F3
// Markdown view (story markdown-code-highlight, ADR-008).  The core reaches
// the plugin only through this symbol: ColorerTokenizeV1(lang, lines, count,
// emit, ctx, deadline_ms).  It gives token classes, not colors:
// keyword 1, string 2, comment 3, number 4.  Colors stay in far2l/src/markdown.

#include "pcolorer.h"
#include "FarEditorSet.h"

#include "colorer/Exception.h"
#include "colorer/FileType.h"
#include "colorer/HrcLibrary.h"
#include "colorer/LineSource.h"
#include "colorer/ParserFactory.h"
#include "colorer/Region.h"
#include "colorer/RegionHandler.h"
#include "colorer/Scheme.h"
#include "colorer/TextParser.h"

#include <chrono>
#include <cwchar>
#include <memory>

extern FarEditorSet* editorSet;

namespace
{
// At most this many wide characters of one source line go to the parser
// (spec.md, Input limits); the rest of the line keeps the code role.
constexpr int32_t MAX_LINE_CHARS = 1000;

// A fence name the file-name lookup misses gets the name Colorer knows.
// A name absent from the table is used as it is (plan T1, step 6; R4).
struct LangAlias
{
	const wchar_t* fence;
	const wchar_t* type;
};

const LangAlias s_aliases[] = {
	{L"c++", L"cpp"},
	{L"tsx", L"jsx"},
	{L"zsh", L"sh"},
	{L"python", L"py"},
	{L"bash", L"sh"},
	{L"shell", L"sh"},
	{L"rust", L"rs"},
	{L"javascript", L"js"},
	{L"typescript", L"ts"},
	{L"csharp", L"cs"},
	{L"golang", L"go"},
	{L"kotlin", L"kt"},
	{L"powershell", L"ps1"},
	{L"docker", L"dockerfile"},
	{L"latex", L"tex"},
	{L"plaintext", L"text"},
	{L"perl", L"pl"},
	{L"elixir", L"ex"},
	{L"erlang", L"erl"},
	{L"haskell", L"hs"},
	{L"julia", L"jl"},
	{L"objectivec", L"m"},
	{L"objc", L"m"},
	{L"obj-c", L"m"},
	{L"fsharp", L"fs"},
	{L"vbnet", L"vb"},
	{L"assembly", L"asm"},
	{L"pascal", L"pas"},
	{L"fortran", L"f"},
	{L"make", L"makefile"},
	{L"gitignore", L"gitignore"},
	{L"apache", L"conf"},
	{L"apacheconf", L"apache"},
	{L"blade", L"php"},
	{L"handlebars", L"twig"},
	{L"brainfuck", L"lisp"},
	{L"graphviz", L"dot"},
	{L"terraform", L"hcl"},
};

const wchar_t* AliasLang(const wchar_t* lang)
{
	if (lang == nullptr) {
		return L"";
	}
	for (const LangAlias& a : s_aliases) {
		if (wcscmp(lang, a.fence) == 0) {
			return a.type;
		}
	}
	return lang;
}

// The lines of one fence, at most MAX_LINE_CHARS wide characters each; the
// shared parse cache of one TextParser carries state (an open block comment)
// from one line to the next (plan T1, step 4).
class VectorSource : public LineSource
{
	std::unique_ptr<UnicodeString> _line;
	const wchar_t* const* _lines;
	int _count;

public:
	VectorSource(const wchar_t* const* lines, int count) : _lines(lines), _count(count) {}

	UnicodeString* getLine(size_t lno) override
	{
		if (lno >= static_cast<size_t>(_count) || _lines[lno] == nullptr) {
			return nullptr;
		}
		const size_t len = wcslen(_lines[lno]);
		const int32_t take = (len > static_cast<size_t>(MAX_LINE_CHARS)) ? MAX_LINE_CHARS : static_cast<int32_t>(len);
		_line = std::make_unique<UnicodeString>(_lines[lno], take);
		return _line.get();
	}
};

// Maps a region name to a token class by its base name (the part after the
// last '.').  A suffix of a class name counts as that class: a
// 'def:StringEdge' is a string, a 'CommentDoc' a comment, a 'NumberDec' a
// number (js.hrc and friends name their leaves this way).
int ClassForName(const UnicodeString& name)
{
	const wchar_t* base = wcsrchr(name.getBuffer(), L'.');
	base = (base != nullptr) ? base + 1 : name.getBuffer();
	const wchar_t* colon = wcsrchr(base, L':');
	base = (colon != nullptr) ? colon + 1 : base;   // R49: 'jScript:Numbers' is a number
	if (wcsncmp(base, L"def:", 4) == 0) {
		base += 4;
	}
	if (wcsncmp(base, L"Key", 3) == 0 || wcsncmp(base, L"Command", 7) == 0
		|| wcscmp(base, L"SymbolStrong") == 0) {
		return 1;   // Keyword, KeyWord, CommandReserved (bash), SymbolStrong (bash)
	}
	if (wcsncmp(base, L"Str", 3) == 0 || wcsncmp(base, L"Character", 9) == 0
		|| wcscmp(base, L"Subregion") == 0) {
		return 2;   // String, StrQuotes, Subregion: the gaps between scheme leaves
	}
	if (wcsncmp(base, L"Comment", 7) == 0) {
		return 3;
	}
	if (wcscmp(base, L"PairStart") == 0 || wcscmp(base, L"PairEnd") == 0) {
		return 3;  // the delimiters and continuation leaves of a comment block
	}
	if (wcsncmp(base, L"Num", 3) == 0) {
		return 4;   // Number, NumberDec, Numbers
	}
	return 0;
}

// Climbs to the nearest class ancestor (story, Scope 2).  0 means "no
// class"; the text keeps the code role.
int ClassForRegion(const Region* region)
{

	for (const Region* r = region; r != nullptr; r = r->getParent()) {
		const int cls = ClassForName(r->getName());
		if (cls != 0) {
			return cls;
		}
	}
	return 0;
}

// Emits every matching region in the order Colorer reports it; a nested
// region emits its span after its parent's, so the printer paints the inner
// span last and it wins a shared cell (R2).  The events carry the region's
// start and end in cells of that source line, half-open [start, end).
class EmitHandler : public RegionHandler
{
	void (*_emit)(void* ctx, int line, int start, int end, int token_class);
	void* _ctx;
	std::chrono::steady_clock::time_point _deadline;
	TextParser* _parser;
	bool _stopped;

	void CheckDeadline()
	{
		if (!_stopped && std::chrono::steady_clock::now() >= _deadline) {
			_stopped = true;
			_parser->breakParse();
		}
	}

public:
	EmitHandler(void (*emit)(void* ctx, int line, int start, int end, int token_class), void* ctx,
		std::chrono::steady_clock::time_point deadline)
		: _emit(emit), _ctx(ctx), _deadline(deadline), _parser(nullptr), _stopped(false)
	{
	}

	void Attach(TextParser* parser) { _parser = parser; }

	bool Stopped() const { return _stopped; }

	void addRegion(size_t lno, UnicodeString*, int sx, int ex, const Region* region) override
	{
		const int cls = ClassForRegion(region);
		if (cls != 0) {
			_emit(_ctx, static_cast<int>(lno), sx, ex, cls);
		}
		CheckDeadline();
	}

	void enterScheme(size_t lno, UnicodeString*, int start, int end, const Region* region,
		const Scheme* scheme) override
	{
		const int cls = ClassForRegion(region);   // block schemes carry their region here
		if (cls != 0) {
			_emit(_ctx, static_cast<int>(lno), start, end, cls);
		}
		CheckDeadline();
	}

	void leaveScheme(size_t, UnicodeString*, int, int, const Region*, const Scheme*) override
	{
		CheckDeadline();
	}
};
} // namespace

// Return codes: 0 all lines emitted, 1 stopped at the deadline, -1 no
// FarEditorSet (a disabled plugin included), -2 unknown language, -3 a
// Colorer Exception (events emitted before it stay).
SHAREDSYMBOL int ColorerTokenizeV1(const wchar_t* lang, const wchar_t* const* lines, int count,
	void (*emit)(void* ctx, int line, int start, int end, int token_class),
	void* ctx, int deadline_ms)
{
	// The plugin's viewer session lives on: it keeps the parser factory
	// (the loaded catalog) alive between calls (R48).
	if (editorSet == nullptr) {
		editorSet = new FarEditorSet();
	}
	editorSet->viewerEvent(0, nullptr); // prime viewer
	ParserFactory* factory = editorSet->GetParserFactory();
	if (factory == nullptr) {
		return -1; // a disabled plugin: ReloadBase returned early
	}
	if (deadline_ms <= 0) {
		return 1; // the budget is already spent
	}
	if (count <= 0) {
		return 0;
	}

	try {
		const wchar_t* type_name = AliasLang(lang);
		UnicodeString file_name(L"x.");
		file_name.append(UnicodeString(type_name));
		HrcLibrary& library = factory->getHrcLibrary();
		FileType* file_type = library.chooseFileType(&file_name, nullptr, 0);
		if (file_type == nullptr || file_type->getName() == UnicodeString("default")) {
			// The name may be a Colorer type name, not a file extension:
			// 'gitignore' has a type, no '*.gitignore' mask (R4).
			file_type = library.getFileType(UnicodeString(type_name));
		}
		if (file_type == nullptr) {
			return -2;
		}
		if (file_type->getBaseScheme() == nullptr) {
			library.loadFileType(file_type);   // the scheme file loads on demand (R45)
		}
		if (file_type->getName() == UnicodeString("default")) {
			return -2; // the type 'default' means no colors
		}

		// One export call per fence (R48): the handler collects the tokens of
		// the whole fence in one parse.
		EmitHandler handler(emit, ctx, std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(deadline_ms));
		const std::unique_ptr<TextParser> parser = factory->createTextParser();
		handler.Attach(parser.get());
		parser->setFileType(file_type);
		parser->setRegionHandler(&handler);
		// One export call per fence (R48, R49): one parse over the fence,
		// the parser cache tree carries the scheme state (an open block
		// comment) from one line to the next.  A sentinel empty line closes
		// the regions that are open at the fence's end (R49).
		const wchar_t* sentinel = L"";
		std::vector<const wchar_t*> all;
		all.reserve(static_cast<size_t>(count) + 1);
		for (int i = 0; i < count; ++i) {
			all.push_back(lines[i]);
		}
		all.push_back(sentinel);
		const VectorSource source(all.data(), count + 1);
		parser->setLineSource(const_cast<VectorSource *>(&source));
		parser->parse(0, count + 1, TextParser::TextParseMode::TPM_CACHE_UPDATE);
		if (handler.Stopped()) {
			return 1; // the budget spent; the lines after it keep plain code
		}
		return 0;
	} catch (const Exception&) {
		return -3;
	} catch (const std::exception&) {
		return -3;
	}
}
