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

#endif // FAR2L_SRC_MARKDOWN_MDCOLORER_HPP
