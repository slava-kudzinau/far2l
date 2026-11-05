// Standalone driver for MermaidEngine (F3 markdown preview).  No TUI needed;
// build and run through testing/markdown/run.sh.
//
// usage: mermaid_driver <flowchart|swimlane|invalid|cells> <path/to/sample.md>
//   flowchart  renders the spec flowchart; SVG must start with <svg and contain Start and Finish
//   swimlane   renders the first ```mermaid fence of sample.md; same SVG expectations
//   invalid    renders broken mermaid text; must fail with a non-empty error
//   cells      renders the flowchart and the swimlane, rasterizes both to 80 cells (SvgCanvas)
//              and prints the grids; each must show Start and Finish, a box glyph, and no row
//              wider than 80 cells
//
// Prints ok=1 (plus the SVG) or ok=0 (plus err=...).  Exit 0 when the case
// meets its expectation, 1 when it does not, 2 on bad usage.

#include "MermaidEngine.hpp"
#include "SvgCanvas.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

const char kFlowchart[] = "flowchart LR\n A[Start] --> B[Finish]\n";
const char kInvalid[] = "this is not a mermaid diagram @@@\n";

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

}   // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s flowchart|swimlane|invalid|cells <sample.md>\n", argv[0]);
        return 2;
    }
    const std::string which = argv[1];
    std::string src;
    bool expect_ok = true;

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
