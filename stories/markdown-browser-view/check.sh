#!/bin/sh
# Story check for md-open.sh (stories/markdown-browser-view/plan.md, Spec).
# Runs every Spec row: the pages are written with --no-open, and the diagram,
# hostile and BOM pages are rendered in headless Chrome. Mermaid comes from the
# CDN, so the check needs network. Exit 0 only when every row passes.
# Env CHROME overrides the browser path.

here=$(cd "$(dirname "$0")" && pwd) || exit 1
repo=$(cd "$here/../.." && pwd) || exit 1
bin=$repo/far2l/bootstrap
script=$bin/md-open.sh
fix=$here/fixtures
cdn=https://cdn.jsdelivr.net/npm/mermaid@12.3.0/dist/mermaid.min.js
base=4b48356e6 # the story's base commit: far2l/src must not differ from it

CHROME=${CHROME:-}
[ -n "$CHROME" ] || CHROME="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
if [ ! -x "$CHROME" ]; then
    echo "chrome not found: $CHROME (set CHROME to a Chrome binary)"
    exit 1
fi
if ! curl -sfI --max-time 30 "$cdn" >/dev/null 2>&1; then
    echo "no network: cannot reach $cdn"
    exit 1
fi

tmp=$(mktemp -d "${TMPDIR:-/tmp}/md-open-check.XXXXXX") || exit 1
trap 'rm -rf "$tmp"' EXIT

fails=0
check() { # check <label> <command...>
    label=$1
    shift
    if "$@"; then
        echo "ok    $label"
    else
        echo "FAIL  $label"
        fails=$((fails + 1))
    fi
}
absent() { ! "$@"; }

# strip_scripts <dump>: the dumped DOM on one line, with every <script> element
# removed, so that only the rendered page is matched.
strip_scripts() {
    tr '\n' ' ' < "$1" | awk '{
        s = $0; out = ""
        while ((i = index(s, "<script")) > 0) {
            out = out substr(s, 1, i - 1)
            t = substr(s, i)
            j = index(t, "</script>")
            if (j == 0) { s = ""; break }
            s = substr(t, j + 9)
        }
        print out s
    }'
}

# mermaid_has_svg <dom>: the text between class="mermaid" and the next </pre>
# holds an <svg>.
mermaid_has_svg() {
    awk '{
        s = $0
        while ((i = index(s, "class=\"mermaid\"")) > 0) {
            t = substr(s, i)
            j = index(t, "</pre>")
            seg = (j > 0) ? substr(t, 1, j) : t
            if (index(seg, "<svg") > 0) found = 1
            s = substr(t, 2)
        }
        exit (found ? 0 : 1)
    }' "$1"
}

# render <page> <name>: headless Chrome dumps the page to $tmp/<name>.dom.
render() {
    prof=$(mktemp -d "$tmp/profile.XXXXXX")
    url=file://$(printf '%s' "$1" | sed 's/ /%20/g')
    "$CHROME" --headless --disable-gpu --no-first-run --no-default-browser-check \
        --user-data-dir="$prof" --virtual-time-budget=5000 --dump-dom "$url" \
        > "$tmp/$2.raw" 2> "$tmp/$2.err"
    status=$?
    strip_scripts "$tmp/$2.raw" > "$tmp/$2.dom"
    return $status
}

# usage_fail <err-file> <args...>: exit 2 and a usage line on stderr.
usage_fail() {
    err=$1
    shift
    sh "$script" "$@" > /dev/null 2> "$err"
    [ $? -eq 2 ] && grep -F -q 'usage:' "$err"
}

# one_cdn_script <page>: the only tag that loads anything from the network is
# the pinned mermaid script, with its integrity hash.
one_cdn_script() {
    n=$(grep -E -c '<(script|link|img|iframe)[^>]*(src|href)=["'"'"']?https?:' "$1")
    [ "$n" = 1 ] &&
        grep -F -q "<script src=\"$cdn\" integrity=\"sha384-" "$1" &&
        grep -F -q 'crossorigin="anonymous"' "$1"
}

# inlined <page> <md>: the page holds the markdown text and marked in full.
inlined() {
    p=$(wc -c < "$1" | tr -d ' ')
    m=$(wc -c < "$2" | tr -d ' ')
    v=$(wc -c < "$bin/marked.min.js" | tr -d ' ')
    [ "$p" -ge $((m + v)) ]
}

# Row: the script is POSIX sh.
check "script: sh syntax" sh -n "$script"

# Row: plain page, --no-open, then the text, marked, mermaid init and network.
plain_page() {
    sh "$script" "$fix/plain.md" --out "$tmp/plain.html" --no-open > /dev/null 2> "$tmp/plain.err" &&
        [ -s "$tmp/plain.html" ]
}
check "plain: exit 0 and page written" plain_page
check "plain: markdown text in the page" grep -F -q '# Plain fixture' "$tmp/plain.html"
check "plain: marked inline" grep -F -q 'a markdown parser' "$tmp/plain.html"
check "plain: mermaid.initialize with securityLevel strict" grep -F -q "mermaid.initialize(" "$tmp/plain.html"
check "plain: securityLevel strict in the init call" grep -F -q "securityLevel:'strict'" "$tmp/plain.html"
check "plain: marked and markdown text inlined in full" inlined "$tmp/plain.html" "$fix/plain.md"
check "plain: one CDN script for mermaid, pinned with integrity" one_cdn_script "$tmp/plain.html"

# Row: the file's folder is the base href, with spaces encoded.
space_page() {
    mkdir -p "$tmp/with space" &&
        cp "$fix/plain.md" "$tmp/with space/plain file.md" &&
        sh "$script" "$tmp/with space/plain file.md" --out "$tmp/with space/out page.html" --no-open > /dev/null 2>&1 &&
        grep -F -q '# Plain fixture' "$tmp/with space/out page.html" &&
        grep -F -q 'with%20space/"' "$tmp/with space/out page.html"
}
check "paths with spaces: page written with a base href" space_page

# Row: an empty file renders an empty page.
empty_page() {
    : > "$tmp/empty.md" &&
        sh "$script" "$tmp/empty.md" --out "$tmp/empty.html" --no-open > /dev/null 2>&1 &&
        grep -F -q '<script type="text/markdown" id="md">' "$tmp/empty.html"
}
check "empty file: page written" empty_page

# Row: mermaid fence. The diagram is an SVG in the browser, with the heading.
diagram_page() {
    sh "$script" "$fix/diagram.md" --out "$tmp/diagram.html" --no-open > /dev/null 2>&1 &&
        render "$tmp/diagram.html" diagram
}
check "diagram: Chrome renders the page" diagram_page
check "diagram: svg inside the mermaid block" mermaid_has_svg "$tmp/diagram.dom"
check "diagram: heading text" grep -F -q 'Mermaid fixture' "$tmp/diagram.dom"

# Row: </script> in the file does not end the script; the rest renders and
# nothing from the file runs or becomes an element.
hostile_page() {
    sh "$script" "$fix/hostile.md" --out "$tmp/hostile.html" --no-open > /dev/null 2>&1 &&
        render "$tmp/hostile.html" hostile
}
check "hostile: Chrome renders the page" hostile_page
check "hostile: text after </script> is shown" grep -F -q 'Still here' "$tmp/hostile.dom"
check "hostile: rest of the file is rendered" grep -F -q 'Last paragraph of the file.' "$tmp/hostile.dom"
check "hostile: no element with id x or pwned" absent grep -E -q '<[A-Za-z][^>]*[[:space:]]id="(x|pwned)"' "$tmp/hostile.dom"
check "hostile: no raw img element from the file" absent grep -F -q '<img' "$tmp/hostile.dom"

# Row: a BOM and CRLF are accepted.
bom_page() {
    printf '\357\273\277# Bom head\r\n\r\ntext\r\n' > "$tmp/bom.md" &&
        sh "$script" "$tmp/bom.md" --out "$tmp/bom.html" --no-open > /dev/null 2>&1 &&
        render "$tmp/bom.html" bom &&
        grep -F -q '<h1>Bom head</h1>' "$tmp/bom.dom"
}
check "BOM and CRLF: heading rendered" bom_page

# Row: no argument, missing file or unknown option: usage, exit 2, no page.
check "usage: no argument" usage_fail "$tmp/u1.err"
missing_case() {
    usage_fail "$tmp/u2.err" "$tmp/missing.md" --out "$tmp/missing.html" --no-open &&
        [ ! -e "$tmp/missing.html" ]
}
check "usage: missing file, no page written" missing_case
bogus_case() {
    usage_fail "$tmp/u3.err" "$fix/plain.md" --bogus --out "$tmp/bogus.html" --no-open &&
        [ ! -e "$tmp/bogus.html" ]
}
check "usage: unknown option, no page written" bogus_case

# Row: without --no-open the page is written to the temp folder and opened.
# A fake open on PATH records the page, so no browser window opens.
mkdir -p "$tmp/fakebin" "$tmp/open dir"
cat > "$tmp/fakebin/open" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" > "$OPEN_LOG"
EOF
chmod +x "$tmp/fakebin/open"
open_run() {
    PATH="$tmp/fakebin:$PATH" TMPDIR="$tmp/open dir" OPEN_LOG="$tmp/opened.txt" \
        sh "$script" "$fix/plain.md" > /dev/null 2>&1 || return 1
    opened=$(cat "$tmp/opened.txt")
    case $opened in
        "$tmp/open dir"/far2l-md-*.html) [ -s "$opened" ] ;;
        *) return 1 ;;
    esac
}
check "open: temp page written and open runs on it" open_run

# Row: --print-association prints the entry, run from another folder.
abs_bin=$(cd "$bin" && pwd)
assoc_run() {
    out=$(cd / && sh "$script" --print-association 2> /dev/null) || return 1
    printf '%s\n' "$out" | grep -F -q '*.md' &&
        printf '%s\n' "$out" | grep -F -q "sh \"$abs_bin/md-open.sh\" \"!.!\"" &&
        printf '%s\n' "$out" | grep -F -q 'File associations'
}
check "association: entry with *.md and the Alt+F3 command" assoc_run

# Row: the terminal Markdown view is not changed (far2l/src matches the base).
check "terminal view: far2l/src unchanged since the story base" git -C "$repo" diff --quiet "$base" -- far2l/src

if [ "$fails" -eq 0 ]; then
    echo "all checks pass"
    exit 0
fi
echo "$fails check(s) failed"
exit 1
