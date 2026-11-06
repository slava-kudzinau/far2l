#!/bin/sh
# Story check for md-open.sh (docs/stories/markdown-browser-view/plan.md, Spec and the
# Final-review rulings). Runs every row: the pages are written with --no-open,
# and the pages are rendered in headless Chrome. Mermaid comes from the CDN, so
# the check needs network. Exit 0 only when every row passes.
# Env CHROME overrides the browser path.

here=$(cd "$(dirname "$0")" && pwd) || exit 1
repo=$(cd "$here/../../.." && pwd) || exit 1
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
if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake not found (the install rules are checked with it)"
    exit 1
fi
if ! curl -sfI --max-time 30 "$cdn" >/dev/null 2>&1; then
    echo "no network: cannot reach $cdn"
    exit 1
fi

tmp=$(mktemp -d "${TMPDIR:-/tmp}/md-open-check.XXXXXX") || exit 1
trap 'rm -rf "$tmp"' EXIT
tmp_abs=$(cd "$tmp" && pwd) || exit 1 # the script prints the path as pwd gives it

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

# far2l puts a backslash before each character of its QuotedSymbols default
# (fnparce.cpp EscapeSpace) before the shell reads the Alt-F3 command.
cat > "$tmp/far2l-escape.sed" <<'EOF'
s/[][ $&()`{};|*?!"'\\]/\\&/g
EOF

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

# render <page> <name>: headless Chrome dumps the page to $tmp/<name>.dom, and
# its log (console and CSP messages) goes to $tmp/<name>.err.
render() {
    prof=$(mktemp -d "$tmp/profile.XXXXXX")
    url=file://$(printf '%s' "$1" | sed 's/ /%20/g')
    "$CHROME" --headless --disable-gpu --no-first-run --no-default-browser-check \
        --user-data-dir="$prof" --enable-logging=stderr --virtual-time-budget=5000 \
        --dump-dom "$url" > "$tmp/$2.raw" 2> "$tmp/$2.err"
    status=$?
    strip_scripts "$tmp/$2.raw" > "$tmp/$2.dom"
    return $status
}

# render_proxy <page> <name> <port>: as render, but every request goes through a
# proxy on 127.0.0.1:<port>. With a silent listener the CDN script stalls; with
# no listener it is refused. The dump is taken after 2.5 s of real time, while
# the script still waits (--virtual-time-budget would not show a stall).
render_proxy() {
    prof=$(mktemp -d "$tmp/profile.XXXXXX")
    url=file://$(printf '%s' "$1" | sed 's/ /%20/g')
    "$CHROME" --headless --disable-gpu --no-first-run --no-default-browser-check \
        --user-data-dir="$prof" --timeout=2500 --proxy-server="http://127.0.0.1:$3" \
        --dump-dom "$url" > "$tmp/$2.raw" 2> "$tmp/$2.err"
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

# one_cdn_script <page>: the pinned mermaid script is the only network address
# in the page. The page adds it with its integrity hash and CORS mode.
one_cdn_script() {
    [ "$(grep -F -o "$cdn" "$1" | wc -l | tr -d ' ')" = 1 ] &&
        grep -F -q "'sha384-HHW54r639fK42Ip0coGF3mz8oSiGn0eEoWNTLs75X7H63pLSDBa0Tqs0RwGmLlXE'" "$1" &&
        grep -F -q "s.crossOrigin = 'anonymous'" "$1" &&
        ! grep -E -q '<(script|link|img|iframe)[^>]*(src|href)=["'"'"']?https?:' "$1"
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
check "plain: one CDN script for mermaid, pinned with integrity and CORS" one_cdn_script "$tmp/plain.html"
csp_policy() {
    grep -F -q 'http-equiv="Content-Security-Policy"' "$1" &&
        grep -F -q "default-src 'none'" "$1" &&
        grep -F -q "script-src 'unsafe-inline' https://cdn.jsdelivr.net" "$1" &&
        grep -F -q "img-src data: file:" "$1"
}
check "plain: content security policy allows only the script origin and local images" csp_policy "$tmp/plain.html"

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
check "diagram: svg inside the mermaid block (under the policy)" mermaid_has_svg "$tmp/diagram.dom"
check "diagram: heading text" grep -F -q 'Mermaid fixture' "$tmp/diagram.dom"

# Row: the CDN stalls. The text shows anyway, and the diagram source with it.
stall_page() {
    sh "$script" "$fix/diagram.md" --out "$tmp/stall.html" --no-open > /dev/null 2>&1 || return 1
    port=$((18000 + $$ % 2000))
    sleep 30 | nc -l 127.0.0.1 "$port" > /dev/null 2>&1 &
    lst=$!
    sleep 0.5
    render_proxy "$tmp/stall.html" stall "$port"
    status=$?
    kill "$lst" 2> /dev/null
    return $status
}
check "stalled CDN: Chrome renders the page" stall_page
check "stalled CDN: the heading shows" grep -F -q 'Mermaid fixture' "$tmp/stall.dom"
check "stalled CDN: the diagram source shows" grep -F -q 'Pitch and POC' "$tmp/stall.dom"

# Row: the CDN is refused. Each diagram shows as a code block.
refused_page() {
    sh "$script" "$fix/diagram.md" --out "$tmp/refused.html" --no-open > /dev/null 2>&1 &&
        render_proxy "$tmp/refused.html" refused 9
}
check "refused CDN: each diagram shows as a code block" refused_page
check "refused CDN: the code block holds the source" grep -F -q 'language-mermaid' "$tmp/refused.dom"

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

# Row: literal "<\/script>" and "<\!--" in the file come back as written.
escapes_page() {
    sh "$script" "$fix/escapes.md" --out "$tmp/escapes.html" --no-open > /dev/null 2>&1 &&
        render "$tmp/escapes.html" escapes
}
check "escapes: Chrome renders the page" escapes_page
check "escapes: literal <\\/script> in the text survives" grep -F -q '&lt;\/script&gt;' "$tmp/escapes.dom"
check "escapes: literal <\\!-- in the text survives" grep -F -q '&lt;\!--' "$tmp/escapes.dom"

# Row: a remote image in the file does not load (the policy blocks it).
remote_page() {
    sh "$script" "$fix/remote.md" --out "$tmp/remote.html" --no-open > /dev/null 2>&1 &&
        render "$tmp/remote.html" remote
}
check "remote image: Chrome renders the page" remote_page
check "remote image: the text after it shows" grep -F -q 'The text after the picture still shows.' "$tmp/remote.dom"
check "remote image: the policy refuses to load it" grep -F -q 'violates the following Content Security Policy directive: "img-src' "$tmp/remote.err"

# Row: a broken mermaid block shows its text and its error; a later one draws.
broken_page() {
    sh "$script" "$fix/broken-diagram.md" --out "$tmp/broken.html" --no-open > /dev/null 2>&1 &&
        render "$tmp/broken.html" broken
}
check "broken diagram: Chrome renders the page" broken_page
check "broken diagram: its source text shows" grep -F -q 'this is not a diagram' "$tmp/broken.dom"
check "broken diagram: the mermaid error shows in the block" grep -F -q 'class="mermaid-error"' "$tmp/broken.dom"
check "broken diagram: the next diagram still draws" mermaid_has_svg "$tmp/broken.dom"

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

# Row: a file name that starts with "-" works after "--" (and with --out).
dash_name() {
    mkdir -p "$tmp/dash" && cp "$fix/plain.md" "$tmp/dash/-dash.md" &&
        (cd "$tmp/dash" && sh "$script" --out -dash.html --no-open -- -dash.md) > /dev/null 2> "$tmp/dash.err" &&
        [ ! -s "$tmp/dash.err" ] &&
        grep -F -q '<title>-dash.md</title>' "$tmp/dash/-dash.html" &&
        grep -F -q '# Plain fixture' "$tmp/dash/-dash.html"
}
check "leading dash after --: page written, no stderr" dash_name

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

# Row: --print-association prints the entry, run from another folder. far2l
# escapes the file name in !.! itself, so the command shows it unquoted.
abs_bin=$(cd "$bin" && pwd)
assoc_run() {
    out=$(cd / && sh "$script" --print-association 2> /dev/null) || return 1
    printf '%s\n' "$out" | grep -F -q '*.md' &&
        printf '%s\n' "$out" | grep -F -q "sh $abs_bin/md-open.sh !.!" &&
        printf '%s\n' "$out" | grep -F -q 'File associations'
}
check "association: entry with *.md and the Alt+F3 command, !.! unquoted" assoc_run

# Row: the printed command opens a file whose name has a space and a quote, the
# way far2l runs it: far2l escapes the name, then the shell reads the line.
assoc_name_run() {
    mkdir -p "$tmp/names" && printf '# Quoted name\n' > "$tmp/names/a \"b\" c (1).md" || return 1
    cmd=$(sh "$script" --print-association 2> /dev/null | sed -n 's/^  View command (used for Alt+F3): *//p') || return 1
    prefix=${cmd%'!.!'}
    [ "$prefix" != "$cmd" ] || return 1
    esc=$(printf '%s' 'a "b" c (1).md' | sed -f "$tmp/far2l-escape.sed") || return 1
    (cd "$tmp/names" && sh -c "$prefix$esc --out '$tmp/names.html' --no-open") > /dev/null 2>&1 &&
        grep -F -q '<title>a &quot;b&quot; c (1).md</title>' "$tmp/names.html"
}
check "association: the printed command opens a name with a space and a quote" assoc_name_run

# Row: a script path with a space is quoted in the printed command.
assoc_space_run() {
    mkdir -p "$tmp/with space" && cp "$script" "$tmp/with space/md-open.sh" || return 1
    out=$(sh "$tmp/with space/md-open.sh" --print-association 2> /dev/null) || return 1
    printf '%s\n' "$out" | grep -F -q "sh \"$tmp_abs/with space/md-open.sh\" !.!"
}
check "association: a script path with a space is quoted" assoc_space_run

# Row: the terminal Markdown view is not changed (far2l/src matches the base).
check "terminal view: far2l/src unchanged since the story base" git -C "$repo" diff --quiet "$base" -- far2l/src

# Row: the install rules. A wrapper project adds only the bootstrap folder, and
# the rules are read from its generated build files and its install.
wrap_configure() { # wrap_configure <dir> <ON|OFF: APPLE>
    mkdir -p "$1" &&
        cat > "$1/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.10)
project(md_wrap NONE)
set(APP_NAME "far2l")
set(APP_DIR "$1/app")
set(INSTALL_DIR "\${APP_DIR}/\${APP_NAME}.app/Contents/MacOS")
set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)
set(APPLE $2)
add_subdirectory("$bin" "$1/bootstrap-build")
EOF
    cmake -S "$1" -B "$1/build" > "$1/configure.log" 2>&1
}
mac=$tmp/wrap-mac
linux=$tmp/wrap-linux
check "install (macOS): the layout configures" wrap_configure "$mac" ON
no_copy_in_macos() { # the build copies the scripts into MacOS without md-open.sh
    bm=$mac/bootstrap-build/CMakeFiles/bootstrap.dir/build.make
    [ -s "$bm" ] && grep -F -q 'cp ' "$bm" && ! grep -F -q -e 'md-open.sh' -e '*.sh' "$bm"
}
check "install (macOS): the MacOS copy leaves md-open.sh out" no_copy_in_macos
app_install() {
    cmake --install "$mac/build" --component app --prefix "$tmp/app-install" > "$tmp/app-install.log" 2>&1 || return 1
    res=$tmp/app-install/far2l.app/Contents/Resources
    [ -x "$res/md-open.sh" ] && [ -s "$res/marked.min.js" ] && [ -s "$res/marked.LICENSE.md" ]
}
check "install (macOS, component app): script, library and license in Resources" app_install
plain_install_no_app() {
    cmake --install "$mac/build" --prefix "$tmp/plain-install" > "$tmp/plain-install.log" 2>&1 || return 1
    [ ! -e "$tmp/plain-install/far2l.app/Contents/Resources/md-open.sh" ]
}
check "install (macOS, plain cmake --install): no partial app files" plain_install_no_app
check "install (Linux layout): the layout configures" wrap_configure "$linux" OFF
linux_install() {
    cmake --install "$linux/build" --prefix "$tmp/linux-install" > "$tmp/linux-install.log" 2>&1 || return 1
    res=$tmp/linux-install/share/far2l
    [ -x "$res/md-open.sh" ] && [ -s "$res/marked.min.js" ] && [ -s "$res/marked.LICENSE.md" ]
}
check "install (Linux layout): script, library and license in share/far2l" linux_install

# Rows: the gate says what the owner needs (rulings 1 and 5) and has no stale
# reference to a local mermaid file.
gate="$here/gate.md"
check "gate: no reference to a local mermaid file" absent grep -F -q 'mermaid.min.js' "$gate"
check "gate: remote images do not load" grep -F -q 'remote images' "$gate"
check "gate: the printed command is sh <path to md-open.sh> !.!" grep -F -q 'sh <path to md-open.sh> !.!' "$gate"

# Row: marked.min.js must not hold "</script", which would end the inline script.
# (It holds one "<!--", which the page tolerates; see the note in marked.LICENSE.md.)
check "marked: no </script in the vendored file" absent grep -F -q '</script' "$bin/marked.min.js"

if [ "$fails" -eq 0 ]; then
    echo "all checks pass"
    exit 0
fi
echo "$fails check(s) failed"
exit 1
