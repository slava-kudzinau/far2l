// Standalone driver for MermaidEngine (F3 markdown preview).  No TUI needed;
// build and run through testing/markdown/run.sh.
//
// usage: mermaid_driver <flowchart|swimlane|invalid|cells|hostile|timeout> <path/to/sample.md>
//   flowchart  renders the spec flowchart; SVG must start with <svg and contain Start and Finish
//   swimlane   renders the first ```mermaid fence of sample.md; same SVG expectations
//   invalid    renders broken mermaid text; must fail with a non-empty error
//   cells      renders the flowchart and the swimlane, rasterizes both to 80 cells (SvgCanvas)
//              and prints the grids; each must show Start and Finish, a box glyph, and no row
//              wider than 80 cells
//   hostile    feeds hostile SVG (deep nesting, a long CSS selector, '&' with no ';') straight
//              into SvgCanvas, without a mermaid render; each must parse and finish in under 2 s
//   timeout    renders 300 nested subgraphs, which mermaid cannot lay out inside the render
//              deadline; Render must fail with "mermaid render timed out" within 10 s, and a
//              flowchart must render afterwards (the stopped engine is rebuilt)
//
// Prints ok=1 (plus the SVG) or ok=0 (plus err=...).  Exit 0 when the case
// meets its expectation, 1 when it does not, 2 on bad usage.

#include "MermaidEngine.hpp"
#include "SvgCanvas.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace {

const char kFlowchart[] = "flowchart LR\n A[Start] --> B[Finish]\n";
const char kInvalid[] = "this is not a mermaid diagram @@@\n";
const long long kHostileBudgetMs = 2000;
const long long kTimeoutBudgetMs = 10000;

std::string to_utf8(const std::wstring &w)
{
    std::string out;
    for (wchar_t wc : w) {
        const unsigned cp = (unsigned)wc;
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
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

// Copies the body of the first ```mermaid fence in `path` into `src`.
bool first_mermaid_fence(const std::string &path, std::string &src)
{
    std::ifstream f(path);
    if (!f)
        return false;
    std::string line;
    bool in_fence = false;
    while (std::getline(f, line)) {
        if (!in_fence) {
            if (line.rfind("```mermaid", 0) == 0)
                in_fence = true;
            continue;
        }
        if (line.rfind("```", 0) == 0)
            return true;
        src += line + "\n";
    }
    return false;
}

bool svg_expected(const std::string &svg)
{
    return svg.rfind("<svg", 0) == 0 && svg.find("Start") != std::string::npos
        && svg.find("Finish") != std::string::npos;
}

// Renders `src`, rasterizes it to `width` cells and prints the grid.
bool check_cells(const char *name, const std::string &src, int width)
{
    std::string svg, err;
    if (!MermaidEngine::Render(src, "m1", width * 8, svg, err)) {
        printf("%s: render failed: %s\n", name, err.c_str());
        return false;
    }
    SvgCanvas canvas;
    if (!canvas.Parse(svg)) {
        printf("%s: no drawable shapes in the svg\n", name);
        return false;
    }
    std::vector<MdLine> lines;
    canvas.ToCells(width, lines, 0);

    std::wstring all;
    bool rows_fit = true;
    printf("== %s, %d cells ==\n", name, width);
    for (const MdLine &line : lines) {
        std::wstring row;
        for (const MdRun &run : line.runs)
            row += run.text;
        if ((int)row.size() > width)
            rows_fit = false;
        all += row + L'\n';
        printf("%s\n", to_utf8(row).c_str());
    }
    const bool has_start = all.find(L"Start") != std::wstring::npos;
    const bool has_finish = all.find(L"Finish") != std::wstring::npos;
    const bool has_box = all.find_first_of(L"┌┐└┘│─") != std::wstring::npos;
    const bool ok = has_start && has_finish && has_box && rows_fit;
    printf("%s: Start=%d Finish=%d box=%d rows_fit=%d: %s\n", name, has_start, has_finish,
           has_box, rows_fit, ok ? "ok" : "FAIL");
    return ok;
}

// SvgCanvas must stay bounded on hostile input.  Each case is parsed and rasterized
// directly; it must yield a drawable shape and finish within kHostileBudgetMs.
bool check_hostile()
{
    const std::string rect = "<rect x=\"0\" y=\"0\" width=\"80\" height=\"32\"/>";
    std::vector<std::pair<std::string, std::string>> cases;

    // 5000 nested <g>.  The rect past the depth cap is ignored; the one above it is drawn.
    std::string deep = "<svg>" + rect;
    for (int i = 0; i < 5000; ++i)
        deep += "<g>";
    deep += rect;
    for (int i = 0; i < 5000; ++i)
        deep += "</g>";
    cases.emplace_back("deep-nesting", deep + "</svg>");

    // A 30-compound selector (30 g) that cannot match under 29 g ancestors.  Every g and the
    // text consult it, so unbounded backtracking over the ancestors would run for seconds.
    std::string sel30;
    for (int i = 0; i < 30; ++i)
        sel30 += "g ";
    std::string css30 = "<svg>" + rect + "<style>" + sel30 + "{ text-anchor: end; }</style>";
    for (int i = 0; i < 29; ++i)
        css30 += "<g>";
    css30 += "<text x=\"200\" y=\"16\">Start</text>";
    for (int i = 0; i < 29; ++i)
        css30 += "</g>";
    cases.emplace_back("selector-30", css30 + "</svg>");

    // One million '&' with no ';' in one text.
    std::string amps = "<svg>" + rect + "<text x=\"0\" y=\"16\">";
    amps.append(1000000, '&');
    cases.emplace_back("ampersands", amps + "</text></svg>");

    bool all_ok = true;
    for (const auto &c : cases) {
        const auto t0 = std::chrono::steady_clock::now();
        SvgCanvas canvas;
        const bool parsed = canvas.Parse(c.second);
        std::vector<MdLine> lines;
        if (parsed)
            canvas.ToCells(80, lines, 0);
        const long long ms = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - t0).count();
        const bool ok = parsed && ms < kHostileBudgetMs;
        printf("hostile %s: parsed=%d rows=%zu elapsed_ms=%lld: %s\n", c.first.c_str(), (int)parsed,
               lines.size(), ms, ok ? "ok" : "FAIL");
        all_ok = all_ok && ok;
    }
    return all_ok;
}

// 300 nested subgraphs with a node in the innermost one: the layout is far too slow for the
// render deadline, so Render must give up on it.
std::string nested_subgraphs(int depth)
{
    std::string src = "flowchart TD\n";
    for (int i = 0; i < depth; ++i)
        src += "subgraph s" + std::to_string(i) + " [group " + std::to_string(i) + "]\n";
    src += "X0[inner] --> X1[inner2]\n";
    for (int i = 0; i < depth; ++i)
        src += "end\n";
    return src;
}

// A render stopped by the deadline must fail with the timeout message and return within
// kTimeoutBudgetMs; the engine must then render a flowchart again.
bool check_timeout()
{
    const auto t0 = std::chrono::steady_clock::now();
    std::string svg, err;
    const bool ok = MermaidEngine::Render(nested_subgraphs(300), "m1", 800, svg, err);
    const long long ms = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - t0).count();
    const bool stopped = !ok && err == "mermaid render timed out";
    const bool in_time = ms < kTimeoutBudgetMs;
    printf("timeout nested-subgraphs: ok=%d elapsed_ms=%lld err=%s: %s\n", (int)ok, ms, err.c_str(),
           (stopped && in_time) ? "ok" : "FAIL");

    std::string after_svg, after_err;
    const bool recovered = MermaidEngine::Render(kFlowchart, "m2", 800, after_svg, after_err)
                           && svg_expected(after_svg);
    printf("timeout recovery flowchart: ok=%d err=%s: %s\n", (int)recovered, after_err.c_str(),
           recovered ? "ok" : "FAIL");
    return stopped && in_time && recovered;
}

}   // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s flowchart|swimlane|invalid|cells|hostile|timeout <sample.md>\n", argv[0]);
        return 2;
    }
    const std::string which = argv[1];
    std::string src;
    bool expect_ok = true;

    if (which == "hostile")
        return check_hostile() ? 0 : 1;

    if (which == "timeout")
        return check_timeout() ? 0 : 1;

    if (which == "cells") {
        std::string swim;
        if (argc < 3 || !first_mermaid_fence(argv[2], swim)) {
            fprintf(stderr, "no ```mermaid fence in %s\n", argc < 3 ? "(no file given)" : argv[2]);
            return 2;
        }
        const bool flow_ok = check_cells("flowchart", kFlowchart, 80);
        const bool swim_ok = check_cells("swimlane", swim, 80);
        return (flow_ok && swim_ok) ? 0 : 1;
    }

    if (which == "flowchart") {
        src = kFlowchart;
    } else if (which == "swimlane") {
        if (argc < 3 || !first_mermaid_fence(argv[2], src)) {
            fprintf(stderr, "no ```mermaid fence in %s\n", argc < 3 ? "(no file given)" : argv[2]);
            return 2;
        }
    } else if (which == "invalid") {
        src = kInvalid;
        expect_ok = false;
    } else {
        fprintf(stderr, "unknown case: %s\n", which.c_str());
        return 2;
    }

    std::string svg, err;
    const bool ok = MermaidEngine::Render(src, "m1", 800, svg, err);
    // A failed render must say why, so ok=0 never comes with an empty err.
    const std::string why = err.empty() ? "render failed without a message" : err;

    if (expect_ok) {
        if (ok && err.empty() && svg_expected(svg)) {
            printf("ok=1\n%s\n", svg.c_str());
            return 0;
        }
        printf("ok=0\nerr=%s\n", ok ? "svg does not start with <svg or lacks Start/Finish" : why.c_str());
        if (ok)
            printf("%s\n", svg.c_str());
        return 1;
    }

    if (!ok) {
        printf("ok=0\nerr=%s\n", why.c_str());
        return 0;
    }
    printf("ok=1\nerr=invalid input unexpectedly rendered\n");
    return 1;
}
