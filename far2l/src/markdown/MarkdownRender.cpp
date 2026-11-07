#include "MarkdownRender.hpp"

#include "md4c.h"
#include "headers.hpp"   // WinPort: MultiByteToWideChar/WideCharToMultiByte (CP_UTF8)
#include "interf.hpp"   // BoxSymbols[]
#include "AsciiMermaid.hpp"

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <functional>

// ---------------------------------------------------------------------------
// md4c event -> styled display lines.
//
// md4c reports UTF-8 text in `text` callbacks and per-block/span events.  md4c 0.5.2 gives
// no byte offsets in block details, but its text callbacks point into the input, so each
// display line takes the offset of its first source text (see cb_text).
// ---------------------------------------------------------------------------

namespace {

// Styling: each run takes a palette role (MdRole); the viewer maps roles to colors when it draws.
// Body text has the body role.  Headings, links, strong text and table header text take the heading
// role; code spans and fences the code role; mermaid art and table borders the art role.  Structure
// is also shown by layout: the box-line quote prefix, the table rules, indented code, bullets.
constexpr int      CODE_INDENT   = 4;     // cells of indent before each code line
constexpr wchar_t  BULLET        = L'\u2022';
// The row after a mermaid fence that AsciiMermaid cannot draw; the fence's code rows stay above it.
const wchar_t      MERMAID_HINT[] = L"(diagram not drawn here - Alt-F3 opens it in the browser)";

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

// A fence is a mermaid diagram when its info word is "mermaid", in any case.
bool is_mermaid_lang(const std::wstring &lang)
{
    std::wstring w = lang;
    for (wchar_t &c : w)
        c = (wchar_t)towlower(c);
    return w == L"mermaid";
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
    // Source offset: bytes of the input consumed so far.  md4c hands out most text as a pointer
    // into the input, so a text callback sets this to that text's offset (see cb_text); other
    // pieces (the static "\n" of a break) advance it by their size.
    size_t pos = 0;
    const char *base = nullptr;   // the UTF-8 input md4c parses
    size_t base_size = 0;

    // File offset of `p` when it points into the input, else -1.
    int64_t offset_of(const MD_CHAR *p) const
    {
        const uintptr_t at = (uintptr_t)p, start = (uintptr_t)base;
        if (!base || at < start || at >= start + base_size)
            return -1;
        return (int64_t)(at - start);
    }

    // File offset where the source line holding offset `at` starts.  Text arrives in input order,
    // so the newlines are counted once, as the scan moves forward.
    int64_t line_start_of(size_t at)
    {
        if (at < ls_scan) {   // out of order: look back directly
            size_t i = at;
            while (i > 0 && base[i - 1] != '\n')
                --i;
            return (int64_t)i;
        }
        for (; ls_scan < at; ++ls_scan)
            if (base[ls_scan] == '\n')
                ls_start = ls_scan + 1;
        return (int64_t)ls_start;
    }

    size_t ls_scan = 0;     // line_start_of() has counted the newlines before this offset
    size_t ls_start = 0;    // start of the source line that holds ls_scan

    std::vector<MdLine> out;

    std::vector<MdRun> cur;       // current display line runs
    int64_t cur_pos = 0;          // file pos of the current display line
    bool cur_pos_set = false;     // cur_pos came from source text (else see flush())
    bool cur_started = false;

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
    // (index in code_text, file offset) of each piece of source text in the block
    std::vector<std::pair<size_t, int64_t>> code_marks;

    // table
    bool in_table = false;
    bool in_cell = false;
    bool header_done = false;
    std::vector<std::vector<MdRun>> table_row;
    std::vector<std::vector<std::vector<MdRun>>> table_rows;
    std::vector<int64_t> table_row_pos;   // file offset of each row's first cell text
    int64_t row_pos = -1;                 // the row being collected (-1: no source text yet)
    std::vector<MdRun> cell;

    // span context
    std::wstring link_href;       // set on span enter, appended on span leave
    std::wstring link_label;      // label text captured while the link is open
    int span_code = 0;            // open code spans
    int span_strong = 0;          // open strong spans
    int span_link = 0;            // open links

    // mermaid fences: the placeholder line range (in `out`) plus the fence body and the lead of
    // its rows, so Render() can splice the box art in place, or add the hint row under the code.
    struct Fence {
        int64_t file_pos;
        std::wstring src;
        size_t first_line = 0;   // index in out of the first placeholder line
        size_t count = 0;        // placeholder lines emitted for this fence
        std::wstring lead;       // row_lead() of the fence (list indent, quote bars)
    };
    std::vector<Fence> fences;

    // --- line helpers ---------------------------------------------------

    // The lead of every row: the list indent (2 cells per list level), then the bar of each
    // enclosing blockquote.  Bars take the quote role; outside a quote the lead stays body.
    std::wstring row_lead() const
    {
        std::wstring lead(2 * list_stack.size(), L' ');
        for (int q = 0; q < quote_depth; ++q) {
            lead += BoxSymbols[BS_V1];
            lead += L' ';
        }
        return lead;
    }

    uint64_t lead_role() const
    {
        return static_cast<uint64_t>(quote_depth > 0 ? MD_ROLE_QUOTE : MD_ROLE_BODY);
    }

    void begin_line()
    {
        if (cur_started)
            return;
        cur.clear();
        cur_pos_set = false;      // set by the line's first source text, see append()
        cur_started = true;
        std::wstring lead = row_lead();
        if (li_prefix_pending) {
            lead += li_prefix;
            li_prefix_pending = false;
        }
        if (!lead.empty())
            cur.push_back(MdRun{lead, lead_role()});
    }

    void set_line_pos(int64_t p)
    {
        cur_pos = p;
        cur_pos_set = true;
    }

    // The palette role of source text read now: code, then link, strong and heading, the spec's
    // precedence for inline runs.
    uint64_t text_role() const
    {
        if (span_code > 0)
            return MD_ROLE_CODE;
        if (span_link > 0)
            return MD_ROLE_LINK;
        if (span_strong > 0)
            return MD_ROLE_STRONG;
        if (heading_level == 1)
            return MD_ROLE_H1;
        if (heading_level == 2)
            return MD_ROLE_H2;
        if (heading_level > 0)
            return MD_ROLE_H3;
        return quote_depth > 0 ? MD_ROLE_QUOTE : MD_ROLE_BODY;
    }

    // `role` is the palette role of `text`; `off` is the file offset of `text` when it comes from the
    // source, else -1.
    void append(const std::wstring &text, uint64_t role, int64_t off = -1)
    {
        if (text.empty())
            return;
        begin_line();
        if (!cur_pos_set && off >= 0)
            set_line_pos(off);
        if (!cur.empty() && cur.back().role == role)
            cur.back().text += text;
        else
            cur.push_back(MdRun{text, role});
    }

    void flush()
    {
        if (!cur_started)
            return;
        MdLine line;
        line.runs = std::move(cur);
        // A line with no source text of its own (a table rule, a blank code line, a queued blank
        // row) takes the position of the line before it, so it groups with that line and never ties
        // with a source line.
        line.nFilePos = cur_pos_set ? cur_pos : (out.empty() ? 0 : out.back().nFilePos);
        out.push_back(std::move(line));
        cur.clear();
        cur_started = false;
    }

    void line_break() { flush(); }   // paragraph soft/hard break = new display line

    // --- spacing ------------------------------------------------------------
    // A block that ends queues one blank row, and the block that comes next emits it before its
    // first row.  So repeated blanks collapse into one, and no blank row comes before the first
    // row.  Source blank lines never produce rows here, so only code fences keep theirs.  A queued
    // blank is soft by default, and a list drops it.  Heading level 1 and code or HTML blocks make
    // it hard, so a list keeps it.  A heading, a code block or an HTML block gets a hard blank before
    // it (force_gap).  A block that starts a list item gets none, and a code or HTML block gets none
    // after a heading of level 2 to 6.  Items of a list stay tight (an item start drops the queued
    // blank), a nested list is part of its outer list, and the outermost list has no blank row around it.
    enum Gap { GAP_NONE, GAP_SOFT, GAP_HARD };
    Gap gap = GAP_NONE;
    size_t spacer_row = (size_t)-1;         // index in out of the last blank row queued here
    size_t tight_heading_row = (size_t)-1;  // index in out of the last heading row of level 2 to 6

    bool last_row_is_spacer() const { return !out.empty() && spacer_row == out.size() - 1; }
    bool after_tight_heading() const { return !out.empty() && tight_heading_row == out.size() - 1; }

    void open_block()
    {
        if (cur_started) {            // text of a tight list item: a block of its own before this one
            flush();
            if (gap < GAP_SOFT)
                gap = GAP_SOFT;
        }
        if (gap == GAP_NONE)
            return;
        gap = GAP_NONE;
        if (out.empty() || last_row_is_spacer())
            return;
        // Offset of the row before, so the offsets never decrease (see flush()).
        MdLine blank;
        blank.nFilePos = out.back().nFilePos;
        const std::wstring lead = row_lead();   // a blank inside a list or a quote keeps its lead
        if (!lead.empty())
            blank.runs.push_back(MdRun{lead, lead_role()});
        out.push_back(std::move(blank));
        spacer_row = out.size() - 1;
    }

    // A hard gap survives a list; a soft gap does not.
    void close_block(Gap kind = GAP_SOFT) { if (gap < kind) gap = kind; }
    void force_gap() { gap = GAP_HARD; }
    void drop_soft_gap() { if (gap == GAP_SOFT) gap = GAP_NONE; }

    // Fence tokens (story markdown-code-highlight): the source, the render
    // budget and the document cache, set by Render before the parse.
    MdTokenSource *token_source = nullptr;
    std::chrono::steady_clock::time_point token_deadline{};
    std::map<std::pair<std::string, uint64_t>, std::vector<MdTokenGroup>> *token_cache = nullptr;
    std::vector<MdTokenGroup> token_groups;   // indexed by MdLine::tokens
};

// One 64-bit FNV-1a: the hash half of a fence token cache key, the form of
// mermaid_key (plan T3).
uint64_t fnv1a64(const std::string &s)
{
    uint64_t h = 1469598103934665603ULL;
    for (const char c : s) {
        h ^= (uint64_t)(unsigned char)c;
        h *= 1099511628211ULL;
    }
    return h;
}

// Tokenize one fence (plan T3): the caps of spec.md (400 lines, 65536 source
// bytes; R32), the shared 40 ms render budget (R29), the document cache
// (R28, R30, R31).  Appends one group per tokenized line to b.token_groups,
// in cells of the code-run text (the 4-cell indent counted), and links
// `count` rows starting at `first_line` to them; rows the source does not
// reach keep the plain code role.
void tokenize_fence(Builder &b, const std::string &lang, const std::wstring &fence_src,
    size_t first_line, size_t count)
{
    if (b.token_source == nullptr || b.token_cache == nullptr || count == 0)
        return;
    std::wstring src = fence_src;
    if (!src.empty() && src.back() == L'\n')
        src.pop_back();
    std::vector<std::string> lines;
    std::string joined;
    size_t start = 0;
    while (lines.size() < 400 && start <= src.size()) {
        size_t nl = src.find(L'\n', start);
        const size_t end = (nl == std::wstring::npos) ? src.size() : nl;
        std::string u8 = wide_to_utf8(src.substr(start, end - start));
        const size_t add = u8.size() + (joined.empty() ? 0 : 1);
        if (!joined.empty() && joined.size() + add > 65536)
            break;
        if (!joined.empty())
            joined += '\n';
        joined += u8;
        lines.push_back(std::move(u8));
        if (nl == std::wstring::npos)
            break;
        start = nl + 1;
    }
    if (lines.empty())
        return;
    const std::pair<std::string, uint64_t> key(lang, fnv1a64(joined));
    std::vector<MdTokenGroup> groups;
    auto hit = b.token_cache->find(key);
    if (hit != b.token_cache->end()) {
        groups = hit->second;
    } else {
        const auto now = std::chrono::steady_clock::now();
        if (b.token_deadline == std::chrono::steady_clock::time_point{}) {
            // R42: the plugin load happens before the budget; the fences share 40 ms.
            b.token_source->Ready();
            b.token_deadline = now + std::chrono::milliseconds(40);
        }
        bool cacheable = true;
        if (now >= b.token_deadline) {
            // The fences not yet started are skipped (spec, Input limits).
            groups.assign(lines.size(), MdTokenGroup());
        } else {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                b.token_deadline - now).count();
            std::vector<MdToken> flat;
            cacheable = b.token_source->Tokenize(lang, lines, left > 0 ? (int)left : 1, flat);   // R48: a plain fence caches too
            groups.resize(lines.size());
            for (const MdToken &t : flat) {
                if (t.line < groups.size())
                    groups[t.line].tokens.push_back(t);
            }
            for (MdTokenGroup &g : groups) {
                for (MdToken &t : g.tokens) {
                    t.start += CODE_INDENT;
                    t.end += CODE_INDENT;
                }
                // Colorer closes nested regions inner-first; the printer walks tokens in cell order.
                std::sort(g.tokens.begin(), g.tokens.end(),
                    [](const MdToken &a, const MdToken &b) {
                        return a.start < b.start || (a.start == b.start && a.end > b.end);
                    });
            }
        }
        if (cacheable)
            b.token_cache->emplace(key, groups);   // a deadline-trimmed group is cached as is (R29)
    }
    const size_t base = b.token_groups.size();
    b.token_groups.insert(b.token_groups.end(), groups.begin(), groups.end());
    for (size_t gi = 0; gi + base < b.token_groups.size() && gi < count; ++gi)
        b.out[first_line + gi].tokens = (int)(base + gi);
}

// File offset of the first source text in the code line [s, e) of code_text, or -1 when the line
// has none (a blank line; the indent is not source text).
int64_t code_line_pos(const Builder &b, size_t s, size_t e)
{
    auto it = std::lower_bound(b.code_marks.begin(), b.code_marks.end(), s,
        [](const std::pair<size_t, int64_t> &m, size_t v) { return m.first < v; });
    return (it != b.code_marks.end() && it->first < e) ? it->second : -1;
}

int cb_enter_block(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    Builder &b = *(Builder *)userdata;
    b.pos; // (offsets tracked in text callback)
    switch (type) {
        case MD_BLOCK_QUOTE: b.open_block(); b.quote_depth++; break;
        case MD_BLOCK_UL: {
            auto *d = (MD_BLOCK_UL_DETAIL *)detail;
            // The outermost list has no blank row before it; a nested one is part of its outer list.
            if (b.list_stack.empty()) { b.drop_soft_gap(); b.open_block(); } else b.flush();
            b.list_stack.push_back(0);
            (void)d;
            break;
        }
        case MD_BLOCK_OL: {
            auto *d = (MD_BLOCK_OL_DETAIL *)detail;
            if (b.list_stack.empty()) { b.drop_soft_gap(); b.open_block(); } else b.flush();
            b.list_stack.push_back(1);
            b.ol_counter.push_back(d ? (long)d->start : 1);
            break;
        }
        case MD_BLOCK_LI: {
            auto *d = (MD_BLOCK_LI_DETAIL *)detail;
            b.li_open = true;
            b.drop_soft_gap();   // items stay tight: no blank row between two items
            b.open_block();      // a hard blank left by a code block that ended the previous item stays
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
            if (!b.li_prefix_pending)
                b.force_gap();   // a heading keeps its blank row next to a list
            b.open_block();
            b.heading_level = d ? (int)d->level : 1;
            b.begin_line();
            break;
        }
        case MD_BLOCK_P:
            b.open_block();
            b.begin_line();
            break;
        case MD_BLOCK_CODE: {
            auto *d = (MD_BLOCK_CODE_DETAIL *)detail;
            // A heading of level 2 to 6 and a list item keep their code block on the row right under them.
            if (!b.after_tight_heading() && !b.li_prefix_pending)
                b.force_gap();
            b.open_block();
            b.in_code = true;
            b.code_text.clear();
            b.code_marks.clear();
            if (d)
                b.code_lang = attr_to_wide(d->lang);
            break;
        }
        case MD_BLOCK_HTML:
            // A heading of level 2 to 6 and a list item keep their HTML block on the row right under them.
            if (!b.after_tight_heading() && !b.li_prefix_pending)
                b.force_gap();
            b.open_block();
            b.in_code = true;   // verbatim, same treatment as code
            b.code_text.clear();
            b.code_marks.clear();
            b.code_lang.clear();
            break;
        case MD_BLOCK_HR: {
            b.open_block();
            b.begin_line();
            std::wstring hr;
            int n = std::max(1, b.width - (int)(2 * b.list_stack.size()));
            hr.append((size_t)n, BoxSymbols[BS_H1]);
            b.append(hr, MD_ROLE_RULE);
            b.flush();
            b.close_block();
            break;
        }
        case MD_BLOCK_TABLE:
            b.open_block();
            b.in_table = true;
            b.table_rows.clear();
            b.table_row_pos.clear();
            b.header_done = false;
            break;
        case MD_BLOCK_THEAD: break;
        case MD_BLOCK_TBODY: b.header_done = true; break;
        case MD_BLOCK_TR:
            b.table_row.clear();
            b.row_pos = -1;
            break;
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
        case MD_BLOCK_QUOTE:
            if (b.quote_depth) b.quote_depth--;
            b.close_block();
            break;
        case MD_BLOCK_UL:
            if (!b.list_stack.empty()) b.list_stack.pop_back();
            if (!b.ol_counter.empty() && b.list_stack.empty()) b.ol_counter.clear();
            if (b.list_stack.empty()) b.drop_soft_gap();   // the outermost list has no blank row after it
            break;
        case MD_BLOCK_OL:
            if (!b.list_stack.empty()) b.list_stack.pop_back();
            if (!b.ol_counter.empty() && b.list_stack.empty()) b.ol_counter.clear();
            if (b.list_stack.empty()) b.drop_soft_gap();
            break;
        case MD_BLOCK_LI:
            b.flush();
            b.li_open = false;
            b.li_prefix_pending = false;   // an empty item must not keep its prefix for the next block
            break;
        case MD_BLOCK_H:
            b.flush();
            // No rule row under a heading: its own palette entry is its layout cue.
            if (b.heading_level == 1)
                b.close_block(Builder::GAP_HARD);   // heading level 1 keeps a blank row after it
            else if (!b.out.empty())
                b.tight_heading_row = b.out.size() - 1;   // level 2 to 6: the next block sits right under it
            b.heading_level = 0;
            break;
        case MD_BLOCK_P:
            b.flush();
            b.close_block();
            break;
        case MD_BLOCK_CODE:
        case MD_BLOCK_HTML: {
            b.in_code = false;
            bool is_mermaid = is_mermaid_lang(b.code_lang);
            // The cache key language: the name the bridge of T2 reports (R28).
            const std::string fence_lang = MdNormalizeLangName(wide_to_utf8(b.code_lang));
            std::wstring fence_src = b.code_text;
            b.code_text.clear();
            b.code_lang.clear();

            std::wstring text = fence_src;
            if (!text.empty() && text.back() == L'\n')
                text.pop_back();
            // The fence body is the placeholder: Render() replaces it with the box art, or keeps
            // it and adds the hint row when the fence is not drawn.
            size_t first_line = b.out.size();
            size_t count = 0, start = 0;
            // The art of a fence is placed at the fence's first source text.
            const int64_t fence_pos = b.code_marks.empty() ? (int64_t)b.pos : b.code_marks.front().second;
            while (start <= text.size()) {
                size_t nl = text.find(L'\n', start);
                std::wstring line = (nl == std::wstring::npos)
                    ? text.substr(start) : text.substr(start, nl - start);
                const size_t end = (nl == std::wstring::npos) ? text.size() : nl;
                b.begin_line();
                const int64_t line_pos = code_line_pos(b, start, end);
                if (line_pos >= 0)
                    b.set_line_pos(line_pos);
                b.append(std::wstring(CODE_INDENT, L' ') + line, MD_ROLE_CODE);
                b.flush();
                ++count;
                if (nl == std::wstring::npos) break;
                start = nl + 1;
            }
            // One call per fence, with the remaining render budget (plan T3).
            // A mermaid fence is tokenized like any fence (R33).
            tokenize_fence(b, fence_lang, fence_src, first_line, count);
            if (is_mermaid)
                b.fences.push_back(Builder::Fence{fence_pos, fence_src, first_line, count, b.row_lead()});
            b.close_block(Builder::GAP_HARD);   // a code block keeps its blank row next to a list
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
            b.table_row_pos.push_back(b.row_pos >= 0 ? b.row_pos : (int64_t)b.pos);
            break;
        case MD_BLOCK_TABLE:
            b.in_table = false;
            b.close_block();
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
                    // Rows take their own source offset; the rule under the header takes the line before it
                    // (flush()).  Header text with no other span is drawn in the strong role; other cells
                    // keep their roles.
                    auto render_row = [&](std::vector<std::vector<MdRun>> &row, int64_t pos, bool header) {
                        // Header text keeps a code, link or strong span of its own; only plain header
                        // text moves to the strong role.
                        auto cell_role = [&](const MdRun &r) {
                            if (!(header && r.role == MD_ROLE_BODY))
                                return r.role;
                            return static_cast<uint64_t>(MD_ROLE_STRONG);
                        };
                        b.begin_line();
                        b.set_line_pos(pos);
                        b.append(std::wstring(1, BoxSymbols[BS_V1]), MD_ROLE_RULE);
                        for (size_t c = 0; c < cols; ++c) {
                            const int cell_w = (c < row.size()) ? (int)concat(row[c]).size() : 0;
                            const int pad = w[c] - cell_w;
                            b.append(L" ", MD_ROLE_BODY);
                            if (c < row.size())
                                for (const MdRun &r : row[c])
                                    b.append(r.text, cell_role(r));
                            b.append(std::wstring(pad > 0 ? pad : 0, L' ') + L" ", MD_ROLE_BODY);
                            b.append(std::wstring(1, BoxSymbols[BS_V1]), MD_ROLE_RULE);
                        }
                        b.flush();
                    };
                    auto render_sep = [&](wchar_t l, wchar_t mid, wchar_t r) {
                        b.begin_line();
                        b.append(std::wstring(1, l), MD_ROLE_RULE);
                        for (size_t c = 0; c < cols; ++c) {
                            b.append(std::wstring((size_t)w[c] + 2, BoxSymbols[BS_H1]), MD_ROLE_RULE);
                            b.append(std::wstring(1, (c + 1 < cols) ? mid : r), MD_ROLE_RULE);
                        }
                        b.flush();
                    };
                    for (size_t ri = 0; ri < b.table_rows.size(); ++ri) {
                        render_row(b.table_rows[ri], b.table_row_pos[ri], ri == 0);
                        if (ri == 0)
                            render_sep(BoxSymbols[BS_L_H1V1], BoxSymbols[BS_C_H1V1], BoxSymbols[BS_R_H1V1]);
                    }
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
            b.span_link++;
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
        case MD_SPAN_STRONG: b.span_strong++; break;
        case MD_SPAN_CODE: b.span_code++; break;
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
            if (b.span_link > 0)
                b.span_link--;
            b.link_href.clear();
            b.link_label.clear();
            break;
        case MD_SPAN_IMG:
            // Label only; the URL stays reachable by toggling markdown off.
            b.link_href.clear();
            b.link_label.clear();
            break;
        case MD_SPAN_STRONG: if (b.span_strong > 0) b.span_strong--; break;
        case MD_SPAN_CODE: if (b.span_code > 0) b.span_code--; break;
        default: break;
    }
    return 0;
}

int cb_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata)
{
    Builder &b = *(Builder *)userdata;
    std::wstring w = utf8_to_wide((const char *)text, size);
    // Source text points into the input, so its offset is exact; other text keeps the running pos.
    // A display line takes the start of the source line its first text sits on, so the markup
    // before the text ("# ", "- ", "| ", the indent) is part of the line.
    const int64_t at = b.offset_of(text);
    if (at >= 0)
        b.pos = (size_t)at;
    b.pos += size;
    const int64_t off = at >= 0 ? b.line_start_of((size_t)at) : -1;
    if (b.in_code) {
        if (off >= 0)
            b.code_marks.emplace_back(b.code_text.size(), off);
        b.code_text += w;
        return 0;
    }
    switch (type) {
        case MD_TEXT_SOFTBR:
        case MD_TEXT_BR:
            b.line_break();
            break;
        case MD_TEXT_NULLCHAR:
            b.append(L" ", b.text_role());
            break;
        default:
            if (b.in_cell) {
                if (b.row_pos < 0 && off >= 0)
                    b.row_pos = off;
                b.cell.push_back(MdRun{w, b.text_role()});
            } else {
                b.append(w, b.text_role(), off);
                b.link_label += w;   // accumulate link/label text
            }
            break;
    }
    return 0;
}

}   // namespace

// ---------------------------------------------------------------------------

void MarkdownDoc::Render(const std::wstring &file_text, int width)
{
    m_text = file_text;
    m_width = width;
    m_lines.clear();
    m_block_starts.clear();
    m_block_first_line.clear();
    m_mermaid_cache.clear();
    m_token_groups.clear();
    m_cursor = 0;
    m_first_pos = 0;
    m_valid = false;

    std::string utf8 = wide_to_utf8(file_text);
    if (utf8.empty()) {
        m_valid = true;
        return;
    }

    Builder b;
    b.width = width;
    b.base = utf8.data();
    b.base_size = utf8.size();
    b.token_source = &MdGetTokenSource();
    b.token_cache = &m_token_cache;

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
    m_token_groups = std::move(b.token_groups);   // shift and link survive the mermaid art swap (R31)
    m_valid = true;

    // Draw each mermaid fence with AsciiMermaid, once per source and width.  An empty entry is a
    // fence that could not be drawn; each failure logs one stderr line with the reason.
    for (auto &f : b.fences) {
        auto key = mermaid_key(f.src, width);
        if (m_mermaid_cache.count(key))
            continue;
        std::vector<std::string> rows;
        std::string err;
        std::vector<MdLine> art;
        if (AsciiMermaid::Render(wide_to_utf8(f.src), rows, err)) {
            for (const std::string &row : rows) {
                MdLine line;
                line.runs.push_back(MdRun{utf8_to_wide(row.data(), row.size()), MD_ROLE_ART});
                art.push_back(std::move(line));
            }
        } else {
            fprintf(stderr, "AsciiMermaid: render failed: %s\n", one_line(err).c_str());
        }
        m_mermaid_cache[key] = std::move(art);
    }

    // Splice in reverse order: earlier indices stay valid.
    for (auto it = b.fences.rbegin(); it != b.fences.rend(); ++it) {
        const std::vector<MdLine> &art = m_mermaid_cache[mermaid_key(it->src, width)];
        const size_t code_end = it->first_line + it->count;
        if (art.empty()) {
            // The code rows stay, and the hint row follows them.  It takes the position of the last
            // code row, so the offsets never decrease.
            MdLine hint;
            hint.runs.push_back(MdRun{it->lead + MERMAID_HINT, MD_ROLE_BODY});
            hint.nFilePos = m_lines[code_end - 1].nFilePos;
            m_lines.insert(m_lines.begin() + code_end, std::move(hint));
            continue;
        }
        // Every art row carries the fence position: the viewer finds lines by nFilePos, so the
        // offsets must stay non-decreasing down the doc (a 0 in the middle would break that).
        std::vector<MdLine> lines = art;
        for (MdLine &line : lines)
            line.nFilePos = it->file_pos;
        m_lines.erase(m_lines.begin() + it->first_line, m_lines.begin() + code_end);
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
    // The token cache outlives a resize: a key is language plus fence hash (R30).
    Render(m_text, width);
}
