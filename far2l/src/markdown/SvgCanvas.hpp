#ifndef FAR2L_SVG_CANVAS_HH
#define FAR2L_SVG_CANVAS_HH

#include <string>
#include <vector>
#include <cstdint>
#include "MarkdownRender.hpp"

// Decodes an SVG string (the XML subset mermaid emits) into a cell grid of
// box-drawing glyphs, emitting MdLines.  Pure C++, no deps.
class SvgCanvas {
public:
    struct Shape {
        enum Kind { RECT, POLY, TEXT } kind = RECT;
        double x1 = 0, y1 = 0;                          // RECT: top-left corner
        double w = 0, h = 0;                            // RECT: size
        std::vector<std::pair<double, double>> pts;     // POLY: vertices in order
        std::wstring text;                              // TEXT: one line of label text
        std::wstring anchor;                            // TEXT: start | middle | end
        double tx = 0, ty = 0;                          // TEXT: start point; ty is the glyph centre
        bool arrow_start = false, arrow_end = false;    // POLY: marker-start / marker-end
    };

    // Parses an SVG string; coordinates are user units (px).  Returns false
    // when the document has no drawable shape.
    bool Parse(const std::string &svg);

    // Replaces the contents of `out` with the box art as MdLines carrying `attr`, at most
    // `width_cells` columns wide.  A cell is 8x16 px; a diagram wider than
    // that is compressed horizontally to fit.
    void ToCells(int width_cells, std::vector<MdLine> &out, uint64_t attr) const;

    bool IsValid() const { return m_valid; }

private:
    std::vector<Shape> m_shapes;
    bool m_valid = false;
};

#endif // FAR2L_SVG_CANVAS_HH
