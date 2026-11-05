#include "headers.hpp"
#include "MermaidEngine.hpp"
#include <quickjs.h>

#include <chrono>
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
  // classList reads and writes the class attribute, so class selectors see d3's changes.
  function classesOf(n) { return (n._attrs['class'] || '').split(/\s+/).filter(function (c) { return c; }); }
  function ClassList(el) { this._el = el; }
  ClassList.prototype.add = function () { var a = classesOf(this._el); for (var i = 0; i < arguments.length; i++) if (a.indexOf(arguments[i]) < 0) a.push(arguments[i]); this._el._attrs['class'] = a.join(' '); };
  ClassList.prototype.remove = function () { var a = classesOf(this._el); for (var i = 0; i < arguments.length; i++) { var j = a.indexOf(arguments[i]); if (j >= 0) a.splice(j, 1); } this._el._attrs['class'] = a.join(' '); };
  ClassList.prototype.contains = function (c) { return classesOf(this._el).indexOf(c) >= 0; };
  ClassList.prototype.toggle = function (c, f) { var on = (f === undefined) ? !this.contains(c) : !!f; if (on) this.add(c); else this.remove(c); return on; };

  // d3 writes inline styles through style.setProperty/removeProperty and reads
  // them through getPropertyValue; values are own string properties.
  function Style() {}
  Object.defineProperty(Style.prototype, 'setProperty', { value: function (k, v) { this[k] = String(v); } });
  Object.defineProperty(Style.prototype, 'removeProperty', { value: function (k) { var old = this[k]; delete this[k]; return old === undefined ? '' : old; } });
  Object.defineProperty(Style.prototype, 'getPropertyValue', { value: function (k) { return Object.prototype.hasOwnProperty.call(this, k) ? String(this[k]) : ''; } });

  function El(tag, ns) {
    this.tagName = String(tag).toUpperCase();
    this.nodeName = this.tagName;
    this.ns = ns || null;
    this.childNodes = [];
    this._attrs = {};
    this.style = new Style();
    this._text = null;
    this.classList = new ClassList(this);
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
  // d3 calls (namespace, localName, value); keep the conventional prefix in the name.
  var NSPREFIX = { 'http://www.w3.org/1999/xlink': 'xlink:', 'http://www.w3.org/XML/1998/namespace': 'xml:', 'http://www.w3.org/2000/xmlns/': 'xmlns:' };
  function qname(ns, local) { var p = NSPREFIX[ns]; return (p && local !== 'xmlns') ? p + local : local; }
  El.prototype.setAttributeNS = function (ns, local, v) { this.setAttribute(qname(ns, local), v); };
  El.prototype.getAttributeNS = function (ns, local) { return this.getAttribute(qname(ns, local)); };
  El.prototype.removeAttributeNS = function (ns, local) { this.removeAttribute(qname(ns, local)); };
  El.prototype.hasAttributeNS = function (ns, local) { return this.hasAttribute(qname(ns, local)); };
  function walk(node, out) { for (var i = 0; i < node.childNodes.length; i++) { var c = node.childNodes[i]; if (c.childNodes) { out.push(c); walk(c, out); } } }
  // Selector matcher for the subset mermaid uses: type, *, #id, .class, [attr] and
  // [attr op value], :first-child, :last-child, :nth-child(n|odd|even), :not(compound),
  // :scope, and the ' ' and '>' combinators in comma lists.  Anything else throws,
  // so an unsupported selector shows up in the render error instead of matching nothing.
  function attrOf(n, k) { return Object.prototype.hasOwnProperty.call(n._attrs, k) ? n._attrs[k] : null; }
  function elementSiblings(n) { var p = n.parentNode; return p ? p.childNodes.filter(function (c) { return c.tagName.charAt(0) !== '#'; }) : [n]; }
  function parseCompound(s) {
    var c = { tag: null, conds: [] }, i = 0, m;
    m = /^(\*|[a-zA-Z][\w-]*)/.exec(s);
    if (m) { if (m[1] !== '*') c.tag = m[1].toUpperCase(); i = m[1].length; }
    while (i < s.length) {
      var rest = s.slice(i);
      if ((m = /^#([\w-]+)/.exec(rest))) c.conds.push({ k: 'id', v: m[1] });
      else if ((m = /^\.([\w-]+)/.exec(rest))) c.conds.push({ k: 'cls', v: m[1] });
      else if ((m = /^\[\s*([\w:-]+)\s*(?:([~^$*|]?=)\s*(?:"([^"]*)"|'([^']*)'|([^\]\s]+)))?\s*\]/.exec(rest)))
        c.conds.push({ k: 'attr', n: m[1], op: m[2] || '', v: m[3] !== undefined ? m[3] : m[4] !== undefined ? m[4] : m[5] });
      else if ((m = /^:not\(([^()]*)\)/.exec(rest))) c.conds.push({ k: 'not', v: parseCompound(m[1]) });
      else if ((m = /^:(first-child|last-child|scope)/.exec(rest))) c.conds.push({ k: m[1] });
      else if ((m = /^:nth-child\((\d+|odd|even)\)/.exec(rest))) c.conds.push({ k: 'nth', v: m[1] });
      else throw new Error('unsupported selector: ' + s);
      i += m[0].length;
    }
    return c;
  }
  function parseComplex(s) {
    var parts = [], re = /(\s*>\s*|\s+)?((?:[^\s>\[]|\[[^\]]*\])+)/g, m, end = 0;
    s = s.trim();
    while ((m = re.exec(s)) !== null) {
      if (m.index !== end) throw new Error('unsupported selector: ' + s);
      end = m.index + m[0].length;
      var comb = parts.length === 0 ? null : (m[1] && m[1].indexOf('>') >= 0 ? '>' : ' ');
      parts.push({ comb: comb, c: parseCompound(m[2]) });
    }
    if (parts.length === 0 || end !== s.length) throw new Error('unsupported selector: ' + s);
    return parts;
  }
  var selCache = {};
  function compileSel(sel) {
    if (selCache[sel]) return selCache[sel];
    var groups = [], cur = '', depth = 0, quote = null;
    for (var i = 0; i < sel.length; i++) {
      var ch = sel[i];
      if (quote) { if (ch === quote) quote = null; }
      else if (ch === '"' || ch === "'") quote = ch;
      else if (ch === '[' || ch === '(') depth++;
      else if (ch === ']' || ch === ')') depth--;
      else if (ch === ',' && depth === 0) { groups.push(cur); cur = ''; continue; }
      cur += ch;
    }
    groups.push(cur);
    return (selCache[sel] = groups.map(parseComplex));
  }
  function matchAttr(n, d) {
    var v = attrOf(n, d.n);
    if (v === null) return false;
    switch (d.op) {
      case '': return true;
      case '=': return v === d.v;
      case '~=': return v.split(/\s+/).indexOf(d.v) >= 0;
      case '^=': return v.indexOf(d.v) === 0;
      case '$=': return v.slice(v.length - d.v.length) === d.v;
      case '*=': return v.indexOf(d.v) >= 0;
      case '|=': return v === d.v || v.indexOf(d.v + '-') === 0;
    }
    return false;
  }
  function matchCompound(n, c, scope) {
    if (!n || !n._attrs || n.tagName.charAt(0) === '#') return false;
    if (c.tag && n.tagName !== c.tag) return false;
    for (var i = 0; i < c.conds.length; i++) {
      var d = c.conds[i], idx;
      switch (d.k) {
        case 'id': if (attrOf(n, 'id') !== d.v) return false; break;
        case 'cls': if (classesOf(n).indexOf(d.v) < 0) return false; break;
        case 'attr': if (!matchAttr(n, d)) return false; break;
        case 'not': if (matchCompound(n, d.v, scope)) return false; break;
        case 'first-child': if (elementSiblings(n).indexOf(n) !== 0) return false; break;
        case 'last-child': { var sib = elementSiblings(n); if (sib.indexOf(n) !== sib.length - 1) return false; break; }
        case 'nth': idx = elementSiblings(n).indexOf(n) + 1;
          if (d.v === 'odd' ? idx % 2 !== 1 : d.v === 'even' ? idx % 2 !== 0 : idx !== +d.v) return false;
          break;
        case 'scope': if (n !== scope) return false; break;
      }
    }
    return true;
  }
  function matchFrom(n, parts, i, scope) {
    if (!matchCompound(n, parts[i].c, scope)) return false;
    if (i === 0) return true;
    if (parts[i].comb === '>') return !!n.parentNode && matchFrom(n.parentNode, parts, i - 1, scope);
    for (var p = n.parentNode; p; p = p.parentNode) if (matchFrom(p, parts, i - 1, scope)) return true;
    return false;
  }
  function matchesAny(n, groups, scope) {
    for (var g = 0; g < groups.length; g++) if (matchFrom(n, groups[g], groups[g].length - 1, scope)) return true;
    return false;
  }
  El.prototype.querySelectorAll = function (sel) {
    var groups = compileSel(String(sel)), all = [], res = [];
    walk(this, all);
    for (var i = 0; i < all.length; i++) if (matchesAny(all[i], groups, this)) res.push(all[i]);
    return res;
  };
  El.prototype.querySelector = function (sel) { var r = this.querySelectorAll(sel); return r.length ? r[0] : null; };
  El.prototype.matches = function (sel) { return matchesAny(this, compileSel(String(sel)), this); };

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
  Object.defineProperty(El.prototype, 'nodeType', { get: function () { return this.tagName === '#text' ? 3 : this.tagName === '#DOCUMENT' ? 9 : 1; } });
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
  // Attributes, plus the inline style object folded into a style attribute (d3 sets
  // text-anchor and the like through style.setProperty, not setAttribute).
  function attrStr(n) {
    var a = {}, k, st = '', s = '';
    for (k in n._attrs) a[k] = n._attrs[k];
    for (k in n.style) if (Object.prototype.hasOwnProperty.call(n.style, k) && typeof n.style[k] === 'string') st += k + ': ' + n.style[k] + '; ';
    if (st) a.style = (a.style ? a.style + ' ' : '') + st.trim();
    for (k in a) s += ' ' + k + '="' + String(a[k]).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;') + '"';
    return s;
  }
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
  // No HTML parser here: innerHTML can only carry raw text, which is what <style> and
  // <script> hold.  Other elements get no children from innerHTML.
  Object.defineProperty(El.prototype, 'innerHTML', { get: function () { var s = ''; for (var i = 0; i < this.childNodes.length; i++) s += serialize(this.childNodes[i]); return s; }, set: function (v) { this.childNodes = []; this._text = (this.tagName === 'STYLE' || this.tagName === 'SCRIPT') ? String(v) : null; } });
  Object.defineProperty(El.prototype, 'outerHTML', { get: function () { return serialize(this); } });

  var document = new El('#document');
  document.createElement = function (t) { return new El(t); };
  document.createElementNS = function (ns, t) { return new El(t, ns); };
  document.createTextNode = function (t) { var n = new El('#text'); n.tagName = '#text'; n._text = String(t); return n; };
  document.createDocumentFragment = function () { return new El('#fragment'); };
  document.getElementById = function (id) { var all = []; walk(document, all); for (var i = 0; i < all.length; i++) if (all[i].id === id) return all[i]; return null; };
  // document -> html -> (head, body).  DOMPurify (inside mermaid) only enables itself
  // when document.nodeType is 9, so the document must not alias its <html> element.
  var htmlEl = document.appendChild(new El('html'));
  htmlEl.setAttribute('lang', 'en');
  document.documentElement = htmlEl;
  document.head = htmlEl.appendChild(new El('head'));
  document.body = htmlEl.appendChild(new El('body'));

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
  globalThis.DOMParser = function () {};
  globalThis.DOMParser.prototype.parseFromString = function (s) { var d = globalThis.document.implementation.createHTMLDocument(); d.body.innerHTML = s; return d; };
  globalThis.navigator = { userAgent: 'far2l' };
  // quickjs has no timers (ELK's worker emulation uses setTimeout).  A timer is a
  // microtask, which Render drains; delays are ignored.  A timer runs only while it is
  // pending, so clearTimeout cancels it.  An exception goes to the render that scheduled
  // the timer (__far2l_fail, kRenderGlue) instead of becoming an unobserved rejection.
  globalThis.__far2l_gen = 0;
  var pendingTimers = {}, nextTimer = 1;
  globalThis.setTimeout = function (fn) {
    var args = Array.prototype.slice.call(arguments, 2), id = nextTimer++, gen = globalThis.__far2l_gen;
    pendingTimers[id] = true;
    Promise.resolve().then(function () {
      if (!pendingTimers[id]) return;
      delete pendingTimers[id];
      try { if (typeof fn === 'function') fn.apply(null, args); }
      catch (e) { globalThis.__far2l_fail(gen, e); }
    });
    return id;
  };
  globalThis.clearTimeout = function (id) { delete pendingTimers[id]; };
  globalThis.setInterval = function () { return 0; };
  globalThis.clearInterval = function () {};
  // base64 over binary strings (chars 0-255), as in the browser.
  var B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  globalThis.btoa = function (s) {
    s = String(s);
    var out = '';
    for (var i = 0; i < s.length; i += 3) {
      var has1 = i + 1 < s.length, has2 = i + 2 < s.length;
      var a = s.charCodeAt(i), b = has1 ? s.charCodeAt(i + 1) : 0, c = has2 ? s.charCodeAt(i + 2) : 0;
      if (a > 255 || b > 255 || c > 255) throw new Error('btoa: invalid character');
      var n = (a << 16) | (b << 8) | c;
      out += B64[(n >> 18) & 63] + B64[(n >> 12) & 63] + (has1 ? B64[(n >> 6) & 63] : '=') + (has2 ? B64[n & 63] : '=');
    }
    return out;
  };
  globalThis.atob = function (s) {
    s = String(s).replace(/[\t\n\f\r ]/g, '').replace(/=+$/, '');
    var out = '', buf = 0, bits = 0;
    for (var i = 0; i < s.length; i++) {
      var v = B64.indexOf(s.charAt(i));
      if (v < 0) throw new Error('atob: invalid character');
      buf = ((buf << 6) | v) & 0xffff;
      bits += 6;
      if (bits >= 8) { bits -= 8; out += String.fromCharCode((buf >> bits) & 255); }
    }
    return out;
  };
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
// The shim has no HTML parser, so mermaid must not build HTML labels (htmlLabels false
// draws labels as SVG text) and must skip its DOMPurify pass (securityLevel 'loose').
// The SVG is only converted to terminal cells, never shown in a page.
globalThis.mermaid.initialize({ startOnLoad: false, securityLevel: 'loose', htmlLabels: false, flowchart: { htmlLabels: false } });
globalThis.__far2l_done = false;
globalThis.__far2l_svg = '';
globalThis.__far2l_err = '';
// Records a failure of the render with generation `gen` (a rejected promise, or a shim
// timer that threw).  A stale generation is dropped.
globalThis.__far2l_fail = function (gen, e) {
  if (gen !== globalThis.__far2l_gen) return;
  globalThis.__far2l_err = (e && e.message) ? (e.message + ' @ ' + String(e.stack).replace(/\n/g,' | ')) : String(e);
  globalThis.__far2l_done = true;
};
// Each call takes a new generation.  Callbacks of an earlier call that Render gave up on
// (drain budget exhausted) stay in the job queue; they see a stale generation and drop
// their result instead of overwriting this call's globals.
globalThis.__far2l_render = function (src, id, w) {
  var gen = ++globalThis.__far2l_gen;
  globalThis.__far2l_done = false;
  globalThis.__far2l_svg = '';
  globalThis.__far2l_err = '';
  try {
    var r = globalThis.mermaid.render(id, src);
    if (r && r.then) r.then(function (o) {
      if (gen !== globalThis.__far2l_gen) return;
      globalThis.__far2l_svg = (o && o.svg) ? o.svg : (typeof o === 'string' ? o : '');
      globalThis.__far2l_done = true;
    }, function (e) { globalThis.__far2l_fail(gen, e); });
    else { globalThis.__far2l_svg = (r && r.svg) ? r.svg : String(r); globalThis.__far2l_done = true; }
  } catch (e) { globalThis.__far2l_fail(gen, e); }
};
)JS";

struct Engine {
    JSRuntime *rt = nullptr;
    JSContext *ctx = nullptr;
    bool ready = false;
    bool tried = false;
};

Engine g_engine;

// Limits for one Render call.  The JS stack budget is measured down from the stack pointer
// at each Render (JS_UpdateStackTop), so it holds wherever the viewer calls from; 1 MB is the
// quickjs default and sits well inside the thread stack.  The wall-clock budget is 5 seconds:
// quickjs polls the interrupt handler while JS runs, so a render past the deadline stops,
// Render returns false (the fence stays raw), and the engine is rebuilt for the next render.
const size_t kJsStackBytes = 1024 * 1024;
const std::chrono::seconds kRenderBudget(5);
const char kTimeoutErr[] = "mermaid render timed out";

std::chrono::steady_clock::time_point g_deadline;
bool g_timed_out = false;

// True once the deadline of the current Render has passed; latches g_timed_out.
bool deadline_passed()
{
    if (!g_timed_out && std::chrono::steady_clock::now() >= g_deadline)
        g_timed_out = true;
    return g_timed_out;
}

int interrupt_at_deadline(JSRuntime *, void *)
{
    return deadline_passed() ? 1 : 0;
}

// Arms the deadline and the stack limit for one Render call; the interrupt handler is
// removed when the call returns.
class RenderScope {
public:
    explicit RenderScope(JSRuntime *rt) : m_rt(rt)
    {
        g_timed_out = false;
        g_deadline = std::chrono::steady_clock::now() + kRenderBudget;
        JS_UpdateStackTop(rt);
        JS_SetInterruptHandler(rt, interrupt_at_deadline, nullptr);
    }
    ~RenderScope() { JS_SetInterruptHandler(m_rt, nullptr, nullptr); }

private:
    JSRuntime *m_rt;
};

// The error for a JS step that failed: the timeout message when the deadline stopped it.
std::string limited_error(const std::string &js_err)
{
    return g_timed_out ? std::string(kTimeoutErr) : js_err;
}

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

// Reads and frees the pending exception of `ctx`: its message, then the stack on a new line.
std::string take_exception(JSContext *ctx)
{
    JSValue ex = JS_GetException(ctx);
    const char *s = JS_ToCString(ctx, ex);
    std::string out = s ? s : "js exception";
    if (s) JS_FreeCString(ctx, s);
    JSValue st = JS_GetPropertyStr(ctx, ex, "stack");
    if (!JS_IsUndefined(st)) {
        const char *ss = JS_ToCString(ctx, st);
        if (ss) {
            out += "\n";
            out += ss;
            JS_FreeCString(ctx, ss);
        }
    }
    JS_FreeValue(ctx, st);
    JS_FreeValue(ctx, ex);
    return out;
}

// Runs up to `budget` pending jobs.  A job that throws stops the drain and its exception
// is returned as text; empty when the jobs ran cleanly.
std::string run_pending_jobs(Engine &e, int budget)
{
    for (int i = 0; i < budget; ++i) {
        JSContext *ctx = nullptr;
        int ret = JS_ExecutePendingJob(e.rt, &ctx);
        if (ret == 0)
            break;
        if (ret < 0)
            return take_exception(ctx ? ctx : e.ctx);
    }
    return std::string();
}

// Evaluate `code`; on exception fills `err`.
bool eval_js(Engine &e, const char *code, const char *name, std::string &err)
{
    JSValue v = JS_Eval(e.ctx, code, std::string(code).size(), name, JS_EVAL_TYPE_GLOBAL);
    bool ok = true;
    if (JS_IsException(v)) {
        err = take_exception(e.ctx);
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
    JS_SetMaxStackSize(e.rt, kJsStackBytes);

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

// Frees the runtime after a render the deadline stopped.  An interrupted render leaves JS
// state behind (queued jobs, half-built DOM) that later renders do not recover from, so
// the next Available() call initializes a fresh engine.
static void discard_engine(Engine &e)
{
    if (e.ctx)
        JS_FreeContext(e.ctx);
    if (e.rt)
        JS_FreeRuntime(e.rt);
    e = Engine();
}

// Runs one render in the initialized engine.  RenderScope arms the limits for the whole call.
static bool render_in_engine(const std::string &mermaid_src, const std::string &id,
                             int width_px, std::string &svg_out, std::string &err)
{
    RenderScope scope(g_engine.rt);

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

    // width_px is not used: the glue ignores its width argument and mermaid sizes the SVG itself.
    std::string call = "globalThis.__far2l_render(" + esc + ", \"" + id + "\", " +
                       std::to_string(width_px) + ");";
    if (!eval_js(g_engine, call.c_str(), "render_call.js", err)) {
        err = limited_error(err);
        return false;
    }

    // Drain the promise microtasks (mermaid uses d3/async rendering).  The job budget is 8000
    // jobs and the deadline caps the time; a render that needs more gives up, and its late
    // callbacks are dropped by generation.
    bool done = false;
    for (int spin = 0; spin < 8 && !done && !deadline_passed(); ++spin) {
        std::string job_err = run_pending_jobs(g_engine, 1000);
        if (!job_err.empty()) {
            err = limited_error(job_err);
            return false;
        }
        JSValue g = JS_GetGlobalObject(g_engine.ctx);
        JSValue d = JS_GetPropertyStr(g_engine.ctx, g, "__far2l_done");
        int flag = 0;
        JS_ToInt32(g_engine.ctx, &flag, d);
        JS_FreeValue(g_engine.ctx, d);
        JS_FreeValue(g_engine.ctx, g);
        done = flag != 0;
    }

    std::string serr = get_global_string(g_engine.ctx, "__far2l_err");
    if (!serr.empty()) {
        err = serr;
        return false;
    }
    if (!done) {
        err = g_timed_out ? std::string(kTimeoutErr) : "mermaid render did not complete";
        return false;
    }
    svg_out = get_global_string(g_engine.ctx, "__far2l_svg");
    if (svg_out.empty()) {
        err = "mermaid render produced no svg";
        return false;
    }
    return true;
}

bool MermaidEngine::Render(const std::string &mermaid_src, const std::string &id,
                           int width_px, std::string &svg_out, std::string &err)
{
    err.clear();
    if (!Available()) {
        err = "mermaid engine not initialized";
        return false;
    }
    const bool ok = render_in_engine(mermaid_src, id, width_px, svg_out, err);
    if (g_timed_out)
        discard_engine(g_engine);
    return ok;
}
