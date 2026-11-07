#!/bin/sh
# md-open.sh: show a Markdown file as a web page (far2l Alt-F3; macOS first).
#
#   md-open.sh [options] <file.md>
#     --out <page.html>   write the page there
#     --no-open           make the page, do not open it
#     --width 60|70|80    start width of the text column, in percent
#     --theme light|dark|auto
#     --browser auto|firefox|chrome|safari|<absolute path>
#     --no-cache          build the page again
#   md-open.sh --init-config
#   md-open.sh --print-association
#
# Writes one page (the Markdown text and marked inline; mermaid from a pinned
# CDN script with an SRI hash) and opens it with the system opener. The page
# goes to ${TMPDIR:-/tmp}/far2l-md-<pid>.html unless --out names another file.
# marked.min.js is read from this script's folder, or from MARKED_JS. The GitHub
# CSS and the highlight.js files are read from this script's folder too.
#
# Defaults come from ${XDG_CONFIG_HOME:-$HOME/.config}/far2l/settings/md-view.ini,
# section [View], keys width, theme and browser. A flag beats the file. No value
# from the file or a flag is run as code; it is used only when it equals an
# accepted word, or (browser only) when it is an absolute path to a program.

LC_ALL=C
export LC_ALL

self_dir=$(cd "$(dirname -- "$0")" && pwd) || exit 2
ini=${XDG_CONFIG_HOME:-$HOME/.config}/far2l/settings/md-view.ini
cr=$(printf '\r')

usage() {
    echo 'usage: md-open.sh [--out <page.html>] [--no-open] [--width 60|70|80]' >&2
    echo '                  [--theme light|dark|auto] [--browser B] [--no-cache] <file.md>' >&2
    echo '       md-open.sh --init-config' >&2
    echo '       md-open.sh --print-association' >&2
    exit 2
}

# trim TEXT: sets trimmed to TEXT without the spaces and tabs at both ends.
trim() {
    trimmed=$1
    trimmed=${trimmed#"${trimmed%%[! 	]*}"}
    trimmed=${trimmed%"${trimmed##*[! 	]}"}
}

# browser_ok VALUE: true if VALUE is an accepted browser= value. An absolute
# path must name an executable file, or on macOS a folder that ends in ".app".
browser_ok() {
    case $1 in
        auto|firefox|chrome|safari)
            return 0 ;;
        /*.app|/*.app/)
            [ "$(uname -s)" = Darwin ] && [ -d "$1" ] && return 0
            [ -f "$1" ] && [ -x "$1" ] ;;
        /*)
            [ -f "$1" ] && [ -x "$1" ] ;;
        *)
            return 1 ;;
    esac
}

# read_ini: reads md-view.ini, section [View]. Sets cfg_width, cfg_theme and
# cfg_browser to the last valid value of each key; sets cfg_browser_bad=1 if a
# browser= line has a value that is not accepted. Other values are ignored.
read_ini() {
    cfg_width=
    cfg_theme=
    cfg_browser=
    cfg_browser_bad=0
    [ -f "$ini" ] && [ -r "$ini" ] || return 0
    section=
    while IFS= read -r line || [ -n "$line" ]; do
        line=${line%"$cr"}
        trim "$line"
        line=$trimmed
        case $line in
            ''|'#'*|';'*)
                continue ;;
            '['*)
                section=$line
                continue ;;
            *=*) ;;
            *)
                continue ;;
        esac
        [ "$section" = '[View]' ] || continue
        trim "${line%%=*}"
        key=$trimmed
        trim "${line#*=}"
        val=$trimmed
        case $key in
            width)
                case $val in
                    60|70|80) cfg_width=$val ;;
                esac ;;
            theme)
                case $val in
                    light|dark|auto) cfg_theme=$val ;;
                esac ;;
            browser)
                if browser_ok "$val"; then
                    cfg_browser=$val
                else
                    cfg_browser_bad=1
                fi ;;
        esac
    done < "$ini"
}

# browsers_found: the browser words that this system has.
browsers_found() {
    found=
    if command -v firefox >/dev/null 2>&1 || [ -d /Applications/Firefox.app ]; then
        found="$found firefox"
    fi
    if command -v google-chrome >/dev/null 2>&1 || command -v chromium >/dev/null 2>&1 \
        || [ -d '/Applications/Google Chrome.app' ]; then
        found="$found chrome"
    fi
    if [ -d /Applications/Safari.app ]; then
        found="$found safari"
    fi
    printf '%s\n' "${found# }"
}

# init_config: writes a commented sample md-view.ini if there is no file. It
# never replaces a file, a link or anything else at that path.
init_config() {
    if [ -e "$ini" ] || [ -L "$ini" ]; then
        echo "md-open: $ini exists, not changed"
        exit 0
    fi
    mkdir -p "$(dirname -- "$ini")" || { echo "md-open: cannot make the folder of $ini" >&2; exit 1; }
    list=$(browsers_found)
    # noclobber: the shell refuses to replace a file made since the test above.
    set -C
    cat > "$ini" <<EOF || { echo "md-open: cannot write $ini" >&2; exit 1; }
# Defaults for the far2l Markdown browser page (md-open.sh, Alt-F3).
# A flag on the command line beats a value here. A line that starts with
# "#" or ";" is a comment. A value that is not accepted is ignored.
#
# width: start width of the text column, in percent: 60, 70 or 80.
# theme: light, dark or auto (auto follows the system).
# browser: auto, firefox, chrome, safari, or an absolute path to a program.
# Browsers found on this system: ${list:-none}

[View]
width=70
theme=auto
browser=auto
EOF
    set +C
    echo "$ini"
    exit 0
}

print_association() {
    exe=$self_dir/md-open.sh
    # far2l escapes the file name in !.! before the shell reads the command, so
    # !.! stays unquoted. The script path is quoted only when the shell needs it.
    case $exe in
        *[!A-Za-z0-9_./+:@%,=-]*)
            q=$(printf '%s' "$exe" | sed -e 's/["$`\\]/\\&/g')
            cmd="sh \"$q\" !.!" ;;
        *)
            cmd="sh $exe !.!" ;;
    esac
    cat <<EOF
far2l file association for Markdown (add it once):

  Mask:                              *.md
  View command (used for Alt+F3):    $cmd

far2l already escapes the file name in !.!, so the command does not quote it.
The command above shows this copy's path; print the entry from the installed copy.

Steps in far2l:
  1. Press F9, open the Commands menu and choose File associations.
  2. Press Ins to add an entry.
  3. In "A file mask or several file masks", type: *.md
  4. Tick "View command (used for Alt+F3)" and type: $cmd
  5. Press OK. Alt+F3 on a .md file now opens it in the browser.

Defaults file (md-open.sh --init-config writes a commented sample):
  $ini
  [View]
  width=60|70|80          (default 70)
  theme=light|dark|auto   (default auto)
  browser=auto|firefox|chrome|safari|<absolute path to a program>
Flags for one run: --width N, --theme T, --browser B, --no-cache.
EOF
}

# open_page PAGE: opens PAGE with the browser in $browser. Each command runs
# with exec in a subshell, with PAGE as one argument, so the exit status shows.
open_page() {
    b=$browser
    if [ "$(uname -s)" = Darwin ]; then
        case $b in
            firefox) app=Firefox ;;
            chrome) app='Google Chrome' ;;
            safari) app=Safari ;;
            /*.app|/*.app/) [ -d "$b" ] && app=$b || app= ;;
            *) app= ;;
        esac
        if [ -n "$app" ]; then
            ( exec open -a "$app" "$1" ) || { echo 'md-open: open failed' >&2; exit 1; }
            return 0
        fi
    else
        case $b in
            firefox)
                command -v firefox >/dev/null 2>&1 && b=firefox || b=auto ;;
            chrome)
                if command -v google-chrome >/dev/null 2>&1; then
                    b=google-chrome
                elif command -v chromium >/dev/null 2>&1; then
                    b=chromium
                else
                    b=auto
                fi ;;
            safari)
                b=auto ;;
        esac
        case $b in
            auto|/*) ;;
            *)
                ( exec "$b" "$1" ) || { echo "md-open: $b failed" >&2; exit 1; }
                return 0 ;;
        esac
    fi
    case $b in
        /*)
            ( exec "$b" "$1" ) || { echo 'md-open: the browser failed' >&2; exit 1; }
            return 0 ;;
    esac
    # auto: a single-word $BROWSER, then open, then xdg-open.
    if [ -n "${BROWSER:-}" ]; then
        case $BROWSER in
            -*|*[!A-Za-z0-9_./+-]*)
                echo 'md-open: BROWSER is not a single command name, ignored' >&2 ;;
            *)
                if command -v "$BROWSER" >/dev/null 2>&1; then
                    ( exec "$BROWSER" "$1" ) || { echo 'md-open: $BROWSER failed' >&2; exit 1; }
                    return 0
                fi ;;
        esac
    fi
    if command -v open >/dev/null 2>&1; then
        ( exec open "$1" ) || { echo 'md-open: open failed' >&2; exit 1; }
    elif command -v xdg-open >/dev/null 2>&1; then
        ( exec xdg-open "$1" ) || { echo 'md-open: xdg-open failed' >&2; exit 1; }
    else
        echo "$1"
    fi
}

file=
out=
do_open=1
do_init=0
no_cache=0
flag_width=
flag_theme=
flag_browser=
flag_browser_set=0
while [ $# -gt 0 ]; do
    case $1 in
        --print-association)
            print_association
            exit 0 ;;
        --init-config)
            do_init=1 ;;
        --no-open)
            do_open=0 ;;
        --no-cache)
            no_cache=1 ;;
        --width)
            [ $# -ge 2 ] || usage
            case $2 in
                60|70|80) flag_width=$2 ;;
                *) usage ;;
            esac
            shift ;;
        --theme)
            [ $# -ge 2 ] || usage
            case $2 in
                light|dark|auto) flag_theme=$2 ;;
                *) usage ;;
            esac
            shift ;;
        --browser)
            [ $# -ge 2 ] || usage
            flag_browser=$2
            flag_browser_set=1
            shift ;;
        --out)
            [ $# -ge 2 ] || usage
            out=$2
            shift ;;
        --)
            # After "--" the one argument is the file name, even if it starts with "-".
            shift
            [ -z "$file" ] && [ $# -eq 1 ] || usage
            file=$1
            break ;;
        -*)
            # A file whose name starts with "-" still works when it exists.
            { [ -z "$file" ] && [ -f "$1" ]; } || usage
            file=$1 ;;
        *)
            [ -z "$file" ] || usage
            file=$1 ;;
    esac
    shift
done

[ "$do_init" = 1 ] && init_config

[ -n "$file" ] || usage

# Resolve the start values once: a flag, else the file, else the built-in default.
read_ini
forced=0
width=${flag_width:-${cfg_width:-70}}
theme=${flag_theme:-${cfg_theme:-auto}}
{ [ -n "$flag_width" ] || [ -n "$flag_theme" ]; } && forced=1
if [ "$flag_browser_set" = 1 ]; then
    if browser_ok "$flag_browser"; then
        browser=$flag_browser
    else
        echo 'md-open: --browser is not auto, firefox, chrome, safari or a program path; using auto' >&2
        browser=auto
    fi
else
    if [ -z "$cfg_browser" ] && [ "$cfg_browser_bad" = 1 ]; then
        echo "md-open: browser= in $ini is not accepted; using auto" >&2
    fi
    browser=${cfg_browser:-auto}
fi
# Only accepted words reach the page.
case $width in 60|70|80) ;; *) width=70 ;; esac
case $theme in light|dark|auto) ;; *) theme=auto ;; esac
if [ "$forced" = 1 ]; then
    html_attrs="lang=\"en\" data-start-width=\"$width\" data-start-theme=\"$theme\" data-forced=\"1\""
else
    html_attrs="lang=\"en\" data-start-width=\"$width\" data-start-theme=\"$theme\""
fi
if [ ! -f "$file" ] || [ ! -r "$file" ]; then
    echo "md-open: cannot read $file" >&2
    usage
fi

if [ -n "$out" ]; then
    page=$out
else
    tmp_dir=${TMPDIR:-/tmp}
    tmp_dir=${tmp_dir%/}
    page=$tmp_dir/far2l-md-$$.html
fi
if [ -d "$page" ] || [ ! -d "$(dirname -- "$page")" ]; then
    echo "md-open: cannot write the page $page (its folder must exist)" >&2
    exit 2
fi

marked_js=${MARKED_JS:-$self_dir/marked.min.js}
if [ ! -r "$marked_js" ]; then
    echo 'md-open: cannot find marked.min.js' >&2
    exit 3
fi
# The page inlines these vendored files, so each must be there before a page is written.
for f in github-markdown-light.css github-markdown-dark.css hljs-github.min.css \
         hljs-github-dark.min.css highlight.min.js; do
    if [ ! -f "$self_dir/$f" ] || [ ! -r "$self_dir/$f" ]; then
        echo "md-open: cannot find $f" >&2
        exit 3
    fi
done

# Folder of the Markdown file as a file:// URL for <base href>.
dir=$(cd "$(dirname -- "$file")" && pwd) || exit 2
case $dir in
    */) ;;
    *) dir=$dir/ ;;
esac
base_url=file://$(printf '%s' "$dir" | sed -e 's/%/%25/g' -e 's/ /%20/g' -e 's/#/%23/g' \
    -e 's/?/%3F/g' -e 's/"/%22/g' -e "s/'/%27/g" -e 's/</%3C/g' -e 's/>/%3E/g' -e 's/&/%26/g')

title=$(basename -- "$file" | sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g' -e 's/"/\&quot;/g')
bom=$(printf '\357\273\277')

{
    printf '<!doctype html>\n<html %s>\n' "$html_attrs"
    cat <<'HTML'
<head>
<meta charset="utf-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'unsafe-inline' https://cdn.jsdelivr.net; style-src 'unsafe-inline'; img-src data: file:">
<meta name="viewport" content="width=device-width, initial-scale=1">
HTML
    printf '<meta name="far2l-md-key" content="w=%s;t=%s;f=%s">\n' "$width" "$theme" "$forced"
    printf '<base href="%s">\n' "$base_url"
    printf '<title>%s</title>\n' "$title"
    # The vendored files go in as they are. Each pair is switched by its media
    # attribute; the page script sets it, and the values here are the start.
    printf '<style id="gh-light" media="all">\n'
    cat "$self_dir/github-markdown-light.css"
    printf '\n</style>\n<style id="gh-dark" media="not all">\n'
    cat "$self_dir/github-markdown-dark.css"
    printf '\n</style>\n<style id="hl-light" media="all">\n'
    cat "$self_dir/hljs-github.min.css"
    printf '\n</style>\n<style id="hl-dark" media="not all">\n'
    cat "$self_dir/hljs-github-dark.min.css"
    printf '\n</style>\n'
    cat <<'HTML'
<style id="page">
body { margin: 0; padding: 2em 16px; }
#content { max-width: none; margin: 0 auto; box-sizing: border-box }
html[data-width="60"] #content { width: 60% }
html[data-width="70"] #content { width: 70% }
html[data-width="80"] #content { width: 80% }
@media (max-width: 699px) {
  html[data-width] #content { width: 100% }
  #md-gear, #md-pop { display: none }
}
pre.mermaid { background: none; }
pre.mermaid[data-processed] { text-align: center; overflow-x: auto; }
.mermaid-error { color: #cf222e; color: light-dark(#cf222e, #ff7b72); font-size: 0.85em; text-align: left; }
.markdown-body pre code.hljs { padding: 0; background: transparent; }
#md-gear { position: fixed; top: 8px; right: 8px; z-index: 20; box-sizing: border-box; width: 28px; height: 28px; margin: 0; padding: 0; border: 1px solid GrayText; border-radius: 6px; background: Canvas; color: CanvasText; font: 16px/1 sans-serif; cursor: pointer; opacity: .45 }
#md-gear:hover, #md-gear:focus { opacity: 1 }
#md-pop { position: fixed; top: 44px; right: 8px; z-index: 21; box-sizing: border-box; width: 264px; max-width: calc(100vw - 16px); padding: 10px; border: 1px solid GrayText; border-radius: 6px; background: Canvas; color: CanvasText; box-shadow: 0 4px 16px rgba(0,0,0,.25); font: 13px/1.4 sans-serif }
#md-pop p { margin: 6px 0 }
#md-pop .md-row { display: flex; flex-wrap: wrap; align-items: center; gap: 4px; margin: 4px 0 }
#md-pop .md-row > span { width: 48px }
#md-pop button { font: inherit; min-width: 44px; padding: 2px 6px; border: 1px solid GrayText; border-radius: 4px; background: ButtonFace; color: ButtonText; cursor: pointer }
#md-pop button[aria-pressed="true"] { border-width: 2px; font-weight: 600 }
#md-pop .md-notice { color: #cf222e; color: light-dark(#cf222e, #ff7b72) }
#md-pop pre.md-paste { margin: 4px 0 8px; padding: 6px; border: 1px solid GrayText; border-radius: 4px; background: none; font: 12px/1.4 ui-monospace, Menlo, monospace; white-space: pre; overflow-x: auto }
@media print { #md-gear, #md-pop { display: none !important } }
</style>
</head>
<body class="markdown-body">
<button id="md-gear" type="button" aria-label="Page settings" aria-expanded="false" aria-controls="md-pop">&#9881;</button>
<div id="md-pop" hidden></div>
<main id="content" class="markdown-body"></main>
<script type="text/markdown" id="md">
HTML
    # The text cannot end its script element or open a comment there. Backslashes
    # are doubled first, then "</" becomes "<\/" and "<!--" becomes "<\!--"; the
    # page undoes the steps in reverse order. A BOM and CRs go too.
    sed -e "1s/^$bom//" -e "s/$cr\$//" -e 's/\\/\\\\/g' -e 's|</|<\\/|g' -e 's|<!--|<\\!--|g' < "$file"
    printf '\n</script>\n<script>\n'
    cat "$marked_js"
    printf '\n</script>\n<script>\n'
    cat "$self_dir/highlight.min.js"
    printf '\n</script>\n<script>\n'
    cat <<'JS'
(function () {
  'use strict';
  var MERMAID_URL = 'https://cdn.jsdelivr.net/npm/mermaid@12.3.0/dist/mermaid.min.js';
  var MERMAID_SRI = 'sha384-HHW54r639fK42Ip0coGF3mz8oSiGn0eEoWNTLs75X7H63pLSDBa0Tqs0RwGmLlXE';
  var root = document.documentElement;
  var themeQuery = window.matchMedia('(prefers-color-scheme: dark)');
  var items = [];
  var mermaidReady = false;
  // Mermaid draws one batch at a time. drawChain runs the batches in order, and
  // drawQueued is set while a batch waits and has not read the theme yet.
  var drawChain = Promise.resolve();
  var drawQueued = false;
  var storageBlocked = false;
  var forced = root.getAttribute('data-forced') === '1';
  var WIDTHS = ['60', '70', '80'];
  var THEMES = ['light', 'dark', 'auto'];
  var KEY_WIDTH = 'far2l-md-width';
  var KEY_THEME = 'far2l-md-theme';
  var gear = document.getElementById('md-gear');
  var pop = document.getElementById('md-pop');
  var widthButtons = {};
  var themeButtons = {};
  var notice = null;
  var paste = null;
  function esc(s) {
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
  }
  // Undo the escapes of the embedded text, in the reverse of the shell's order.
  var src = document.getElementById('md').textContent
    .split('<\\!--').join('<!--')
    .split('<\\/').join('</')
    .split('\\\\').join('\\');
  // A start value is used only if it is an accepted word.
  function accepted(v, words, dflt) {
    return words.indexOf(v) >= 0 ? v : dflt;
  }
  // Each storage call is inside try. A failed read or write sets storageBlocked,
  // and the popover then shows a notice. The page works without storage.
  function storageGet(key) {
    try { return window.localStorage.getItem(key); } catch (e) { storageBlocked = true; return null; }
  }
  function storageSet(key, value) {
    try { window.localStorage.setItem(key, value); } catch (e) { storageBlocked = true; }
  }
  function storageRemove(key) {
    try { window.localStorage.removeItem(key); } catch (e) { storageBlocked = true; }
  }
  // The start values come from md-view.ini or a flag. Each is used only if accepted.
  var startWidth = accepted(root.getAttribute('data-start-width'), WIDTHS, '70');
  var startTheme = accepted(root.getAttribute('data-start-theme'), THEMES, 'auto');
  // Sets the used width and theme: the saved choice, unless data-forced is 1, then the start value.
  function load() {
    var w = startWidth;
    var t = startTheme;
    if (!forced) {
      w = accepted(storageGet(KEY_WIDTH), WIDTHS, startWidth);
      t = accepted(storageGet(KEY_THEME), THEMES, startTheme);
    }
    root.setAttribute('data-width', w);
    root.setAttribute('data-theme', t);
  }
  load();
  // The choice in data-theme: light, dark or auto.
  function themeSetting() {
    var t = root.getAttribute('data-theme');
    return t === 'light' || t === 'dark' ? t : 'auto';
  }
  // The theme in use, dark or light. Auto follows the system.
  function usedTheme() {
    var t = themeSetting();
    if (t === 'auto') return themeQuery.matches ? 'dark' : 'light';
    return t;
  }
  function setMedia(id, value) {
    var el = document.getElementById(id);
    if (el) el.setAttribute('media', value);
  }
  // Enables the GitHub style and the highlight theme of the used theme, sets
  // color-scheme, and draws the diagrams again in that theme.
  function applyTheme() {
    var dark = usedTheme() === 'dark';
    setMedia('gh-light', dark ? 'not all' : 'all');
    setMedia('gh-dark', dark ? 'all' : 'not all');
    setMedia('hl-light', dark ? 'not all' : 'all');
    setMedia('hl-dark', dark ? 'all' : 'not all');
    root.style.colorScheme = dark ? 'dark' : 'light';
    if (mermaidReady) redraw(items);
  }
  function onSystemTheme() {
    if (themeSetting() === 'auto') applyTheme();
  }
  if (themeQuery.addEventListener) {
    themeQuery.addEventListener('change', onSystemTheme);
  } else if (themeQuery.addListener) {
    themeQuery.addListener(onSystemTheme);
  }
  marked.use({
    renderer: {
      // Raw HTML in the file is shown as text, never as markup.
      html: function (token) { return esc(token.text); },
      // A mermaid fence becomes a diagram block; other fences stay plain code.
      code: function (token) {
        if ((token.lang || '').split(/\s+/)[0] !== 'mermaid') return false;
        return '<pre class="mermaid">' + esc(token.text) + '</pre>\n';
      }
    }
  });
  var content = document.getElementById('content');
  content.innerHTML = marked.parse(src);
  // A link with a script scheme loses its target. This runs after parsing, so
  // entity-encoded schemes are caught as well.
  Array.prototype.forEach.call(content.querySelectorAll('a[href]'), function (a) {
    if (/^(javascript|vbscript|data):/i.test(a.getAttribute('href').replace(/[\x00-\x20]/g, ''))) {
      a.removeAttribute('href');
    }
  });
  // Code with a known language gets highlight.js. A block with no language, with
  // an unknown one, or a mermaid block stays plain. An error leaves it plain.
  Array.prototype.forEach.call(content.querySelectorAll('pre > code'), function (code) {
    var m = /(?:^|\s)language-(\S+)/.exec(code.className || '');
    if (!m || m[1] === 'mermaid') return;
    try {
      if (window.hljs && hljs.getLanguage(m[1])) hljs.highlightElement(code);
    } catch (e) {
      // The plain block stays.
    }
  });

  // The text is shown now. Mermaid is added by a script element created here, so
  // a slow or blocked CDN cannot hide the text. Its load or its error decides how
  // the diagrams are shown.
  items = Array.prototype.map.call(content.querySelectorAll('pre.mermaid'), function (pre) {
    return { pre: pre, src: pre.textContent };
  });
  applyTheme();
  buildPopover();
  wireControls();
  if (!items.length) return;
  var s = document.createElement('script');
  s.src = MERMAID_URL;
  s.integrity = MERMAID_SRI;
  s.crossOrigin = 'anonymous';
  s.onload = function () {
    if (typeof mermaid === 'undefined') {
      items.forEach(function (it) { showCode(it.pre, it.src, null); });
    } else {
      mermaidReady = true;
      redraw(items);
    }
  };
  s.onerror = function () {
    items.forEach(function (it) { showCode(it.pre, it.src, null); });
  };
  document.head.appendChild(s);

  // Shows a diagram's source as a code block, with the error text when there is one.
  function showCode(pre, text, err) {
    pre.removeAttribute('class');
    pre.removeAttribute('data-processed');
    pre.textContent = '';
    var code = document.createElement('code');
    code.className = 'language-mermaid';
    code.textContent = text;
    pre.appendChild(code);
    if (err) {
      var msg = document.createElement('div');
      msg.className = 'mermaid-error';
      msg.textContent = 'mermaid: ' + (err && err.message ? err.message : String(err));
      pre.appendChild(msg);
    }
  }

  function drawDiagrams(list) {
    mermaid.initialize({ startOnLoad: false, securityLevel: 'strict', theme: usedTheme() === 'dark' ? 'dark' : 'default' });
    // Each diagram is parsed alone first, so a broken one shows its error and the
    // others still draw.
    var parsed = list.map(function (it) {
      return mermaid.parse(it.src).then(function () { return it; }, function (err) {
        showCode(it.pre, it.src, err);
        return null;
      });
    });
    return Promise.all(parsed).then(function (results) {
      var good = results.filter(Boolean);
      if (!good.length) return;
      return mermaid.run({ nodes: good.map(function (it) { return it.pre; }) }).catch(function (e) {
        console.error(e);
        good.forEach(function (it) {
          if (!it.pre.querySelector('svg')) showCode(it.pre, it.src, null);
        });
      });
    }).catch(function (e) { console.error(e); });
  }

  // Draws each diagram again in the used theme. The batches run one after the
  // other, so a call that overlaps waits for the one before it. A batch that is
  // waiting and has not started is not added again: it reads the theme when it
  // runs. A batch gets the source text back and loses its earlier drawing.
  function redraw(list) {
    if (drawQueued) return;
    drawQueued = true;
    drawChain = drawChain.then(function () {
      drawQueued = false;
      list.forEach(function (it) {
        it.pre.className = 'mermaid';
        it.pre.removeAttribute('data-processed');
        it.pre.textContent = it.src;
      });
      return drawDiagrams(list);
    }).catch(function (e) { console.error(e); });
  }

  // The popover is made with DOM calls, and its text is set with textContent.
  function makeButton(text, onClick) {
    var b = document.createElement('button');
    b.type = 'button';
    b.textContent = text;
    b.addEventListener('click', onClick);
    return b;
  }
  function makeRow(name, buttons) {
    var row = document.createElement('div');
    row.className = 'md-row';
    var label = document.createElement('span');
    label.textContent = name;
    row.appendChild(label);
    buttons.forEach(function (b) { row.appendChild(b); });
    return row;
  }
  // The three lines of md-view.ini for the current choice.
  function pasteText() {
    return ['[View]', 'width=' + root.getAttribute('data-width'), 'theme=' + themeSetting()].join('\n');
  }
  // Shows the current choice on the buttons, the notice and the paste block.
  function refresh() {
    var w = root.getAttribute('data-width');
    var t = themeSetting();
    WIDTHS.forEach(function (v) { widthButtons[v].setAttribute('aria-pressed', v === w ? 'true' : 'false'); });
    THEMES.forEach(function (v) { themeButtons[v].setAttribute('aria-pressed', v === t ? 'true' : 'false'); });
    notice.hidden = !storageBlocked;
    paste.textContent = pasteText();
  }
  function setWidth(w) {
    root.setAttribute('data-width', w);
    storageSet(KEY_WIDTH, w);
    refresh();
  }
  function setTheme(t) {
    root.setAttribute('data-theme', t);
    storageSet(KEY_THEME, t);
    applyTheme();
    refresh();
  }
  // Reset removes the saved choice and applies the start values, in place.
  function resetChoice() {
    storageRemove(KEY_WIDTH);
    storageRemove(KEY_THEME);
    root.setAttribute('data-width', startWidth);
    root.setAttribute('data-theme', startTheme);
    applyTheme();
    refresh();
  }
  function nextWord(list, v) {
    return list[(list.indexOf(v) + 1) % list.length];
  }
  function openPop() {
    refresh();
    pop.hidden = false;
    gear.setAttribute('aria-expanded', 'true');
  }
  function closePop() {
    pop.hidden = true;
    gear.setAttribute('aria-expanded', 'false');
  }
  function togglePop() {
    if (pop.hidden) openPop(); else closePop();
  }
  function buildPopover() {
    WIDTHS.forEach(function (v) { widthButtons[v] = makeButton(v, function () { setWidth(v); }); });
    THEMES.forEach(function (v) { themeButtons[v] = makeButton(v, function () { setTheme(v); }); });
    pop.appendChild(makeRow('Width', WIDTHS.map(function (v) { return widthButtons[v]; })));
    pop.appendChild(makeRow('Theme', THEMES.map(function (v) { return themeButtons[v]; })));
    var help = document.createElement('p');
    help.textContent = 'Keys: t theme, w width, ? help';
    pop.appendChild(help);
    notice = document.createElement('p');
    notice.className = 'md-notice';
    notice.textContent = 'Storage is blocked. Your choice is not kept.';
    notice.hidden = true;
    pop.appendChild(notice);
    var label = document.createElement('p');
    label.textContent = 'Paste into md-view.ini:';
    pop.appendChild(label);
    paste = document.createElement('pre');
    paste.className = 'md-paste';
    pop.appendChild(paste);
    pop.appendChild(makeButton('Reset', resetChoice));
    refresh();
  }
  // The gear, a click outside the popover, and the keys.
  function wireControls() {
    gear.addEventListener('click', togglePop);
    document.addEventListener('click', function (e) {
      if (pop.hidden || pop.contains(e.target) || gear.contains(e.target)) return;
      closePop();
    });
    document.addEventListener('keydown', function (e) {
      // Ctrl, Alt and Meta belong to the browser. Form fields keep their own keys.
      if (e.ctrlKey || e.altKey || e.metaKey) return;
      var tg = e.target;
      var tag = tg && tg.nodeName ? String(tg.nodeName).toLowerCase() : '';
      if (tag === 'input' || tag === 'textarea' || tag === 'select' || (tg && tg.isContentEditable)) return;
      if (e.key === 'Escape') closePop();
      else if (e.key === 't') setTheme(nextWord(THEMES, themeSetting()));
      else if (e.key === 'w') setWidth(nextWord(WIDTHS, root.getAttribute('data-width')));
      else if (e.key === '?') togglePop();
    });
  }
})();
JS
    printf '</script>\n</body>\n</html>\n'
} > "$page" || { echo "md-open: cannot write $page" >&2; exit 1; }

if [ "$do_open" = 1 ]; then
    open_page "$page"
fi
exit 0
