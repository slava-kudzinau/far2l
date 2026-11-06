#!/bin/sh
# md-open.sh: show a Markdown file as a web page (far2l Alt-F3; macOS first).
#
#   md-open.sh <file.md> [--out <page.html>] [--no-open]
#   md-open.sh --print-association
#
# Writes one page (the Markdown text and marked inline; mermaid from a pinned
# CDN script with an SRI hash) and opens it with the system opener. The page
# goes to ${TMPDIR:-/tmp}/far2l-md-<pid>.html unless --out names another file.
# marked.min.js is read from this script's folder, or from MARKED_JS.

LC_ALL=C
export LC_ALL

self_dir=$(cd "$(dirname -- "$0")" && pwd) || exit 2

usage() {
    echo 'usage: md-open.sh <file.md> [--out <page.html>] [--no-open]' >&2
    echo '       md-open.sh --print-association' >&2
    exit 2
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
EOF
}

file=
out=
do_open=1
while [ $# -gt 0 ]; do
    case $1 in
        --print-association)
            print_association
            exit 0 ;;
        --no-open)
            do_open=0 ;;
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

[ -n "$file" ] || usage
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
cr=$(printf '\r')

{
    cat <<'HTML'
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'unsafe-inline' https://cdn.jsdelivr.net; style-src 'unsafe-inline'; img-src data: file:">
<meta name="viewport" content="width=device-width, initial-scale=1">
HTML
    printf '<base href="%s">\n' "$base_url"
    printf '<title>%s</title>\n' "$title"
    cat <<'HTML'
<style>
:root { color-scheme: light dark; }
body { max-width: 50em; margin: 2em auto; padding: 0 1em; font: 16px/1.5 system-ui, -apple-system, "Helvetica Neue", sans-serif; }
pre { overflow-x: auto; padding: 0.75em; border-radius: 6px; background: #f2f2f2; }
pre.mermaid[data-processed] { background: none; text-align: center; }
code { font-family: ui-monospace, Menlo, monospace; font-size: 0.9em; }
.mermaid-error { color: #b00020; font-size: 0.85em; text-align: left; }
table { border-collapse: collapse; }
th, td { border: 1px solid #bbb; padding: 0.3em 0.6em; text-align: left; }
@media (prefers-color-scheme: dark) {
  body { background: #1e1e1e; color: #e4e4e4; }
  pre { background: #2a2a2a; }
  th, td { border-color: #555; }
  a { color: #7ab8ff; }
  .mermaid-error { color: #ff8a8a; }
}
</style>
</head>
<body>
<main id="content"></main>
<script type="text/markdown" id="md">
HTML
    # The text cannot end its script element or open a comment there. Backslashes
    # are doubled first, then "</" becomes "<\/" and "<!--" becomes "<\!--"; the
    # page undoes the steps in reverse order. A BOM and CRs go too.
    sed -e "1s/^$bom//" -e "s/$cr\$//" -e 's/\\/\\\\/g' -e 's|</|<\\/|g' -e 's|<!--|<\\!--|g' < "$file"
    printf '\n</script>\n<script>\n'
    cat "$marked_js"
    printf '\n</script>\n<script>\n'
    cat <<'JS'
(function () {
  'use strict';
  var MERMAID_URL = 'https://cdn.jsdelivr.net/npm/mermaid@12.3.0/dist/mermaid.min.js';
  var MERMAID_SRI = 'sha384-HHW54r639fK42Ip0coGF3mz8oSiGn0eEoWNTLs75X7H63pLSDBa0Tqs0RwGmLlXE';
  function esc(s) {
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
  }
  // Undo the escapes of the embedded text, in the reverse of the shell's order.
  var src = document.getElementById('md').textContent
    .split('<\\!--').join('<!--')
    .split('<\\/').join('</')
    .split('\\\\').join('\\');
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

  // The text is shown now. Mermaid is added by a script element created here, so
  // a slow or blocked CDN cannot hide the text. Its load or its error decides how
  // the diagrams are shown.
  var items = Array.prototype.map.call(content.querySelectorAll('pre.mermaid'), function (pre) {
    return { pre: pre, src: pre.textContent };
  });
  if (!items.length) return;
  var s = document.createElement('script');
  s.src = MERMAID_URL;
  s.integrity = MERMAID_SRI;
  s.crossOrigin = 'anonymous';
  s.onload = function () {
    if (typeof mermaid === 'undefined') {
      items.forEach(function (it) { showCode(it.pre, it.src, null); });
    } else {
      drawDiagrams(items);
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
    var dark = window.matchMedia('(prefers-color-scheme: dark)').matches;
    mermaid.initialize({ startOnLoad: false, securityLevel:'strict', theme: dark ? 'dark' : 'default' });
    // Each diagram is parsed alone first, so a broken one shows its error and the
    // others still draw.
    var parsed = list.map(function (it) {
      return mermaid.parse(it.src).then(function () { return it; }, function (err) {
        showCode(it.pre, it.src, err);
        return null;
      });
    });
    Promise.all(parsed).then(function (results) {
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
})();
JS
    printf '</script>\n</body>\n</html>\n'
} > "$page" || { echo "md-open: cannot write $page" >&2; exit 1; }

if [ "$do_open" = 1 ]; then
    if command -v open >/dev/null 2>&1; then
        open "$page" || { echo 'md-open: open failed' >&2; exit 1; }
    elif command -v xdg-open >/dev/null 2>&1; then
        xdg-open "$page" || { echo 'md-open: xdg-open failed' >&2; exit 1; }
    else
        echo "$page"
    fi
fi
exit 0
