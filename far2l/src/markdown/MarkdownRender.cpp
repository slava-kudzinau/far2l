#include "MarkdownRender.hpp"

#include "md4c.h"
#include "headers.hpp"   // WinPort: MultiByteToWideChar/WideCharToMultiByte (CP_UTF8)
#include "interf.hpp"   // BoxSymbols[]
#include "MermaidEngine.hpp"
#include "SvgCanvas.hpp"

#include <algorithm>
#include <cstring>
#include <functional>

// ---------------------------------------------------------------------------
// md4c event -> styled display lines.
//
// md4c reports UTF-8 text in `text` callbacks and per-block/span events.  We
// keep no byte offsets in block details (md4c 0.5.2 exposes none), so a
// running byte offset (the contingency from the plan) tracks the source line
// start: every `text` call advances it by `size`, and each block start records
// it as that block's display-line nFilePos.
// ---------------------------------------------------------------------------

namespace {

// Styling: every run takes the viewer text color (`base_attr`, which the viewer
// reads from COL_VIEWERTEXT when it renders).  Structure is shown by layout only:
// heading rule lines, the box-line quote prefix, indented code, bullets and table borders.
constexpr int      CODE_INDENT   = 4;     // cells of indent before each code line
constexpr wchar_t  BULLET        = L'\u2022';

std::wstring concat(const std::vector<MdRun> &runs);
std::wstring utf8_to_wide(const char *data, size_t size);
std::wstring attr_to_wide(const MD_ATTRIBUTE &a);

std::wstring utf8_to_wide(const char *data, size_t size)
{
    if (!data || size == 0)
        return std::wstring();
    int need = WINPORT(MultiByteToWideChar)(CP_UTF8, 0, data, (int)size, nullptr, 0);
    std::wstring out;
    if (need > 0) {
        out.resize((size_t)need);
        WINPORT(MultiByteToWideChar)(CP_UTF8, 0, data, (int)size, &out[0], need);
    }
    return out;
}

// Plain text of an MD_ATTRIBUTE (e.g. a link href / code lang): a contiguous
// byte region in the input, addressed via attr->text + attr->size.
std::wstring attr_to_wide(const MD_ATTRIBUTE &a)
{
    if (!a.text || a.size == 0)
        return std::wstring();
    return utf8_to_wide(a.text, a.size);
}

std::string wide_to_utf8(const std::wstring &w)
{
    if (w.empty())
        return std::string();
    int need = WINPORT(WideCharToMultiByte)(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                             nullptr, 0, nullptr, nullptr);
    std::string out;
    if (need > 0) {
        out.resize((size_t)need);
        WINPORT(WideCharToMultiByte)(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                     &out[0], need, nullptr, nullptr);
    }
    return out;
}

// Mermaid cache key: the fence source (hashed) and the width it was laid out at.  Fences with
// the same source get the same art, so they may share an entry; the file offset is not part
// of the key.
std::pair<size_t, int> mermaid_key(const std::wstring &src, int width)
{
    return std::make_pair(std::hash<std::wstring>()(src), width);
}

// Engine errors can carry multi-line JS stacks; each log line holds one failure.
std::string one_line(const std::string &s)
{
    std::string out;
    for (char c : s) {
        if (c == '\n')
            out += " | ";
        else
            out += c;
    }
    return out;
}

struct Builder {
    // running source offset (bytes consumed so far)
    size_t pos = 0;

    std::vector<MdLine> out;

    std::vector<MdRun> cur;       // current display line runs
    int64_t cur_pos = 0;          // file pos of the current display line
    bool cur_started = false;

    uint64_t base_attr;           // viewer text color; every run takes it
    int width;

    // block context
    int quote_depth = 0;
    int heading_level = 0;
    std::vector<int> list_stack;  // 0 = bullet, 1 = ordered
    std::vector<long> ol_counter;
    bool li_open = false;         // a list item is collecting inline content

    // list-item prefix that applies to the *first* line of a list item
    std::wstring li_prefix;
    bool li_prefix_pending = false;

    // code fence
    bool in_code = false;
    std::wstring code_lang;
    std::wstring code_text;       // verbatim text incl '\n'
    int64_t code_pos = 0;

    // table
    bool in_table = false;
    bool in_cell = false;
    bool header_done = false;
    std::vector<std::vector<MdRun>> table_row;
    std::vector<std::vector<std::vector<MdRun>>> table_rows;
    std::vector<MdRun> cell;

    // span context
    std::wstring link_href;       // set on span enter, appended on span leave
    std::wstring link_label;      // label text captured while the link is open

    // mermaid fences: the placeholder line range (in `out`) plus the fence
    // body, so Render() can splice engine-produced box art in place.
    struct Fence {
        int64_t file_pos;
        std::wstring src;
        size_t first_line = 0;   // index in out of the first placeholder line
        size_t count = 0;        // placeholder lines emitted for this fence
    };
    std::vector<Fence> fences;

    // --- line helpers ---------------------------------------------------

    void begin_line()
    {
        if (cur_started)
            return;
        cur.clear();
        cur_pos = (int64_t)pos;
        cur_started = true;
        // leading indent: list depth (2 cells) + blockquote prefix.
        std::wstring lead;
        lead.append(2 * (size_t)list_stack.size(), L' ');
        for (int q = 0; q < quote_depth; ++q) {
            lead += BoxSymbols[BS_V1];
            lead += L' ';
        }
        if (li_prefix_pending) {
            lead += li_prefix;
            li_prefix_pending = false;
        }
        if (!lead.empty())
            cur.push_back(MdRun{lead, base_attr});
    }

    void append(const std::wstring &text)
    {
        if (text.empty())
            return;
        begin_line();
        if (!cur.empty() && cur.back().attr == base_attr)
            cur.back().text += text;
        else
            cur.push_back(MdRun{text, base_attr});
    }

    void flush()
    {
        if (!cur_started)
            return;
        MdLine line;
        line.runs = std::move(cur);
        line.nFilePos = cur_pos;
        out.push_back(std::move(line));
        cur.clear();
        cur_started = false;
    }

    void line_break() { flush(); }   // paragraph soft/hard break = new display line
};

int cb_enter_block(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    Builder &b = *(Builder *)userdata;
    b.pos; // (offsets tracked in text callback)
    switch (type) {
        case MD_BLOCK_QUOTE: b.quote_depth++; break;
        case MD_BLOCK_UL: {
            auto *d = (MD_BLOCK_UL_DETAIL *)detail;
            b.list_stack.push_back(0);
            (void)d;
            break;
        }
        case MD_BLOCK_OL: {
            auto *d = (MD_BLOCK_OL_DETAIL *)detail;
            b.list_stack.push_back(1);
            b.ol_counter.push_back(d ? (long)d->start : 1);
            break;
        }
        case MD_BLOCK_LI: {
            auto *d = (MD_BLOCK_LI_DETAIL *)detail;
            b.li_open = true;
            std::wstring pfx;
            if (!b.list_stack.empty() && b.list_stack.back() == 1) {
                pfx += std::to_wstring(b.ol_counter.back());
                pfx += L". ";
                b.ol_counter.back()++;
            } else {
                pfx += BULLET;
                pfx += L' ';
            }
            if (d && d->is_task) {
                pfx += L'[';
                pfx += (d->task_mark == ' ' ? L' ' : L'x');
                pfx += L"] ";
            }
            b.li_prefix = pfx;
            b.li_prefix_pending = true;
            break;
        }
        case MD_BLOCK_H: {
            auto *d = (MD_BLOCK_H_DETAIL *)detail;
            b.heading_level = d ? (int)d->level : 1;
            b.begin_line();
            break;
        }
        case MD_BLOCK_P:
            b.begin_line();
            break;
        case MD_BLOCK_CODE: {
            auto *d = (MD_BLOCK_CODE_DETAIL *)detail;
            b.in_code = true;
            b.code_text.clear();
            b.code_pos = (int64_t)b.pos;
            if (d)
                b.code_lang = attr_to_wide(d->lang);
            break;
        }
        case MD_BLOCK_HTML:
            b.in_code = true;   // verbatim, same treatment as code
            b.code_text.clear();
            b.code_pos = (int64_t)b.pos;
            b.code_lang.clear();
            break;
        case MD_BLOCK_HR: {
            b.begin_line();
            std::wstring hr;
            int n = std::max(1, b.width - (int)(2 * b.list_stack.size()));
            hr.append((size_t)n, BoxSymbols[BS_H1]);
            b.append(hr);
            b.flush();
            break;
        }
        case MD_BLOCK_TABLE:
            b.in_table = true;
            b.table_rows.clear();
            b.header_done = false;
            break;
        case MD_BLOCK_THEAD: break;
        case MD_BLOCK_TBODY: b.header_done = true; break;
        case MD_BLOCK_TR: b.table_row.clear(); break;
        case MD_BLOCK_TH:
        case MD_BLOCK_TD:
            b.in_cell = true;
            b.cell.clear();
            break;
        default: break;
    }
    return 0;
}

int cb_leave_block(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    Builder &b = *(Builder *)userdata;
    (void)detail;
    switch (type) {
        case MD_BLOCK_QUOTE: if (b.quote_depth) b.quote_depth--; break;
        case MD_BLOCK_UL:
            if (!b.list_stack.empty()) b.list_stack.pop_back();
            if (!b.ol_counter.empty() && b.list_stack.empty()) b.ol_counter.clear();
            break;
        case MD_BLOCK_OL:
            if (!b.list_stack.empty()) b.list_stack.pop_back();
            if (!b.ol_counter.empty() && b.list_stack.empty()) b.ol_counter.clear();
            break;
        case MD_BLOCK_LI:
            b.flush();
            b.li_open = false;
            break;
        case MD_BLOCK_H:
            b.flush();
            // Heading rule under every level (double for H1, single for H2-H6).
            if (b.heading_level >= 1 && b.heading_level <= 6) {
                b.begin_line();
                int lead = (int)concat(b.cur).size();
                int n = std::max(1, b.width - lead);
                b.append(std::wstring((size_t)n, b.heading_level == 1 ? BoxSymbols[BS_H2] : BoxSymbols[BS_H1]));
                b.flush();
            }
            b.heading_level = 0;
            break;
        case MD_BLOCK_P:
            b.flush();
            break;
        case MD_BLOCK_CODE:
        case MD_BLOCK_HTML: {
            b.in_code = false;
            bool is_mermaid = (b.code_lang == L"mermaid");
            std::wstring fence_src = b.code_text;
            b.code_text.clear();
            b.code_lang.clear();

            std::wstring text = fence_src;
            if (!text.empty() && text.back() == L'\n')
                text.pop_back();
            // The fence body is the placeholder; Render() replaces it with the
            // engine's box art when available (Step 6 splice below).
            size_t first_line = b.out.size();
            size_t count = 0, start = 0;
            while (start <= text.size()) {
                size_t nl = text.find(L'\n', start);
                std::wstring line = (nl == std::wstring::npos)
                    ? text.substr(start) : text.substr(start, nl - start);
                b.begin_line();
                b.append(std::wstring(CODE_INDENT, L' ') + line);
                b.flush();
                ++count;
                if (nl == std::wstring::npos) break;
                start = nl + 1;
            }
            if (is_mermaid)
                b.fences.push_back(Builder::Fence{b.code_pos, fence_src, first_line, count});
            break;
        }
        case MD_BLOCK_TH:
        case MD_BLOCK_TD:
            b.table_row.push_back(std::move(b.cell));
            b.cell.clear();
            b.in_cell = false;
            break;
        case MD_BLOCK_TR:
            b.table_rows.push_back(std::move(b.table_row));
            b.table_row.clear();
            break;
        case MD_BLOCK_TABLE:
            b.in_table = false;
            // grid render via BoxSymbols; column widths = max cell width.
            if (!b.table_rows.empty()) {
                size_t cols = 0;
                for (auto &r : b.table_rows)
                    cols = std::max(cols, r.size());
                if (cols) {
                    std::vector<int> w(cols, 0);
                    for (auto &r : b.table_rows)
                        for (size_t c = 0; c < r.size(); ++c)
                            w[c] = std::max(w[c], (int)concat(r[c]).size());
                    auto render_row = [&](std::vector<std::vector<MdRun>> &row) {
                        b.begin_line();
                        b.append(std::wstring(1, BoxSymbols[BS_V1]));
                        for (size_t c = 0; c < cols; ++c) {
                            std::wstring cell_txt = (c < row.size()) ? concat(row[c]) : L"";
                            int pad = w[c] - (int)cell_txt.size();
                            b.append(L" " + cell_txt + std::wstring(pad > 0 ? pad : 0, L' ') + L" ");
                            b.append(std::wstring(1, BoxSymbols[BS_V1]));
                        }
                        b.flush();
                    };
                    auto render_sep = [&](wchar_t l, wchar_t mid, wchar_t r) {
                        b.begin_line();
                        b.append(std::wstring(1, l));
                        for (size_t c = 0; c < cols; ++c) {
                            b.append(std::wstring((size_t)w[c] + 2, BoxSymbols[BS_H1]));
                            b.append(std::wstring(1, (c + 1 < cols) ? mid : r));
                        }
                        b.flush();
                    };
                    render_sep(BoxSymbols[BS_LT_H1V1], BoxSymbols[BS_T_H1V1], BoxSymbols[BS_RT_H1V1]);
                    for (size_t ri = 0; ri < b.table_rows.size(); ++ri) {
                        render_row(b.table_rows[ri]);
                        if (ri == 0)
                            render_sep(BoxSymbols[BS_L_H1V1], BoxSymbols[BS_C_H1V1], BoxSymbols[BS_R_H1V1]);
                    }
                    render_sep(BoxSymbols[BS_LB_H1V1], BoxSymbols[BS_B_H1V1], BoxSymbols[BS_RB_H1V1]);
                }
            }
            break;
        default: break;
    }
    return 0;
}

std::wstring concat(const std::vector<MdRun> &runs)
{
    std::wstring s;
    for (auto &r : runs)
        s += r.text;
    return s;
}

int cb_enter_span(MD_SPANTYPE type, void *detail, void *userdata)
{
    Builder &b = *(Builder *)userdata;
    switch (type) {
        case MD_SPAN_A: {
            auto *d = (MD_SPAN_A_DETAIL *)detail;
            b.link_href.clear();
            b.link_label.clear();
            if (d)
                b.link_href = attr_to_wide(d->href);
            break;
        }
        case MD_SPAN_IMG: {
            auto *d = (MD_SPAN_IMG_DETAIL *)detail;
            b.link_href.clear();
            b.link_label.clear();
            if (d)
                b.link_href = attr_to_wide(d->src);
            break;
        }
        default: break;
    }
    return 0;
}

int cb_leave_span(MD_SPANTYPE type, void *detail, void *userdata)
{
    Builder &b = *(Builder *)userdata;
    (void)detail;
    switch (type) {
        case MD_SPAN_A:
        case MD_SPAN_IMG:
            // Label only; the URL stays reachable by toggling markdown off.
            b.link_href.clear();
            b.link_label.clear();
            break;
        default: break;
    }
    return 0;
}

int cb_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata)
{
    Builder &b = *(Builder *)userdata;
    std::wstring w = utf8_to_wide((const char *)text, size);
    b.pos += size;
    if (b.in_code) {
        b.code_text += w;
        return 0;
    }
    switch (type) {
        case MD_TEXT_SOFTBR:
        case MD_TEXT_BR:
            b.line_break();
            break;
        case MD_TEXT_NULLCHAR:
            b.append(L" ");
            break;
        default:
            if (b.in_cell)
                b.cell.push_back(MdRun{w, b.base_attr});
            else {
                b.append(w);
                b.link_label += w;   // accumulate link/label text
            }
            break;
    }
    return 0;
}

}   // namespace

// ---------------------------------------------------------------------------

void MarkdownDoc::Render(const std::wstring &file_text, int width, uint64_t base_attr)
{
    m_text = file_text;
    m_width = width;
    m_base_attr = base_attr;
    m_lines.clear();
    m_block_starts.clear();
    m_block_first_line.clear();
    m_mermaid_cache.clear();
    m_cursor = 0;
    m_first_pos = 0;
    m_valid = false;

    std::string utf8 = wide_to_utf8(file_text);
    if (utf8.empty()) {
        m_valid = true;
        return;
    }

    Builder b;
    b.base_attr = base_attr;
    b.width = width;

    MD_PARSER parser = {};
    parser.flags = MD_FLAG_TABLES | MD_FLAG_TASKLISTS | MD_FLAG_STRIKETHROUGH
                 | MD_FLAG_PERMISSIVEAUTOLINKS | MD_FLAG_UNDERLINE;
    parser.enter_block = cb_enter_block;
    parser.leave_block = cb_leave_block;
    parser.enter_span = cb_enter_span;
    parser.leave_span = cb_leave_span;
    parser.text = cb_text;
    parser.debug_log = nullptr;
    parser.syntax = nullptr;

    md_parse((const MD_CHAR *)utf8.data(), (MD_SIZE)utf8.size(), &parser, &b);
    b.flush();

    m_lines = std::move(b.out);
    m_valid = true;

    // Hand rendered mermaid fences to the engine (Step 4/5); placeholder lines
    // from the fence body are replaced by box art when the engine is available.
    // A failed fence stays raw; each failure logs one stderr line (the reason is in it).
    for (auto &f : b.fences) {
        auto key = mermaid_key(f.src, width);
        if (m_mermaid_cache.count(key))
            continue;
        std::string src = wide_to_utf8(f.src);
        std::string svg, err;
        std::vector<MdLine> cell;
        if (!MermaidEngine::Available() ||
            !MermaidEngine::Render(src, "mmd", width * 8, svg, err)) {
            fprintf(stderr, "MermaidEngine: render failed: %s\n",
                    one_line(err.empty() ? std::string("engine not available") : err).c_str());
        } else {
            SvgCanvas canvas;
            if (!canvas.Parse(svg)) {
                fprintf(stderr, "MarkdownRender: SvgCanvas parse failed\n");
            } else {
                canvas.ToCells(width, cell, base_attr);
                if (cell.empty())
                    fprintf(stderr, "MarkdownRender: mermaid art is empty\n");
            }
        }
        m_mermaid_cache[key] = std::move(cell);
    }

    // Splice rendered fences in place (reverse order: earlier indices stay valid).
    for (auto it = b.fences.rbegin(); it != b.fences.rend(); ++it) {
        auto cell = m_mermaid_cache.find(mermaid_key(it->src, width));
        if (cell == m_mermaid_cache.end() || cell->second.empty())
            continue;
        std::vector<MdLine> &lines = cell->second;
        // Every art row carries the fence position: the viewer reads nFilePos as the row's
        // position, and a 0 in the middle of the sequence breaks the lookup in NextLines().
        for (MdLine &line : lines)
            line.nFilePos = it->file_pos;
        m_lines.erase(m_lines.begin() + it->first_line,
                      m_lines.begin() + it->first_line + it->count);
        m_lines.insert(m_lines.begin() + it->first_line, lines.begin(), lines.end());
    }
}

bool MarkdownDoc::NextLines(std::vector<MdLine> &out, int64_t at_file_pos)
{
    out.clear();
    if (!m_valid || m_cursor >= (int64_t)m_lines.size())
        return false;

    // (Re)prime the cursor to the block containing at_file_pos on a fresh pass.
    if (at_file_pos != m_cursor_pos) {
        m_cursor = 0;
        for (size_t i = 0; i < m_lines.size(); ++i) {
            if (m_lines[i].nFilePos >= at_file_pos) { m_cursor = (int64_t)i; break; }
            m_cursor = (int64_t)i + 1;
        }
        m_cursor_pos = at_file_pos;
    }
    if (m_cursor >= (int64_t)m_lines.size())
        return false;

    out.push_back(m_lines[m_cursor]);
    ++m_cursor;
    m_cursor_pos = (m_cursor < (int64_t)m_lines.size())
        ? m_lines[m_cursor].nFilePos : -1;
    return true;
}

void MarkdownDoc::Invalidate(int width)
{
    m_mermaid_cache.clear();
    Render(m_text, width, m_base_attr);
}
