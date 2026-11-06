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
	{L"rust", L"rs"},
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

// Maps a region to a token class by climbing to the nearest def:* ancestor
// (story, Scope 2).  0 means "no class"; the text keeps the code role.
int ClassForRegion(const Region* region)
{
	static const UnicodeString s_keyword(L"def:Keyword");
	static const UnicodeString s_string(L"def:String");
	static const UnicodeString s_character(L"def:Character");
	static const UnicodeString s_comment(L"def:Comment");
	static const UnicodeString s_number(L"def:Number");

	for (const Region* r = region; r != nullptr; r = r->getParent()) {
		const UnicodeString& name = r->getName();
		if (name == s_keyword) {
			return 1;
		}
		if (name == s_string || name == s_character) {
			return 2;
		}
		if (name == s_comment) {
			return 3;
		}
		if (name == s_number) {
			return 4;
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

	void enterScheme(size_t, UnicodeString*, int, int, const Region*, const Scheme*) override
	{
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
	if (editorSet == nullptr) {
		return -1;
	}
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
		if (file_type == nullptr) {
			return -2;
		}
		if (file_type->getName() == UnicodeString("default")) {
			return -2; // the type 'default' means no colors
		}

		VectorSource source(lines, count);
		EmitHandler handler(emit, ctx, std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(deadline_ms));

		const std::unique_ptr<TextParser> parser = factory->createTextParser();
		handler.Attach(parser.get());
		parser->setFileType(file_type);
		parser->setLineSource(&source);
		parser->setRegionHandler(&handler);
		parser->parse(0, count, TextParser::TextParseMode::TPM_CACHE_UPDATE);
		return handler.Stopped() ? 1 : 0;
	} catch (const Exception&) {
		return -3;
	} catch (const std::exception&) {
		return -3;
	}
}
