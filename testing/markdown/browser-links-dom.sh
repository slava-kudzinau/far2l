#!/bin/sh
# Test of the page script of far2l/bootstrap/md-open.sh in headless Chrome
# (story markdown-browser-tree-panel): the hash route, the link rules, images,
# heading ids and anchors. Each case builds a page with md-open.sh --out, then
# dumps the DOM of its file:// URL with Chrome.
#
# usage: sh testing/markdown/browser-links-dom.sh
#   MD_OPEN  the script under test (default far2l/bootstrap/md-open.sh)
#   CHROME   the Chrome program (default: the macOS path, then google-chrome,
#            then chromium in PATH)
#
# Prints "ok <case>" or "FAIL <case>". With no Chrome it prints "skip all" and
# exits 0.

LC_ALL=C
export LC_ALL

here=$(cd "$(dirname -- "$0")" && pwd) || exit 1
root=$(cd "$here/../.." && pwd) || exit 1
md_open=${MD_OPEN:-$root/far2l/bootstrap/md-open.sh}
case $md_open in
    /*) ;;
    *) md_open=$(pwd)/$md_open ;;
esac

CHROME=${CHROME:-}
if [ -z "$CHROME" ]; then
    for c in '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' google-chrome chromium; do
        case $c in
            /*)
                [ -x "$c" ] && { CHROME=$c; break; } ;;
            *)
                p=$(command -v "$c" 2>/dev/null) && [ -n "$p" ] && { CHROME=$p; break; } ;;
        esac
    done
fi
if [ -z "$CHROME" ]; then
    echo "skip all"
    exit 0
fi

[ -f "$md_open" ] || { echo "FAIL setup: no script $md_open"; exit 1; }

tmp=$(mktemp -d "${TMPDIR:-/tmp}/browser-links.XXXXXX") || exit 1
trap 'rm -rf "$tmp"' EXIT
trap 'exit 1' INT TERM HUP

fail=0

bad() {
    echo "  $case_name: $*" >&2
    exit 1
}

# fresh: a new environment for one case. Sets d, proj, page.
fresh() {
    d=$tmp/$case_name
    proj=$d/${1:-proj}
    mkdir -p "$d/home" "$d/config" "$d/cache" "$d/tmpdir" "$proj" || bad "mkdir"
    HOME=$d/home
    XDG_CONFIG_HOME=$d/config
    XDG_CACHE_HOME=$d/cache
    TMPDIR=$d/tmpdir
    PATH=$d/stub:$PATH
    export HOME XDG_CONFIG_HOME XDG_CACHE_HOME TMPDIR PATH
    page=$d/page.html
    ini=$d/config/far2l/settings/md-view.ini
    cd "$proj" || bad "cd"
}

# write_ini LINE...: the choice file for the run.
write_ini() {
    mkdir -p "$(dirname -- "$ini")" || bad "mkdir ini folder"
    printf '%s\n' "$@" > "$ini" || bad "write ini"
}

# repeat TEXT N: TEXT N times on one line.
repeat() {
    awk -v t="$1" -v n="$2" 'BEGIN { for (i = 0; i < n; i++) printf "%s", t; printf "\n" }'
}

# build [ARGS...]: builds $page from the last argument, which is the file.
build() {
    sh "$md_open" --no-open --out "$page" "$@" > "$d/out" 2> "$d/err" || bad "md-open exit $?: $(cat "$d/err")"
    [ -s "$page" ] || bad "no page"
    cp "$page" "$d/base.html" || bad "cp"
}

# file_url PATH: the file:// URL of PATH, with the escapes of md-open.sh.
file_url() {
    printf 'file://%s' "$(printf '%s' "$1" | sed -e 's/%/%25/g' -e 's/ /%20/g' -e 's/#/%23/g' \
        -e 's/?/%3F/g' -e 's/"/%22/g' -e "s/'/%27/g" -e 's/</%3C/g' -e 's/>/%3E/g' -e 's/&/%26/g')"
}

# dump URL: the DOM of URL in a fresh Chrome profile. WINDOW sets the size.
dump() {
    prof=$d/chrome.$$.$(date +%s)
    if [ -n "$WINDOW" ]; then ws="--window-size=$WINDOW"; else ws=; fi
    "$CHROME" --headless=new --disable-gpu --no-first-run \
        --use-mock-keychain --password-store=basic $ws \
        --user-data-dir="$prof" --virtual-time-budget=3000 --dump-dom "$1" 2>/dev/null
    rm -rf "$prof"
}

# main_of: the dumped DOM from the reading pane. The page script is not in it.
main_of() {
    sed -n '/<main id="content"/,/<\/main>/p'
}

# tree_of: the dumped DOM of the file tree panel.
tree_of() {
    sed -n 's|.*\(<nav id="md-tree-panel".*</nav>\).*|\1|p'
}

# with_script FILE SCRIPT: FILE with SCRIPT added before </body>.
# The injected script runs after the page script wired its handlers.
with_script() {
    awk -v s="$2" '{ i = index($0, "</body>"); if (i) print substr($0, 1, i - 1) s substr($0, i); else print }' "$1" \
        > "$d/injected.html" || bad "awk"
}

# dom [HASH]: the dumped DOM of $page with the hash HASH.
dom() {
    u=$(file_url "$page")
    [ -n "$1" ] && u=$u'#'"$1"
    dump "$u"
}

run() {
    case_name=$1
    ( "case_$1" )
    st=$?
    case $st in
        0) echo "ok $1" ;;
        *) echo "FAIL $1"; fail=1 ;;
    esac
}

# --- route ------------------------------------------------------------------

case_dom_start_file() {
    fresh
    printf '# A-OPEN\n\n[in](a.md)\n' > o.md || bad "write"
    printf '# B-ONLY\n' > a.md || bad "write"
    build o.md
    dom > "$d/dom"
    main_of < "$d/dom" | grep -qF '<main id="content" class="markdown-body"><h1 id="user-content-a-open">A-OPEN</h1>' \
        || bad "the opened file is not shown"
    grep -qF 'A-OPEN' "$d/dom" || bad "no A-OPEN"
    exit 0
}

case_dom_route_file() {
    fresh
    printf '# A-OPEN\n' > o.md || bad "write"
    printf '# B-ONLY\n' > a.md || bad "write"
    build o.md
    dom '/a.md' > "$d/dom"
    main_of < "$d/dom" | grep -qF '<h1 id="user-content-b-only">B-ONLY</h1>' || bad "the route file is not shown"
    grep -qxF '<title>a.md</title>' "$d/dom" || bad "the title is not the base name: $(grep -o '<title>[^<]*</title>' "$d/dom")"
    exit 0
}

case_dom_route_unknown() {
    fresh
    printf '# A-OPEN\n' > o.md || bad "write"
    printf '# B-ONLY\n' > a.md || bad "write"
    build o.md
    dom '/nope.md' > "$d/dom"
    main_of < "$d/dom" | grep -qF 'A-OPEN' || bad "an unknown route does not show the opened file"
    exit 0
}

# --- links ------------------------------------------------------------------

# plain_note NAME: the link NAME is plain text with the note after it.
plain_note() {
    main_of < "$d/dom" | grep -qF "<a>$1</a><span class=\"md-note\"> (not in this tree)</span>" \
        || bad "$1 is not plain text with the note"
}

case_dom_link_in_tree() {
    fresh 'pro ject'
    printf '# A-OPEN\n\n[L-IN](a.md)\n' > o.md || bad "write"
    printf '# B-ONLY\n' > a.md || bad "write"
    build o.md
    dom > "$d/dom"
    want='<a href="'"$(file_url "$page")"'#/a.md">L-IN</a>'
    main_of < "$d/dom" | grep -qF "$want" || bad "the link in the tree is not a route: $(main_of < "$d/dom")"
    exit 0
}

case_dom_link_not_in_tree() {
    fresh
    printf '# A-OPEN\n\n[L-MISS](missing.md)\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom"
    plain_note L-MISS
    exit 0
}

case_dom_link_out_of_root() {
    fresh
    printf '# A-OPEN\n\n[L-UP](../a.md)\n' > o.md || bad "write"
    printf '# B-ONLY\n' > a.md || bad "write"
    build o.md
    dom > "$d/dom"
    plain_note L-UP
    exit 0
}

case_dom_link_out_of_root_query() {
    fresh
    printf '# A-OPEN\n\n[L-Q](../outside.md?x)\n[L-Q2](a.md?x)\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom"
    plain_note L-Q
    plain_note L-Q2
    exit 0
}

case_dom_link_out_of_root_backslash() {
    fresh
    printf '# A-OPEN\n\n[L-BS](..%%5Coutside.md)\n[L-BS2](a%%5Cnotes.txt)\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom"
    plain_note L-BS
    plain_note L-BS2
    exit 0
}

case_dom_link_absolute() {
    fresh
    printf '# A-OPEN\n\n[L-ABS](/a.md)\n' > o.md || bad "write"
    printf '# B-ONLY\n' > a.md || bad "write"
    build o.md
    dom > "$d/dom"
    plain_note L-ABS
    exit 0
}

case_dom_link_bad_escape() {
    fresh
    printf '# A-OPEN\n\n[L-Ctl](bad%%01.md)\n[L-Esc](bad%%zz.md)\n[L-Name](a%%01.txt)\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom"
    plain_note L-Ctl
    plain_note L-Esc
    plain_note L-Name
    exit 0
}

case_dom_link_file_scheme() {
    fresh
    printf '# A-OPEN\n\n[L-File](file:///etc/passwd)\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom"
    plain_note L-File
    exit 0
}

case_dom_link_percent() {
    fresh
    printf '# A-OPEN\n\n[L-Pct](%%2e%%2e/up.md)\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom"
    plain_note L-Pct
    exit 0
}

case_dom_link_big() {
    fresh
    printf '# A-OPEN\n\n[L-Big](big.md)\n' > o.md || bad "write"
    awk 'BEGIN { for (i = 0; i < 1200000; i++) printf "x"; printf "\n" }' > big.md || bad "write big"
    build o.md
    dom > "$d/dom"
    want='<a href="'"$(file_url "$page")"'#/big.md" data-md-note="big">L-Big</a>'
    main_of < "$d/dom" | grep -qF "$want" || bad "a big file link is not a route: $(main_of < "$d/dom")"
    dom '/big.md' > "$d/dom2"
    main_of < "$d/dom2" | grep -qF 'This file is too large for the page.' || bad "no note for the big file"
    exit 0
}

case_dom_other_link() {
    fresh
    printf '# A-OPEN\n\n[L-Web](https://example.com/x)\n[L-Txt](notes.txt)\n' > o.md || bad "write"
    printf 't\n' > notes.txt || bad "write"
    build o.md
    dom > "$d/dom"
    main_of < "$d/dom" | grep -qF '<a href="https://example.com/x">L-Web</a>' || bad "an http link lost its href"
    grep -q 'L-Txt</a>' "$d/dom" || bad "no L-Txt link"
    main_of < "$d/dom" | grep -qF '<a href="file://' || bad "a relative file link has no absolute URL"
    main_of < "$d/dom" | grep -qF 'notes.txt">L-Txt</a>' || bad "the relative link does not name notes.txt"
    exit 0
}

# --- images -----------------------------------------------------------------

case_dom_image_base() {
    fresh
    mkdir sub || bad "mkdir"
    printf '# A-OPEN\n\n[go](sub/f.md)\n' > o.md || bad "write"
    printf '# F\n\n![i](img.png)\n' > sub/f.md || bad "write"
    build o.md
    dom '/sub/f.md' > "$d/dom"
    main_of < "$d/dom" | grep -qF '/sub/img.png"' || bad "the image is not in the folder of the file: $(main_of < "$d/dom")"
    exit 0
}

# --- headings and anchors ---------------------------------------------------

case_dom_heading_ids() {
    fresh
    printf '# A-OPEN\n\n[go](h.md)\n' > o.md || bad "write"
    printf '# H\n\n## Hi, There!\n\n## Hi, There!\n\n## snake_case name\n' > h.md || bad "write"
    build o.md
    dom '/h.md' > "$d/dom"
    main_of < "$d/dom" | grep -qF 'id="user-content-hi-there"' || bad "no first heading id"
    main_of < "$d/dom" | grep -qF 'id="user-content-hi-there-1"' || bad "no second heading id"
    main_of < "$d/dom" | grep -qF 'id="user-content-snake_case-name"' || bad "no snake_case heading id"
    exit 0
}

case_dom_anchor_same_file() {
    fresh
    printf '# A-OPEN\n\n[L-Sec](#sec)\n\n## Sec\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom"
    want='<a href="'"$(file_url "$page")"'#/o.md#sec">L-Sec</a>'
    main_of < "$d/dom" | grep -qF "$want" || bad "the same-file anchor is not a route: $(main_of < "$d/dom")"
    exit 0
}

case_dom_anchor_other_file() {
    fresh
    printf '# A-OPEN\n\n[L-Oth](a.md#sec)\n' > o.md || bad "write"
    printf '# A\n\n## Sec\n' > a.md || bad "write"
    build o.md
    dom > "$d/dom"
    want='<a href="'"$(file_url "$page")"'#/a.md#sec">L-Oth</a>'
    main_of < "$d/dom" | grep -qF "$want" || bad "the anchor of another file is not a route: $(main_of < "$d/dom")"
    exit 0
}

case_dom_one_file_anchor() {
    fresh
    printf '# A-OPEN\n\n[L-Sec](#sec)\n\n## Sec\n' > o.md || bad "write"
    printf '# B-ONLY\n' > a.md || bad "write"
    build --tree off o.md
    grep -q 'data-tree="off"' "$page" || bad "the page is not in one-file mode"
    dom > "$d/dom"
    want='<a href="'"$(file_url "$page")"'#sec">L-Sec</a>'
    main_of < "$d/dom" | grep -qF "$want" || bad "the one-file anchor is not a same-page link: $(main_of < "$d/dom")"
    main_of < "$d/dom" | grep -qF 'id="user-content-sec"' || bad "no heading id in one-file mode"
    exit 0
}

# --- untrusted names and text -----------------------------------------------

case_dom_names_hostile() {
    fresh
    hostile='"><script>.md'
    printf '# A-OPEN\n' > o.md || bad "write"
    printf '# H\n' > "$hostile" || bad "write hostile"
    build o.md
    dom > "$d/dom"
    grep -qF 'page %22%3E%3Cscript%3E.md' "$d/dom" || bad "the hostile name is not an encoded tree line"
    if main_of < "$d/dom" | grep -qF '<script'; then bad "a name made an element"; fi
    if grep -q '<img src=x' "$d/dom"; then bad "a name made an img"; fi
    exit 0
}

case_dom_raw_html_text() {
    fresh
    printf '# A-OPEN\n\n[go](raw.md)\n' > o.md || bad "write"
    printf '# R\n\n<b>x</b>\n\n<script>alert(1)</script>\n' > raw.md || bad "write"
    printf '# R2\n\n<i>y</i>\n' > raw2.md || bad "write"
    build o.md
    dom '/raw.md' > "$d/dom"
    main_of < "$d/dom" | grep -qF '&lt;b&gt;x&lt;/b&gt;' || bad "raw HTML is not text"
    if main_of < "$d/dom" | grep -qF '<b>x</b>'; then bad "raw HTML made an element"; fi
    if main_of < "$d/dom" | grep -qF '<script>alert'; then bad "a raw script tag made an element"; fi
    dom '/raw2.md' > "$d/dom2"
    main_of < "$d/dom2" | grep -qF '&lt;i&gt;y&lt;/i&gt;' || bad "raw HTML in a second file is not text"
    exit 0
}

case_dom_script_scheme() {
    fresh
    printf '# A-OPEN\n\n[L-Js](javascript:alert(1))\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom"
    main_of < "$d/dom" | grep -qF '<a>L-Js</a>' || bad "a script scheme link kept an href or got a note: $(main_of < "$d/dom")"
    if main_of < "$d/dom" | grep -qiF 'javascript:'; then bad "the script scheme is in the page"; fi
    exit 0
}

# --- file tree panel --------------------------------------------------------

# measure SCRIPT: a page with SCRIPT added after the page script; prints the
# dumped DOM of it. SCRIPT runs after the page script wired its handlers.
measured() {
    with_script "$d/base.html" "$1"
    page=$d/injected.html
    dom
}

# geo: the geometry of the page in one line: left,width of #content, the rect
# of the gear and of the toggle, and the inner width. Call it from a script.
GEO="var c=document.getElementById('content').getBoundingClientRect(),g=document.getElementById('md-gear').getBoundingClientRect(),t=document.getElementById('md-tree-toggle').getBoundingClientRect();document.title=['content',Math.round(c.left),Math.round(c.width),'gear',Math.round(g.left),Math.round(g.top),Math.round(g.right),'toggle',Math.round(t.left),Math.round(t.top),Math.round(t.right),'window',window.innerWidth].join(' ');"

case_dom_panel_tree() {
    fresh
    mkdir -p sub deep/a || bad "mkdir"
    printf '# A-OPEN\n\n[go](sub/a.md)\n' > o.md || bad "write"
    printf '# A\n' > sub/a.md || bad "write"
    printf '# B\n' > b.md || bad "write"
    printf '# D\n' > deep/a/d.md || bad "write"
    build o.md
    dom > "$d/dom"
    t=$(tree_of < "$d/dom")
    printf '%s\n' "$t" | grep -qF '<div class="md-root">proj</div>' || bad "no root name: $t"
    printf '%s\n' "$t" | grep -qF '<summary>sub</summary>' || bad "no folder sub: $t"
    printf '%s\n' "$t" | grep -qF '<summary>a</summary>' || bad "no nested folder a: $t"
    printf '%s\n' "$t" | grep -qF 'href="'"$(file_url "$page")"'#/sub/a.md">a.md</a>' || bad "no link of sub/a.md: $t"
    printf '%s\n' "$t" | grep -qF 'href="'"$(file_url "$page")"'#/deep/a/d.md">d.md</a>' || bad "no link of deep/a/d.md: $t"
    printf '%s\n' "$t" | grep -qF 'href="'"$(file_url "$page")"'#/o.md"' || bad "no link of o.md: $t"
    # The tree order: b.md before o.md before sub before deep.
    printf '%s\n' "$t" | grep -o 'href="[^"]*"' | sed 's|.*#/||; s|"$||' > "$d/order"
    printf 'b.md\no.md\nsub/a.md\ndeep/a/d.md\n' > "$d/want"
    cmp -s "$d/want" "$d/order" || bad "tree order: $(cat "$d/order")"
    exit 0
}

case_dom_panel_current() {
    fresh
    mkdir sub || bad "mkdir"
    printf '# A-OPEN\n\n[go](sub/a.md)\n' > o.md || bad "write"
    printf '# A\n' > sub/a.md || bad "write"
    build o.md
    dom '/sub/a.md' > "$d/dom"
    t=$(tree_of < "$d/dom")
    printf '%s\n' "$t" | grep -qF '#/sub/a.md" aria-current="page">a.md</a>' || bad "the route file is not current: $t"
    [ "$(printf '%s\n' "$t" | grep -c 'aria-current="page"')" = 1 ] || bad "not one current file: $t"
    grep -qxF '<title>a.md</title>' "$d/dom" || bad "the title is not the base name: $(grep -o '<title>[^<]*</title>' "$d/dom")"
    exit 0
}

case_dom_panel_folders_open() {
    fresh
    mkdir -p deep/a else || bad "mkdir"
    printf '# X\n' > deep/a/x.md || bad "write"
    printf '# Y\n' > else/y.md || bad "write"
    printf '# O\n' > o.md || bad "write"
    build o.md
    dom '/deep/a/x.md' > "$d/dom"
    t=$(tree_of < "$d/dom")
    printf '%s\n' "$t" | grep -qF '<details open=""' || bad "no folder is open: $t"
    [ "$(printf '%s\n' "$t" | grep -o '<details open="">' | wc -l | tr -d ' ')" = 2 ] || bad "the folders of the file are not open: $t"
    printf '%s\n' "$t" | grep -qF '<details><summary>else</summary>' || bad "an unrelated folder is open: $t"
    exit 0
}

case_dom_panel_root_name() {
    fresh 'pro ject <r>'
    printf '# A-OPEN\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom"
    t=$(tree_of < "$d/dom")
    printf '%s\n' "$t" | grep -qF '<div class="md-root">pro ject &lt;r&gt;</div>' || bad "the root name is not decoded text: $t"
    exit 0
}

case_dom_panel_cap_note() {
    fresh
    n=0
    while [ $n -lt 501 ]; do
        n=$((n + 1))
        : > "f$n.md" || bad "touch"
    done
    printf '# A-OPEN\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom"
    tree_of < "$d/dom" | grep -qF '<div class="md-cap">Showing the first 500 files.</div>' \
        || bad "no cap note: $(tree_of < "$d/dom" | tail -3)"
    # The opened file is past the cap: it is in the tree, and the note counts the others.
    [ "$(tree_of < "$d/dom" | grep -o 'href="' | wc -l | tr -d ' ')" = 501 ] || bad "the panel lists $(tree_of < "$d/dom" | grep -o 'href="' | wc -l | tr -d ' ') files"
    exit 0
}

case_dom_panel_big_style() {
    fresh
    printf '# A-OPEN\n' > o.md || bad "write"
    # More than 1 MiB: the tree state is big.
    repeat 'x' 1100000 > big.md || bad "write big"
    build o.md
    dom > "$d/dom"
    tree_of < "$d/dom" | grep -qF 'class="md-off"' || bad "the big file has no md-off class"
    tree_of < "$d/dom" | grep -qF '<a href="'"$(file_url "$page")"'#/big.md" class="md-off">big.md</a>' \
        || bad "the big link: $(tree_of < "$d/dom" | grep big)"
    exit 0
}

case_dom_panel_toggle_markup() {
    fresh
    printf '# A-OPEN\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom"
    grep -qF '<button id="md-tree-toggle" type="button" aria-label="Files" aria-controls="md-tree-panel" aria-expanded="true">' "$d/dom" \
        || bad "no toggle"
    grep -q '^<html lang="en".*data-panel="open"' "$d/dom" || bad "the panel is not open first"
    measured "<script>document.getElementById('md-tree-toggle').click();</script>" > "$d/dom2"
    grep -qF 'data-panel="closed"' "$d/dom2" || bad "a click did not close the panel"
    grep -qF 'aria-expanded="false"' "$d/dom2" || bad "the toggle still says expanded"
    measured "<script>document.getElementById('md-tree-toggle').click();document.dispatchEvent(new KeyboardEvent('keydown',{key:'b'}));</script>" > "$d/dom3"
    grep -qF 'data-panel="open"' "$d/dom3" || bad "the b key did not open the panel"
    measured "<script>document.getElementById('md-tree-toggle').click();document.dispatchEvent(new KeyboardEvent('keydown',{key:'b',ctrlKey:true}));</script>" > "$d/dom4"
    grep -qF 'data-panel="closed"' "$d/dom4" || bad "Ctrl-b changed the panel"
    exit 0
}

case_dom_panel_off_notice() {
    fresh
    printf '# A-OPEN\n' > o.md || bad "write"
    build --tree off o.md
    dom > "$d/dom"
    if grep -qF '<nav id="md-tree-panel"' "$d/dom"; then bad "the panel is on a tree=off page"; fi
    if grep -qF 'id="md-tree-toggle"' "$d/dom"; then bad "the toggle is on a tree=off page"; fi
    grep -qF '<p class="md-notice">File tree: off (--tree off)</p>' "$d/dom" || bad "no flag notice"
    grep -qF '<p>Keys: t theme, w width, ? help</p>' "$d/dom" || bad "the keys line of a one-file page: $(grep -o '<p>Keys:[^<]*' "$d/dom")"
    write_ini '[View]' 'tree=off'
    build o.md
    dom > "$d/dom2"
    grep -qF '<p class="md-notice">File tree: off (tree=off in md-view.ini)</p>' "$d/dom2" || bad "no ini notice"
    # The home folder as the root: one-file mode, with the home notice.
    fresh homeh
    rm -f "$ini"
    printf '# A-OPEN\n' > "$d/home/h.md" || bad "write"
    build "$d/home/h.md"
    dom > "$d/dom3"
    grep -qF '<p class="md-notice">File tree: off (the root is the home folder)</p>' "$d/dom3" || bad "no home notice"
    # The tree page has the b key in its keys line.
    fresh
    printf '# A-OPEN\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom4"
    grep -qF '<p>Keys: t theme, w width, b tree, ? help</p>' "$d/dom4" || bad "the tree keys line: $(grep -o '<p>Keys:[^<]*' "$d/dom4")"
    exit 0
}

# geo_of FILE: the geometry line of a dumped page, from its title.
geo_of() {
    sed -n 's/.*<title>\(.*\)<\/title>.*/\1/p' "$1"
}

# same NUM WANT: NUM is within 2 of WANT.
same() {
    awk -v a="$1" -v b="$2" 'BEGIN { d = a - b; if (d < 0) d = -d; exit !(d <= 2) }'
}

case_dom_panel_width_rule() {
    fresh
    printf '# A-OPEN\n' > o.md || bad "write"
    build o.md
    measured "<script>$GEO</script>" > "$d/dom"
    g=$(geo_of "$d/dom")
    w=$(printf '%s\n' "$g" | awk '{ print $NF }')
    cl=$(printf '%s\n' "$g" | awk '{ print $2 }')
    cw=$(printf '%s\n' "$g" | awk '{ print $3 }')
    # The panel takes 260px and two gaps of 8px; the column keeps 70% of the rest.
    same "$cw" "$(awk -v w="$w" 'BEGIN { printf "%.0f", (w - 292) * 0.7 }')" || bad "the column width is $cw, window $w"
    same "$cl" "$(awk -v w="$w" -v cw="$cw" 'BEGIN { printf "%.0f", 276 + (w - 292 - cw) / 2 }')" || bad "the column left is $cl, window $w"
    measured "<script>document.getElementById('md-tree-toggle').click();$GEO</script>" > "$d/dom2"
    g=$(geo_of "$d/dom2")
    cl=$(printf '%s\n' "$g" | awk '{ print $2 }')
    cw=$(printf '%s\n' "$g" | awk '{ print $3 }')
    same "$cw" "$(awk -v w="$w" 'BEGIN { printf "%.0f", (w - 32) * 0.7 }')" || bad "the closed column width is $cw, window $w"
    same "$cl" "$(awk -v w="$w" -v cw="$cw" 'BEGIN { printf "%.0f", 16 + (w - 32 - cw) / 2 }')" || bad "the closed column left is $cl, window $w"
    exit 0
}

case_dom_panel_gear_fixed() {
    fresh
    printf '# A-OPEN\n' > o.md || bad "write"
    build o.md
    measured "<script>$GEO</script>" > "$d/dom"
    go=$(geo_of "$d/dom")
    measured "<script>document.getElementById('md-tree-toggle').click();$GEO</script>" > "$d/dom2"
    gc=$(geo_of "$d/dom2")
    # The gear keeps its place at the top right.
    [ "$(printf '%s\n' "$go" | cut -d' ' -f5-8)" = "$(printf '%s\n' "$gc" | cut -d' ' -f5-8)" ] \
        || bad "the gear moved: $go / $gc"
    gl=$(printf '%s\n' "$go" | awk '{ print $5 }')
    gt=$(printf '%s\n' "$go" | awk '{ print $6 }')
    same "$gt" 8 || bad "the gear top is $gt"
    w=$(printf '%s\n' "$go" | awk '{ print $NF }')
    same "$gl" "$((w - 28 - 8))" || bad "the gear left is $gl, window $w"
    # The toggle is left of the gear and does not cover it.
    tl=$(printf '%s\n' "$go" | awk '{ print $9 }')
    tr=$(printf '%s\n' "$go" | awk '{ print $11 }')
    same "$tl" 268 || bad "the open toggle left is $tl"
    [ "$tr" -le "$gl" ] || bad "the toggle covers the gear: $tr > $gl"
    tl=$(printf '%s\n' "$gc" | awk '{ print $9 }')
    same "$tl" 8 || bad "the closed toggle left is $tl"
    exit 0
}

case_dom_panel_names_hostile() {
    fresh
    for f in '"><script>.md' '<x>.md' '100%.md' "it's.md" 'a&b.md' 'a b.md' 'q?*[x].md'; do
        : > "$f" || bad "touch [$f]"
    done
    printf '# A-OPEN\n' > o.md || bad "write"
    build o.md
    dom > "$d/dom"
    t=$(tree_of < "$d/dom")
    printf '%s\n' "$t" | grep -qF '>&lt;x&gt;.md</a>' || bad "the <x>.md name is not text: $t"
    printf '%s\n' "$t" | grep -qF '>"&gt;&lt;script&gt;.md</a>' || bad "the quote name is not text: $t"
    printf '%s\n' "$t" | grep -qF '>a b.md</a>' || bad "the space name is not text: $t"
    printf '%s\n' "$t" | grep -qF ">it's.md</a>" || bad "the quote name is not text: $t"
    printf '%s\n' "$t" | grep -qF '>a&amp;b.md</a>' || bad "the amp name is not text: $t"
    printf '%s\n' "$t" | grep -qF '>100%.md</a>' || bad "the percent name is not text: $t"
    printf '%s\n' "$t" | grep -qF '>q?*[x].md</a>' || bad "the wildcard name is not text: $t"
    # The names made no element, and the links are routes of the page.
    if printf '%s\n' "$t" | grep -qF '<script'; then bad "a name made a script element"; fi
    printf '%s\n' "$t" | grep -qF '#/%3Cx%3E.md"' || bad "the <x>.md link is not encoded: $t"
    exit 0
}

case_dom_panel_link_click() {
    fresh
    mkdir sub || bad "mkdir"
    printf '# A-OPEN\n\n[go](sub/a.md)\n' > o.md || bad "write"
    printf '# A-ONLY\n' > sub/a.md || bad "write"
    build o.md
    with_script "$page" "<script>Array.prototype.forEach.call(document.querySelectorAll('#md-tree-panel a'),function(a){if(a.textContent==='a.md')a.click();});</script>"
    page=$d/injected.html
    dom > "$d/dom"
    main_of < "$d/dom" | grep -qF 'A-ONLY' || bad "a tree link did not show its file"
    tree_of < "$d/dom" | grep -qF '#/sub/a.md" aria-current="page">a.md</a>' \
        || bad "the clicked file is not current"
    grep -qxF '<title>a.md</title>' "$d/dom" || bad "the title is not the new file"
    exit 0
}

case_dom_panel_focus() {
    fresh
    mkdir sub || bad "mkdir"
    printf '# A-OPEN\n\n[go](sub/a.md)\n' > o.md || bad "write"
    printf '# A\n' > sub/a.md || bad "write"
    build o.md
    # A click in the tree puts the focus in the reading pane, not on the link.
    measured "<script>var l=document.querySelector('#md-tree-panel a');l.focus();l.click();setTimeout(function(){document.documentElement.setAttribute('data-focus',document.activeElement&&document.activeElement.id);},50);</script>" > "$d/dom"
    grep -q 'data-focus="content"' "$d/dom" || bad "the focus is $(grep -o 'data-focus="[^"]*"' "$d/dom")"
    exit 0
}

case_dom_panel_narrow() {
    fresh
    printf '# A-OPEN\n' > o.md || bad "write"
    build o.md
    with_script "$page" "<script>document.getElementById('md-tree-toggle').click();document.title='panel='+getComputedStyle(document.getElementById('md-tree-panel')).display+',state='+document.documentElement.getAttribute('data-panel');</script>"
    page=$d/injected.html
    WINDOW=600,800; export WINDOW
    dom > "$d/dom"
    grep -qxF '<title>panel=none,state=open</title>' "$d/dom" \
        || bad "a narrow window did not hide the panel: $(grep -o '<title>[^<]*</title>' "$d/dom")"
    exit 0
}

for c in dom_start_file dom_route_file dom_route_unknown \
         dom_link_in_tree dom_link_not_in_tree dom_link_out_of_root \
         dom_link_out_of_root_query dom_link_out_of_root_backslash \
         dom_link_absolute dom_link_bad_escape dom_link_file_scheme \
         dom_link_percent dom_link_big dom_other_link dom_image_base \
         dom_heading_ids dom_anchor_same_file dom_anchor_other_file \
         dom_one_file_anchor dom_names_hostile dom_raw_html_text \
         dom_script_scheme dom_panel_tree dom_panel_current dom_panel_folders_open \
         dom_panel_root_name dom_panel_cap_note dom_panel_big_style \
         dom_panel_toggle_markup dom_panel_off_notice dom_panel_width_rule \
         dom_panel_gear_fixed dom_panel_names_hostile dom_panel_link_click dom_panel_focus \
         dom_panel_narrow; do
    run "$c"
done

exit "$fail"
