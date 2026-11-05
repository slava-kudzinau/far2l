#include "headers.hpp"
#include "MermaidEngine.hpp"
#include <quickjs.h>

#include <vector>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <fstream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Embedded mermaid (F3 markdown preview): a singleton quickjs-ng runtime that
// runs mermaid.min.js against a minimal DOM shim and returns the generated SVG.
//
// mermaid.render() returns a Promise resolving to the SVG string.  quickjs
// drives its microtask queue via JS_ExecutePendingJob, so after evaluating the
// render call we drain pending jobs, then read the result off the global.
// ---------------------------------------------------------------------------

namespace {

// Headless DOM/XMLSerializer shim, sufficient for the d3 bundled in mermaid.min.js.
// d3 needs: createElement/NS, appendChild/insertBefore/removeChild, attribute
// get/set, classList, textContent, style, getBoundingClientRect, querySelector(All).
const char kDomShim[] = R"JS(
(function () {
  function ClassList() {}
  ClassList.prototype.add = function () { for (var i = 0; i < arguments.length; i++) this._s[arguments[i]] = 1; };
  ClassList.prototype.remove = function (c) { delete this._s[c]; };
  ClassList.prototype.contains = function (c) { return !!this._s[c]; };
  ClassList.prototype.toggle = function (c, f) { if (f) this.add(c); else this.remove(c); };

  function El(tag, ns) {
    this.tagName = String(tag).toUpperCase();
    this.nodeName = this.tagName;
    this.ns = ns || null;
    this.childNodes = [];
    this._attrs = {};
    this.style = {};
    this._text = null;
    this.classList = new ClassList();
    this.classList._s = {};
    this.parentNode = null;
    this.id = '';
    this._listeners = {};
  }
  El.prototype.setAttribute = function (k, v) { this._attrs[k] = String(v); if (k === 'id') this.id = String(v); };
  El.prototype.getAttribute = function (k) { return (k in this._attrs) ? this._attrs[k] : null; };
  El.prototype.removeAttribute = function (k) { delete this._attrs[k]; };
  El.prototype.hasAttribute = function (k) { return k in this._attrs; };
  El.prototype.appendChild = function (c) { this.childNodes.push(c); c.parentNode = this; return c; };
  El.prototype.removeChild = function (c) { var i = this.childNodes.indexOf(c); if (i >= 0) { this.childNodes.splice(i, 1); c.parentNode = null; } return c; };
  El.prototype.insertBefore = function (c, ref) { var i = ref ? this.childNodes.indexOf(ref) : -1; if (i < 0) i = this.childNodes.length; this.childNodes.splice(i, 0, c); c.parentNode = this; return c; };
  El.prototype.remove = function () { if (this.parentNode) this.parentNode.removeChild(this); };
  El.prototype.getBoundingClientRect = function () {
    var w = parseFloat(this._attrs['width'] || this._attrs['data-width'] || '0') || 0;
    var h = parseFloat(this._attrs['height'] || this._attrs['data-height'] || '0') || 0;
    return { width: w, height: h, top: 0, left: 0, right: w, bottom: h, x: 0, y: 0 };
  };
  El.prototype.getBBox = function () {
    var w = parseFloat(this._attrs['width'] || '0') || 0, h = parseFloat(this._attrs['height'] || '0') || 0;
    return { x: 0, y: 0, width: w, height: h };
  };
  El.prototype.addEventListener = function (t, fn) { (this._listeners[t] = this._listeners[t] || []).push(fn); };
  El.prototype.setAttributeNS = El.prototype.setAttribute;
  El.prototype.getAttributeNS = El.prototype.getAttribute;
  function walk(node, out) { for (var i = 0; i < node.childNodes.length; i++) { var c = node.childNodes[i]; if (c.childNodes) { out.push(c); walk(c, out); } } }
  function tagOf(sel) { var m = String(sel).match(/([a-zA-Z][\w-]*)$/); return m ? m[1].toUpperCase() : null; }
  El.prototype.querySelectorAll = function (sel) { var all = []; walk(this, all); var t = tagOf(sel), id = (sel[0] === '#') ? sel.slice(1) : null; var res = []; for (var i = 0; i < all.length; i++) { var n = all[i]; if (id && n.id === id) res.push(n); else if (t && n.tagName === t) res.push(n); } return res; };
  El.prototype.querySelector = function (sel) { var r = this.querySelectorAll(sel); return r.length ? r[0] : null; };

  var SVGNS = 'http://www.w3.org/2000/svg';
  Object.defineProperty(El.prototype, 'ownerDocument', { get: function () { return globalThis.document; } });
  Object.defineProperty(El.prototype, 'namespaceURI', { get: function () { return this.ns || 'http://www.w3.org/1999/xhtml'; } });
  Object.defineProperty(El.prototype, 'localName', { get: function () { return this.tagName.toLowerCase(); } });
  Object.defineProperty(El.prototype, 'firstChild', { get: function () { return this.childNodes[0] || null; } });
  Object.defineProperty(El.prototype, 'lastChild', { get: function () { return this.childNodes[this.childNodes.length - 1] || null; } });
  Object.defineProperty(El.prototype, 'children', { get: function () { return this.childNodes.filter(function (c) { return c.tagName !== '#text'; }); } });
  Object.defineProperty(El.prototype, 'parentElement', { get: function () { return this.parentNode; } });
  Object.defineProperty(El.prototype, 'nextSibling', { get: function () { var p = this.parentNode; if (!p) return null; var i = p.childNodes.indexOf(this); return p.childNodes[i + 1] || null; } });
  Object.defineProperty(El.prototype, 'previousSibling', { get: function () { var p = this.parentNode; if (!p) return null; var i = p.childNodes.indexOf(this); return i > 0 ? p.childNodes[i - 1] : null; } });
  Object.defineProperty(El.prototype, 'nodeType', { get: function () { return this.tagName === '#text' ? 3 : 1; } });
  Object.defineProperty(El.prototype, 'dataset', { get: function () { return this._ds || (this._ds = {}); } });
  Object.defineProperty(El.prototype, 'className', { get: function () { return this._attrs['class'] || ''; }, set: function (v) { this._attrs['class'] = String(v); } });
  Object.defineProperty(El.prototype, 'nodeValue', { get: function () { return this._text; }, set: function (v) { this._text = String(v); } });
  Object.defineProperty(El.prototype, 'data', { get: function () { return this._text; }, set: function (v) { this._text = String(v); } });
  El.prototype.contains = function (n) { while (n) { if (n === this) return true; n = n.parentNode; } return false; };
  El.prototype.hasChildNodes = function () { return this.childNodes.length > 0; };
  El.prototype.append = function () { for (var i = 0; i < arguments.length; i++) { var a = arguments[i]; this.appendChild(typeof a === 'string' ? document.createTextNode(a) : a); } };
  El.prototype.prepend = function (a) { this.insertBefore(a, this.childNodes[0] || null); };
  El.prototype.replaceChild = function (n, o) { this.insertBefore(n, o); this.removeChild(o); return o; };
  El.prototype.cloneNode = function (deep) { var n = new El(this.tagName, this.ns); for (var k in this._attrs) n._attrs[k] = this._attrs[k]; n._text = this._text; for (var s in this.style) n.style[s] = this.style[s]; if (deep) for (var i = 0; i < this.childNodes.length; i++) n.appendChild(this.childNodes[i].cloneNode(true)); return n; };
  El.prototype.removeEventListener = function () {};
  El.prototype.dispatchEvent = function () { return true; };
  El.prototype.focus = function () {};
  El.prototype.matches = function (sel) { var t = tagOf(sel); return !!t && this.tagName === t; };
  El.prototype.getElementsByTagName = function (t) { return this.querySelectorAll(t); };
  function textLen(n) { var fs = parseFloat(n.style['font-size'] || n._attrs['font-size'] || '16') || 16; return n.textContent.length * fs * 0.6; }
  El.prototype.getComputedTextLength = function () { return textLen(this); };
  El.prototype.getBBox = function () {
    var self = this;
    if (this.tagName === 'TEXT' || this.tagName === 'TSPAN') { var fs = parseFloat(this.style['font-size'] || this._attrs['font-size'] || '16') || 16; return { x: 0, y: 0, width: textLen(this), height: fs * 1.2 }; }
    var w = parseFloat(this._attrs['width'] || '0') || 0, h = parseFloat(this._attrs['height'] || '0') || 0;
    var minx = 0, miny = 0, maxx = w, maxy = h;
    this.childNodes.forEach(function (c) { if (c.getBBox && c.tagName !== '#text') { var b = c.getBBox(); maxx = Math.max(maxx, b.x + b.width); maxy = Math.max(maxy, b.y + b.height); } });
    return { x: minx, y: miny, width: maxx - minx, height: maxy - miny };
  };
  function defText() {
    Object.defineProperty(El.prototype, 'textContent', {
      get: function () { if (this._text != null) return this._text; var s = ''; for (var i = 0; i < this.childNodes.length; i++) s += this.childNodes[i].textContent; return s; },
      set: function (v) { this.childNodes = []; this._text = String(v); }
    });
  }
  function attrStr(n) { var s = ''; for (var k in n._attrs) s += ' ' + k + '="' + String(n._attrs[k]).replace(/"/g, '&quot;') + '"'; return s; }
  function serialize(n) {
    if (n.tagName === '#text') return String(n._text).replace(/&/g, '&amp;').replace(/</g, '&lt;');
    var tag = n.tagName.toLowerCase();
    var s = '<' + tag + attrStr(n) + '>';
    if (n._text != null) s += String(n._text).replace(/&/g, '&amp;').replace(/</g, '&lt;');
    for (var i = 0; i < n.childNodes.length; i++) s += serialize(n.childNodes[i]);
    s += '</' + tag + '>';
    return s;
  }
  defText();
  Object.defineProperty(El.prototype, 'innerHTML', { get: function () { var s = ''; for (var i = 0; i < this.childNodes.length; i++) s += serialize(this.childNodes[i]); return s; }, set: function (v) { this.childNodes = []; this._text = null; } });
  Object.defineProperty(El.prototype, 'outerHTML', { get: function () { return serialize(this); } });

  var document = new El('#document');
  document.createElement = function (t) { return new El(t); };
  document.createElementNS = function (ns, t) { return new El(t, ns); };
  document.createTextNode = function (t) { var n = new El('#text'); n.tagName = '#text'; n._text = String(t); return n; };
  document.createDocumentFragment = function () { return new El('#fragment'); };
  document.getElementById = function (id) { var all = []; walk(document, all); for (var i = 0; i < all.length; i++) if (all[i].id === id) return all[i]; return null; };
  document.body = document.appendChild(new El('body'));
  document.documentElement = document;
  document.documentElement.tagName = 'HTML';
  document.documentElement.setAttribute('lang', 'en');

  globalThis.document = document;
  globalThis.window = globalThis;
  globalThis.addEventListener = function () {};
  globalThis.removeEventListener = function () {};
  globalThis.CSSStyleSheet = function () { this.cssRules = []; };
  globalThis.CSSStyleSheet.prototype.insertRule = function (r, i) { this.cssRules.splice(i || 0, 0, r); return i || 0; };
  globalThis.CSSStyleSheet.prototype.replaceSync = function () {};
  globalThis.Node = El; globalThis.Element = El; globalThis.HTMLElement = El; globalThis.SVGElement = El; globalThis.Document = El;
  globalThis.DocumentFragment = El; globalThis.HTMLFormElement = El; globalThis.NodeFilter = {};
  globalThis.MutationObserver = function () { this.observe = function () {}; this.disconnect = function () {}; };
  globalThis.ResizeObserver = globalThis.MutationObserver;
  globalThis.matchMedia = function () { return { matches: false, addListener: function () {}, addEventListener: function () {} }; };
  globalThis.location = { href: 'about:blank', hash: '', protocol: 'about:' };
  globalThis.performance = globalThis.performance || { now: function () { return Date.now(); } };
  globalThis.document.implementation = { createHTMLDocument: function () { var d = new El('#document'); d.createElement = document.createElement; d.createElementNS = document.createElementNS; d.createTextNode = document.createTextNode; d.body = d.appendChild(new El('body')); return d; } };
  globalThis.document.addEventListener = function () {};
  globalThis.document.createComment = function (t) { var n = new El('#comment'); n._text = String(t); return n; };
  globalThis.document.createTreeWalker = function () { return { nextNode: function () { return null; } }; };
  globalThis.document.head = document.appendChild(new El('head'));
  globalThis.DOMParser = function () {};
  globalThis.DOMParser.prototype.parseFromString = function (s) { var d = globalThis.document.implementation.createHTMLDocument(); d.body.innerHTML = s; return d; };
  globalThis.navigator = { userAgent: 'far2l' };
  globalThis.requestAnimationFrame = function (cb) { cb(0); return 0; };
  globalThis.cancelAnimationFrame = function () {};
  globalThis.getComputedStyle = function () { return { getPropertyValue: function () { return ''; } }; };
  globalThis.XMLSerializer = function () {};
  globalThis.XMLSerializer.prototype.serializeToString = function (node) { return serialize(node); };
  if (!globalThis.console) globalThis.console = { log: function () {}, warn: function () {}, error: function () {}, debug: function () {} };
})();
)JS";

// Render helper: calls mermaid.render (async) and stashes the SVG on the global.
const char kRenderGlue[] = R"JS(
globalThis.__far2l_done = false;
globalThis.__far2l_svg = '';
globalThis.__far2l_err = '';
globalThis.__far2l_render = function (src, id, w) {
  globalThis.__far2l_done = false;
  globalThis.__far2l_svg = '';
  globalThis.__far2l_err = '';
  try {
    var r = globalThis.mermaid.render(id, src);
    if (r && r.then) r.then(function (o) {
      globalThis.__far2l_svg = (o && o.svg) ? o.svg : (typeof o === 'string' ? o : '');
      globalThis.__far2l_done = true;
    }, function (e) { globalThis.__far2l_err = (e && e.message) ? (e.message + ' @ ' + String(e.stack).replace(/\n/g,' | ')) : String(e); globalThis.__far2l_done = true; });
    else { globalThis.__far2l_svg = (r && r.svg) ? r.svg : String(r); globalThis.__far2l_done = true; }
  } catch (e) { globalThis.__far2l_err = (e && e.message) ? (e.message + ' @ ' + String(e.stack).replace(/\n/g,' | ')) : String(e); globalThis.__far2l_done = true; }
};
)JS";

struct Engine {
    JSRuntime *rt = nullptr;
    JSContext *ctx = nullptr;
    bool ready = false;
    bool tried = false;
};

Engine g_engine;

std::string get_global_string(JSContext *ctx, const char *name)
{
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue v = JS_GetPropertyStr(ctx, g, name);
    std::string out;
    if (JS_IsString(v)) {
        const char *s = JS_ToCString(ctx, v);
        if (s) { out = s; JS_FreeCString(ctx, s); }
    }
    JS_FreeValue(ctx, v);
    JS_FreeValue(ctx, g);
    return out;
}

void run_pending_jobs(JSRuntime *rt, int budget)
{
    for (int i = 0; i < budget; ++i) {
        JSContext *ctx = nullptr;
        int ret = JS_ExecutePendingJob(rt, &ctx);
        if (ret <= 0)
            break;
    }
}

// Evaluate `code`; on exception fills `err`.
bool eval_js(Engine &e, const char *code, const char *name, std::string &err)
{
    JSValue v = JS_Eval(e.ctx, code, std::string(code).size(), name, JS_EVAL_TYPE_GLOBAL);
    bool ok = true;
    if (JS_IsException(v)) {
        JSValue ex = JS_GetException(e.ctx);
        const char *s = JS_ToCString(e.ctx, ex);
        err = s ? s : "js exception";
        if (s) JS_FreeCString(e.ctx, s);
        JSValue st = JS_GetPropertyStr(e.ctx, ex, "stack");
        if (!JS_IsUndefined(st)) {
            const char *ss = JS_ToCString(e.ctx, st);
            if (ss) {
                err += "\n";
                err += ss;
                JS_FreeCString(e.ctx, ss);
            }
        }
        JS_FreeValue(e.ctx, st);
        JS_FreeValue(e.ctx, ex);
        ok = false;
    }
    JS_FreeValue(e.ctx, v);
    return ok;
}

// Locate the installed mermaid.min.js: exe-relative first (robust to cwd),
// then system share dirs, then cwd as last resort.
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

std::string locate_asset()
{
    std::vector<std::string> cands;
    std::string ed = exe_dir();
    if (!ed.empty()) {
        // macOS bundle: Contents/MacOS/far2l -> Contents/Resources/mermaid.min.js
        cands.push_back(ed + "/../Resources/mermaid.min.js");
        // flat install: <prefix>/bin/far2l -> <prefix>/share/far2l/mermaid.min.js
        cands.push_back(ed + "/../share/far2l/mermaid.min.js");
        cands.push_back(ed + "/mermaid.min.js");
    }
    cands.push_back("/usr/local/share/far2l/mermaid.min.js");
    cands.push_back("/usr/share/far2l/mermaid.min.js");
    cands.push_back("/usr/local/lib/far2l/mermaid.min.js");
    cands.push_back("/usr/lib/far2l/mermaid.min.js");
    cands.push_back("mermaid.min.js");
    for (const auto &c : cands) {
        std::ifstream f(c);
        if (f.good())
            return c;
    }
    return std::string();
}

bool init(Engine &e)
{
    e.rt = JS_NewRuntime();
    if (!e.rt)
        return false;
    e.ctx = JS_NewContext(e.rt);
    if (!e.ctx)
        return false;
    JS_SetMemoryLimit(e.rt, 128 * 1024 * 1024);
    JS_SetMaxStackSize(e.rt, 8 * 1024 * 1024);

    std::string err;
    if (!eval_js(e, kDomShim, "dom_shim.js", err)) {
        fprintf(stderr, "MermaidEngine: dom_shim failed: %s\n", err.c_str());
        return false;
    }

    std::string path = locate_asset();
    fprintf(stderr, "MermaidEngine: asset=%s\n", path.c_str());
    if (path.empty())
        return false;
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string js = ss.str();
    if (js.empty())
        return false;

    if (!eval_js(e, js.c_str(), "mermaid.min.js", err)) {
        fprintf(stderr, "MermaidEngine: mermaid.min.js eval failed: %s\n", err.c_str());
        return false;
    }
    if (!eval_js(e, kRenderGlue, "render_glue.js", err)) {
        fprintf(stderr, "MermaidEngine: render_glue failed: %s\n", err.c_str());
        return false;
    }

    e.ready = true;
    return true;
}

}   // namespace

bool MermaidEngine::Available()
{
    if (!g_engine.tried) {
        g_engine.tried = true;
        init(g_engine);
    }
    return g_engine.ready;
}

bool MermaidEngine::Render(const std::string &mermaid_src, const std::string &id,
                           int width_px, std::string &svg_out, std::string &err)
{
    if (!Available()) {
        err = "mermaid engine not initialized";
        return false;
    }

    // Escape the source for a JS string literal and run the render glue.
    std::string esc = "\"";
    for (char c : mermaid_src) {
        switch (c) {
            case '\"': esc += "\\\""; break;
            case '\\': esc += "\\\\"; break;
            case '\n': esc += "\\n"; break;
            case '\r': break;
            case '\t': esc += "\\t"; break;
            default: esc += c; break;
        }
    }
    esc += "\"";

    std::string call = "globalThis.__far2l_render(" + esc + ", \"" + id + "\", " +
                       std::to_string(width_px) + ");";
    if (!eval_js(g_engine, call.c_str(), "render_call.js", err))
        return false;

    // Drain the promise microtasks (mermaid uses d3/async rendering).
    for (int spin = 0; spin < 8; ++spin) {
        run_pending_jobs(g_engine.rt, 1000);
        JSValue g = JS_GetGlobalObject(g_engine.ctx);
        JSValue d = JS_GetPropertyStr(g_engine.ctx, g, "__far2l_done");
        int done = 0;
        JS_ToInt32(g_engine.ctx, &done, d);
        JS_FreeValue(g_engine.ctx, d);
        JS_FreeValue(g_engine.ctx, g);
        if (done)
            break;
    }

    std::string serr = get_global_string(g_engine.ctx, "__far2l_err");
    if (!serr.empty()) {
        err = serr;
        return false;
    }
    svg_out = get_global_string(g_engine.ctx, "__far2l_svg");
    return !svg_out.empty();
}
