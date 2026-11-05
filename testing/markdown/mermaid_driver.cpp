// Standalone driver for MermaidEngine (F3 markdown preview).  No TUI needed;
// build and run through testing/markdown/run.sh.
//
// usage: mermaid_driver <flowchart|swimlane|invalid> <path/to/sample.md>
//   flowchart  renders the spec flowchart; SVG must start with <svg and contain Start and Finish
//   swimlane   renders the first ```mermaid fence of sample.md; same SVG expectations
//   invalid    renders broken mermaid text; must fail with a non-empty error
//
// Prints ok=1 (plus the SVG) or ok=0 (plus err=...).  Exit 0 when the case
// meets its expectation, 1 when it does not, 2 on bad usage.

#include "MermaidEngine.hpp"

#include <cstdio>
#include <fstream>
#include <string>

namespace {

const char kFlowchart[] = "flowchart LR\n A[Start] --> B[Finish]\n";
const char kInvalid[] = "this is not a mermaid diagram @@@\n";

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

}   // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s flowchart|swimlane|invalid <sample.md>\n", argv[0]);
        return 2;
    }
    const std::string which = argv[1];
    std::string src;
    bool expect_ok = true;

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
