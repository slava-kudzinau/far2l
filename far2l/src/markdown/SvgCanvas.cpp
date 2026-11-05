#include "SvgCanvas.hpp"
#include "headers.hpp"
#include "interf.hpp"   // BoxSymbols[]

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

// ---------------------------------------------------------------------------
// Minimal SVG -> cell grid rasterizer for mermaid's output: axis-aligned
// rects become box-drawing frames, straight segments (line/path/polygon)
// become ─│╱╲ runs, and <text> becomes plain label cells.  Coordinates are
// mapped from SVG user units to cells at a ~2:1 cell aspect ratio.  This is a
// legibility-first rasterizer (recognizable flow/sequence diagrams), not a
// pixel-perfect renderer: curves are approximated by their endpoints.
// ---------------------------------------------------------------------------


namespace {

using Shape = SvgCanvas::Shape;

std::wstring utf8_to_wide(const char *s, size_t n)
{
    std::wstring out;
    const unsigned char *p = (const unsigned char *)s;
    for (size_t i = 0; i < n; ) {
        unsigned cp = p[i];
        int extra = 0;
        if (cp < 0x80) { out.push_back(wchar_t(cp)); i += 1; continue; }
        else if ((cp & 0xE0) == 0xC0) { cp &= 0x1F; extra = 1; }
        else if ((cp & 0xF0) == 0xE0) { cp &= 0x0F; extra = 2; }
        else if ((cp & 0xF8) == 0xF0) { cp &= 0x07; extra = 3; }
        else { i += 1; continue; }
        for (int k = 0; k < extra && i + 1 + k < n; ++k)
            cp = (cp << 6) | (p[i + 1 + k] & 0x3F);
        out.push_back(wchar_t(cp));
        i += 1 + extra;
    }
    return out;
}

std::string unescape(const std::string &s)
{
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&') {
            if (s.compare(i, 5, "&amp;") == 0) { o += '&'; i += 4; }
            else if (s.compare(i, 4, "&lt;") == 0) { o += '<'; i += 3; }
            else if (s.compare(i, 4, "&gt;") == 0) { o += '>'; i += 3; }
            else if (s.compare(i, 6, "&quot;") == 0) { o += '"'; i += 5; }
            else if (s.compare(i, 6, "&apos;") == 0) { o += '\''; i += 5; }
            else if (s.compare(i, 6, "&nbsp;") == 0) { o += ' '; i += 5; }
            else o += s[i];
        } else
            o += s[i];
    }
    return o;
}

double attr(const std::string &attrs, const char *key, double def = 0)
{
    std::string k = std::string(key) + "=\"";
    size_t p = attrs.find(k);
    if (p == std::string::npos)
        return def;
    p += k.size();
    size_t e = attrs.find('"', p);
    return std::strtod(attrs.substr(p, e - p).c_str(), nullptr);
}

std::string attr_str(const std::string &attrs, const char *key)
{
    std::string k = std::string(key) + "=\"";
    size_t p = attrs.find(k);
    if (p == std::string::npos)
        return std::string();
    p += k.size();
    size_t e = attrs.find('"', p);
    return attrs.substr(p, e - p);
}

}   // namespace

bool SvgCanvas::Parse(const std::string &svg)
{
    m_shapes.clear();
    m_valid = false;
    m_w = m_h = 0;

    // viewBox="x y w h" wins, else width/height attrs.
    std::string vb = attr_str(svg, "viewBox");
    if (!vb.empty()) {
        double v[4];
        int n = sscanf(vb.c_str(), "%lf %lf %lf %lf", &v[0], &v[1], &v[2], &v[3]);
        if (n == 4) { m_x = v[0]; m_y = v[1]; m_w = v[2]; m_h = v[3]; }
    }
    if (m_w <= 0) m_w = attr(svg, "width", 0);
    if (m_h <= 0) m_h = attr(svg, "height", 0);
    if (m_w <= 0 || m_h <= 0)
        return false;

    // Scan tags, tracking <g transform="translate(x,y)"> offsets, collecting shapes.
    std::vector<std::pair<double, double>> stack;   // g translate stack
    double tx = 0, ty = 0;
    size_t i = 0;
    const size_t n = svg.size();
    while (i < n) {
        size_t lt = svg.find('<', i);
        if (lt == std::string::npos) break;
        size_t gt = svg.find('>', lt);
        if (gt == std::string::npos) break;
        std::string tag = svg.substr(lt + 1, gt - lt - 1);
        i = gt + 1;
        if (tag.empty()) continue;

        bool closing = tag[0] == '/';
        if (closing) {
            if (tag.rfind("/g", 0) == 0 && !stack.empty()) {
                stack.pop_back();
                tx = stack.empty() ? 0 : stack.back().first;
                ty = stack.empty() ? 0 : stack.back().second;
            }
            continue;
        }
        if (tag.back() == '/') tag.pop_back();   // self-closing
        // tag = "name attrs"
        size_t sp = tag.find_first_of(" \t\n");
        std::string name = (sp == std::string::npos) ? tag : tag.substr(0, sp);
        std::string attrs = (sp == std::string::npos) ? "" : tag.substr(sp);
        (void)name;

        // g with translate
        if (name == "g") {
            std::string tf = attr_str(attrs, "transform");
            double nx = tx, ny = ty;
            size_t t = tf.find("translate(");
            if (t != std::string::npos) {
                double a = 0, b = 0;
                sscanf(tf.c_str() + t + 10, " %lf %lf", &a, &b);
                nx += a; ny += b;
            }
            stack.push_back({ nx, ny });
            tx = nx; ty = ny;
            continue;
        }
        // text: consume until </text>
        if (name == "text" || name == "tspan" || name == "tflowRoot") {
            size_t end = svg.find("</" + name, gt);
            std::string body = (end == std::string::npos) ? "" : svg.substr(i, end - i);
            if (end != std::string::npos) i = svg.find('>', end) + 1;
            // strip inner tags (tspan inside text)
            std::string plain;
            for (size_t b = 0; b < body.size(); ) {
                if (body[b] == '<') { size_t e = body.find('>', b); b = (e == std::string::npos) ? body.size() : e + 1; }
                else { plain += body[b++]; }
            }
            Shape s;
            s.kind = Shape::TEXT;
            s.tx = attr(attrs, "x", 0) + tx;
            s.ty = attr(attrs, "y", 0) + ty;
            s.anchor = utf8_to_wide(unescape(attr_str(attrs, "text-anchor")).c_str(), unescape(attr_str(attrs, "text-anchor")).size());
            s.text = utf8_to_wide(unescape(plain).c_str(), unescape(plain).size());
            if (!s.text.empty()) m_shapes.push_back(std::move(s));
            continue;
        }
        Shape s;
        if (name == "rect") {
            s.kind = Shape::RECT;
            s.x1 = attr(attrs, "x", 0) + tx;
            s.y1 = attr(attrs, "y", 0) + ty;
            s.w = attr(attrs, "width", 0);
            s.h = attr(attrs, "height", 0);
        } else if (name == "line") {
            s.kind = Shape::LINE;
            s.x1 = attr(attrs, "x1", 0) + tx;
            s.y1 = attr(attrs, "y1", 0) + ty;
            s.x2 = attr(attrs, "x2", 0) + tx;
            s.y2 = attr(attrs, "y2", 0) + ty;
        } else if (name == "polygon" || name == "polyline") {
            s.kind = Shape::POLY;
            std::string ps = attr_str(attrs, "points");
            double a = 0, b = 0;
            const char *c = ps.c_str();
            bool closed = (name == "polygon");
            bool first = true, startx = true;
            double sx = 0, sy = 0;
            while (*c) {
                char *nxt;
                double v = std::strtod(c, &nxt);
                if (nxt == c) { c++; continue; }
                c = nxt;
                if (startx) { a = v; startx = false; }
                else { b = v; startx = true;
                    if (first) { sx = a + tx; sy = b + ty; first = false; }
                    s.pts.push_back({ a + tx, b + ty }); }
            }
            (void)sx; (void)sy;
            if (closed && s.pts.size() >= 2) s.pts.push_back(s.pts.front());
        } else if (name == "path") {
            // Approximate: M/L segments (drop curves).
            s.kind = Shape::POLY;
            std::string d = attr_str(attrs, "d");
            double cx = 0, cy = 0, px = 0, py = 0;
            for (size_t p = 0; p < d.size(); ) {
                char cmd = d[p];
                if (cmd == 'M' || cmd == 'L' || cmd == 'H' || cmd == 'V' ||
                    cmd == 'm' || cmd == 'l' || cmd == 'h' || cmd == 'v' ||
                    cmd == 'C' || cmd == 'c' || cmd == 'Q' || cmd == 'q' ||
                    cmd == 'S' || cmd == 's' || cmd == 'T' || cmd == 't' ||
                    cmd == 'Z' || cmd == 'z') {
                    p++;
                    std::string nums;
                    while (p < d.size() && (isdigit((unsigned char)d[p]) || d[p] == '.' || d[p] == '-' || d[p] == ',' || d[p] == ' ' || d[p] == '+' || d[p] == 'e' || d[p] == 'E'))
                        nums += d[p++];
                    double a = 0, b = 0;
                    char *nxt;
                    if (!nums.empty()) {
                        a = std::strtod(nums.c_str(), &nxt);
                        const char *r = nxt;
                        b = std::strtod(r, &nxt);
                        (void)b;
                    }
                    // re-parse the two numbers robustly
                    double v0 = 0, v1 = 0;
                    int cnt = 0;
                    {
                        std::string t;
                        const char *q = nums.c_str();
                        while (*q) {
                            char *n2;
                            double val = std::strtod(q, &n2);
                            if (n2 == q) { q++; continue; }
                            q = n2;
                            if (cnt == 0) v0 = val; else if (cnt == 1) v1 = val;
                            cnt++;
                        }
                    }
                    switch (cmd) {
                        case 'M': case 'L': cx = v0 + tx; cy = v1 + ty; break;
                        case 'm': case 'l': cx += v0; cy += v1; break;
                        case 'H': cx = v0 + tx; break;
                        case 'h': cx += v0; break;
                        case 'V': cy = v1 + ty; break;
                        case 'v': cy += v1; break;
                        case 'C': case 'c': case 'Q': case 'q': case 'S': case 's': case 'T': case 't':
                            cx = v0 + tx; cy = v1 + ty; break;
                    }
                    if (cmd == 'M' || cmd == 'm') s.pts.push_back({ cx, cy });
                    else if (cmd != 'Z' && cmd != 'z' && (cmd == 'L' || cmd == 'l' || cmd == 'H' || cmd == 'h' || cmd == 'V' || cmd == 'v'
                            || cmd == 'C' || cmd == 'c' || cmd == 'Q' || cmd == 'q' || cmd == 'S' || cmd == 's' || cmd == 'T' || cmd == 't'))
                        s.pts.push_back({ cx, cy });
                    if (cmd == 'Z' || cmd == 'z') s.pts.push_back(s.pts.front());
                    px = cx; py = cy;
                    (void)px; (void)py;
                    continue;
                }
                p++;
            }
        } else {
            continue;
        }
        m_shapes.push_back(std::move(s));
    }

    m_valid = !m_shapes.empty();
    return m_valid;
}

void SvgCanvas::ToCells(int width_cells, std::vector<MdLine> &out, uint16_t attr) const
{
    if (!m_valid || width_cells <= 0)
        return;

    // Scale: user units -> cells (cell ~ 2 units wide, 4 tall).
    const double cell_w = 2.0, cell_h = 4.0;
    double sx = (m_w > 0) ? (double)width_cells / m_w : 1.0;
    double sy = sx / (cell_h / cell_w);   // keep aspect for square-ish glyphs
    int cols = width_cells;
    int rows = (int)(m_h * sy) + 1;
    if (rows <= 0) rows = 1;

    std::vector<std::wstring> grid((size_t)rows, std::wstring((size_t)cols, L' '));
    auto cell = [&](int cx, int cy, wchar_t g) {
        if (cx < 0) cx = 0;
        if (cy < 0) cy = 0;
        if (cx >= cols) cx = cols - 1;
        if (cy >= rows) cy = rows - 1;
        grid[(size_t)cy][(size_t)cx] = g;
    };
    auto px = [&](double x) { return (int)((x - m_x) * sx); };
    auto py = [&](double y) { return (int)((y - m_y) * sy); };

    for (const Shape &s : m_shapes) {
        if (s.kind == Shape::RECT) {
            int x = px(s.x1), y = py(s.y1);
            int w = (int)(s.w * sx), h = (int)(s.h * sy);
            if (w < 1) w = 1;
            if (h < 1) h = 1;
            int x2 = x + w, y2 = y + h;
            for (int cx = x + 1; cx < x2; ++cx) { cell(cx, y, BoxSymbols[BS_H1]); cell(cx, y2, BoxSymbols[BS_H1]); }
            for (int cy = y + 1; cy < y2; ++cy) { cell(x, cy, BoxSymbols[BS_V1]); cell(x2, cy, BoxSymbols[BS_V1]); }
            cell(x, y, BoxSymbols[BS_LT_H1V1]);
            cell(x2, y, BoxSymbols[BS_RT_H1V1]);
            cell(x, y2, BoxSymbols[BS_LB_H1V1]);
            cell(x2, y2, BoxSymbols[BS_RB_H1V1]);
        } else if (s.kind == Shape::LINE) {
            int x1 = px(s.x1), y1 = py(s.y1), x2 = px(s.x2), y2 = py(s.y2);
            if (y1 == y2) { for (int cx = std::min(x1, x2); cx <= std::max(x1, x2); ++cx) cell(cx, y1, BoxSymbols[BS_H1]); }
            else if (x1 == x2) { for (int cy = std::min(y1, y2); cy <= std::max(y1, y2); ++cy) cell(x1, cy, BoxSymbols[BS_V1]); }
            else {
                int dx = abs(x2 - x1), dy = abs(y2 - y1);
                int sxn = (x1 < x2) ? 1 : -1, syn = (y1 < y2) ? 1 : -1;
                int err = dx - dy, x = x1, y = y1;
                wchar_t diag = (sxn * syn > 0) ? L'╲' : L'╱';
                for (;;) {
                    cell(x, y, diag);
                    if (x == x2 && y == y2) break;
                    int e2 = 2 * err;
                    if (e2 > -dy) { err -= dy; x += sxn; }
                    if (e2 < dx) { err += dx; y += syn; }
                }
            }
        } else if (s.kind == Shape::POLY) {
            for (size_t k = 1; k < s.pts.size(); ++k) {
                int x1 = px(s.pts[k - 1].first), y1 = py(s.pts[k - 1].second);
                int x2 = px(s.pts[k].first), y2 = py(s.pts[k].second);
                if (y1 == y2) { for (int cx = std::min(x1, x2); cx <= std::max(x1, x2); ++cx) cell(cx, y1, BoxSymbols[BS_H1]); }
                else if (x1 == x2) { for (int cy = std::min(y1, y2); cy <= std::max(y1, y2); ++cy) cell(x1, cy, BoxSymbols[BS_V1]); }
                else {
                    int dx = abs(x2 - x1), dy = abs(y2 - y1);
                    int sxn = (x1 < x2) ? 1 : -1, syn = (y1 < y2) ? 1 : -1;
                    int err = dx - dy, x = x1, y = y1;
                    wchar_t diag = (sxn * syn > 0) ? L'╲' : L'╱';
                    for (;;) {
                        cell(x, y, diag);
                        if (x == x2 && y == y2) break;
                        int e2 = 2 * err;
                        if (e2 > -dy) { err -= dy; x += sxn; }
                        if (e2 < dx) { err += dx; y += syn; }
                    }
                }
            }
        } else if (s.kind == Shape::TEXT) {
            int cx = px(s.tx), cy = py(s.ty);
            size_t len = s.text.size();
            if (s.anchor == L"middle") cx -= (int)(len / 2);
            else if (s.anchor == L"end") cx -= (int)len;
            for (size_t k = 0; k < len; ++k)
                cell(cx + (int)k, cy, s.text[k]);
        }
    }

    // Trim empty leading/trailing rows.
    auto row_empty = [&](int r) {
        for (auto c : grid[(size_t)r])
            if (c != L' ') return false;
        return true;
    };
    int top = 0, bot = rows;
    while (top < bot && row_empty(top)) ++top;
    while (bot > top && row_empty(bot - 1)) --bot;
    if (bot <= top) { top = 0; bot = rows; }

    out.clear();
    for (int r = top; r < bot; ++r) {
        std::wstring line;
        size_t first = 0, last = (size_t)cols;
        while (first < last && grid[(size_t)r][first] == L' ') ++first;
        while (last > first && grid[(size_t)r][last - 1] == L' ') --last;
        line = grid[(size_t)r].substr(first, last - first);
        MdLine ml;
        ml.nFilePos = 0;
        if (!line.empty())
            ml.runs.push_back(MdRun{line, attr});
        out.push_back(std::move(ml));
    }
}
