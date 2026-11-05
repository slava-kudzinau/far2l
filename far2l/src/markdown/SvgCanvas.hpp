#ifndef FAR2L_SVG_CANVAS_HH
#define FAR2L_SVG_CANVAS_HH

#include <string>
#include <vector>
#include <cstdint>
#include "MarkdownRender.hpp"

// Decodes an SVG string (XML subset produced by mermaid) into a cell grid of
// Unicode box-drawing glyphs, emitting MarkdownLines.  Pure C++, no deps.
class SvgCanvas {
public:
    struct Shape {
        enum Kind { RECT, LINE, POLY, TEXT } kind;
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        double w = 0, h = 0;
        std::vector<std::pair<double, double>> pts;
        std::wstring text;
        std::wstring anchor;   // start | middle | end
        double tx = 0, ty = 0;
    };

    // Parses an SVG string (tags/attrs/entities, geometry subset).
    bool Parse(const std::string &svg);

    // Rasterizes into `width_cells` wide box art appended to `out` as
    // MdLines carrying `attr`.
    void ToCells(int width_cells, std::vector<MdLine> &out, uint16_t attr) const;

    bool IsValid() const { return m_valid; }

private:
    std::vector<Shape> m_shapes;
    double m_x = 0, m_y = 0, m_w = 0, m_h = 0;
    bool m_valid = false;
};

#endif // FAR2L_SVG_CANVAS_HH
