// Standalone driver for AsciiMermaid (story markdown-ascii-diagrams).  No TUI needed; built and
// run by testing/markdown/ascii-driver.sh, which starts each mode in its own process.
//
// usage: ascii_driver <mode> <mermaid-ascii.js> <golden-dir> <scratch-dir>
//   flowchart    golden/flowchart.mmd renders to golden/flowchart.txt row for row; the rows have
//                no ANSI codes and no trailing spaces
//   unsupported  golden/swimlane.mmd fails with the text of golden/swimlane.txt
//   broken       golden/bad.mmd fails with the one-line parse message of golden/bad.txt
//   missing      with FAR2L_MERMAID_ASCII_JS naming no file, Render fails with "bundle missing"
//   timeout      a bundle whose renderAscii loops fails with "timeout" after the default 2 s;
//                FAR2L_MD_DEADLINE_MS=300 shortens that to about 0.3 s; a loop at the top level of
//                the bundle times out too; the flowchart renders again after the timeouts
//   speed        three flowchart renders in one process; the 2nd and 3rd take under 50 ms each
//
// Exit 0 when every check of the mode passes, 1 when one fails, 2 on bad usage.

#include "AsciiMermaid.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Paths {
    std::string bundle;
    std::string golden;
    std::string scratch;
};

int g_failures = 0;

void check(bool ok, const std::string &what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}

// The file's bytes, or "" when it cannot be read.
std::string read_file(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void write_file(const std::string &path, const std::string &text)
{
    std::ofstream f(path, std::ios::binary);
    f << text;
}

// A golden .txt holds the message or the art followed by one final newline.
std::string without_final_newline(std::string s)
{
    if (!s.empty() && s.back() == '\n')
        s.pop_back();
    return s;
}

// The rows as a golden .txt stores them: each row followed by a newline.
std::string rows_text(const std::vector<std::string> &lines)
{
    std::string out;
    for (const auto &row : lines) {
        out += row;
        out += '\n';
    }
    return out;
}

bool has_trailing_space(const std::string &row)
{
    return !row.empty() && row.back() == ' ';
}

bool render(const std::string &src, std::vector<std::string> &lines, std::string &err,
            double *ms = nullptr)
{
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = AsciiMermaid::Render(src, lines, err);
    if (ms)
        *ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return ok;
}

void use_bundle(const std::string &path)
{
    setenv("FAR2L_MERMAID_ASCII_JS", path.c_str(), 1);
}

int mode_flowchart(const Paths &p)
{
    const std::string src = read_file(p.golden + "/flowchart.mmd");
    const std::string want = read_file(p.golden + "/flowchart.txt");
    use_bundle(p.bundle);

    std::vector<std::string> lines;
    std::string err;
    const bool ok = render(src, lines, err);
    check(ok, "flowchart: Render is true" + (ok ? std::string() : " (err: " + err + ")"));
    check(rows_text(lines) == want, "flowchart: rows equal golden/flowchart.txt");

    bool clean = true;
    for (const auto &row : lines) {
        if (row.find('\x1b') != std::string::npos || has_trailing_space(row))
            clean = false;
    }
    check(clean, "flowchart: no ANSI codes and no trailing spaces");
    return 0;
}

int mode_unsupported(const Paths &p)
{
    const std::string src = read_file(p.golden + "/swimlane.mmd");
    const std::string want = without_final_newline(read_file(p.golden + "/swimlane.txt"));
    use_bundle(p.bundle);

    std::vector<std::string> lines;
    std::string err;
    const bool ok = render(src, lines, err);
    check(!ok, "unsupported: Render is false");
    check(err == want, "unsupported: err equals golden/swimlane.txt (" + err + ")");
    check(err.rfind("unsupported:", 0) == 0, "unsupported: err starts with 'unsupported:'");
    check(lines.empty(), "unsupported: no rows");
    return 0;
}

int mode_broken(const Paths &p)
{
    const std::string src = read_file(p.golden + "/bad.mmd");
    const std::string want = without_final_newline(read_file(p.golden + "/bad.txt"));
    use_bundle(p.bundle);

    std::vector<std::string> lines;
    std::string err;
    const bool ok = render(src, lines, err);
    check(!ok, "broken: Render is false");
    check(err == want, "broken: err equals golden/bad.txt (" + err + ")");
    check(!err.empty() && err.find('\n') == std::string::npos, "broken: err is one non-empty line");
    check(err.rfind("unsupported:", 0) != 0, "broken: err is not an 'unsupported:' row");
    check(lines.empty(), "broken: no rows");
    return 0;
}

int mode_missing(const Paths &p)
{
    const std::string src = read_file(p.golden + "/flowchart.mmd");
    const std::string absent = p.scratch + "/no-such-mermaid-ascii.js";
    std::remove(absent.c_str());
    use_bundle(absent);

    for (int call = 1; call <= 2; ++call) {
        std::vector<std::string> lines;
        std::string err;
        const bool ok = render(src, lines, err);
        check(!ok && err == "bundle missing",
              "missing: call " + std::to_string(call) + " fails with 'bundle missing' (" + err + ")");
    }
    return 0;
}

int mode_timeout(const Paths &p)
{
    const std::string src = read_file(p.golden + "/flowchart.mmd");
    const std::string want = read_file(p.golden + "/flowchart.txt");
    const std::string loop_call = p.scratch + "/loop-call.js";
    const std::string loop_load = p.scratch + "/loop-load.js";
    write_file(loop_call, "globalThis.renderAscii = function () { for (;;) {} };\n");
    write_file(loop_load, "for (;;) {}\n");

    std::vector<std::string> lines;
    std::string err;
    double ms = 0;

    unsetenv("FAR2L_MD_DEADLINE_MS");
    use_bundle(loop_call);
    bool ok = render(src, lines, err, &ms);
    check(!ok && err == "timeout", "timeout: a looping call fails with 'timeout' (" + err + ")");
    check(ms >= 1900 && ms < 2500,
          "timeout: the default deadline is about 2 s (" + std::to_string((int)ms) + " ms)");

    setenv("FAR2L_MD_DEADLINE_MS", "300", 1);
    use_bundle(loop_call);
    ok = render(src, lines, err, &ms);
    check(!ok && err == "timeout" && ms < 1000,
          "timeout: FAR2L_MD_DEADLINE_MS=300 gives 'timeout' in under 1 s (" +
              std::to_string((int)ms) + " ms)");

    use_bundle(loop_load);
    ok = render(src, lines, err, &ms);
    check(!ok && err == "timeout", "timeout: a loop in the bundle's top level fails with 'timeout' (" + err + ")");

    unsetenv("FAR2L_MD_DEADLINE_MS");
    use_bundle(p.bundle);
    ok = render(src, lines, err);
    check(ok && rows_text(lines) == want, "timeout: the flowchart renders after the timeouts (" + err + ")");
    return 0;
}

int mode_speed(const Paths &p)
{
    const std::string src = read_file(p.golden + "/flowchart.mmd");
    const std::string want = read_file(p.golden + "/flowchart.txt");
    use_bundle(p.bundle);

    double ms[3] = {0, 0, 0};
    bool all_ok = true;
    bool all_same = true;
    for (int i = 0; i < 3; ++i) {
        std::vector<std::string> lines;
        std::string err;
        const bool ok = render(src, lines, err, &ms[i]);
        all_ok = all_ok && ok;
        all_same = all_same && rows_text(lines) == want;
    }
    check(all_ok && all_same, "speed: three flowchart renders succeed and equal the golden");
    printf("     render times: %.1f ms, %.1f ms, %.1f ms\n", ms[0], ms[1], ms[2]);
    check(ms[1] < 50, "speed: the 2nd render takes under 50 ms");
    check(ms[2] < 50, "speed: the 3rd render takes under 50 ms");
    return 0;
}

}   // namespace

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: %s <flowchart|unsupported|broken|missing|timeout|speed> "
                        "<mermaid-ascii.js> <golden-dir> <scratch-dir>\n", argv[0]);
        return 2;
    }
    const std::string mode = argv[1];
    const Paths p{argv[2], argv[3], argv[4]};

    if (mode == "flowchart")
        mode_flowchart(p);
    else if (mode == "unsupported")
        mode_unsupported(p);
    else if (mode == "broken")
        mode_broken(p);
    else if (mode == "missing")
        mode_missing(p);
    else if (mode == "timeout")
        mode_timeout(p);
    else if (mode == "speed")
        mode_speed(p);
    else {
        fprintf(stderr, "unknown mode: %s\n", mode.c_str());
        return 2;
    }
    return g_failures == 0 ? 0 : 1;
}
