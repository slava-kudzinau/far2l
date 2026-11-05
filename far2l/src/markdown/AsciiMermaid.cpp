#include "headers.hpp"
#include "AsciiMermaid.hpp"
#include <quickjs.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

// ---------------------------------------------------------------------------
// ASCII Mermaid (F3 markdown view).  One quickjs-ng runtime for the process, built on the first
// render: it evaluates mermaid-ascii.js, whose global renderAscii(src) returns the art (or throws
// Error 'unsupported: <type>' or a parse message).  Each render runs under the same bounds as
// MermaidEngine: a wall-clock deadline (interrupt handler), a 1 MB JS stack and a memory limit.
// A render that hits the deadline frees the runtime; the next render builds it again from disk.
// ---------------------------------------------------------------------------

namespace {

const size_t kJsStackBytes = 1024 * 1024;
const size_t kMemoryLimitBytes = 128 * 1024 * 1024;
const long long kDefaultDeadlineMs = 2000;
const char kBundleName[] = "mermaid-ascii.js";

std::mutex g_mutex;

struct Engine {
    JSRuntime *rt = nullptr;
    JSContext *ctx = nullptr;
};

Engine g_engine;

std::chrono::steady_clock::time_point g_deadline;
bool g_timed_out = false;

// The render deadline: FAR2L_MD_DEADLINE_MS when it is a positive number, else 2 s.
long long deadline_ms()
{
    if (const char *env = getenv("FAR2L_MD_DEADLINE_MS")) {
        char *end = nullptr;
        const long long v = strtoll(env, &end, 10);
        if (end != env && *end == 0 && v > 0)
            return v;
    }
    return kDefaultDeadlineMs;
}

// The interrupt handler: stops the running JS once the deadline has passed, and latches it.
int interrupt_at_deadline(JSRuntime *, void *)
{
    if (!g_timed_out && std::chrono::steady_clock::now() >= g_deadline)
        g_timed_out = true;
    return g_timed_out ? 1 : 0;
}

// Frees the runtime.  The next render builds a fresh one.
void discard_engine()
{
    if (g_engine.ctx)
        JS_FreeContext(g_engine.ctx);
    if (g_engine.rt)
        JS_FreeRuntime(g_engine.rt);
    g_engine = Engine();
}

bool read_file(const std::string &path, std::string &out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return !out.empty();
}

// The directory of the running executable, or "" when it is unknown.
std::string exe_dir()
{
#ifdef __APPLE__
    char path[4096];
    uint32_t n = sizeof(path);
    if (_NSGetExecutablePath(path, &n) == 0) {
        std::string p(path);
        auto slash = p.rfind('/');
        if (slash != std::string::npos)
            return p.substr(0, slash);
    }
#elif defined(__linux__)
    char path[4096];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n > 0) {
        path[n] = 0;
        std::string p(path);
        auto slash = p.rfind('/');
        if (slash != std::string::npos)
            return p.substr(0, slash);
    }
#endif
    return std::string();
}

// The bundle path: FAR2L_MERMAID_ASCII_JS when set (and not searched for elsewhere), else the
// first readable copy in the places MermaidEngine looks for mermaid.min.js.
std::string locate_bundle()
{
    if (const char *env = getenv("FAR2L_MERMAID_ASCII_JS")) {
        if (*env)
            return env;
    }
    std::vector<std::string> cands;
    std::string ed = exe_dir();
    if (!ed.empty()) {
        // macOS bundle: Contents/MacOS/far2l -> Contents/Resources/mermaid-ascii.js
        cands.push_back(ed + "/../Resources/" + kBundleName);
        // flat install: <prefix>/bin/far2l -> <prefix>/share/far2l/mermaid-ascii.js
        cands.push_back(ed + "/../share/far2l/" + kBundleName);
        cands.push_back(ed + "/" + kBundleName);
    }
    cands.push_back(std::string("/usr/local/share/far2l/") + kBundleName);
    cands.push_back(std::string("/usr/share/far2l/") + kBundleName);
    cands.push_back(std::string("/usr/local/lib/far2l/") + kBundleName);
    cands.push_back(std::string("/usr/lib/far2l/") + kBundleName);
    cands.push_back(kBundleName);
    for (const auto &c : cands) {
        std::ifstream f(c);
        if (f.good())
            return c;
    }
    return std::string();
}

// The property `name` of an object as text, or "" when it has none.
std::string property_text(JSContext *ctx, JSValueConst obj, const char *name)
{
    JSValue v = JS_GetPropertyStr(ctx, obj, name);
    std::string out;
    if (!JS_IsException(v) && !JS_IsUndefined(v)) {
        if (const char *s = JS_ToCString(ctx, v)) {
            out = s;
            JS_FreeCString(ctx, s);
        }
    }
    JS_FreeValue(ctx, v);
    return out;
}

// Takes the pending exception as the error text.  A plain Error (the bundle's own throws) gives
// its message; any other exception gives "js: <message>".
std::string take_error(JSContext *ctx)
{
    JSValue ex = JS_GetException(ctx);
    std::string out;
    if (JS_IsObject(ex)) {
        const bool plain = property_text(ctx, ex, "name") == "Error";
        out = property_text(ctx, ex, "message");
        if (out.empty()) {
            if (const char *s = JS_ToCString(ctx, ex)) {
                out = s;
                JS_FreeCString(ctx, s);
            }
        }
        if (!plain)
            out = "js: " + out;
    } else {
        if (const char *s = JS_ToCString(ctx, ex)) {
            out = s;
            JS_FreeCString(ctx, s);
        }
        out = "js: " + out;
    }
    JS_FreeValue(ctx, ex);
    return out;
}

// Builds the runtime and evaluates the bundle.  On failure `err` is set and nothing is kept.
bool build_engine(std::string &err)
{
    const std::string path = locate_bundle();
    std::string js;
    if (path.empty() || !read_file(path, js)) {
        err = "bundle missing";
        return false;
    }

    g_engine.rt = JS_NewRuntime();
    g_engine.ctx = g_engine.rt ? JS_NewContext(g_engine.rt) : nullptr;
    if (!g_engine.ctx) {
        discard_engine();
        err = "js: cannot create a quickjs context";
        return false;
    }
    JS_SetMemoryLimit(g_engine.rt, kMemoryLimitBytes);
    JS_SetMaxStackSize(g_engine.rt, kJsStackBytes);
    JS_SetInterruptHandler(g_engine.rt, interrupt_at_deadline, nullptr);
    JS_UpdateStackTop(g_engine.rt);

    JSValue v = JS_Eval(g_engine.ctx, js.data(), js.size(), kBundleName, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) {
        err = take_error(g_engine.ctx);
        JS_FreeValue(g_engine.ctx, v);
        discard_engine();
        return false;
    }
    JS_FreeValue(g_engine.ctx, v);
    return true;
}

// Calls renderAscii(src) and returns its text in `out`.
bool call_render(const std::string &src, std::string &out, std::string &err)
{
    JSContext *ctx = g_engine.ctx;
    JS_UpdateStackTop(g_engine.rt);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "renderAscii");
    JSValue arg = JS_NewStringLen(ctx, src.data(), src.size());
    JSValue r = JS_Call(ctx, fn, global, 1, &arg);

    bool ok = false;
    if (JS_IsException(r)) {
        err = take_error(ctx);
    } else if (!JS_IsString(r)) {
        err = "js: renderAscii did not return a string";
    } else if (const char *s = JS_ToCString(ctx, r)) {
        out = s;
        JS_FreeCString(ctx, s);
        ok = true;
    } else {
        err = take_error(ctx);
    }
    JS_FreeValue(ctx, arg);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, r);
    JS_FreeValue(ctx, global);
    return ok;
}

// Splits `text` on newlines and drops the empty rows at the end.
void split_rows(const std::string &text, std::vector<std::string> &lines)
{
    size_t start = 0;
    for (;;) {
        const size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    while (!lines.empty() && lines.back().empty())
        lines.pop_back();
}

}   // namespace

bool AsciiMermaid::Render(const std::string &src, std::vector<std::string> &lines, std::string &err)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    lines.clear();
    err.clear();
    g_timed_out = false;
    g_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(deadline_ms());

    std::string text;
    bool ok = g_engine.ctx != nullptr || build_engine(err);
    if (ok)
        ok = call_render(src, text, err);

    if (g_timed_out) {
        discard_engine();
        lines.clear();
        err = "timeout";
        return false;
    }
    if (!ok)
        return false;

    split_rows(text, lines);
    if (lines.empty()) {
        err = "empty output";
        return false;
    }
    return true;
}
