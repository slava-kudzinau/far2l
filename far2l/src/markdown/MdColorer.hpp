#ifndef FAR2L_SRC_MARKDOWN_MDCOLORER_HPP
#define FAR2L_SRC_MARKDOWN_MDCOLORER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// One token span of one fence line, half-open [start, end), in cells of that
// line's code-run text (the 4-cell code indent counted).  cls is the token
// class: 1 keyword, 2 string, 3 comment, 4 number.
struct MdToken
{
	uint32_t start;
	uint32_t end;
	uint8_t cls;
	uint32_t line = 0;   // 0-based index into the lines of the Tokenize call
};

// One token span carrying the scheme's look, not a class (story
// markdown-colorer-fences, ADR-010): fg is the region's 0xRRGGBB foreground,
// fg4 the scheme's 4-bit foreground (0..15), underline the style word's bit 4.
// Coordinates are half-open [start, end), in cells of that line's code-run
// text (the 4-cell code indent counted), like MdToken.
struct MdTokenRange
{
	uint32_t start;
	uint32_t end;
	uint32_t fg;       // 0xRRGGBB
	uint32_t fg4;      // 0..15
	bool underline = false;
	uint32_t line = 0; // 0-based index into the lines of the TokenizeV2 call
};

// The alias row of a Colorer token class: the fork table's 24-bit default
// and its 4-bit foreground (MdPalette.cpp; the V1 fallback of TokenizeV2).
void MdTokenClassLook(uint8_t cls, uint32_t *fg, uint32_t *fg4);


// Token source for fence lines: the Colorer plugin export, or a fake for tests.
class MdTokenSource
{
public:
	virtual ~MdTokenSource() = default;

	// Tokenizes the lines of a fence named lang within budget_ms.  Appends
	// the tokens of all lines to out, in line order; a token's coordinates
	// address its own line.  Returns false when no colors are available; the
	// caller then keeps the plain code role.
	virtual bool Tokenize(const std::string &lang, const std::vector<std::string> &lines,
		int budget_ms, std::vector<MdToken> &out) = 0;

	// Tokenizes the lines of a fence named lang within budget_ms, in the
	// scheme's colors: appends the ranges of all lines to out, in line order;
	// a range's coordinates address its own line, and its `line` is the
	// 0-based index into lines.  `generation` answers the scheme generation of
	// the answer, 0 when no scheme backs it.  Returns false when no colors are
	// available; the caller then keeps the plain code role.
	// The plugin source calls ColorerTokenizeV2; a missing symbol (an old
	// plugin binary) falls back to V1's classes through the alias table: the
	// range keeps the class's four alias rows as the 24-bit and 4-bit pair.
	virtual bool TokenizeV2(const std::string &lang, const std::vector<std::string> &lines,
		int budget_ms, std::vector<MdTokenRange> &out, unsigned long long &generation)
	{
		// The V1 fallback of the color bridge (spec.md, The core bridge): the
		// export's four classes, each carrying its alias row as the 24-bit and
		// 4-bit pair.  The key's generation rides the answer: the alias rows
		// are the fork's own table, stable under a scheme, so the cache stays
		// warm across resizes.  Inline, like Ready: the drivers link the core.
		std::vector<MdToken> flat;
		if (!Tokenize(lang, lines, budget_ms, flat)) {
			return false;
		}
		for (const MdToken &t : flat) {
			MdTokenRange r{t.start, t.end, 0, 0, false, t.line};
			MdTokenClassLook(t.cls, &r.fg, &r.fg4);
			out.push_back(r);
		}
		return true;
	}

	// Loads the source's backing (the plugin), once, outside the render budget
	// (R42).  True when tokens are available; the caller keeps the plain code role.
	virtual bool Ready() { return true; }
};

// The fence name the export call gets: lower-cased and alias-mapped (c++ ->
// cpp, tsx -> jsx, zsh -> sh).  Empty when the name is not usable: at most
// 32 characters of [A-Za-z0-9+#_-], non-empty (spec.md, Language names).
#if defined(__GNUC__)
#define MD_COLORER_API \
	__attribute__((visibility("default"), used))
#else
#define MD_COLORER_API
#endif
MD_COLORER_API std::string MdNormalizeLangName(const std::string &lang);

// The plugin source, or the source installed for the test.
MdTokenSource &MdGetTokenSource();

// Keeps the source for the life of the process.
void MdSetTokenSourceForTest(std::unique_ptr<MdTokenSource> src);


// The look of one Colorer region under the active scheme: fore is 0xRRGGBB,
// style is the scheme's style word (bit 4 = underline).
struct MdRegionLook
{
	uint32_t fore;
	uint32_t style;
};

// Look source for the Markdown roles: the Colorer plugin export, or a fake
// for tests.  The plugin source reuses the module handle of the fence-token
// source: one module load, two symbols (ColorerTokenizeV1,
// ColorerRegionLookV1).  A missing ColorerRegionLookV1 (an old plugin
// binary) gives the fork table, and the fence tokens keep working.
class MdLookSource
{
public:
	virtual ~MdLookSource() = default;

	// The current scheme generation; 0 when no look is available.  Cheap: the
	// plugin source answers it with a count-0 export call.  Safe to call per
	// run.
	virtual unsigned long long Generation() = 0;

	// Fills out[0..count) for names[0..count).  Returns the scheme generation,
	// or 0 when no look is available (the export answered 1, 2, 3, 4, dlsym
	// found nothing, or every name came back empty).
	virtual unsigned long long Fetch(const char *const *names, int count,
		MdRegionLook *out) = 0;
};

// The plugin source, or the source installed for the test.
MD_COLORER_API MdLookSource &MdGetLookSource();

// Keeps the source for the life of the process.
void MdSetLookSourceForTest(std::unique_ptr<MdLookSource> src);

// The ten region names of the Region map, in the fixed order, and their
// count.  MdColorer.cpp owns the table; MdPalette.cpp maps a role to index.
MD_COLORER_API const char *const *MdRegionNames(int *count);

#endif // FAR2L_SRC_MARKDOWN_MDCOLORER_HPP
