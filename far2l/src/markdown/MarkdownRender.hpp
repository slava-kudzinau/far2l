#ifndef FAR2L_MARKDOWN_RENDER_HH
#define FAR2L_MARKDOWN_RENDER_HH

#include <cstdint>
#include <map>
#include <vector>
#include <string>
#include <farplug-wide.h>   // FarColor 16-bit cell attributes (F_*/B_*)
#include "colors.hpp"       // F_LIGHTGRAY, B_BLACK, ...

// A run of text sharing one console color: a full FarColorToReal() value (palette
// entry resolved, 24-bit parts included), drawn with SetColor() by the viewer.
struct MdRun {
    std::wstring text;
    uint64_t attr = 0;
};

// One display line: an ordered sequence of styled runs plus the file offset of
// the source line it came from (used by the viewer for poscache / scrollbar).
struct MdLine {
    std::vector<MdRun> runs;
    int64_t nFilePos = 0;

    // Total visible width in cells (sum of run lengths; cells, not codepoints).
    int Width() const;
};

// Renders a whole markdown document (markdown -> styled display lines) and
// hands out lines block-by-block to the viewer.  A markdown fence tagged
// "mermaid" is delegated to the MermaidEngine -> SvgCanvas pipeline (see
// MermaidEngine.hpp / SvgCanvas.hpp) and cached.
class MarkdownDoc {
public:
    MarkdownDoc() = default;

    // Full-document pass: parses UTF-8 `file_text`, lays out to `width` cells.
    // Every run takes `base_attr`, the viewer text color (FarColorToReal(COL_VIEWERTEXT)).
    void Render(const std::wstring &file_text, int width, uint64_t base_attr);

    // Fills `out` with the display lines of the next block and advances the
    // internal block cursor.  `at_file_pos` is the viewer's current read
    // position: on the first call after Render/Invalidate the cursor is primed
    // to the block containing that offset.  Returns false at document end.
    bool NextLines(std::vector<MdLine> &out, int64_t at_file_pos);

    // Re-run the layout pass at a new width (drop mermaid cache: geometry depends on width).
    void Invalidate(int width);

    // File offset of the first block (document start).
    int64_t FirstBlockFilePos() const { return m_first_pos; }

    bool IsValid() const { return m_valid; }
    // The rendered display lines (for MarkdownPrinter, which consumes rows
    // sequentially starting at the cursor primed by the last NextLines call).
    const std::vector<MdLine> &Lines() const { return m_lines; }
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
    uint64_t m_base_attr = 0;
    bool m_valid = false;

    // The original text + base attr, retained so Invalidate(width) re-renders.
    std::wstring m_text;

    // mermaid render cache keyed by (fence source hash, width).
    std::map<std::pair<size_t, int>, std::vector<MdLine>> m_mermaid_cache;
};

#endif // FAR2L_MARKDOWN_RENDER_HH
