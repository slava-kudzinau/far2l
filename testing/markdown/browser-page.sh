#!/bin/sh
# Test of far2l/bootstrap/md-open.sh (story markdown-browser-github-look).
# Checks the flags, the defaults file md-view.ini, the browser launch, hostile
# file names and config values, and the vendored files. It needs only sh and
# common tools (openssl for the sha384 case). It does not need the far2l binary.
#
# usage: sh testing/markdown/browser-page.sh
#   MD_OPEN     the script under test (default far2l/bootstrap/md-open.sh)
#   VENDOR_DIR  the folder of the vendored files (default: the folder of MD_OPEN)
#
# Each case runs in a subshell with a fresh HOME, XDG_CONFIG_HOME,
# XDG_CACHE_HOME and TMPDIR under one temporary folder. Stub "open" and
# "xdg-open" commands come first in PATH, so no real browser ever starts.
# Prints "ok <case>" or "FAIL <case>" and exits 1 on any failure.

LC_ALL=C
export LC_ALL

here=$(cd "$(dirname -- "$0")" && pwd) || exit 1
root=$(cd "$here/../.." && pwd) || exit 1
md_open=${MD_OPEN:-$root/far2l/bootstrap/md-open.sh}
case $md_open in
    /*) ;;
    *) md_open=$(pwd)/$md_open ;;
esac
vendor=${VENDOR_DIR:-$(dirname -- "$md_open")}
sample=$here/browser-page/sample.md
[ -f "$md_open" ] || { echo "FAIL setup: no script $md_open"; exit 1; }
[ -f "$sample" ] || { echo "FAIL setup: no fixture $sample"; exit 1; }

tmp=$(mktemp -d "${TMPDIR:-/tmp}/browser-page.XXXXXX") || exit 1
trap 'rm -rf "$tmp"' EXIT
trap 'exit 1' INT TERM HUP

# A stub opener writes its argument count and arguments, one per line, to $OPEN_LOG.
mkdir "$tmp/stub" || exit 1
cat > "$tmp/stub/opener" <<'EOF'
#!/bin/sh
printf '%s\n' "$#" "$@" > "$OPEN_LOG"
exit "${OPEN_RC:-0}"
EOF
chmod 755 "$tmp/stub/opener" || exit 1
for n in open xdg-open; do
    cp "$tmp/stub/opener" "$tmp/stub/$n" || exit 1
done

fail=0

# bad MESSAGE: the case fails. Only called inside a case subshell.
bad() {
    echo "  $case_name: $*" >&2
    exit 1
}

# fresh: a new environment for one case. Sets d, page, ini.
fresh() {
    d=$tmp/$case_name
    mkdir -p "$d/home" "$d/config" "$d/cache" "$d/tmpdir" "$d/work" || bad "mkdir"
    HOME=$d/home
    XDG_CONFIG_HOME=$d/config
    XDG_CACHE_HOME=$d/cache
    TMPDIR=$d/tmpdir
    OPEN_LOG=$d/opened
    PATH=$tmp/stub:$PATH
    export HOME XDG_CONFIG_HOME XDG_CACHE_HOME TMPDIR OPEN_LOG PATH
    unset BROWSER OPEN_RC
    page=$d/page.html
    ini=$XDG_CONFIG_HOME/far2l/settings/md-view.ini
    cd "$d/work" || bad "cd"
}

# md ARGS...: runs the script under test. Sets rc; output in $d/out and $d/err.
md() {
    sh "$md_open" "$@" > "$d/out" 2> "$d/err"
    rc=$?
}

# write_ini LINE...: writes md-view.ini, one argument per line.
write_ini() {
    mkdir -p "$(dirname -- "$ini")" || bad "mkdir ini folder"
    printf '%s\n' "$@" > "$ini" || bad "write ini"
}

# html_tag: the <html> line of the page.
html_tag() {
    grep '^<html' "$page"
}

# expect_tag W T F: the page starts at width W and theme T; F is 1 when forced.
expect_tag() {
    want="<html lang=\"en\" data-start-width=\"$1\" data-start-theme=\"$2\">"
    [ "$3" = 1 ] && want="<html lang=\"en\" data-start-width=\"$1\" data-start-theme=\"$2\" data-forced=\"1\">"
    got=$(html_tag)
    [ "$got" = "$want" ] || bad "html tag is [$got], want [$want]"
}

stderr_lines() {
    wc -l < "$d/err" | tr -d ' '
}

no_pwned() {
    found=$(find "$tmp" "$root" -maxdepth 3 -name PWNED 2>/dev/null)
    [ -z "$found" ] || bad "PWNED file made: $found"
}

# expect_usage: exit 2, usage on stderr, no page.
expect_usage() {
    [ "$rc" = 2 ] || bad "exit $rc, want 2 for: $*"
    grep -q '^usage: md-open.sh' "$d/err" || bad "no usage on stderr for: $*"
    [ ! -e "$page" ] || bad "page made for: $*"
}

run() {
    case_name=$1
    if ( "case_$1" ); then
        echo "ok $1"
    else
        echo "FAIL $1"
        fail=1
    fi
}

# --- flags ------------------------------------------------------------------

case_flags_ok() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "plain run exit $rc"
    [ -s "$page" ] || bad "no page"
    [ ! -s "$d/out" ] || bad "--no-open printed: $(cat "$d/out")"
    [ ! -s "$d/err" ] || bad "stderr: $(cat "$d/err")"
    expect_tag 70 auto 0
    for w in 60 70 80; do
        for t in light dark auto; do
            rm -f "$page"
            md --width "$w" --theme "$t" --no-open --out "$page" "$sample"
            [ "$rc" = 0 ] || bad "exit $rc for --width $w --theme $t"
            expect_tag "$w" "$t" 1
        done
    done
    rm -f "$page"
    md --theme dark --no-open --no-cache --browser firefox --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc for --theme dark --no-cache --browser firefox"
    [ ! -s "$d/err" ] || bad "stderr: $(cat "$d/err")"
    expect_tag 70 dark 1
    rm -f "$page"
    md --width 80 --no-open --out "$page" "$sample"
    expect_tag 80 auto 1
    # The flag beats the file.
    write_ini '[View]' 'width=60' 'theme=light'
    rm -f "$page"
    md --width 80 --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc with ini"
    expect_tag 80 light 1
    exit 0
}

case_flag_width_bad() {
    fresh
    for v in 75 0 600 '' '70;x' '70 ' ' 70' '<script>' '$(touch PWNED)' auto; do
        rm -f "$page"
        md --width "$v" --no-open --out "$page" "$sample"
        expect_usage "--width [$v]"
    done
    no_pwned
    exit 0
}

case_flag_theme_bad() {
    fresh
    for v in blue Light DARK '' 'dark;x' 'auto ' '"><script>' '$(touch PWNED)' 70; do
        rm -f "$page"
        md --theme "$v" --no-open --out "$page" "$sample"
        expect_usage "--theme [$v]"
    done
    no_pwned
    exit 0
}

case_flag_width_missing() {
    fresh
    md --width
    expect_usage "--width"
    md --no-open --out "$page" "$sample" --width
    expect_usage "file then --width"
    md --no-open --out "$page" "$sample" --theme
    expect_usage "file then --theme"
    md --no-open --out "$page" "$sample" --browser
    expect_usage "file then --browser"
    md --width --theme dark --no-open --out "$page" "$sample"
    expect_usage "--width --theme dark"
    md --no-open --out "$page"
    expect_usage "no file"
    exit 0
}

# --- md-view.ini ------------------------------------------------------------

case_ini_valid() {
    fresh
    cr=$(printf '\r')
    write_ini \
        '# a comment with width=80' \
        '; another comment theme=dark' \
        'width=80' \
        '[Other]' \
        'width=80' \
        'theme=dark' \
        "  [View]  $cr" \
        "  width  =  80  $cr" \
        "width=99$cr" \
        "theme = light$cr" \
        "# width=70$cr" \
        "Width=70$cr" \
        "browser = auto$cr" \
        '' \
        '[Last]' \
        'theme=dark'
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    [ ! -s "$d/err" ] || bad "stderr: $(cat "$d/err")"
    expect_tag 80 light 0
    # The last valid line of a key wins; a final line with no newline counts.
    printf '[View]\nwidth=80\nwidth=60\ntheme=dark' > "$ini"
    rm -f "$page"
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    expect_tag 60 dark 0
    # An empty file gives the defaults.
    : > "$ini"
    rm -f "$page"
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc for an empty ini"
    expect_tag 70 auto 0
    exit 0
}

case_ini_hostile() {
    fresh
    write_ini '[View]' \
        'width=<script>alert(1)</script>' \
        'theme="><script>alert(1)</script>' \
        'width=$(touch PWNED)' \
        'theme=`touch PWNED`' \
        'width=70;touch PWNED' \
        'theme=dark;touch PWNED' \
        'browser=$(touch PWNED)' \
        'browser=`touch PWNED`'
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    expect_tag 70 auto 0
    if grep -q 'alert(1)' "$page"; then bad "a config value reached the page"; fi
    if grep -q 'touch PWNED' "$page"; then bad "a config value reached the page"; fi
    [ "$(stderr_lines)" = 1 ] || bad "want one stderr line, got: $(cat "$d/err")"
    no_pwned
    # A binary and very long file still gives the defaults.
    head -c 4096 /dev/urandom > "$ini" 2>/dev/null || bad "random ini"
    rm -f "$page"
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc for a binary ini"
    expect_tag 70 auto 0
    no_pwned
    exit 0
}

case_ini_missing() {
    fresh
    [ ! -e "$ini" ] || bad "ini exists before the run"
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    [ ! -s "$d/err" ] || bad "stderr: $(cat "$d/err")"
    expect_tag 70 auto 0
    [ ! -e "$ini" ] || bad "a normal run made md-view.ini"
    # An ini that is a folder, or that cannot be read, gives the defaults too.
    mkdir -p "$ini" || bad "mkdir"
    rm -f "$page"
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc for an ini folder"
    expect_tag 70 auto 0
    rmdir "$ini" || bad "rmdir"
    write_ini '[View]' 'width=80'
    chmod 000 "$ini" || bad "chmod"
    if [ ! -r "$ini" ]; then
        rm -f "$page"
        md --no-open --out "$page" "$sample"
        [ "$rc" = 0 ] || bad "exit $rc for an unreadable ini"
        expect_tag 70 auto 0
    fi
    chmod 600 "$ini"
    exit 0
}

case_ini_browser_bad() {
    fresh
    printf '#!/bin/sh\nexit 0\n' > "$d/notexec" || bad "write"
    chmod 644 "$d/notexec" || bad "chmod"
    for v in netscape Firefox 'fire fox' 'a b;c' bin/sh "$d/notexec" "$d" /nonexistent/browser 'safari;x'; do
        write_ini '[View]' "browser=$v"
        rm -f "$page"
        md --no-open --out "$page" "$sample"
        [ "$rc" = 0 ] || bad "exit $rc for browser=[$v]"
        [ -s "$page" ] || bad "no page for browser=[$v]"
        [ "$(stderr_lines)" = 1 ] || bad "want one stderr line for browser=[$v], got: $(cat "$d/err")"
        grep -q 'browser' "$d/err" || bad "stderr does not name the browser for [$v]"
        expect_tag 70 auto 0
    done
    # Accepted values give no message.
    for v in auto firefox chrome safari /bin/sh; do
        write_ini '[View]' "browser=$v"
        rm -f "$page"
        md --no-open --out "$page" "$sample"
        [ "$rc" = 0 ] || bad "exit $rc for browser=[$v]"
        [ ! -s "$d/err" ] || bad "stderr for browser=[$v]: $(cat "$d/err")"
    done
    # The same rule for the flag.
    md --browser 'a b;c' --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc for --browser 'a b;c'"
    [ "$(stderr_lines)" = 1 ] || bad "want one stderr line for --browser, got: $(cat "$d/err")"
    exit 0
}

# --- --init-config and --print-association -----------------------------------

case_init_config_writes() {
    fresh
    md --init-config
    [ "$rc" = 0 ] || bad "exit $rc"
    [ -f "$ini" ] || bad "no file at $ini"
    grep -qxF "$ini" "$d/out" || bad "path not printed: $(cat "$d/out")"
    for l in '[View]' 'width=70' 'theme=auto' 'browser=auto'; do
        grep -qxF "$l" "$ini" || bad "sample has no line $l"
    done
    grep -q '^# .*found' "$ini" || bad "sample has no line with the browsers found"
    # Every other line is a comment or empty.
    others=$(grep -v -x -e '\[View\]' -e 'width=70' -e 'theme=auto' -e 'browser=auto' -e '' -e '[#;].*' "$ini")
    [ -z "$others" ] || bad "sample has lines that are not comments: $others"
    # The sample gives the defaults and no message.
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc with the sample"
    [ ! -s "$d/err" ] || bad "stderr with the sample: $(cat "$d/err")"
    expect_tag 70 auto 0
    exit 0
}

case_init_config_keeps() {
    fresh
    write_ini '[View]' 'width=80' '# mine'
    cp "$ini" "$d/before" || bad "cp"
    md --init-config
    [ "$rc" = 0 ] || bad "exit $rc"
    cmp -s "$ini" "$d/before" || bad "the file changed"
    [ -s "$d/out" ] || [ -s "$d/err" ] || bad "no line printed"
    # A dangling link is not replaced and its target is not made.
    rm -f "$ini"
    ln -s "$d/target" "$ini" || bad "ln"
    md --init-config
    [ "$rc" = 0 ] || bad "exit $rc for a link"
    [ -L "$ini" ] || bad "the link was replaced"
    [ ! -e "$d/target" ] || bad "the link target was made"
    exit 0
}

case_print_association_ok() {
    fresh
    md --print-association
    [ "$rc" = 0 ] || bad "exit $rc"
    for l in 'far2l file association for Markdown (add it once):' \
             '  Mask:                              *.md' \
             '  3. In "A file mask or several file masks", type: *.md' \
             '  5. Press OK. Alt+F3 on a .md file now opens it in the browser.'; do
        grep -qxF "$l" "$d/out" || bad "old line missing: $l"
    done
    grep -q '^  View command (used for Alt+F3):    sh .*md-open.sh"\{0,1\} !\.!$' "$d/out" \
        || bad "the association command changed"
    grep -qF "$ini" "$d/out" || bad "no md-view.ini path"
    for w in 'width' 'theme' 'browser' '60' '80' 'light' 'dark' 'auto' 'firefox' 'chrome' 'safari'; do
        grep -q "$w" "$d/out" || bad "accepted value $w not listed"
    done
    exit 0
}

# --- the browser launch -----------------------------------------------------

# open_log LINE...: the opener got exactly these arguments.
expect_open() {
    [ -f "$OPEN_LOG" ] || bad "no opener ran (want: $*)"
    want=$(printf '%s\n' "$#" "$@")
    got=$(cat "$OPEN_LOG")
    [ "$got" = "$want" ] || bad "opener got [$got], want [$want]"
    rm -f "$OPEN_LOG"
}

case_browser_no_shell() {
    fresh
    # Hostile values with --no-open: nothing runs, one line, auto.
    for v in ';touch PWNED' 'firefox;touch PWNED' '$(touch PWNED)' '`touch PWNED`' '/bin/sh;touch PWNED' '/bin/sh -c touch PWNED'; do
        write_ini '[View]' "browser=$v"
        md --no-open --out "$page" "$sample"
        [ "$rc" = 0 ] || bad "exit $rc for browser=[$v]"
        [ "$(stderr_lines)" = 1 ] || bad "want one stderr line for [$v], got: $(cat "$d/err")"
        [ ! -f "$OPEN_LOG" ] || bad "an opener ran with --no-open"
        md --browser "$v" --no-open --out "$page" "$sample"
        [ "$rc" = 0 ] || bad "exit $rc for --browser [$v]"
        [ "$(stderr_lines)" = 1 ] || bad "want one stderr line for --browser [$v], got: $(cat "$d/err")"
    done
    no_pwned
    rm -f "$ini"

    # The launch itself, with stub openers. Hostile values fall back to auto,
    # which runs the first stub ("open"), with the page path as one argument.
    pg="$d/a page;\$(touch PWNED).html"
    for v in ';touch PWNED' '$(touch PWNED)' 'firefox;touch PWNED'; do
        md --browser "$v" --out "$pg" "$sample"
        no_pwned
        [ "$rc" = 0 ] || bad "exit $rc for a launch with [$v]"
        expect_open "$pg"
    done
    no_pwned

    # An executable whose name holds shell text runs as one plain path.
    mkdir "$d/bin" || bad "mkdir"
    exe="$d/bin/x;touch PWNED \$(touch PWNED)"
    cp "$tmp/stub/opener" "$exe" || bad "cp"
    write_ini '[View]' "browser=$exe"
    md --out "$pg" "$sample"
    no_pwned
    [ "$rc" = 0 ] || bad "exit $rc for a path browser"
    [ ! -s "$d/err" ] || bad "stderr for a path browser: $(cat "$d/err")"
    expect_open "$pg"
    no_pwned
    rm -f "$ini"

    # A single-word $BROWSER runs first; one with shell text is ignored with one line.
    cp "$tmp/stub/opener" "$d/bin/mybrowser" || bad "cp"
    PATH=$d/bin:$PATH
    BROWSER=mybrowser
    export PATH BROWSER
    md --out "$pg" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc for BROWSER=mybrowser"
    expect_open "$pg"
    BROWSER='mybrowser;touch PWNED'
    md --out "$pg" "$sample"
    no_pwned
    [ "$rc" = 0 ] || bad "exit $rc for a hostile BROWSER"
    [ "$(stderr_lines)" = 1 ] || bad "want one stderr line for a hostile BROWSER, got: $(cat "$d/err")"
    expect_open "$pg"
    unset BROWSER
    no_pwned

    # A word maps to its command.
    if [ "$(uname -s)" = Darwin ]; then
        md --browser chrome --out "$pg" "$sample"
        [ "$rc" = 0 ] || bad "exit $rc for chrome"
        expect_open -a 'Google Chrome' "$pg"
        md --browser firefox --out "$pg" "$sample"
        expect_open -a Firefox "$pg"
        md --browser safari --out "$pg" "$sample"
        expect_open -a Safari "$pg"
    else
        cp "$tmp/stub/opener" "$d/bin/firefox" || bad "cp"
        md --browser firefox --out "$pg" "$sample"
        [ "$rc" = 0 ] || bad "exit $rc for firefox"
        expect_open "$pg"
    fi

    # A failed opener: one line and exit 1.
    OPEN_RC=1
    export OPEN_RC
    md --out "$pg" "$sample"
    [ "$rc" = 1 ] || bad "exit $rc for a failed opener, want 1"
    [ "$(stderr_lines)" = 1 ] || bad "want one stderr line for a failed opener, got: $(cat "$d/err")"
    unset OPEN_RC
    no_pwned
    exit 0
}

# --- hostile file names -----------------------------------------------------

case_file_name_hostile() {
    fresh
    mkdir "$d/a dir \"<zq>\$(touch PWNED)" || bad "mkdir"
    for n in 'q"<script>alert(1).md' '$(touch PWNED).md' '`touch PWNED`;x.md' "it's & <zq>.md" '-dash.md'; do
        f="$d/a dir \"<zq>\$(touch PWNED)/$n"
        cp "$sample" "$f" || bad "cp $n"
        rm -f "$page"
        md --no-open --out "$page" -- "$f"
        [ "$rc" = 0 ] || bad "exit $rc for [$n]: $(cat "$d/err")"
        [ -s "$page" ] || bad "no page for [$n]"
        title=$(printf '%s' "$n" | sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g' -e 's/"/\&quot;/g')
        grep -qxF "<title>$title</title>" "$page" || bad "title not escaped for [$n]"
        if grep -qF '<script>alert(1)' "$page"; then bad "the name reached the page raw for [$n]"; fi
        if grep -qF '<zq>' "$page"; then bad "a raw <zq> reached the page for [$n]"; fi
        expect_tag 70 auto 0
    done
    # The same names in the work folder, given as relative paths.
    cd "$d/a dir \"<zq>\$(touch PWNED)" || bad "cd"
    md --no-open --out "$page" '$(touch PWNED).md'
    [ "$rc" = 0 ] || bad "exit $rc for a relative hostile name"
    md --no-open --out "$page" -dash.md
    [ "$rc" = 0 ] || bad "exit $rc for a name that starts with -"
    no_pwned
    exit 0
}

# --- vendored files ---------------------------------------------------------

vendored='github-markdown-light.css github-markdown-dark.css highlight.min.js hljs-github.min.css hljs-github-dark.min.css marked.min.js'

# The page puts each vendored file inside a style or script element, so no file
# may end that element. Only marked.min.js and highlight.min.js may hold one "<!--".
case_vendor_no_close_tag() {
    for f in $vendored; do
        p=$vendor/$f
        [ -f "$p" ] || bad "missing $p"
        n=$(grep -o -i -e '</script' -e '</style' "$p" | wc -l | tr -d ' ')
        [ "$n" = 0 ] || bad "$f holds $n closing script or style tags"
        n=$(grep -o -e '<!--' "$p" | wc -l | tr -d ' ')
        case $f in
            marked.min.js|highlight.min.js) [ "$n" -le 1 ] || bad "$f holds $n <!--" ;;
            *) [ "$n" = 0 ] || bad "$f holds $n <!--" ;;
        esac
    done
    exit 0
}

# check_sha LICENSE FILE...: each FILE is listed in LICENSE with its sha384.
check_sha() {
    lic_name=$1
    lic=$vendor/$1
    shift
    [ -f "$lic" ] || bad "missing $lic"
    for f in "$@"; do
        want=$(sed -n "s/^- \`$f\`: sha384-\([A-Za-z0-9+/=]*\)\$/\1/p" "$lic")
        [ -n "$want" ] || bad "$lic does not list the sha384 of $f"
        [ "$(printf '%s\n' "$want" | wc -l | tr -d ' ')" = 1 ] || bad "$lic lists $f twice"
        got=$(openssl dgst -sha384 -binary "$vendor/$f" | openssl base64 -A) || bad "openssl failed on $f"
        [ "$got" = "$want" ] || bad "$f has sha384-$got, $lic_name says sha384-$want"
    done
}

case_vendor_sha384() {
    command -v openssl > /dev/null 2>&1 || bad "openssl not found"
    check_sha github-markdown.LICENSE.md github-markdown-light.css github-markdown-dark.css
    check_sha highlight.LICENSE.md highlight.min.js hljs-github.min.css hljs-github-dark.min.css
    exit 0
}

for c in flags_ok flag_width_bad flag_theme_bad flag_width_missing \
         ini_valid ini_hostile ini_missing ini_browser_bad \
         init_config_writes init_config_keeps print_association_ok \
         browser_no_shell file_name_hostile \
         vendor_no_close_tag vendor_sha384; do
    run "$c"
done

exit "$fail"
