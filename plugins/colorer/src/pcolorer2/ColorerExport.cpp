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

#include "colorer/handlers/RegionMapper.h"
#include "colorer/handlers/StyledRegion.h"

#include <chrono>
#include <cstdint>
#include <cwchar>
#include <memory>
#include <vector>

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

// ---- V2: the real region look per token (story markdown-colorer-fences,
// ADR-010).  An 8-word cell carries the token's span and the region's real
// look: [0] start, [1] end (1-based, inclusive), [2] the scheme's foreground
// as 0xRRGGBB, [3] the scheme's 4-bit foreground (0..15), [4] the Colorer
// style word (bit 4 = underline), [5]..[7] zero.  Colors resolve through the
// active scheme's RegionMapper, which climbs the HRC `parent=` chain (the
// F4 path, LineRegionsSupport): a region whose define leaves the foreground
// unset takes def:Text.

namespace
{
constexpr int V2_WORDS = 8;      // words per cell (the spec's stride)
constexpr int V2_MAX_LINES = 32; // V1's line cap
constexpr int V2_MAX_CELLS = 256; // V1's cell cap

// The base 16 colors as 24-bit, for the 24-bit word of a 4-bit scheme (the
// classic VGA rows; a console hrd writes `fore="#B"` as the 4-bit 11).
const uint32_t s_base16[16] = {
	0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
	0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

// The 24-bit word of a scheme foreground: a 4-bit scheme's index expands
// through the base palette; an rgb scheme's 0xRRGGBB rides as it is.
uint32_t Fore24(unsigned int fore)
{
	if (fore < 16) {
		return s_base16[fore];
	}
	return fore & 0xFFFFFF;
}

// The 4-bit word: an rgb scheme's 0xRRGGBB quantizes to the base palette
// (one bit per channel, the bright bit when any channel is high); a 4-bit
// scheme's index rides as it is.
unsigned int Fore4(unsigned int fore)
{
	if (fore < 16) {
		return fore;
	}
	const unsigned int r = (fore >> 16) & 0xFF;
	const unsigned int g = (fore >> 8) & 0xFF;
	const unsigned int b = fore & 0xFF;
	unsigned int idx = ((b >> 7) & 1) | (((g >> 7) & 1) << 1) | (((r >> 7) & 1) << 2);
	const unsigned int mx = (r > g ? r : g) > b ? (r > g ? r : g) : b;
	if (mx >= 0x80) {
		idx |= 8; // the bright row of the base palette
	}
	return idx;
}

// Writes one 8-word cell per region, in the order Colorer reports it (the
// inner region writes last and wins a shared column, as in V1's R2).  The
// scheme's mapper answers the look; def:Text is the last stop of the chain.
class CellHandler : public RegionHandler
{
	uint64_t* _cells;
	int _capacity;
	int _written;
	RegionMapper* _mapper;
	const StyledRegion* _text; // def:Text's define, null when the scheme has none
	std::chrono::steady_clock::time_point _deadline;
	TextParser* _parser;
	bool _stopped;
	// One open block scheme per entry: the cell it opened (index into
	// _cells, -1 when skipped or capacity was spent) and the line it
	// opened on, so clearLine and leaveScheme can close the span.
	struct Frame { int cell; size_t lno; };
	std::vector<Frame> _frames;
	int _last_line_len = 0;
	void CheckDeadline()
	{
		if (!_stopped && std::chrono::steady_clock::now() >= _deadline) {
			_stopped = true;
			_parser->breakParse();
		}
	}

	void WriteCell(const Region* region, int sx, int ex)
	{
		const StyledRegion* rd = region != nullptr
			? StyledRegion::cast(_mapper->getRegionDefine(region))
			: nullptr;
		uint32_t fore = 0;
		unsigned int style = 0;
		if (rd != nullptr && rd->isForeSet) {
			fore = rd->fore;
			style = rd->style;
		} else if (rd != nullptr) {
			// The region's define exists but leaves the foreground unset:
			// F4 paints def:Text's foreground under it (LineRegionsSupport).
			style = rd->style;
			if (_text != nullptr && _text->isForeSet) {
				fore = _text->fore;
			}
		} else if (_text != nullptr && _text->isForeSet) {
			fore = _text->fore;
			style = _text->style;
		}
		uint64_t* cell = _cells + static_cast<size_t>(_written) * V2_WORDS;
		cell[0] = static_cast<uint64_t>(sx) + 1; // 1-based, inclusive
		cell[1] = static_cast<uint64_t>(ex);
		cell[2] = Fore24(fore);
		cell[3] = Fore4(fore);
		cell[4] = style;
		cell[5] = 0;
		cell[6] = 0;
		cell[7] = 0;
		++_written;
	}

public:
	CellHandler(uint64_t* cells, int capacity, RegionMapper* mapper,
		std::chrono::steady_clock::time_point deadline)
		: _cells(cells), _capacity(capacity), _written(0), _mapper(mapper)
		, _text(StyledRegion::cast(mapper->getRegionDefine(UnicodeString("def:Text"))))
		, _deadline(deadline), _parser(nullptr), _stopped(false)
	{
	}

	void Attach(TextParser* parser) { _parser = parser; }

	bool Stopped() const { return _stopped; }
	int Written() const { return _written; }
	void clearLine(size_t lno, UnicodeString* line) override
	{
		if (lno > 0) {
			for (const Frame& f : _frames) {
				if (f.cell >= 0 && f.lno == lno - 1) {
					_cells[static_cast<size_t>(f.cell) * V2_WORDS + 1] =
						static_cast<uint64_t>(_last_line_len);
				}
			}
		}
		_last_line_len = line != nullptr ? line->length() : 0;
	}

	void addRegion(size_t, UnicodeString*, int sx, int ex, const Region* region) override
	{
		if (_written < _capacity) {
			WriteCell(region, sx, ex);
		}
		CheckDeadline();
	}

	void enterScheme(size_t lno, UnicodeString*, int start, int end, const Region* region,
		const Scheme*) override
	{
		// A block scheme opens here (its enter event spans the start edge).
		// F4 paints the region's foreground over the whole block, so the
		// cell opens at enter and leaveScheme (or clearLine at EOL) closes
		// its span; the leaves written in between paint on top.
		// Skip transparent block schemes (region == nullptr), matching
		// LineRegionsSupport::enterScheme in F4.
		const int cell = (region != nullptr && _written < _capacity) ? _written : -1;
		if (cell >= 0) {
			WriteCell(region, start, end);
		}
		_frames.push_back(Frame{cell, lno});
		CheckDeadline();
	}

	void leaveScheme(size_t lno, UnicodeString*, int, int end, const Region*, const Scheme*) override
	{
		if (!_frames.empty()) {
			const Frame f = _frames.back();
			if (f.cell >= 0 && f.lno == lno) {
				_cells[static_cast<size_t>(f.cell) * V2_WORDS + 1] =
					static_cast<uint64_t>(end); // the block closed at `end`
			}
			_frames.pop_back();
		}
		CheckDeadline();
	}
};
} // namespace

// The V2 export (spec.md, Plugin export ColorerTokenizeV2): the same input
// path as V1 — catalog, language, scheme, tokenize, deadline — and the same
// caps (32 lines, 256 cells).  The return is the number of cells written
// (a deadline stop answers its partial count); -1 no catalog (a disabled
// plugin included), -2 unknown language or an abi the shape does not know,
// -3 a Colorer Exception.  *generation carries the scheme generation of the
// answer, 0 on a code.
SHAREDSYMBOL int ColorerTokenizeV2(int abi, const wchar_t* language, const wchar_t* const* lines,
	int nLines, int budget_ms, uint64_t* cells, int capacity, uint64_t* generation)
{
	if (generation != nullptr) {
		*generation = 0;
	}
	if (abi != 1) {
		return -2; // a larger abi answers -2 (the shape is fixed, ADR-008 precedent)
	}
	// The plugin's viewer session lives on: it keeps the parser factory
	// (the loaded catalog) alive between calls (R48).  The export never
	// creates it: the first call answers a code (the look export's shape).
	if (editorSet == nullptr) {
		return -1; // no FarEditorSet yet: no catalog
	}
	editorSet->viewerEvent(0, nullptr); // prime viewer
	ParserFactory* factory = editorSet->GetParserFactory();
	if (factory == nullptr) {
		return -1; // a disabled plugin: ReloadBase returned early
	}

	try {
		const wchar_t* type_name = AliasLang(language);
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

		RegionMapper* mapper = editorSet->GetRegionMapper();
		if (mapper == nullptr) {
			return -1;
		}
		if (nLines <= 0) {
			if (generation != nullptr) {
				*generation = editorSet->GetBaseGeneration();
			}
			return 0;
		}
		int count = nLines > V2_MAX_LINES ? V2_MAX_LINES : nLines;
		if (capacity > V2_MAX_CELLS) {
			capacity = V2_MAX_CELLS;
		}
		CellHandler handler(cells, capacity, mapper, std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(budget_ms > 0 ? budget_ms : 0));
		const std::unique_ptr<TextParser> parser = factory->createTextParser();
		handler.Attach(parser.get());
		parser->setFileType(file_type);
		parser->setRegionHandler(&handler);
		// One export call per fence (R48, R49): one parse over the fence, the
		// parser cache tree carries the scheme state from one line to the
		// next.  A sentinel empty line closes the regions open at the end.
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
		if (generation != nullptr) {
			*generation = editorSet->GetBaseGeneration();
		}
		return handler.Written(); // 0 when the budget was spent before the first cell
	} catch (const Exception&) {
		return -3;
	} catch (const std::exception&) {
		return -3;
	}
}
