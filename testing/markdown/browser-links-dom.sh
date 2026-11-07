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
    cd "$proj" || bad "cd"
}

# build [ARGS...]: builds $page from the last argument, which is the file.
build() {
    sh "$md_open" --no-open --out "$page" "$@" > "$d/out" 2> "$d/err" || bad "md-open exit $?: $(cat "$d/err")"
    [ -s "$page" ] || bad "no page"
}

# file_url PATH: the file:// URL of PATH, with the escapes of md-open.sh.
file_url() {
    printf 'file://%s' "$(printf '%s' "$1" | sed -e 's/%/%25/g' -e 's/ /%20/g' -e 's/#/%23/g' \
        -e 's/?/%3F/g' -e 's/"/%22/g' -e "s/'/%27/g" -e 's/</%3C/g' -e 's/>/%3E/g' -e 's/&/%26/g')"
}

# dump URL: the DOM of URL in a fresh Chrome profile.
dump() {
    prof=$d/chrome.$$.$(date +%s)
    "$CHROME" --headless=new --disable-gpu --no-first-run \
        --use-mock-keychain --password-store=basic \
        --user-data-dir="$prof" --virtual-time-budget=3000 --dump-dom "$1" 2>/dev/null
    rm -rf "$prof"
}

# main_of: the dumped DOM from the reading pane. The page script is not in it.
main_of() {
    sed -n '/<main id="content"/,/<\/main>/p'
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

for c in dom_start_file dom_route_file dom_route_unknown \
         dom_link_in_tree dom_link_not_in_tree dom_link_out_of_root \
         dom_link_out_of_root_query dom_link_out_of_root_backslash \
         dom_link_absolute dom_link_bad_escape dom_link_file_scheme \
         dom_link_percent dom_link_big dom_other_link dom_image_base \
         dom_heading_ids dom_anchor_same_file dom_anchor_other_file \
         dom_one_file_anchor dom_names_hostile dom_raw_html_text \
         dom_script_scheme; do
    run "$c"
done

exit "$fail"
