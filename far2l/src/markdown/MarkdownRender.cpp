#include "MarkdownRender.hpp"

#include "md4c.h"
#include "headers.hpp"   // WinPort: MultiByteToWideChar/WideCharToMultiByte (CP_UTF8)
#include "interf.hpp"   // BoxSymbols[]
#include "CharClasses.h"   // CharClasses::IsFullWidth / IsXxxfix / ZERO_WIDTH_JOINER
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
    // Soft-wrap (F2) state of the line being collected: `cur_flow` marks prose (a paragraph,
    // heading, blockquote or list item), `cur_prefix` is the cell count of the lead that a
    // wrapped continuation line repeats.  flush() copies both to the line.
    bool cur_flow = false;
    int cur_prefix = 0;

    int width;

    // block context
    int quote_depth = 0;
    int heading_level = 0;
    size_t heading_src;
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
    int span_em = 0;              // open emphasis spans (story markdown-colorer-look)
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
        cur_flow = false;         // set by append() for prose (see flush())
        std::wstring lead = row_lead();
        cur_prefix = (int)lead.size();   // all lead cells are 1 cell wide
        if (!lead.empty())
            cur.push_back(MdRun{lead, lead_role()});
        // The list-item prefix (the bullet, `1. `, the task box) is its own run: the scheme
        // paints it with markdown:ListMark (story markdown-colorer-look).  It shares the line's
        // source line with the lead above, as the merged lead run did.
        if (li_prefix_pending) {
            li_prefix_pending = false;
            if (!li_prefix.empty())
                cur.push_back(MdRun{li_prefix, MD_ROLE_LISTMARK});
        }
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
        if (span_em > 0)
            return MD_ROLE_EM;
        if (heading_level == 1)
            return MD_ROLE_H1;
        if (heading_level == 2)
            return MD_ROLE_H2;
        if (heading_level > 0)
            return MD_ROLE_H3;
        return quote_depth > 0 ? MD_ROLE_QUOTE : MD_ROLE_BODY;
    }

    // `role` is the palette role of `text`; `off` is the file offset of `text` when it comes from the
    // source, else -1.  `flow` is false for the fixed layout of a table row, a code fence row and
    // a diagram row, which the wrap pass must not split.
    void append(const std::wstring &text, uint64_t role, int64_t off = -1, bool flow = true)
    {
        if (text.empty())
            return;
        begin_line();
        if (flow)
            cur_flow = true;
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
        // A line with no source text of its own (a table rule, a blank code line, a blank row
        // from the source) takes the position of the line before it, so it groups with that line
        // and never ties with a source line.
        line.nFilePos = cur_pos_set ? cur_pos : (out.empty() ? 0 : out.back().nFilePos);
        line.flow = cur_flow;
        line.prefix = cur_prefix;
        out.push_back(std::move(line));
        cur.clear();
        cur_started = false;
    }

    void rule_row()
    {
        begin_line();
        const int n = std::max(1, width - (int)(2 * list_stack.size()));
        append(std::wstring((size_t)n, BoxSymbols[BS_H1]), MD_ROLE_RULE);
        flush();
    }

    void line_break() { flush(); }   // paragraph soft/hard break = new display line

    // --- blank rows from the source -------------------------------------------------
    // One blank row per empty source line.  md4c gives no block offsets, so the count runs at the
    // first text of the block that comes next (cb_text) or, for a thematic break, at a scan of the
    // source (cb_enter_block).  The count starts after the rest of the last text's line and stops
    // at the anchor line, so it counts only whole empty lines strictly between the blocks; a text
    // on the same line as the last text counts nothing.  A blank row keeps the lead of the
    // enclosing list or quote; the renderer adds no blank row of its own.
    int64_t last_text_end = -1;   // start offset of the next unconsumed line, -1 before any

    // End offset of the line that starts at `at` (past its LF, else base_size).
    size_t line_end(size_t at) const
    {
        size_t e = at;
        while (e < base_size && base[e] != '\n')
            ++e;
        if (e < base_size)
            ++e;
        return e;
    }

    // A line is empty when it holds no character other than CR, LF, space and tab.
    bool line_is_blank(size_t at, size_t end) const
    {
        for (size_t i = at; i < end; ++i) {
            const char c = base[i];
            if (c != '\r' && c != '\n' && c != ' ' && c != '\t')
                return false;
        }
        return true;
    }

    // A setext underline: a run of `marker` ('=' at level 1, '-' at level 2) after up to 3
    // leading spaces, with only blanks to the end of the line.
    bool line_is_setext_rule(size_t at, size_t end, char marker) const
    {
        size_t i = at;
        for (int s = 0; s < 3 && i < end && base[i] == ' '; ++s)
            ++i;
        size_t run = 0;
        while (i < end && base[i] == marker) {
            ++i;
            ++run;
        }
        if (run == 0)
            return false;
        for (; i < end; ++i) {
            const char c = base[i];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                return false;
        }
        return true;
    }

    // Start offset of the first non-empty line at or after `from` (else base_size).
    size_t first_nonblank_line(size_t from) const
    {
        size_t scan = from;
        while (scan < base_size) {
            const size_t end = line_end(scan);
            if (!line_is_blank(scan, end))
                break;
            scan = end;
        }
        return scan;
    }

    // Emit one blank row per whole empty source line between the last source text and the start of
    // the source line that holds `up_to` (the anchor: the start of the next text's line, the line
    // of a thematic break, base_size for the trailing empty lines).  Leading empty lines count
    // from offset 0 (`last_text_end == -1`).  A row takes the nFilePos of the row before it, so the
    // offsets never decrease (see flush()).  The scan consumes each counted line: `last_text_end`
    // moves to its end, so no empty line counts twice; a fence's text is source text (the in_code
    // path of cb_text advances `last_text_end`), so the empty lines inside a fence stay fence rows.
    void blank_rows(int64_t up_to)
    {
        const size_t stop = (up_to < 0) ? base_size : (size_t)up_to;
        const size_t start = (last_text_end < 0) ? 0 : (size_t)last_text_end;
        if (stop <= start)
            return;
        const std::wstring lead = row_lead();   // a blank inside a list or a quote keeps its lead
        for (size_t i = start; i < stop; ) {
            const size_t end = line_end(i);
            if (line_is_blank(i, end)) {
                MdLine blank;
                blank.nFilePos = out.empty() ? 0 : out.back().nFilePos;
                if (!lead.empty())
                    blank.runs.push_back(MdRun{lead, lead_role()});
                out.push_back(std::move(blank));
                last_text_end = (int64_t)end;
            }
            i = end;
        }
    }

    void open_block()
    {
        if (cur_started)            // text of a tight list item: a block of its own before this one
            flush();
    }

    // Fence tokens (story markdown-code-highlight): the source, the render
    // budget and the document cache, set by Render before the parse.
    MdTokenSource *token_source = nullptr;
    std::chrono::steady_clock::time_point token_deadline{};
    std::map<std::tuple<std::string, uint64_t, unsigned long long>,
        std::vector<MdTokenGroup>> *token_cache = nullptr;
    // The scheme generation of this render, fetched at the first fence's cache
    // key and kept for every later fence's key (spec.md, Cache; story
    // markdown-colorer-fences): a scheme change re-keys the cache, a resize
    // keeps it.  ~0 = not fetched yet.
    unsigned long long token_generation = ~0ULL;
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
// bytes; R32), the shared 40 ms render budget (R29), the document cache keyed
// on the scheme generation (R28, R30, R31; story markdown-colorer-fences).
// Appends one group per tokenized line to b.token_groups,
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
    // The cache key: the language, the hash of the fence source and the scheme
    // generation (spec.md, Cache; story markdown-colorer-fences): a resize
    // re-keys nothing and reuses the ranges (R30); a scheme change re-keys, so
    // the next render refetches the fence once.
    if (b.token_generation == ~0ULL) {
        b.token_generation = MdGetLookSource().Generation(); // the key's generation
    }
    const auto key = std::make_tuple(lang, fnv1a64(joined), b.token_generation);
    std::vector<MdTokenGroup> groups;
    auto hit = b.token_cache->find(key);
    if (hit != b.token_cache->end()) {
        groups = hit->second;
    } else {
        if (b.token_deadline == std::chrono::steady_clock::time_point{}) {
            // R42: the plugin load happens before the budget; the fences share 40 ms.
            b.token_source->Ready();
            b.token_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
        }
        const auto now = std::chrono::steady_clock::now();
        bool cacheable = true;
        if (now >= b.token_deadline) {
            // The fences not yet started are skipped (spec, Input limits).
            groups.assign(lines.size(), MdTokenGroup());
        } else {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                b.token_deadline - now).count();
            std::vector<MdTokenRange> flat;
            // The scheme's colors, not classes; a V2 answer updates the key's
            // generation to the scheme the colors came from (T2).
            cacheable = b.token_source->TokenizeV2(lang, lines, left > 0 ? (int)left : 1,
                flat, b.token_generation);   // R48: a plain fence caches too
            groups.resize(lines.size());
            for (const MdTokenRange &r : flat) {
                if (r.line < groups.size())
                    groups[r.line].ranges.push_back(r);
            }
            for (MdTokenGroup &g : groups) {
                for (MdTokenRange &r : g.ranges) {
                    r.start += CODE_INDENT;
                    r.end += CODE_INDENT;
                }
                // Colorer emits nested regions inner-after-outer; flatten overlapping
                // spans into disjoint cell-order slices so a later inner span wins a
                // shared cell (R2, LineRegionsCompactSupport).
                std::vector<MdTokenRange> disjoint;
                for (const MdTokenRange &r : g.ranges) {
                    if (r.start >= r.end)
                        continue;
                    std::vector<MdTokenRange> next;
                    bool inserted = false;
                    for (const MdTokenRange &d : disjoint) {
                        if (d.end <= r.start) {
                            next.push_back(d);
                            continue;
                        }
                        if (!inserted) {
                            next.push_back(r);
                            inserted = true;
                        }
                        if (d.start >= r.end) {
                            next.push_back(d);
                            continue;
                        }
                        if (d.start < r.start) {
                            MdTokenRange left_part = d;
                            left_part.end = r.start;
                            next.insert(next.end() - 1, left_part);
                        }
                        if (d.end > r.end) {
                            MdTokenRange right_part = d;
                            right_part.start = r.end;
                            next.push_back(right_part);
                        }
                    }
                    if (!inserted)
                        next.push_back(r);
                    disjoint.swap(next);
                }
                g.ranges.swap(disjoint);
            }
        }
        if (cacheable)
            b.token_cache->emplace(key, groups);   // a deadline-trimmed group is cached as is (R29)
    }
    // In a list or blockquote, each row begins with a lead run (and optional list-item
    // prefix) before the code run.  Shift the cached group's ranges by the row's lead
    // width so PrintCodeRuns matches the running cell position `pos` across the row.
    const size_t base = b.token_groups.size();
    for (size_t gi = 0; gi < groups.size() && gi < count; ++gi) {
        MdTokenGroup g = groups[gi];
        uint32_t lead_cells = 0;
        const MdLine &row = b.out[first_line + gi];
        for (size_t ri = 0; ri + 1 < row.runs.size(); ++ri)
            lead_cells += (uint32_t)row.runs[ri].text.size();
        if (lead_cells > 0) {
            for (MdTokenRange &r : g.ranges) {
                r.start += lead_cells;
                r.end += lead_cells;
            }
        }
        b.token_groups.push_back(std::move(g));
        b.out[first_line + gi].tokens = (int)(base + gi);
    }
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
    switch (type) {
        case MD_BLOCK_QUOTE: b.open_block(); b.quote_depth++; break;
        case MD_BLOCK_UL: {
            auto *d = (MD_BLOCK_UL_DETAIL *)detail;
            // A nested list is part of its outer list.
            if (b.list_stack.empty()) b.open_block(); else b.flush();
            b.list_stack.push_back(0);
            (void)d;
            break;
        }
        case MD_BLOCK_OL: {
            auto *d = (MD_BLOCK_OL_DETAIL *)detail;
            if (b.list_stack.empty()) b.open_block(); else b.flush();
            b.list_stack.push_back(1);
            b.ol_counter.push_back(d ? (long)d->start : 1);
            break;
        }
        case MD_BLOCK_LI: {
            auto *d = (MD_BLOCK_LI_DETAIL *)detail;
            b.li_open = true;
            b.open_block();      // flush the text of a tight item before this one
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
            b.open_block();
            b.heading_level = d ? (int)d->level : 1;
            b.heading_src = b.first_nonblank_line((b.last_text_end < 0) ? 0 : (size_t)b.last_text_end);
            b.begin_line();
            break;
        }
        case MD_BLOCK_P:
            b.open_block();
            b.begin_line();
            break;
        case MD_BLOCK_CODE: {
            auto *d = (MD_BLOCK_CODE_DETAIL *)detail;
            b.open_block();
            b.in_code = true;
            b.code_text.clear();
            b.code_marks.clear();
            if (d)
                b.code_lang = attr_to_wide(d->lang);
            break;
        }
        case MD_BLOCK_HTML:
            b.open_block();
            b.in_code = true;   // verbatim, same treatment as code
            b.code_text.clear();
            b.code_marks.clear();
            b.code_lang.clear();
            break;
        case MD_BLOCK_HR: {
            b.open_block();
            // A thematic break fires no text callback: anchor its blank rows by a scan of the
            // source from the end of the last source text.  The scan stops at the first line that
            // is not empty: the break's own line; the count stops there too.
            const size_t from = (b.last_text_end < 0) ? 0 : (size_t)b.last_text_end;
            const size_t hr_line = b.first_nonblank_line(from);
            b.blank_rows((int64_t)hr_line);
            b.last_text_end = (int64_t)b.line_end(hr_line);
            b.rule_row();
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
            break;
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
            b.li_prefix_pending = false;   // an empty item must not keep its prefix for the next block
            break;
        case MD_BLOCK_H: {
            b.flush();
            if (b.heading_level == 1 || b.heading_level == 2) {
                const char first = (b.heading_src < b.base_size) ? b.base[b.heading_src] : '\n';
                const size_t start = (b.last_text_end < 0) ? 0 : (size_t)b.last_text_end;
                const size_t end = b.line_end(start);
                const char marker = (b.heading_level == 1) ? '=' : '-';
                if (first != '#' && b.line_is_setext_rule(start, end, marker)) {
                    b.last_text_end = (int64_t)end;
                    b.rule_row();
                }
            }
            b.heading_level = 0;
            break;
        }
        case MD_BLOCK_P:
            b.flush();
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
                b.append(std::wstring(CODE_INDENT, L' ') + line, MD_ROLE_CODE, -1, false);
                b.flush();
                // The fence's rows take the block's tint (story markdown-code-tint): a
                // non-negative tint marks the row, the printer resolves the theme's blend
                // at draw time, so a theme change shows on the next redraw.
                b.out.back().tint = 0;
                ++count;
                if (nl == std::wstring::npos) break;
                start = nl + 1;
            }
            // One call per fence, with the remaining render budget (plan T3).
            // A mermaid fence is tokenized like any fence (R33).
            tokenize_fence(b, fence_lang, fence_src, first_line, count);
            if (is_mermaid)
                b.fences.push_back(Builder::Fence{fence_pos, fence_src, first_line, count, b.row_lead()});
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
                        b.append(std::wstring(1, BoxSymbols[BS_V1]), MD_ROLE_RULE, -1, false);
                        for (size_t c = 0; c < cols; ++c) {
                            const int cell_w = (c < row.size()) ? (int)concat(row[c]).size() : 0;
                            const int pad = w[c] - cell_w;
                            b.append(L" ", MD_ROLE_BODY, -1, false);
                            if (c < row.size())
                                for (const MdRun &r : row[c])
                                    b.append(r.text, cell_role(r), -1, false);
                            b.append(std::wstring(pad > 0 ? pad : 0, L' ') + L" ", MD_ROLE_BODY, -1, false);
                            b.append(std::wstring(1, BoxSymbols[BS_V1]), MD_ROLE_RULE, -1, false);
                        }
                        b.flush();
                    };
                    auto render_sep = [&](wchar_t l, wchar_t mid, wchar_t r) {
                        b.begin_line();
                        b.append(std::wstring(1, l), MD_ROLE_RULE, -1, false);
                        for (size_t c = 0; c < cols; ++c) {
                            b.append(std::wstring((size_t)w[c] + 2, BoxSymbols[BS_H1]), MD_ROLE_RULE, -1, false);
                            b.append(std::wstring(1, (c + 1 < cols) ? mid : r), MD_ROLE_RULE, -1, false);
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
        case MD_SPAN_EM: b.span_em++; break;
        case MD_SPAN_U: b.span_em++; break; // md4c parses _text_ as U under MD_FLAG_UNDERLINE
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
        case MD_SPAN_EM: if (b.span_em > 0) b.span_em--; break;
        case MD_SPAN_U: if (b.span_em > 0) b.span_em--; break;
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
        if (b.code_text.empty()) {
            // First callback of a code/HTML block: count the empty lines before the block.
            // If the first line of the fence is non-empty, `off >= 0` is its line start;
            // if the first line of the fence is empty (`at < 0`), `first_nonblank_line` finds
            // the opening fence marker line so pre-fence empty lines still count.
            const size_t from = (b.last_text_end < 0) ? 0 : (size_t)b.last_text_end;
            const int64_t anchor = (off >= 0) ? off : (int64_t)b.first_nonblank_line(from);
            b.blank_rows(anchor);
            if (off < 0) {
                // Opening fence line + this first empty verbatim line:
                const size_t fence_open = b.first_nonblank_line(from);
                b.last_text_end = (int64_t)b.line_end(fence_open);
            }
        }
        if (off >= 0) {
            b.code_marks.emplace_back(b.code_text.size(), off);
            b.last_text_end = (int64_t)b.line_end((size_t)at);
        } else if (w == L"\n" && (b.code_text.empty() || b.code_text.back() == L'\n')
                && b.last_text_end >= 0) {
            // Empty verbatim line inside the code block (`md4c.c:4738` static "\n"):
            // advance past this empty fence line so it is not counted again after the fence.
            b.last_text_end = (int64_t)b.line_end((size_t)b.last_text_end);
        }
        b.code_text += w;
        return 0;
    }
    // The empty source lines before this text: the count runs at the anchor of this text's line,
    // so a text on the same display line (a styled span: cb_text fires more than once per line)
    // counts nothing.  Leading empty lines count from offset 0 (`last_text_end == -1`).
    if (off >= 0)
        b.blank_rows(off);
    if (at >= 0)
        b.last_text_end = (int64_t)b.line_end((size_t)at);
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

// --- soft wrap (F2) -----------------------------------------------------------------

// Cells the code point at `p` takes, advancing past it.  The rules of MarkdownPrinter::Print,
// so a wrapped line and a scrolled row agree on the columns: a full-width code point takes 2
// cells, a combining mark and the target of a joiner ride the cell before them.
int flow_cells(const wchar_t *&p, bool &joining)
{
    int cells = 0;
    const wchar_t ch = *p;
    if (static_cast<uint32_t>(ch) <= 0x7f) {
        if (!joining)
            cells = 1;
        joining = false;
    } else if (ch != 0xFEFF) {   // the BOM takes no cell, as in MarkdownPrinter::Print
        if (ch == CharClasses::ZERO_WIDTH_JOINER) {
            joining = true;
        } else if (CharClasses::IsFullWidth(ch)) {
            if (!joining)
                cells = 2;
            joining = false;
        } else if (joining) {
            // A combining mark, the target of a joiner, and the variation selector ride the
            // cell before them (the printer's rule): they add no cell.
            joining = false;
        } else {
            cells = 1;
        }
    }
    ++p;
    return cells;
}

// One code point of a line, with the role of its run and the cells it takes: the unit the wrap
// pass measures, and the unit it rebuilds runs from.
struct FlowCell {
    wchar_t ch;
    uint64_t role;
    int cells;
};

void flow_add(std::vector<MdRun> &runs, wchar_t ch, uint64_t role)
{
    if (!runs.empty() && runs.back().role == role)
        runs.back().text += ch;
    else
        runs.push_back(MdRun{std::wstring(1, ch), role});
}

// Soft-wrap one prose line to `width` cells.  The lead (MdLine::prefix cells: the list indent
// and the blockquote bars) repeats on every piece, so a wrapped list item and a wrapped quote
// keep their hanging indent; the list mark rides the first piece only.  A piece breaks at the
// last space it took; a word longer than the room is cut.  A line that fits is handed out
// unchanged, so its runs keep the split the parser gave them.
void wrap_line(const MdLine &in, int width, std::vector<MdLine> &out)
{
    const int head_cells = in.prefix;
    if (width <= 0 || head_cells >= width) {
        out.push_back(in);
        return;
    }

    std::vector<FlowCell> head, body;   // the lead, then the text that flows
    int total = 0;
    for (const MdRun &run : in.runs) {
        const wchar_t *p = run.text.c_str();
        bool joining = false;
        while (*p) {
            const wchar_t ch = *p;
            const int cells = flow_cells(p, joining);
            if ((int)head.size() < head_cells)
                head.push_back(FlowCell{ch, run.role, cells});
            else {
                body.push_back(FlowCell{ch, run.role, cells});
                total += cells;
            }
        }
    }

    if (body.empty() || head_cells + total <= width) {
        out.push_back(in);
        return;
    }

    const int room = width - head_cells;
    size_t i = 0;
    while (i < body.size()) {
        // Greedy fill: take cells up to `room`, then fall back to the last space taken, so a
        // word is not cut where it fits whole.
        int used = 0, stop_used = 0;
        size_t end = i, stop = i;
        while (end < body.size() && used + body[end].cells <= room) {
            used += body[end].cells;
            ++end;
            if (body[end - 1].cells == 1 && body[end - 1].ch == L' ') {
                stop = end;
                stop_used = used;
            }
        }
        if (end < body.size() && stop > i) {
            end = stop;
            used = stop_used;
        }

        MdLine piece;
        piece.nFilePos = in.nFilePos;
        piece.tokens = in.tokens;
        piece.tint = in.tint;
        piece.flow = in.flow;
        piece.prefix = in.prefix;
        for (const FlowCell &c : head)
            flow_add(piece.runs, c.ch, c.role);
        for (size_t k = i; k < end; ++k)
            flow_add(piece.runs, body[k].ch, body[k].role);
        // The spaces at the end of a piece are the break's: the next piece starts clean.
        while (!piece.runs.empty()) {
            std::wstring &t = piece.runs.back().text;
            t.erase(t.find_last_not_of(L' ') + 1);
            if (!t.empty())
                break;
            piece.runs.pop_back();
        }
        out.push_back(std::move(piece));
        i = end;
    }
}

}   // namespace

// ---------------------------------------------------------------------------

void MarkdownDoc::Render(const std::wstring &file_text, int width, bool wrap)
{
    m_text = file_text;
    m_width = width;
    m_wrap = wrap;
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
    b.blank_rows((int64_t)b.base_size);   // the trailing empty lines of the file

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
            hint.tint = 0;   // the hint sits under the fence's code rows: same block, same tint
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

    // The wrap pass runs last: the splice above addresses lines by index, and splitting a prose
    // line changes no offset, so scrolling, the position cache and the mode switch keep working
    // on the wrapped doc.
    if (m_wrap) {
        std::vector<MdLine> wrapped;
        wrapped.reserve(m_lines.size());
        for (const MdLine &line : m_lines) {
            if (line.flow)
                wrap_line(line, width, wrapped);
            else
                wrapped.push_back(line);
        }
        m_lines.swap(wrapped);
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

void MarkdownDoc::Invalidate(int width, bool wrap)
{
    m_mermaid_cache.clear();
    // The token cache outlives a resize: a key is language, fence hash and
    // scheme generation (R30); a scheme change re-keys, a resize does not.
    Render(m_text, width, wrap);
}
