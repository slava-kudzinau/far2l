#ifndef FAR2L_MARKDOWN_RENDER_HH
#define FAR2L_MARKDOWN_RENDER_HH

#include <cstdint>
#include <map>
#include <vector>
#include <string>
#include <chrono>
#include <tuple>
#include <cstdint>
#include "MdColorer.hpp"

// Palette role of a run.  The viewer maps it to a far palette entry when it draws, so a theme
// change shows on the next redraw.  Plain integers, so the layout code can build runs as (text, role).
enum MdRole : uint64_t {
    MD_ROLE_BODY = 0,      // body text and list text (the viewer text entry)
    MD_ROLE_H1,            // heading level 1
    MD_ROLE_H2,            // heading level 2
    MD_ROLE_H3,            // headings level 3 to 6
    MD_ROLE_STRONG,        // strong text and table header text with no other span
    MD_ROLE_LINK,          // link text
    MD_ROLE_CODE,          // code spans and code fences
    MD_ROLE_QUOTE,         // block quote text and quote bars
    MD_ROLE_RULE,          // horizontal rules and table borders
    MD_ROLE_ART,           // mermaid art
    MD_ROLE_TOKEN_KEYWORD, // keyword token in a fenced code block
    MD_ROLE_TOKEN_STRING,  // string token in a fenced code block
    MD_ROLE_TOKEN_COMMENT, // comment token in a fenced code block
    MD_ROLE_TOKEN_NUMBER,  // number token in a fenced code block
    MD_ROLE_EM,            // emphasis text: _text_ and *text* (story markdown-colorer-look)
    MD_ROLE_LISTMARK,      // the bullet, the number, the task box of a list item
    MD_ROLE_COUNT,         // one past the last role
};

// A run of text sharing one palette role.
struct MdRun {
    std::wstring text;
    uint64_t role = MD_ROLE_BODY;   // an MdRole
};

// The token ranges of one fence: one group per fence, shared by its lines,
// in cells of that row's single code run (the 4-cell code indent counted), so
// the printer needs no offset math and a horizontal scroll works on the same
// coordinates.  A range carries the scheme's look (fg, fg4, underline), not a
// class (story markdown-colorer-fences): PrintCodeRuns paints it.
struct MdTokenGroup {
    std::vector<MdTokenRange> ranges;
};

// One display line: an ordered sequence of styled runs plus its source offset: the file
// offset where the source line holding its first source text starts.  Offsets never decrease
// down the doc, so the viewer finds the line for a file position (poscache, mode switch) by searching them.
struct MdLine {
    std::vector<MdRun> runs;
    int64_t nFilePos = 0;
    int tokens = -1;   // index into MarkdownDoc::m_token_groups; -1: no token group
    // The code block's background (story markdown-code-tint): 0 marks a tinted row, -1
    // none.  The fence builder sets it on the fence's rows (the code rows and the hint row
    // under undrawn mermaid art); art rows keep their own background.  The printer resolves
    // the row's tint to the live theme through MdCodeTint, so a theme change shows on the
    // next redraw.
    int64_t tint = -1;

    // Total visible width in cells (sum of run lengths; cells, not codepoints).
    int Width() const;
};

// Renders a whole markdown document (markdown -> styled display lines) and
// hands out lines block-by-block to the viewer.  A markdown fence tagged
// "mermaid" is drawn by AsciiMermaid (see AsciiMermaid.hpp) and cached; a fence
// it cannot draw keeps its code rows and gets a hint row under them.
class MarkdownDoc {
public:
    MarkdownDoc() = default;

    // Full-document pass: parses UTF-8 `file_text`, lays out to `width` cells.  Runs carry
    // palette roles (MdRole), not colors, so the colors follow the theme at draw time.
    void Render(const std::wstring &file_text, int width);

    // Fills `out` with the display lines of the next block and advances the
    // internal block cursor.  `at_file_pos` is the viewer's current read
    // position: on the first call after Render/Invalidate the cursor is primed
    // to the block containing that offset.  Returns false at document end.
    bool NextLines(std::vector<MdLine> &out, int64_t at_file_pos);

    // Re-run the layout pass at a new width (the mermaid cache is dropped with it).
    void Invalidate(int width);

    // File offset of the first block (document start).
    int64_t FirstBlockFilePos() const { return m_first_pos; }

    bool IsValid() const { return m_valid; }
    // The rendered display lines; the viewer draws rows from them by index.
    const std::vector<MdLine> &Lines() const { return m_lines; }
    // The token groups of the current render, indexed by MdLine::tokens.
    const std::vector<MdTokenGroup> &TokenGroups() const { return m_token_groups; }
    int64_t Cursor() const { return m_cursor; }
    void SetCursor(int64_t c) { m_cursor = c; m_cursor_pos = (c >= 0 && c < (int64_t)m_lines.size()) ? m_lines[c].nFilePos : -1; }
    int Width() const { return m_width; }

private:
    std::vector<MdLine> m_lines;              // all display lines, in order
    std::vector<int64_t> m_block_starts;      // file offsets where blocks begin
    std::vector<size_t> m_block_first_line;   // index into m_lines for each block
    int64_t m_cursor = 0;                     // next block to hand out
    int64_t m_first_pos = 0;
    int64_t m_cursor_pos = 0;   // last at_file_pos the cursor was primed to
    int m_width = 0;
    bool m_valid = false;

    // The original text, retained so Invalidate(width) re-renders.
    std::wstring m_text;

    // mermaid render cache keyed by (fence source hash, width).
    std::map<std::pair<size_t, int>, std::vector<MdLine>> m_mermaid_cache;

    // Token groups of the current render, indexed by MdLine::tokens.
    std::vector<MdTokenGroup> m_token_groups;

    // Fence token cache keyed by (normalized fence language, FNV-1a of the
    // cap-trimmed fence source, the Colorer scheme generation), in the form of
    // m_mermaid_cache: a scheme change re-keys every fence, so the next render
    // refetches it once; a resize does not (R30).  No eviction: entries live
    // for the doc's life.
    std::map<std::tuple<std::string, uint64_t, unsigned long long>,
        std::vector<MdTokenGroup>> m_token_cache;
};

#endif // FAR2L_MARKDOWN_RENDER_HH
