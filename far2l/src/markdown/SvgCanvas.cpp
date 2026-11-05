#include "SvgCanvas.hpp"
#include "headers.hpp"
#include "interf.hpp"   // BoxSymbols[] and the BS_* indices into it

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// SVG -> cell grid for mermaid's output (htmlLabels off, so labels are <text>).
//
// Geometry: a cell is 8 px wide and 16 px high and SVG user units are px, so x
// covers 1/8 cell per unit and y covers 1/16.  The frame is the extent of the
// drawn shapes rather than the viewBox: mermaid derives the viewBox from
// getBBox, which the headless DOM shim only approximates.  A diagram wider than
// the target is compressed horizontally; its rows stay 16 px.
//
// Lines: each segment adds direction bits (up, down, left, right) to the cells
// it covers, so a cell shows the box glyph for the directions meeting there (a
// border with a branch becomes a junction).  Diagonal runs use the slash glyphs
// where no box line is.  A marker on a line puts an arrow in its end cell.  Text
// is written last and overwrites glyphs; its spaces leave the glyph underneath.
//
// Parsing: a small tag scanner with a stack of open elements.  It applies
// transform="translate()" offsets, <style> rules and inline styles for
// text-anchor (inherited), skips <defs>/<marker> and hidden subtrees, follows
// <text>/<tspan> line positions (absolute y and dy), and samples curves into
// polylines.
// ---------------------------------------------------------------------------

namespace {

using Shape = SvgCanvas::Shape;
using SvgPoint = std::pair<double, double>;

const double kPxPerCellX = 8.0;
const double kPxPerCellY = 16.0;
const double kDefaultEm = 16.0;   // SVG default font size, px; mermaid's root style sets 14px
const double kMaxCoord = 1e5;     // shapes beyond this (px) are dropped as garbage
const double kMaxRows = 2000;     // grid height cap, so a stray extent cannot exhaust memory
const int kCubicSteps = 6;        // samples per cubic segment
const int kQuadSteps = 4;         // samples per quadratic segment

// Line directions through a cell.
enum : unsigned { DIR_U = 1, DIR_D = 2, DIR_L = 4, DIR_R = 8 };

bool IsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

bool IsNameChar(char c)
{
    return std::isalnum((unsigned char)c) || c == '-' || c == '_';
}

std::string Trim(const std::string &s)
{
    size_t b = 0, e = s.size();
    while (b < e && IsSpace(s[b])) ++b;
    while (e > b && IsSpace(s[e - 1])) --e;
    return s.substr(b, e - b);
}

std::string Lower(std::string s)
{
    for (char &c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

std::vector<std::string> SplitWords(const std::string &s)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && IsSpace(s[i]))
            ++i;
        const size_t b = i;
        while (i < s.size() && !IsSpace(s[i]))
            ++i;
        if (i > b)
            out.push_back(s.substr(b, i - b));
    }
    return out;
}

std::wstring utf8_to_wide(const std::string &s)
{
    std::wstring out;
    const unsigned char *p = (const unsigned char *)s.data();
    const size_t n = s.size();
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

void AppendUtf8(std::string &out, unsigned long cp)
{
    if (cp < 0x80) {
        out += (char)cp;
    } else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | ((cp >> 18) & 0x07));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

// Named (the five XML entities and nbsp) and numeric character references.
std::string DecodeEntities(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') {
            out += s[i];
            continue;
        }
        const size_t semi = s.find(';', i);
        if (semi == std::string::npos || semi - i > 10) {
            out += s[i];
            continue;
        }
        const std::string name = s.substr(i + 1, semi - i - 1);
        if (name == "amp") out += '&';
        else if (name == "lt") out += '<';
        else if (name == "gt") out += '>';
        else if (name == "quot") out += '"';
        else if (name == "apos") out += '\'';
        else if (name == "nbsp") out += ' ';
        else if (name.size() > 1 && name[0] == '#') {
            const bool hex = name[1] == 'x' || name[1] == 'X';
            const unsigned long cp = hex ? std::strtoul(name.c_str() + 2, nullptr, 16)
                                         : std::strtoul(name.c_str() + 1, nullptr, 10);
            AppendUtf8(out, cp);
        } else {
            out += s[i];   // unknown reference: kept as written
            continue;
        }
        i = semi;
    }
    return out;
}

// Whitespace runs become one space; leading and trailing space is dropped.
std::string CollapseSpaces(const std::string &s)
{
    std::string out;
    bool pending = false;
    for (char c : s) {
        if (IsSpace(c)) {
            pending = !out.empty();
        } else {
            if (pending)
                out += ' ';
            pending = false;
            out += c;
        }
    }
    return out;
}

void SkipSeparators(const std::string &s, size_t &p)
{
    while (p < s.size() && (IsSpace(s[p]) || s[p] == ','))
        ++p;
}

// Reads one number at `p` (separators skipped first).  On failure `p` is unchanged.
bool ScanNumber(const std::string &s, size_t &p, double &out)
{
    size_t i = p;
    SkipSeparators(s, i);
    const size_t begin = i;
    if (i < s.size() && (s[i] == '+' || s[i] == '-'))
        ++i;
    bool digits = false;
    while (i < s.size() && IsDigit(s[i])) { ++i; digits = true; }
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && IsDigit(s[i])) { ++i; digits = true; }
    }
    if (!digits)
        return false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        size_t j = i + 1;
        if (j < s.size() && (s[j] == '+' || s[j] == '-'))
            ++j;
        if (j < s.size() && IsDigit(s[j])) {
            while (j < s.size() && IsDigit(s[j]))
                ++j;
            i = j;
        }
    }
    out = std::strtod(s.substr(begin, i - begin).c_str(), nullptr);
    p = i;
    return true;
}

// SVG length: a number with an optional unit.  em and ex scale with the font size.
double ParseLength(const std::string &s, double em, double def)
{
    const std::string t = Trim(s);
    size_t p = 0;
    double v = 0;
    if (!ScanNumber(t, p, v))
        return def;
    const std::string unit = Lower(t.substr(p));
    if (unit == "em") return v * em;
    if (unit == "ex") return v * em / 2;
    if (unit == "%") return def;
    return v;
}

// Value of `prop` in a declaration list ("a: b; c: d"), or empty.
std::string DeclValue(const std::string &decls, const char *prop)
{
    size_t p = 0;
    while (p <= decls.size()) {
        size_t e = decls.find(';', p);
        if (e == std::string::npos)
            e = decls.size();
        const std::string d = Trim(decls.substr(p, e - p));
        const size_t colon = d.find(':');
        if (colon != std::string::npos && Lower(Trim(d.substr(0, colon))) == prop) {
            std::string v = Trim(d.substr(colon + 1));
            const size_t imp = v.find("!important");
            if (imp != std::string::npos)
                v = Trim(v.substr(0, imp));
            return v;
        }
        p = e + 1;
    }
    return std::string();
}

bool IsAnchor(const std::string &v)
{
    return v == "start" || v == "middle" || v == "end";
}

struct SvgAttr {
    std::string name, value;
};
using SvgAttrs = std::vector<SvgAttr>;

const std::string *FindAttr(const SvgAttrs &attrs, const char *name)
{
    for (const SvgAttr &a : attrs)
        if (a.name == name)
            return &a.value;
    return nullptr;
}

double AttrLength(const SvgAttrs &attrs, const char *name, double em, double def = 0)
{
    const std::string *v = FindAttr(attrs, name);
    return v ? ParseLength(*v, em, def) : def;
}

// The value of a CSS property from the inline style, else the presentation attribute.
std::string PropOrAttr(const std::string &style, const SvgAttrs &attrs, const char *name)
{
    std::string v = DeclValue(style, name);
    if (v.empty()) {
        const std::string *a = FindAttr(attrs, name);
        if (a)
            v = Trim(*a);
    }
    return v;
}

bool HasMarker(const std::string &style, const SvgAttrs &attrs, const char *prop)
{
    return PropOrAttr(style, attrs, prop).compare(0, 4, "url(") == 0;
}

bool IsUndrawn(const std::string &name)
{
    return name == "defs" || name == "marker" || name == "clipPath" || name == "mask"
        || name == "pattern" || name == "symbol" || name == "filter" || name == "title"
        || name == "desc" || name == "metadata" || name == "foreignObject"
        || name == "linearGradient" || name == "radialGradient" || name == "script"
        || name == "style";
}

bool IsHidden(const std::string &style, const SvgAttrs &attrs)
{
    if (PropOrAttr(style, attrs, "display") == "none")
        return true;
    const std::string vis = PropOrAttr(style, attrs, "visibility");
    if (vis == "hidden" || vis == "collapse")
        return true;
    return PropOrAttr(style, attrs, "opacity") == "0";
}

// Adds every translate(x[, y]) of a transform list to dx, dy.
void ParseTranslate(const std::string &t, double &dx, double &dy)
{
    size_t pos = 0;
    while ((pos = t.find("translate(", pos)) != std::string::npos) {
        pos += 10;
        size_t p = pos;
        double a = 0, b = 0;
        if (ScanNumber(t, p, a)) {
            if (!ScanNumber(t, p, b))
                b = 0;
            dx += a;
            dy += b;
        }
    }
}

std::vector<SvgPoint> ParsePoints(const std::string &s)
{
    std::vector<SvgPoint> pts;
    size_t p = 0;
    double x = 0, y = 0;
    while (ScanNumber(s, p, x) && ScanNumber(s, p, y))
        pts.push_back({x, y});
    return pts;
}

int ArgCount(char cmd)
{
    switch (cmd) {
        case 'M': case 'L': case 'T': return 2;
        case 'H': case 'V': return 1;
        case 'C': return 6;
        case 'S': case 'Q': return 4;
        case 'A': return 7;
        default: return 0;
    }
}

void SampleCubic(std::vector<SvgPoint> &out, double x0, double y0, double x1, double y1,
                 double x2, double y2, double x3, double y3)
{
    for (int k = 1; k <= kCubicSteps; ++k) {
        const double t = (double)k / kCubicSteps, u = 1 - t;
        const double a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, e = t * t * t;
        out.push_back({a * x0 + b * x1 + c * x2 + e * x3, a * y0 + b * y1 + c * y2 + e * y3});
    }
}

void SampleQuad(std::vector<SvgPoint> &out, double x0, double y0, double x1, double y1,
                double x2, double y2)
{
    for (int k = 1; k <= kQuadSteps; ++k) {
        const double t = (double)k / kQuadSteps, u = 1 - t;
        const double a = u * u, b = 2 * u * t, c = t * t;
        out.push_back({a * x0 + b * x1 + c * x2, a * y0 + b * y1 + c * y2});
    }
}

// Flattens path data into polylines, one per subpath.  Curves are sampled; an
// arc keeps only its end point.
void ParsePath(const std::string &d, std::vector<std::vector<SvgPoint>> &subpaths)
{
    size_t p = 0;
    char cmd = 0;
    char prev = 0;                       // previous command, upper case (for S and T)
    double cx = 0, cy = 0;               // current point
    double sx = 0, sy = 0;               // subpath start
    double ctrlx = 0, ctrly = 0;         // last control point (for S and T)
    std::vector<SvgPoint> cur;

    auto flush = [&]() {
        if (cur.size() >= 2)
            subpaths.push_back(cur);
        cur.clear();
    };
    auto start = [&]() {
        if (cur.empty())
            cur.push_back({cx, cy});
    };

    while (true) {
        SkipSeparators(d, p);
        if (p >= d.size())
            break;
        if (std::isalpha((unsigned char)d[p]))
            cmd = d[p++];
        else if (cmd == 0)
            break;
        const char up = (char)std::toupper((unsigned char)cmd);
        const bool rel = std::islower((unsigned char)cmd) != 0;

        if (up == 'Z') {
            if (!cur.empty())
                cur.push_back({sx, sy});
            flush();
            cx = sx;
            cy = sy;
            prev = 'Z';
            cmd = 0;
            continue;
        }
        const int need = ArgCount(up);
        if (need == 0)
            break;   // a command letter this subset does not know

        double a[7] = {0, 0, 0, 0, 0, 0, 0};
        bool ok = true;
        for (int k = 0; k < need && ok; ++k)
            ok = ScanNumber(d, p, a[k]);
        if (!ok)
            break;

        const double bx = rel ? cx : 0, by = rel ? cy : 0;
        switch (up) {
            case 'M':
                flush();
                cx = a[0] + bx;
                cy = a[1] + by;
                sx = cx;
                sy = cy;
                cur.push_back({cx, cy});
                cmd = rel ? 'l' : 'L';   // later pairs after a moveto are linetos
                break;
            case 'L':
                start();
                cx = a[0] + bx;
                cy = a[1] + by;
                cur.push_back({cx, cy});
                break;
            case 'H':
                start();
                cx = a[0] + (rel ? cx : 0);
                cur.push_back({cx, cy});
                break;
            case 'V':
                start();
                cy = a[0] + (rel ? cy : 0);
                cur.push_back({cx, cy});
                break;
            case 'C':
            case 'S': {
                double x1 = cx, y1 = cy, x2 = 0, y2 = 0, x = 0, y = 0;
                if (up == 'C') {
                    x1 = a[0] + bx; y1 = a[1] + by;
                    x2 = a[2] + bx; y2 = a[3] + by;
                    x = a[4] + bx;  y = a[5] + by;
                } else {
                    if (prev == 'C' || prev == 'S') {
                        x1 = 2 * cx - ctrlx;
                        y1 = 2 * cy - ctrly;
                    }
                    x2 = a[0] + bx; y2 = a[1] + by;
                    x = a[2] + bx;  y = a[3] + by;
                }
                start();
                SampleCubic(cur, cx, cy, x1, y1, x2, y2, x, y);
                ctrlx = x2;
                ctrly = y2;
                cx = x;
                cy = y;
                break;
            }
            case 'Q':
            case 'T': {
                double x1 = cx, y1 = cy, x = 0, y = 0;
                if (up == 'Q') {
                    x1 = a[0] + bx; y1 = a[1] + by;
                    x = a[2] + bx;  y = a[3] + by;
                } else {
                    if (prev == 'Q' || prev == 'T') {
                        x1 = 2 * cx - ctrlx;
                        y1 = 2 * cy - ctrly;
                    }
                    x = a[0] + bx; y = a[1] + by;
                }
                start();
                SampleQuad(cur, cx, cy, x1, y1, x, y);
                ctrlx = x1;
                ctrly = y1;
                cx = x;
                cy = y;
                break;
            }
            case 'A': {
                const double x = a[5] + bx, y = a[6] + by;
                start();
                cur.push_back({x, y});
                cx = x;
                cy = y;
                break;
            }
        }
        prev = up;
    }
    flush();
}

// ---------------------------------------------------------------------------
// CSS selectors, for text-anchor rules.  Only tag, #id and .class compounds are
// matched; a compound with an attribute or pseudo-class never matches.  The
// child combinator is treated as a descendant one.
// ---------------------------------------------------------------------------

struct SvgFrame {
    std::string name, id, style, anchor_attr;
    std::vector<std::string> classes;
    double ox = 0, oy = 0;      // offset of this element's coordinate system
    bool skip = false;          // inside a subtree that is not drawn
    bool is_text = false;       // owns the open text state
};

struct SvgCompound {
    std::string tag, id;
    std::vector<std::string> classes;
    bool ok = true;
};
using SvgSelector = std::vector<SvgCompound>;

struct SvgCssRule {
    std::vector<SvgSelector> selectors;
    std::string anchor;
};

SvgCompound ParseCompound(const std::string &s)
{
    SvgCompound c;
    if (s.empty()) {
        c.ok = false;
        return c;
    }
    size_t i = 0;
    if (s[0] == '*') {
        c.tag = "*";
        i = 1;
    } else if (std::isalpha((unsigned char)s[0])) {
        while (i < s.size() && IsNameChar(s[i]))
            ++i;
        c.tag = s.substr(0, i);
    }
    while (i < s.size()) {
        const char k = s[i];
        if (k != '.' && k != '#') {
            c.ok = false;
            break;
        }
        const size_t b = ++i;
        while (i < s.size() && IsNameChar(s[i]))
            ++i;
        if (i == b) {
            c.ok = false;
            break;
        }
        const std::string name = s.substr(b, i - b);
        if (k == '.')
            c.classes.push_back(name);
        else
            c.id = name;
    }
    return c;
}

SvgSelector ParseSelector(const std::string &s)
{
    SvgSelector sel;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (IsSpace(s[i]) || s[i] == '>' || s[i] == '+' || s[i] == '~'))
            ++i;
        const size_t b = i;
        while (i < s.size() && !IsSpace(s[i]) && s[i] != '>' && s[i] != '+' && s[i] != '~')
            ++i;
        if (i > b)
            sel.push_back(ParseCompound(s.substr(b, i - b)));
    }
    return sel;
}

bool CompoundMatches(const SvgCompound &c, const SvgFrame &f)
{
    if (!c.ok)
        return false;
    if (!c.tag.empty() && c.tag != "*" && c.tag != f.name)
        return false;
    if (!c.id.empty() && c.id != f.id)
        return false;
    for (const std::string &cls : c.classes)
        if (std::find(f.classes.begin(), f.classes.end(), cls) == f.classes.end())
            return false;
    return true;
}

struct SvgTextState {
    std::wstring anchor = L"start";
    double em = kDefaultEm;
    double cur_x = 0, cur_y = 0;    // current text position
    double buf_x = 0, buf_y = 0;    // where the pending line starts
    std::string buf;                // pending line, UTF-8, not yet collapsed
};

// ---------------------------------------------------------------------------
// Tag scanner and shape builder.
// ---------------------------------------------------------------------------

class SvgParser {
public:
    SvgParser(const std::string &svg, std::vector<Shape> &out) : m_s(svg), m_out(out) {}

    void Run();

private:
    size_t ReadTag(size_t at);
    void OpenElement(const std::string &name, const SvgAttrs &attrs, bool selfclose);
    void CloseElement(const std::string &name);
    void PopFrame();
    void AddChars(const std::string &raw);
    void EmitShape(const std::string &name, const SvgAttrs &attrs);
    void EmitPolylines(const std::vector<std::vector<SvgPoint>> &subs, bool arrow_start, bool arrow_end);
    void StartText(const SvgAttrs &attrs);
    void TspanPosition(const SvgAttrs &attrs);
    void FlushLine();
    void FinishText();
    void ParseCss(const std::string &css);
    std::string AnchorOf(size_t top) const;
    std::string CssAnchor(size_t idx) const;
    bool MatchesAt(const SvgSelector &sel, size_t ci, size_t si) const;

    const std::string &m_s;
    std::vector<Shape> &m_out;
    std::vector<SvgFrame> m_stack;
    std::vector<SvgCssRule> m_rules;
    double m_em = kDefaultEm;
    bool m_em_set = false;
    std::unique_ptr<SvgTextState> m_text;
};

void SvgParser::Run()
{
    const size_t n = m_s.size();
    size_t i = 0;
    while (i < n) {
        if (m_s[i] != '<') {
            size_t lt = m_s.find('<', i);
            if (lt == std::string::npos)
                lt = n;
            AddChars(m_s.substr(i, lt - i));
            i = lt;
        } else if (m_s.compare(i, 4, "<!--") == 0) {
            const size_t e = m_s.find("-->", i + 4);
            i = (e == std::string::npos) ? n : e + 3;
        } else if (m_s.compare(i, 2, "<?") == 0 || m_s.compare(i, 2, "<!") == 0) {
            const size_t e = m_s.find('>', i);
            i = (e == std::string::npos) ? n : e + 1;
        } else {
            i = ReadTag(i);
        }
    }
    while (!m_stack.empty())
        PopFrame();
}

size_t SvgParser::ReadTag(size_t at)
{
    const size_t n = m_s.size();
    size_t p = at + 1;
    const bool closing = p < n && m_s[p] == '/';
    if (closing)
        ++p;
    const size_t name_begin = p;
    while (p < n && !IsSpace(m_s[p]) && m_s[p] != '/' && m_s[p] != '>')
        ++p;
    const std::string name = m_s.substr(name_begin, p - name_begin);

    SvgAttrs attrs;
    bool selfclose = false;
    while (p < n && m_s[p] != '>') {
        if (IsSpace(m_s[p])) {
            ++p;
            continue;
        }
        if (m_s[p] == '/') {
            selfclose = true;
            ++p;
            continue;
        }
        selfclose = false;
        SvgAttr a;
        const size_t an = p;
        while (p < n && !IsSpace(m_s[p]) && m_s[p] != '=' && m_s[p] != '>' && m_s[p] != '/')
            ++p;
        a.name = m_s.substr(an, p - an);
        while (p < n && IsSpace(m_s[p]))
            ++p;
        if (p < n && m_s[p] == '=') {
            ++p;
            while (p < n && IsSpace(m_s[p]))
                ++p;
            if (p < n && (m_s[p] == '"' || m_s[p] == '\'')) {
                const char q = m_s[p++];
                size_t ve = m_s.find(q, p);
                if (ve == std::string::npos)
                    ve = n;
                a.value = DecodeEntities(m_s.substr(p, ve - p));
                p = (ve < n) ? ve + 1 : n;
            } else {
                const size_t vb = p;
                while (p < n && !IsSpace(m_s[p]) && m_s[p] != '>')
                    ++p;
                a.value = m_s.substr(vb, p - vb);
            }
        }
        if (!a.name.empty())
            attrs.push_back(a);
    }
    const size_t next = (p < n) ? p + 1 : n;

    if (closing) {
        CloseElement(name);
        return next;
    }
    if (!selfclose && (name == "style" || name == "script")) {
        // Raw text up to the end tag: CSS for style, ignored for script.
        const size_t e = m_s.find("</" + name, next);
        if (name == "style")
            ParseCss(m_s.substr(next, (e == std::string::npos ? n : e) - next));
        if (e == std::string::npos)
            return n;
        const size_t gt = m_s.find('>', e);
        return gt == std::string::npos ? n : gt + 1;
    }
    OpenElement(name, attrs, selfclose);
    return next;
}

void SvgParser::OpenElement(const std::string &name, const SvgAttrs &attrs, bool selfclose)
{
    SvgFrame f;
    f.name = name;
    if (const std::string *v = FindAttr(attrs, "id"))
        f.id = *v;
    if (const std::string *v = FindAttr(attrs, "class"))
        f.classes = SplitWords(*v);
    if (const std::string *v = FindAttr(attrs, "style"))
        f.style = *v;
    if (const std::string *v = FindAttr(attrs, "text-anchor"))
        f.anchor_attr = Trim(*v);
    if (!m_stack.empty()) {
        f.ox = m_stack.back().ox;
        f.oy = m_stack.back().oy;
        f.skip = m_stack.back().skip;
    }
    if (const std::string *v = FindAttr(attrs, "transform"))
        ParseTranslate(*v, f.ox, f.oy);
    if (IsUndrawn(name) || IsHidden(f.style, attrs))
        f.skip = true;
    f.is_text = !f.skip && name == "text" && !m_text;
    m_stack.push_back(f);

    if (m_stack.back().is_text)
        StartText(attrs);
    else if (!m_stack.back().skip) {
        if (name == "tspan") {
            if (m_text)
                TspanPosition(attrs);
        } else {
            EmitShape(name, attrs);
        }
    }
    if (selfclose)
        PopFrame();
}

void SvgParser::CloseElement(const std::string &name)
{
    for (size_t k = m_stack.size(); k-- > 0; ) {
        if (m_stack[k].name == name) {
            while (m_stack.size() > k)
                PopFrame();
            return;
        }
    }
}

void SvgParser::PopFrame()
{
    if (m_stack.empty())
        return;
    const bool was_text = m_stack.back().is_text;
    m_stack.pop_back();
    if (was_text)
        FinishText();
}

void SvgParser::AddChars(const std::string &raw)
{
    if (!m_text || m_stack.empty() || m_stack.back().skip)
        return;
    m_text->buf += DecodeEntities(raw);
}

void SvgParser::EmitShape(const std::string &name, const SvgAttrs &attrs)
{
    const SvgFrame &f = m_stack.back();
    const double ox = f.ox, oy = f.oy, em = m_em;
    const bool stroked = PropOrAttr(f.style, attrs, "stroke") != "none";

    if (name == "rect" || name == "circle" || name == "ellipse") {
        if (!stroked)
            return;
        double x = 0, y = 0, w = 0, h = 0;
        if (name == "rect") {
            x = AttrLength(attrs, "x", em);
            y = AttrLength(attrs, "y", em);
            w = AttrLength(attrs, "width", em);
            h = AttrLength(attrs, "height", em);
        } else if (name == "circle") {
            const double r = AttrLength(attrs, "r", em);
            x = AttrLength(attrs, "cx", em) - r;
            y = AttrLength(attrs, "cy", em) - r;
            w = h = 2 * r;
        } else {
            const double rx = AttrLength(attrs, "rx", em), ry = AttrLength(attrs, "ry", em);
            x = AttrLength(attrs, "cx", em) - rx;
            y = AttrLength(attrs, "cy", em) - ry;
            w = 2 * rx;
            h = 2 * ry;
        }
        if (w <= 0 || h <= 0)
            return;
        Shape s;
        s.kind = Shape::RECT;
        s.x1 = x + ox;
        s.y1 = y + oy;
        s.w = w;
        s.h = h;
        m_out.push_back(s);
    } else if (name == "line") {
        if (!stroked)
            return;
        std::vector<std::vector<SvgPoint>> subs(1);
        subs[0].push_back({AttrLength(attrs, "x1", em) + ox, AttrLength(attrs, "y1", em) + oy});
        subs[0].push_back({AttrLength(attrs, "x2", em) + ox, AttrLength(attrs, "y2", em) + oy});
        EmitPolylines(subs, HasMarker(f.style, attrs, "marker-start"),
                      HasMarker(f.style, attrs, "marker-end"));
    } else if (name == "polyline" || name == "polygon") {
        if (!stroked)
            return;
        std::vector<SvgPoint> pts;
        if (const std::string *v = FindAttr(attrs, "points"))
            pts = ParsePoints(*v);
        for (SvgPoint &p : pts) {
            p.first += ox;
            p.second += oy;
        }
        if (name == "polygon" && pts.size() >= 2)
            pts.push_back(pts.front());
        std::vector<std::vector<SvgPoint>> subs(1, pts);
        EmitPolylines(subs, HasMarker(f.style, attrs, "marker-start"),
                      HasMarker(f.style, attrs, "marker-end"));
    } else if (name == "path") {
        if (!stroked)
            return;
        std::vector<std::vector<SvgPoint>> subs;
        if (const std::string *d = FindAttr(attrs, "d"))
            ParsePath(*d, subs);
        for (auto &sub : subs)
            for (SvgPoint &p : sub) {
                p.first += ox;
                p.second += oy;
            }
        EmitPolylines(subs, HasMarker(f.style, attrs, "marker-start"),
                      HasMarker(f.style, attrs, "marker-end"));
    }
}

void SvgParser::EmitPolylines(const std::vector<std::vector<SvgPoint>> &subs, bool arrow_start, bool arrow_end)
{
    for (size_t k = 0; k < subs.size(); ++k) {
        if (subs[k].size() < 2)
            continue;
        Shape s;
        s.kind = Shape::POLY;
        s.pts = subs[k];
        s.arrow_start = arrow_start && k == 0;
        s.arrow_end = arrow_end && k + 1 == subs.size();
        m_out.push_back(s);
    }
}

// The text-anchor in effect for the element at `top`: the nearest element, itself
// included, that sets it (inline style, then a CSS rule, then the attribute).
std::string SvgParser::AnchorOf(size_t top) const
{
    for (size_t i = top + 1; i-- > 0; ) {
        const SvgFrame &f = m_stack[i];
        std::string v = DeclValue(f.style, "text-anchor");
        if (!IsAnchor(v))
            v = CssAnchor(i);
        if (!IsAnchor(v))
            v = f.anchor_attr;
        if (IsAnchor(v))
            return v;
    }
    return "start";
}

std::string SvgParser::CssAnchor(size_t idx) const
{
    std::string val;
    for (const SvgCssRule &r : m_rules)
        for (const SvgSelector &sel : r.selectors)
            if (MatchesAt(sel, sel.size() - 1, idx)) {
                val = r.anchor;   // the last matching rule wins
                break;
            }
    return val;
}

bool SvgParser::MatchesAt(const SvgSelector &sel, size_t ci, size_t si) const
{
    if (!CompoundMatches(sel[ci], m_stack[si]))
        return false;
    if (ci == 0)
        return true;
    for (size_t k = si; k-- > 0; )
        if (MatchesAt(sel, ci - 1, k))
            return true;
    return false;
}

void SvgParser::ParseCss(const std::string &css)
{
    size_t p = 0;
    while (p < css.size()) {
        const size_t lb = css.find('{', p);
        if (lb == std::string::npos)
            break;
        const std::string head = Trim(css.substr(p, lb - p));
        size_t q = lb + 1;
        int depth = 1;
        while (q < css.size() && depth > 0) {
            if (css[q] == '{')
                ++depth;
            else if (css[q] == '}')
                --depth;
            ++q;
        }
        const size_t body_end = (depth == 0) ? q - 1 : q;
        const std::string body = css.substr(lb + 1, body_end - lb - 1);
        p = q;
        if (head.empty() || head[0] == '@')
            continue;   // at-rules such as @keyframes and @media

        if (!m_em_set) {
            const std::string fs = DeclValue(body, "font-size");
            size_t fp = 0;
            double v = 0;
            if (!fs.empty() && ScanNumber(fs, fp, v) && fs.compare(fp, 2, "px") == 0) {
                m_em = v;
                m_em_set = true;
            }
        }

        const std::string anchor = DeclValue(body, "text-anchor");
        if (!IsAnchor(anchor))
            continue;
        SvgCssRule rule;
        rule.anchor = anchor;
        size_t sp = 0;
        while (sp <= head.size()) {
            size_t comma = head.find(',', sp);
            if (comma == std::string::npos)
                comma = head.size();
            SvgSelector sel = ParseSelector(head.substr(sp, comma - sp));
            if (!sel.empty())
                rule.selectors.push_back(sel);
            sp = comma + 1;
        }
        m_rules.push_back(rule);
    }
}

void SvgParser::StartText(const SvgAttrs &attrs)
{
    m_text.reset(new SvgTextState());
    m_text->em = m_em;
    m_text->anchor = utf8_to_wide(AnchorOf(m_stack.size() - 1));
    const SvgFrame &f = m_stack.back();
    m_text->cur_x = m_text->buf_x = AttrLength(attrs, "x", m_em) + f.ox;
    m_text->cur_y = m_text->buf_y = AttrLength(attrs, "y", m_em) + f.oy;
}

// A tspan that sets x, y, dx or dy starts a new line at that position.  The
// position follows SVG: an absolute x or y replaces the current one, dx and dy
// add to it.
void SvgParser::TspanPosition(const SvgAttrs &attrs)
{
    if (!FindAttr(attrs, "x") && !FindAttr(attrs, "y") && !FindAttr(attrs, "dx") && !FindAttr(attrs, "dy"))
        return;
    FlushLine();
    SvgTextState &t = *m_text;
    const SvgFrame &f = m_stack.back();
    if (FindAttr(attrs, "x"))
        t.cur_x = AttrLength(attrs, "x", t.em) + f.ox;
    if (FindAttr(attrs, "dx"))
        t.cur_x += AttrLength(attrs, "dx", t.em);
    if (FindAttr(attrs, "y"))
        t.cur_y = AttrLength(attrs, "y", t.em) + f.oy;
    if (FindAttr(attrs, "dy"))
        t.cur_y += AttrLength(attrs, "dy", t.em);
    t.buf_x = t.cur_x;
    t.buf_y = t.cur_y;
}

void SvgParser::FlushLine()
{
    if (!m_text)
        return;
    const std::string line = CollapseSpaces(m_text->buf);
    m_text->buf.clear();
    if (line.empty())
        return;
    Shape s;
    s.kind = Shape::TEXT;
    s.text = utf8_to_wide(line);
    s.anchor = m_text->anchor;
    s.tx = m_text->buf_x;
    s.ty = m_text->buf_y - 0.35 * m_text->em;   // baseline to the middle of the x-height
    m_out.push_back(s);
}

void SvgParser::FinishText()
{
    FlushLine();
    m_text.reset();
}

}   // namespace

// ---------------------------------------------------------------------------
// Rasterizer.
// ---------------------------------------------------------------------------

namespace {

// Glyph of the box set for a direction mask.  Indexes are BoxSymbols indices.
int BoxIndexForDirs(unsigned d)
{
    switch (d) {
        case DIR_U: case DIR_D: case DIR_U | DIR_D:
            return BS_V1;
        case DIR_L: case DIR_R: case DIR_L | DIR_R:
            return BS_H1;
        case DIR_D | DIR_R:                 return BS_LT_H1V1;   // top-left corner
        case DIR_D | DIR_L:                 return BS_RT_H1V1;   // top-right corner
        case DIR_U | DIR_R:                 return BS_LB_H1V1;   // bottom-left corner
        case DIR_U | DIR_L:                 return BS_RB_H1V1;   // bottom-right corner
        case DIR_U | DIR_D | DIR_R:         return BS_L_H1V1;    // left junction
        case DIR_U | DIR_D | DIR_L:         return BS_R_H1V1;    // right junction
        case DIR_L | DIR_R | DIR_D:         return BS_T_H1V1;    // top junction
        case DIR_L | DIR_R | DIR_U:         return BS_B_H1V1;    // bottom junction
        case DIR_U | DIR_D | DIR_L | DIR_R: return BS_C_H1V1;    // cross
        default:                            return -1;
    }
}

struct SvgCell {
    unsigned dirs = 0;          // DIR_* bits of the box lines through the cell
    wchar_t diag = 0;           // slash of a diagonal run
    wchar_t arrow = 0;          // marker arrow
    wchar_t text = 0;           // label character
};

wchar_t GlyphOf(const SvgCell &c)
{
    if (c.text)
        return c.text;
    if (c.arrow)
        return c.arrow;
    if (c.dirs) {
        const int b = BoxIndexForDirs(c.dirs);
        if (b >= 0)
            return BoxSymbols[b];
    }
    return c.diag;
}

class SvgRaster {
public:
    SvgRaster(double x0, double y0, double sx, double sy, int cols, int rows)
        : m_x0(x0), m_y0(y0), m_sx(sx), m_sy(sy), m_cols(cols), m_rows(rows),
          m_cells((size_t)cols * (size_t)rows) {}

    void Rect(double x, double y, double w, double h)
    {
        int c1 = Col(x), c2 = Col(x + w), r1 = Row(y), r2 = Row(y + h);
        if (c2 <= c1)
            c2 = c1 + 1;
        if (r2 <= r1)
            r2 = r1 + 1;
        Segment(c1, r1, c2, r1);
        Segment(c1, r2, c2, r2);
        Segment(c1, r1, c1, r2);
        Segment(c2, r1, c2, r2);
    }

    void Polyline(const std::vector<SvgPoint> &pts)
    {
        for (size_t k = 1; k < pts.size(); ++k)
            Segment(Col(pts[k - 1].first), Row(pts[k - 1].second),
                    Col(pts[k].first), Row(pts[k].second));
    }

    // The arrow sits in the cell of the line's end and points along the last
    // segment (outwards from the start when at_end is false).  When that cell is
    // already a box border (a line crossing it at right angles), the arrow goes one
    // cell back, so the border stays whole.
    void Arrow(const std::vector<SvgPoint> &pts, bool at_end)
    {
        if (pts.size() < 2)
            return;
        const size_t tip = at_end ? pts.size() - 1 : 0;
        const size_t from = at_end ? pts.size() - 2 : 1;
        const double dx = pts[tip].first - pts[from].first;
        const double dy = pts[tip].second - pts[from].second;
        if (dx == 0 && dy == 0)
            return;
        int dc = 0, dr = 0;   // the arrow's step, in cells
        wchar_t g;
        if (std::fabs(dx) >= std::fabs(dy)) {
            dc = dx > 0 ? 1 : -1;
            g = dc > 0 ? L'►' : L'◄';   // right / left pointer
        } else {
            dr = dy > 0 ? 1 : -1;
            g = dr > 0 ? L'▼' : L'▲';   // down / up pointer
        }
        const int tc = Col(pts[tip].first), tr = Row(pts[tip].second);
        SvgCell *c = At(tc, tr);
        if (!c)
            return;
        SvgCell *target = c;
        const unsigned across = dc != 0 ? (DIR_U | DIR_D) : (DIR_L | DIR_R);
        if ((c->dirs & across) != 0) {
            if (SvgCell *back = At(tc - dc, tr - dr))
                target = back;
        }
        target->arrow = g;
    }

    // Label text: one cell per character, anchored as the SVG says.  Spaces leave
    // the glyph underneath.
    void Text(const Shape &s)
    {
        const int len = (int)s.text.size();
        int c0 = Col(s.tx);
        if (s.anchor == L"middle")
            c0 -= len / 2;
        else if (s.anchor == L"end")
            c0 -= len;
        const int r = Row(s.ty);
        for (int k = 0; k < len; ++k) {
            if (s.text[k] == L' ')
                continue;
            if (SvgCell *c = At(c0 + k, r))
                c->text = s.text[k];
        }
    }

    std::vector<std::wstring> Glyphs() const
    {
        std::vector<std::wstring> rows((size_t)m_rows, std::wstring((size_t)m_cols, L' '));
        for (int r = 0; r < m_rows; ++r)
            for (int c = 0; c < m_cols; ++c) {
                const wchar_t g = GlyphOf(m_cells[(size_t)r * m_cols + c]);
                if (g)
                    rows[(size_t)r][(size_t)c] = g;
            }
        return rows;
    }

private:
    // Clamped so that a stray far-off point cannot overflow the int conversion.
    static int Clamp(double v)
    {
        return (int)std::floor(std::max(-1e6, std::min(1e6, v)));
    }
    int Col(double x) const { return Clamp((x - m_x0) * m_sx); }
    int Row(double y) const { return Clamp((y - m_y0) * m_sy); }

    SvgCell *At(int c, int r)
    {
        if (c < 0 || r < 0 || c >= m_cols || r >= m_rows)
            return nullptr;
        return &m_cells[(size_t)r * m_cols + c];
    }

    // Each cell on the segment gets the directions it connects to along the
    // segment: an end cell only the one towards the other end.
    void Segment(int c1, int r1, int c2, int r2)
    {
        if (c1 == c2 && r1 == r2)
            return;
        if (r1 == r2) {
            const int a = std::min(c1, c2), b = std::max(c1, c2);
            for (int c = a; c <= b; ++c)
                if (SvgCell *cell = At(c, r1))
                    cell->dirs |= (c > a ? DIR_L : 0u) | (c < b ? DIR_R : 0u);
        } else if (c1 == c2) {
            const int a = std::min(r1, r2), b = std::max(r1, r2);
            for (int r = a; r <= b; ++r)
                if (SvgCell *cell = At(c1, r))
                    cell->dirs |= (r > a ? DIR_U : 0u) | (r < b ? DIR_D : 0u);
        } else {
            // Diagonal: Bresenham, glyph by the direction of travel.
            const int dx = std::abs(c2 - c1), dy = -std::abs(r2 - r1);
            const int sxs = c1 < c2 ? 1 : -1, sys = r1 < r2 ? 1 : -1;
            const wchar_t g = sxs * sys > 0 ? L'╲' : L'╱';
            int err = dx + dy, c = c1, r = r1;
            for (;;) {
                SvgCell *cell = At(c, r);
                if (cell && !cell->diag)
                    cell->diag = g;
                if (c == c2 && r == r2)
                    break;
                const int e2 = 2 * err;
                if (e2 >= dy) { err += dy; c += sxs; }
                if (e2 <= dx) { err += dx; r += sys; }
            }
        }
    }

    double m_x0, m_y0, m_sx, m_sy;
    int m_cols, m_rows;
    std::vector<SvgCell> m_cells;
};

struct SvgExtent {
    double x0, y0, x1, y1;
};

// Bounds of the drawn shapes in px.  A label counts one cell per character.
SvgExtent ShapesExtent(const std::vector<Shape> &shapes)
{
    SvgExtent e = {1e300, 1e300, -1e300, -1e300};
    auto grow = [&e](double x, double y) {
        e.x0 = std::min(e.x0, x);
        e.y0 = std::min(e.y0, y);
        e.x1 = std::max(e.x1, x);
        e.y1 = std::max(e.y1, y);
    };
    for (const Shape &s : shapes) {
        if (s.kind == Shape::RECT) {
            grow(s.x1, s.y1);
            grow(s.x1 + s.w, s.y1 + s.h);
        } else if (s.kind == Shape::POLY) {
            for (const SvgPoint &p : s.pts)
                grow(p.first, p.second);
        } else {
            const double len = (double)s.text.size() * kPxPerCellX;
            const double left = s.tx - (s.anchor == L"middle" ? len / 2 : s.anchor == L"end" ? len : 0.0);
            grow(left, s.ty - kPxPerCellY / 2);
            grow(left + len, s.ty + kPxPerCellY / 2);
        }
    }
    if (e.x1 - e.x0 < 1)
        e.x1 = e.x0 + 1;
    if (e.y1 - e.y0 < 1)
        e.y1 = e.y0 + 1;
    return e;
}

}   // namespace

bool SvgCanvas::Parse(const std::string &svg)
{
    m_shapes.clear();
    m_valid = false;
    SvgParser parser(svg, m_shapes);
    parser.Run();
    // Drop shapes with non-finite or absurd coordinates (malformed numbers).
    auto out_of_range = [](double v) { return !std::isfinite(v) || std::fabs(v) > kMaxCoord; };
    m_shapes.erase(std::remove_if(m_shapes.begin(), m_shapes.end(), [&](const Shape &s) {
        if (s.kind == Shape::RECT)
            return out_of_range(s.x1) || out_of_range(s.y1) || out_of_range(s.w) || out_of_range(s.h);
        if (s.kind == Shape::TEXT)
            return out_of_range(s.tx) || out_of_range(s.ty);
        for (const SvgPoint &p : s.pts)
            if (out_of_range(p.first) || out_of_range(p.second))
                return true;
        return false;
    }), m_shapes.end());
    m_valid = !m_shapes.empty();
    return m_valid;
}

void SvgCanvas::ToCells(int width_cells, std::vector<MdLine> &out, uint16_t attr) const
{
    if (!m_valid || width_cells <= 0)
        return;

    const SvgExtent ext = ShapesExtent(m_shapes);
    // Cells per px.  Rows keep their 16 px height; a frame wider than the target is
    // compressed horizontally instead (uniform scaling would squash box heights into
    // one or two rows, and labels would then sit on the borders).
    double sx = 1.0 / kPxPerCellX;
    const double sy = 1.0 / kPxPerCellY;
    // The frame's right edge must land in the last column, not one past it.
    const double limit = width_cells - 0.001;
    const double frame_cells = (ext.x1 - ext.x0) * sx;
    if (frame_cells > limit)
        sx *= limit / frame_cells;

    // The grid is only as wide as the frame, and +2 leaves room for minimum-size boxes.
    const int cols = std::min(width_cells, (int)std::floor(std::min(frame_cells, limit)) + 2);
    const int rows = (int)std::min(kMaxRows, std::floor((ext.y1 - ext.y0) * sy) + 2);
    SvgRaster ras(ext.x0, ext.y0, sx, sy, cols, rows);
    // Lines first, then arrows (which look at the lines around their tip), then text.
    for (const Shape &s : m_shapes) {
        if (s.kind == Shape::RECT)
            ras.Rect(s.x1, s.y1, s.w, s.h);
        else if (s.kind == Shape::POLY)
            ras.Polyline(s.pts);
    }
    for (const Shape &s : m_shapes) {
        if (s.kind != Shape::POLY)
            continue;
        if (s.arrow_end)
            ras.Arrow(s.pts, true);
        if (s.arrow_start)
            ras.Arrow(s.pts, false);
    }
    for (const Shape &s : m_shapes)
        if (s.kind == Shape::TEXT)
            ras.Text(s);

    // Cut the columns that are empty in every row (the same cut for all rows, so
    // the rows stay aligned) and the blank rows at both ends.
    const std::vector<std::wstring> glyphs = ras.Glyphs();
    int left = cols, right = -1, top = -1, bottom = -1;
    for (int r = 0; r < rows; ++r) {
        const std::wstring &row = glyphs[(size_t)r];
        for (int c = 0; c < cols; ++c)
            if (row[(size_t)c] != L' ') {
                left = std::min(left, c);
                right = std::max(right, c);
                if (top < 0)
                    top = r;
                bottom = r;
            }
    }
    out.clear();
    if (top < 0)
        return;
    for (int r = top; r <= bottom; ++r) {
        std::wstring line = glyphs[(size_t)r].substr((size_t)left, (size_t)(right - left + 1));
        const size_t last = line.find_last_not_of(L' ');
        line = (last == std::wstring::npos) ? std::wstring() : line.substr(0, last + 1);
        MdLine ml;
        ml.nFilePos = 0;
        if (!line.empty())
            ml.runs.push_back(MdRun{line, attr});
        out.push_back(std::move(ml));
    }
}
