#!/bin/sh
# Test of far2l/bootstrap/md-open.sh (story markdown-browser-github-look).
# Checks the flags, the defaults file md-view.ini, the browser launch, hostile
# file names and config values, the vendored files, the page template and the
# stable cache page and its reuse. It needs only sh and
# common tools (openssl for the sha384 case). It does not need the far2l binary.
#
# usage: sh testing/markdown/browser-page.sh
#   MD_OPEN     the script under test (default far2l/bootstrap/md-open.sh)
#   VENDOR_DIR  the folder of the vendored files (default: the folder of MD_OPEN)
#
# Each case runs in a subshell with a fresh HOME, XDG_CONFIG_HOME,
# XDG_CACHE_HOME and TMPDIR under one temporary folder. Stub "open" and
# "xdg-open" commands come first in PATH, so no real browser ever starts.
# Prints "ok <case>", "skip <case>" (the case does not apply on this system) or
# "FAIL <case>". It exits 1 on any failure. A skip does not fail the run.

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

# skip TEXT: the case does not apply on this system. Only called inside a case subshell.
skip() {
    echo "  $case_name: skipped, $*" >&2
    exit 77
}

run() {
    case_name=$1
    ( "case_$1" )
    st=$?
    case $st in
        0) echo "ok $1" ;;
        77) echo "skip $1" ;;
        *) echo "FAIL $1"; fail=1 ;;
    esac
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
    dd if=/dev/urandom of="$ini" bs=4096 count=1 2>/dev/null || bad "random ini"
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

# browser= may name a folder that ends in .app: on macOS it runs open -a with it.
case_ini_browser_app_dir() {
    fresh
    if [ "$(uname -s)" != Darwin ]; then
        skip "open -a is macOS only"
    fi
    mkdir "$d/Tool.app" || bad "mkdir"
    write_ini '[View]' "browser=$d/Tool.app"
    md --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    [ ! -s "$d/err" ] || bad "stderr: $(cat "$d/err")"
    expect_open -a "$d/Tool.app" "$page"
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

# A $BROWSER that starts with "-" is not a command name: it is ignored with one
# line, and auto runs.
case_browser_env_dash() {
    fresh
    mkdir "$d/bin" || bad "mkdir"
    printf '#!/bin/sh\n: > "%s"\n' "$d/dashran" > "$d/bin/-mybrowser" || bad "write"
    chmod 755 "$d/bin/-mybrowser" || bad "chmod"
    PATH=$d/bin:$PATH
    BROWSER=-mybrowser
    export PATH BROWSER
    md --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc for BROWSER=-mybrowser"
    [ "$(stderr_lines)" = 1 ] || bad "want one stderr line for BROWSER=-mybrowser, got: $(cat "$d/err")"
    [ ! -e "$d/dashran" ] || bad "the BROWSER value ran"
    expect_open "$page"
    unset BROWSER
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

# --- the page ---------------------------------------------------------------

# line_of TEXT: sets ln to the number of the one line of the page that holds TEXT.
line_of() {
    ln=$(grep -n -F -- "$1" "$page" | cut -d: -f1)
    [ "$(printf '%s\n' "$ln" | grep -c .)" = 1 ] || bad "want one line with [$1]"
}

# style_block_is ID FILE: the style element that starts with ID holds FILE, byte for byte at its start.
style_block_is() {
    sed -n -e "/^<style id=\"$1\"/,/^<\/style>\$/p" "$page" | sed -e '1d' -e '$d' > "$d/block" || return 1
    n=$(wc -c < "$2" | tr -d ' ')
    [ "$(wc -c < "$d/block" | tr -d ' ')" -ge "$n" ] || return 1
    dd if="$d/block" bs="$n" count=1 2>/dev/null | cmp -s - "$2"
}

case_page_has_github_css() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    for pair in gh-light:github-markdown-light.css gh-dark:github-markdown-dark.css \
                hl-light:hljs-github.min.css hl-dark:hljs-github-dark.min.css; do
        id=${pair%%:*}
        f=${pair#*:}
        [ "$(grep -c -F "<style id=\"$id\"" "$page")" = 1 ] || bad "want one <style id=\"$id\">"
        style_block_is "$id" "$vendor/$f" || bad "style $id does not hold $f"
    done
    [ "$(grep -c -F '<style>' "$page")" = 0 ] || bad "the old bare style element is still there"
    grep -qF '<style id="page">' "$page" || bad "no style for the page's own rules"
    line_of '<style id="gh-light"'; a=$ln
    line_of '<style id="gh-dark"'; b=$ln
    line_of '<style id="hl-light"'; c=$ln
    line_of '<style id="hl-dark"'; e=$ln
    line_of '<style id="page">'; f=$ln
    [ "$a" -lt "$b" ] && [ "$b" -lt "$c" ] && [ "$c" -lt "$e" ] && [ "$e" -lt "$f" ] \
        || bad "style blocks are not in the order gh-light, gh-dark, hl-light, hl-dark, page"
    grep -qF '<body class="markdown-body">' "$page" || bad "body has no markdown-body class"
    grep -qF '<main id="content" class="markdown-body">' "$page" || bad "#content has no markdown-body class"
    # A missing vendored file stops the run with exit 3 and no page.
    for f in github-markdown-light.css github-markdown-dark.css hljs-github.min.css \
             hljs-github-dark.min.css highlight.min.js; do
        rm -rf "$d/missing"
        mkdir "$d/missing" || bad "mkdir"
        cp "$md_open" "$d/missing/md-open.sh" || bad "cp script"
        for g in marked.min.js github-markdown-light.css github-markdown-dark.css \
                 hljs-github.min.css hljs-github-dark.min.css highlight.min.js; do
            [ "$g" = "$f" ] || cp "$vendor/$g" "$d/missing/" || bad "cp $g"
        done
        rm -f "$page"
        sh "$d/missing/md-open.sh" --no-open --out "$page" "$sample" > "$d/out" 2> "$d/err"
        rc=$?
        [ "$rc" = 3 ] || bad "exit $rc without $f, want 3"
        [ "$(cat "$d/err")" = "md-open: cannot find $f" ] || bad "stderr without $f: $(cat "$d/err")"
        [ ! -e "$page" ] || bad "a page was written without $f"
    done
    exit 0
}

case_page_width_rules() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    for w in 60 70 80; do
        grep -qF "html[data-width=\"$w\"] #content { width: $w% }" "$page" \
            || bad "no width rule for $w"
    done
    grep -qF '#content { max-width: none; margin: 0 auto; box-sizing: border-box }' "$page" \
        || bad "no #content rule for max-width, margin and box-sizing"
    exit 0
}

case_page_start_values() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    expect_tag 70 auto 0
    grep -qxF '<meta name="far2l-md-key" content="w=70;t=auto;f=0">' "$page" \
        || bad "no page key for the defaults"
    rm -f "$page"
    md --width 60 --theme light --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc for a flag run"
    expect_tag 60 light 1
    grep -qxF '<meta name="far2l-md-key" content="w=60;t=light;f=1">' "$page" \
        || bad "no page key for a forced run"
    rm -f "$page"
    write_ini '[View]' 'width=80' 'theme=dark'
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc with ini"
    expect_tag 80 dark 0
    grep -qxF '<meta name="far2l-md-key" content="w=80;t=dark;f=0">' "$page" \
        || bad "no page key from the ini file"
    grep -qF "getAttribute('data-start-width')" "$page" || bad "the page script does not read data-start-width"
    grep -qF "getAttribute('data-start-theme')" "$page" || bad "the page script does not read data-start-theme"
    exit 0
}

case_page_policy_unchanged() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    grep -qxF "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; script-src 'unsafe-inline' https://cdn.jsdelivr.net; style-src 'unsafe-inline'; img-src data: file:\">" "$page" \
        || bad "the page policy changed"
    [ "$(grep -c -F 'Content-Security-Policy' "$page")" = 1 ] || bad "want one policy"
    [ "$(grep -c -F 'unsafe-eval' "$page")" = 0 ] || bad "the page allows eval"
    [ "$(grep -c -F '<link' "$page")" = 0 ] || bad "the page has a link element"
    [ "$(grep -c -i -F '<script src' "$page")" = 0 ] || bad "the page has a script with src"
    grep -qF "var MERMAID_URL = 'https://cdn.jsdelivr.net/npm/mermaid@12.3.0/dist/mermaid.min.js';" "$page" \
        || bad "the mermaid URL changed"
    grep -qF "var MERMAID_SRI = 'sha384-HHW54r639fK42Ip0coGF3mz8oSiGn0eEoWNTLs75X7H63pLSDBa0Tqs0RwGmLlXE';" "$page" \
        || bad "the mermaid SRI hash changed"
    exit 0
}

case_page_narrow_rule() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    line_of 'html[data-width="80"] #content { width: 80% }'; w80=$ln
    line_of '@media (max-width: 699px) {'; media=$ln
    line_of 'html[data-width] #content { width: 100% }'; narrow=$ln
    [ "$media" -gt "$w80" ] && [ "$narrow" -gt "$media" ] \
        || bad "the narrow rule is not after the width rules"
    # The gear is hidden by a rule inside the narrow block, between its @media line
    # and its closing brace. The popover is not hidden below 700 px.
    nb=$(sed -n '/^@media (max-width: 699px) {$/,/^}$/p' "$page")
    printf '%s\n' "$nb" | grep -q 'md-pop' && bad "the popover is hidden under 700 px"
    printf '%s\n' "$nb" | grep -qxF '  #md-gear { display: none }' \
        || bad "the gear is not hidden under 700 px, inside the narrow block"
    exit 0
}

case_page_hljs_after_marked() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    line_of '<script type="text/markdown" id="md">'; l_md=$ln
    line_of 'marked v18.1.3'; l_marked=$ln
    line_of 'Highlight.js v11.11.1'; l_hl=$ln
    line_of "  var MERMAID_URL = 'https://cdn.jsdelivr.net/npm/mermaid@12.3.0/dist/mermaid.min.js';"; l_page=$ln
    [ "$l_md" -lt "$l_marked" ] && [ "$l_marked" -lt "$l_hl" ] && [ "$l_hl" -lt "$l_page" ] \
        || bad "the scripts are not in the order text, marked, highlight, page"
    line_of 'content.innerHTML = marked.parse(src);'; l_parse=$ln
    line_of 'hljs.highlightElement(code)'; l_hlel=$ln
    [ "$l_parse" -lt "$l_hlel" ] || bad "highlight runs before marked renders the text"
    exit 0
}

case_page_mermaid_theme_hook() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    for s in \
        'function usedTheme() {' \
        'function applyTheme() {' \
        'function drawDiagrams(list, batch) {' \
        'function redraw(list) {' \
        "theme: usedTheme() === 'dark' ? 'dark' : 'default'" \
        "matchMedia('(prefers-color-scheme: dark)')" \
        "securityLevel: 'strict'" \
        "it.pre.removeAttribute('data-processed');" \
        'it.pre.textContent = it.src;' \
        'mermaid.run(' \
        "themeQuery.addEventListener('change', onSystemTheme);" \
        "if (themeSetting() === 'auto') applyTheme();" \
        'if (mermaidReady && usedTheme() !== drawnTheme) redraw(items);' \
        'var drawChain = Promise.resolve();' \
        'var drawQueued = false;' \
        'drawChain = drawChain.then(function () {' \
        'if (drawQueued) return;' \
        'drawQueued = false;' \
        'var drawnTheme = null;' \
        'drawnTheme = usedTheme();' \
        'function withTimeout(p, ms, onTimeout) {' \
        'var MERMAID_TIMEOUT = 10000;' \
        'if (!good.length || batch.cancelled) return;'; do
        grep -qF -- "$s" "$page" || bad "no [$s] in the page"
    done
    exit 0
}

# iife: the page script, from its first line "(function () {" to the end of the page.
iife() {
    sed -n '/^(function () {$/,$p' "$page"
}

# The gear and the popover: the markup, the fixed position and the narrow rule.
case_page_gear_markup() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    [ "$(grep -c -F '<button id="md-gear"' "$page")" = 1 ] || bad "want one gear button"
    gear_tag=$(grep -F '<button id="md-gear"' "$page")
    for s in 'type="button"' 'aria-label="Page settings"' 'aria-controls="md-pop"'; do
        case $gear_tag in
            *"$s"*) ;;
            *) bad "gear button has no [$s]" ;;
        esac
    done
    [ "$(grep -c -F '<div id="md-pop" hidden></div>' "$page")" = 1 ] || bad "want one hidden popover element"
    gear=$(grep '^#md-gear {' "$page")
    [ "$(printf '%s\n' "$gear" | grep -c .)" = 1 ] || bad "want one #md-gear rule"
    for s in 'position: fixed' 'top: 8px' 'right: 8px' 'width: 28px' 'height: 28px' 'opacity: .45'; do
        printf '%s\n' "$gear" | grep -qF "$s" || bad "gear rule has no [$s]"
    done
    grep -qxF '#md-gear:hover, #md-gear:focus { opacity: 1 }' "$page" \
        || bad "the gear is not full opacity on hover or focus"
    pop=$(grep '^#md-pop {' "$page")
    [ "$(printf '%s\n' "$pop" | grep -c .)" = 1 ] || bad "want one #md-pop rule"
    printf '%s\n' "$pop" | grep -qF 'position: fixed' || bad "the popover is not fixed"
    # Values reach the page through textContent. The one innerHTML is the marked output.
    [ "$(iife | grep -c -F 'innerHTML')" = 1 ] || bad "innerHTML is used for more than the marked output"
    iife | grep -qF 'content.innerHTML = marked.parse(src);' || bad "the marked output does not use innerHTML"
    exit 0
}

case_page_print_hidden() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    sed -n '/^<style id="page">$/,/^<\/style>$/p' "$page" > "$d/page-style" || bad "sed"
    grep -qxF '@media print { #md-gear, #md-pop { display: none !important } }' "$d/page-style" \
        || bad "no print rule that hides the gear and the popover in the page style"
    exit 0
}

# The saved choice: localStorage only inside try, the two keys, the accepted words and the notice.
case_page_storage_try_catch() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    iife > "$d/script" || bad "no script"
    for k in far2l-md-width far2l-md-theme; do
        grep -qF "'$k'" "$d/script" || bad "no saved key [$k]"
    done
    n=$(grep -c -F 'localStorage' "$d/script")
    [ "$n" -ge 3 ] || bad "want localStorage read, write and remove, got $n lines"
    bare=$(grep -F 'localStorage' "$d/script" | grep -v -F 'try {')
    [ -z "$bare" ] || bad "localStorage outside try: $bare"
    [ "$(grep -c -F 'storageBlocked = true' "$d/script")" -ge 3 ] || bad "a catch block does not set the blocked flag"
    grep -qF 'accepted(storageGet(KEY_WIDTH), WIDTHS, startWidth)' "$d/script" \
        || bad "the saved width is not checked against the accepted words"
    grep -qF 'accepted(storageGet(KEY_THEME), THEMES, startTheme)' "$d/script" \
        || bad "the saved theme is not checked against the accepted words"
    grep -qF "root.getAttribute('data-forced') === '1'" "$d/script" \
        || bad "data-forced does not switch off the saved choice"
    grep -qF 'storageRemove(KEY_WIDTH);' "$d/script" || bad "Reset does not remove the saved width"
    grep -qF 'storageRemove(KEY_THEME);' "$d/script" || bad "Reset does not remove the saved theme"
    grep -qF 'function save(key, value) {' "$d/script" || bad "no save(key, value) wrapper"
    grep -qF 'function load() {' "$d/script" || bad "no load() function"
    [ "$(grep -c -F 'Storage is blocked. Your choice is not kept.' "$page")" = 1 ] || bad "want one storage notice"
    grep -qF 'notice.hidden = !storageBlocked;' "$d/script" \
        || bad "the notice is not shown only when storage is blocked"
    exit 0
}

# The keys: the modifier and field guards come before the keys, and a click outside closes the popover.
case_page_keys_guard() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    [ "$(grep -c -F "addEventListener('keydown'" "$page")" = 1 ] || bad "want one keydown listener"
    grep -qF "addEventListener('keyup'" "$page" && bad "the keys do not use keydown"
    grep -qF "document.addEventListener('click', function (e) {" "$page" || bad "no click outside handler"
    line_of 'if (e.ctrlKey || e.altKey || e.metaKey) return;'; g_mod=$ln
    line_of "if (tag === 'input' || tag === 'textarea' || tag === 'select' || (tg && tg.isContentEditable)) return;"; g_tag=$ln
    line_of "if (e.key === 't')"; k_t=$ln
    line_of "else if (e.key === 'w')"; k_w=$ln
    line_of "else if (e.key === '?')"; k_q=$ln
    [ "$g_mod" -lt "$g_tag" ] && [ "$g_tag" -lt "$k_t" ] && [ "$k_t" -lt "$k_w" ] && [ "$k_w" -lt "$k_q" ] \
        || bad "the key guards do not come before the keys"
    grep -qF "if (!pop.hidden) { closePop(); gear.focus(); }" "$page" \
        || bad "Esc does not close the popover and return focus to the gear"
    grep -qF "else if (e.key === '?') openPop();" "$page" || bad "? does not open the popover"
    exit 0
}

# The paste block: three lines built as text, with the notice above it and the Reset button.
case_page_paste_line() {
    fresh
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc"
    line_of "return ['[View]', 'width=' + root.getAttribute('data-width'), 'theme=' + themeSetting()].join('\\n');"
    line_of 'paste.textContent = pasteText();'
    line_of 'notice.hidden = true;'
    line_of 'pop.appendChild(notice);'; n1=$ln
    line_of 'pop.appendChild(paste);'; n2=$ln
    [ "$n1" -lt "$n2" ] || bad "the notice is not above the paste block"
    line_of "makeButton('Reset', resetChoice)"
    line_of "'Keys: t theme, w width, ? help'"
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

# --- the stable page path and reuse -----------------------------------------

# cache_page FILE: sets cpage to the cache page of FILE, as the spec names it.
cache_page() {
    cdir=$(cd "$(dirname -- "$1")" && pwd -P) || bad "cd for cache_page"
    creal=$cdir/$(basename -- "$1")
    ccrc=$(printf '%s' "$creal" | cksum | sed 's/ .*//')
    cbase=$(basename -- "$1" | sed 's/[^A-Za-z0-9._-]/_/g' | cut -c1-40)
    cpage=${XDG_CACHE_HOME:-$HOME/.cache}/far2l/md/$cbase-$ccrc.html
}

# old FILE... and future FILE...: set the modification time far back or far ahead.
old() {
    touch -t 200001010000 "$@" || bad "touch old $*"
}
future() {
    touch -t 203501010000 "$@" || bad "touch future $*"
}

# mark: adds a line to the cache page that only an old page has. A page that is
# built again loses it. The Markdown text cannot hold it, as "<!--" is escaped.
mark() {
    printf '<!-- test-marker -->\n' >> "$cpage" || bad "mark"
}
marked() {
    grep -qxF '<!-- test-marker -->' "$cpage"
}

# cache_files: the number of entries in the cache folder.
cache_files() {
    ls -A "$XDG_CACHE_HOME/far2l/md" | wc -l | tr -d ' '
}

# no_tmp: no temporary page is left in the cache folder or in TMPDIR.
no_tmp() {
    left=$(find "$d" -name '*.tmp.*' 2>/dev/null)
    [ -z "$left" ] || bad "a temporary file is left: $left"
}

# scratch: a copy of the script and every vendored file in $d/s; md then runs it.
scratch() {
    mkdir -p "$d/s" || bad "mkdir scratch"
    cp "$md_open" "$d/s/md-open.sh" || bad "cp script"
    for g in $vendored; do
        cp "$vendor/$g" "$d/s/" || bad "cp $g"
    done
    md_open=$d/s/md-open.sh
}

case_cache_path_stable() {
    fresh
    cp "$sample" s.md || bad "cp"
    old s.md
    cache_page s.md
    md s.md
    [ "$rc" = 0 ] || bad "exit $rc: $(cat "$d/err")"
    [ ! -s "$d/err" ] || bad "stderr: $(cat "$d/err")"
    [ -f "$cpage" ] || bad "no page at $cpage"
    expect_open "$cpage"
    [ "$(cache_files)" = 1 ] || bad "want one file in the cache folder, got $(ls -A "$XDG_CACHE_HOME/far2l/md")"
    [ -z "$(ls -A "$TMPDIR")" ] || bad "a page was made in TMPDIR: $(ls -A "$TMPDIR")"
    grep -qxF '<meta name="far2l-md-key" content="w=70;t=auto;f=0">' "$cpage" || bad "no page key"
    # The same file by an absolute path, by another relative path and through a
    # linked folder gives the same page.
    md --no-cache --no-open "$d/work/s.md"
    [ "$rc" = 0 ] || bad "exit $rc for the absolute path"
    mkdir "$d/other" || bad "mkdir"
    ln -s "$d/work" "$d/link" || bad "ln"
    cd "$d/other" || bad "cd"
    md --no-cache --no-open ../work/s.md
    [ "$rc" = 0 ] || bad "exit $rc for ../work/s.md"
    md --no-cache "$d/link/s.md"
    [ "$rc" = 0 ] || bad "exit $rc for the linked folder"
    expect_open "$cpage"
    [ "$(cache_files)" = 1 ] || bad "one file gave more pages: $(ls -A "$XDG_CACHE_HOME/far2l/md")"
    # With no XDG_CACHE_HOME, the folder is under HOME.
    unset XDG_CACHE_HOME
    cd "$d/work" || bad "cd"
    cache_page s.md
    case $cpage in
        "$HOME/.cache/far2l/md/s.md-"*.html) ;;
        *) bad "page without XDG_CACHE_HOME is $cpage" ;;
    esac
    md --no-open s.md
    [ "$rc" = 0 ] || bad "exit $rc without XDG_CACHE_HOME"
    [ -f "$cpage" ] || bad "no page at $cpage"
    no_tmp
    exit 0
}

case_cache_two_files() {
    fresh
    mkdir a b || bad "mkdir"
    printf '# alpha-text\n' > a/note.md || bad "write"
    printf '# beta-text\n' > b/note.md || bad "write"
    md --no-open a/note.md
    [ "$rc" = 0 ] || bad "exit $rc for a/note.md"
    md --no-open b/note.md
    [ "$rc" = 0 ] || bad "exit $rc for b/note.md"
    cache_page a/note.md; pa=$cpage
    cache_page b/note.md; pb=$cpage
    [ "$pa" != "$pb" ] || bad "two files have one page name $pa"
    [ "$(cache_files)" = 2 ] || bad "want two pages, got $(ls -A "$XDG_CACHE_HOME/far2l/md")"
    grep -qF 'alpha-text' "$pa" || bad "the page of a/note.md does not hold its text"
    grep -qF 'beta-text' "$pb" || bad "the page of b/note.md does not hold its text"
    if grep -qF 'beta-text' "$pa"; then bad "the page of a/note.md holds the text of b/note.md"; fi
    exit 0
}

case_cache_reuse() {
    fresh
    cp "$sample" s.md || bad "cp"
    old s.md
    cache_page s.md
    md --no-open s.md
    [ "$rc" = 0 ] || bad "exit $rc"
    mark
    cp "$cpage" "$d/before" || bad "cp"
    # The folder cannot take a new file now, so a write fails the run.
    chmod 500 "$XDG_CACHE_HOME/far2l/md" || bad "chmod"
    md s.md
    st=$rc
    chmod 700 "$XDG_CACHE_HOME/far2l/md" || bad "chmod back"
    [ "$st" = 0 ] || bad "exit $st for a reuse: $(cat "$d/err")"
    [ ! -s "$d/err" ] || bad "stderr for a reuse: $(cat "$d/err")"
    expect_open "$cpage"
    cmp -s "$cpage" "$d/before" || bad "the page changed on a reuse"
    marked || bad "the page was built again"
    md --no-open s.md
    [ "$rc" = 0 ] || bad "exit $rc for a reuse with --no-open"
    marked || bad "the page was built again with --no-open"
    [ "$(cache_files)" = 1 ] || bad "want one page, got $(ls -A "$XDG_CACHE_HOME/far2l/md")"
    no_tmp
    exit 0
}

case_cache_rebuild_on_edit() {
    fresh
    scratch
    cp "$sample" s.md || bad "cp"
    cache_page s.md
    for f in s.md "$d/s/md-open.sh" $vendored; do
        case $f in
            /*|s.md) ;;
            *) f=$d/s/$f ;;
        esac
        old s.md "$d/s/md-open.sh"
        for g in $vendored; do old "$d/s/$g"; done
        md --no-open s.md
        [ "$rc" = 0 ] || bad "exit $rc before an edit of $f"
        mark
        md --no-open s.md
        marked || bad "no reuse before an edit of $f"
        future "$f"
        md --no-open s.md
        [ "$rc" = 0 ] || bad "exit $rc after an edit of $f"
        if marked; then bad "the page was not built again after an edit of $f"; fi
    done
    # A real edit of the text reaches the page.
    old s.md
    md --no-open s.md
    mark
    printf '\nedited-text-zq\n' >> s.md || bad "edit"
    future s.md
    md --no-open s.md
    [ "$rc" = 0 ] || bad "exit $rc after a text edit"
    grep -qF 'edited-text-zq' "$cpage" || bad "the edited text is not in the page"
    no_tmp
    exit 0
}

case_cache_rebuild_on_flag() {
    fresh
    cp "$sample" s.md || bad "cp"
    old s.md
    cache_page s.md
    md --no-open s.md
    mark
    md --width 80 --no-open s.md
    [ "$rc" = 0 ] || bad "exit $rc for --width 80"
    if marked; then bad "the page was not built again for --width 80"; fi
    grep -qxF '<meta name="far2l-md-key" content="w=80;t=auto;f=1">' "$cpage" || bad "no key for --width 80"
    expect_tag_at "$cpage" 80 auto 1
    mark
    md --width 80 --no-open s.md
    marked || bad "no reuse for the same flag"
    md --theme dark --width 80 --no-open s.md
    if marked; then bad "the page was not built again for --theme dark"; fi
    mark
    # A run with no flag is not forced, so the forced page is not used.
    md --width 80 --no-open s.md
    if marked; then bad "the page was not built again for --width 80 after --theme dark"; fi
    mark
    md --no-open s.md
    if marked; then bad "the page was not built again for no flag after a forced run"; fi
    grep -qxF '<meta name="far2l-md-key" content="w=70;t=auto;f=0">' "$cpage" || bad "no key for no flag"
    exit 0
}

# expect_tag_at PAGE W T F: as expect_tag, for another page.
expect_tag_at() {
    page=$1
    shift
    expect_tag "$@"
}

case_cache_rebuild_on_ini() {
    fresh
    cp "$sample" s.md || bad "cp"
    old s.md
    cache_page s.md
    md --no-open s.md
    mark
    # An old ini that gives the same key: the page records that there was none.
    write_ini '[View]' 'browser=auto'
    old "$ini"
    md --no-open s.md
    [ "$rc" = 0 ] || bad "exit $rc with a new ini"
    if marked; then bad "the page was not built again when md-view.ini was added"; fi
    mark
    md --no-open s.md
    marked || bad "no reuse with the same ini"
    # The same time, another size.
    write_ini '[View]' 'browser=safari'
    old "$ini"
    md --no-open s.md
    if marked; then bad "the page was not built again when md-view.ini changed size"; fi
    mark
    # A newer ini.
    future "$ini"
    md --no-open s.md
    if marked; then bad "the page was not built again for a newer md-view.ini"; fi
    old "$ini"
    md --no-open s.md
    mark
    md --no-open s.md
    marked || bad "no reuse with the same ini"
    # A deleted ini.
    rm -f "$ini"
    md --no-open s.md
    [ "$rc" = 0 ] || bad "exit $rc after the ini was deleted"
    if marked; then bad "the page was not built again when md-view.ini was deleted"; fi
    mark
    md --no-open s.md
    marked || bad "no reuse with no ini"
    # A changed value changes the key.
    write_ini '[View]' 'width=60'
    old "$ini"
    md --no-open s.md
    grep -qxF '<meta name="far2l-md-key" content="w=60;t=auto;f=0">' "$cpage" || bad "no key from the new ini"
    exit 0
}

case_cache_no_cache_flag() {
    fresh
    cp "$sample" s.md || bad "cp"
    old s.md
    cache_page s.md
    md --no-open s.md
    mark
    md --no-cache --no-open s.md
    [ "$rc" = 0 ] || bad "exit $rc for --no-cache"
    [ -f "$cpage" ] || bad "no page at the cache path after --no-cache"
    if marked; then bad "--no-cache did not build the page again"; fi
    [ "$(cache_files)" = 1 ] || bad "want one page, got $(ls -A "$XDG_CACHE_HOME/far2l/md")"
    mark
    # --out builds its own page and leaves the cache page.
    md --no-open --out "$page" s.md
    [ "$rc" = 0 ] || bad "exit $rc for --out"
    [ -s "$page" ] || bad "no --out page"
    marked || bad "--out changed the cache page"
    if grep -qxF '<!-- test-marker -->' "$page"; then bad "--out reused the cache page"; fi
    printf '<!-- test-marker -->\n' >> "$page" || bad "mark --out page"
    md --no-open --out "$page" s.md
    if grep -qxF '<!-- test-marker -->' "$page"; then bad "--out reused its old page"; fi
    no_tmp
    exit 0
}

case_cache_dir_mode() {
    fresh
    umask 022
    cp "$sample" s.md || bad "cp"
    cache_page s.md
    md --no-open s.md
    [ "$rc" = 0 ] || bad "exit $rc"
    case $(ls -ld "$XDG_CACHE_HOME/far2l/md") in
        drwx------*) ;;
        *) bad "folder mode: $(ls -ld "$XDG_CACHE_HOME/far2l/md")" ;;
    esac
    case $(ls -l "$cpage") in
        -rw-------*) ;;
        *) bad "page mode: $(ls -l "$cpage")" ;;
    esac
    # An old folder with a wider mode gets 700.
    rm -rf "$XDG_CACHE_HOME/far2l"
    mkdir -p "$XDG_CACHE_HOME/far2l/md" || bad "mkdir"
    chmod 755 "$XDG_CACHE_HOME/far2l/md" || bad "chmod"
    md --no-open s.md
    [ "$rc" = 0 ] || bad "exit $rc with an old folder"
    case $(ls -ld "$XDG_CACHE_HOME/far2l/md") in
        drwx------*) ;;
        *) bad "old folder mode: $(ls -ld "$XDG_CACHE_HOME/far2l/md")" ;;
    esac
    case $(ls -l "$cpage") in
        -rw-------*) ;;
        *) bad "page mode in an old folder: $(ls -l "$cpage")" ;;
    esac
    exit 0
}

# expect_tmp_page: the run used the old temporary page and opened it.
expect_tmp_page() {
    [ "$rc" = 0 ] || bad "exit $rc for [$1]: $(cat "$d/err")"
    [ "$(stderr_lines)" = 1 ] || bad "want one stderr line for [$1], got: $(cat "$d/err")"
    [ -f "$OPEN_LOG" ] || bad "no opener ran for [$1]"
    got=$(sed -n 2p "$OPEN_LOG")
    [ "$(sed -n 1p "$OPEN_LOG")" = 1 ] || bad "opener got [$(cat "$OPEN_LOG")] for [$1]"
    case $got in
        "$TMPDIR/far2l-md-"*.html) ;;
        *) bad "opened [$got] for [$1], want a page in TMPDIR" ;;
    esac
    [ -s "$got" ] || bad "no page at $got for [$1]"
    rm -f "$OPEN_LOG" "$got"
}

case_cache_dir_fallback() {
    fresh
    cp "$sample" s.md || bad "cp"
    # The cache home is a file, so the folder cannot be made.
    : > "$d/afile"
    XDG_CACHE_HOME=$d/afile
    md s.md
    expect_tmp_page "cache home is a file"
    # The folder is a link to another folder: nothing is written there.
    XDG_CACHE_HOME=$d/cache
    mkdir -p "$d/elsewhere" "$XDG_CACHE_HOME/far2l" || bad "mkdir"
    ln -s "$d/elsewhere" "$XDG_CACHE_HOME/far2l/md" || bad "ln"
    md s.md
    expect_tmp_page "folder is a link"
    [ -z "$(ls -A "$d/elsewhere")" ] || bad "a file was written through the folder link"
    [ -L "$XDG_CACHE_HOME/far2l/md" ] || bad "the folder link was replaced"
    rm -f "$XDG_CACHE_HOME/far2l/md"
    # The page path is a link: it is not written through or replaced.
    mkdir "$XDG_CACHE_HOME/far2l/md" || bad "mkdir"
    cache_page s.md
    printf 'victim\n' > "$d/victim" || bad "write"
    ln -s "$d/victim" "$cpage" || bad "ln"
    md s.md
    expect_tmp_page "page is a link"
    [ "$(cat "$d/victim")" = victim ] || bad "the page link target was written"
    [ -L "$cpage" ] || bad "the page link was replaced"
    # A dangling link is not followed either.
    rm -f "$cpage" "$d/victim"
    ln -s "$d/victim" "$cpage" || bad "ln"
    md --no-cache s.md
    expect_tmp_page "page is a dangling link"
    [ ! -e "$d/victim" ] || bad "the dangling link target was made"
    no_tmp
    exit 0
}

case_cache_name_safe() {
    fresh
    long=$(printf 'n%.0s' 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 \
        31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60)
    uni=$(printf 'caf\303\251 \342\200\224 x.md')
    mkdir "$d/w \"q\" \$(touch PWNED)" || bad "mkdir"
    for n in 'q"<script>.md' "it's a b.md" '$(touch PWNED).md' '`touch PWNED`;x&y.md' \
             '-dash.md' "$long.md" "$uni" '..md'; do
        f="$d/w \"q\" \$(touch PWNED)/$n"
        cp "$sample" "$f" || bad "cp [$n]"
        md --no-open -- "$f"
        [ "$rc" = 0 ] || bad "exit $rc for [$n]: $(cat "$d/err")"
        [ ! -s "$d/err" ] || bad "stderr for [$n]: $(cat "$d/err")"
        cache_page "$f"
        [ -f "$cpage" ] || bad "no page $cpage for [$n]"
        name=${cpage##*/}
        case $name in
            *[!A-Za-z0-9._-]*) bad "page name [$name] has an unsafe character for [$n]" ;;
        esac
        b=${name%-*.html}
        [ "${#b}" -le 40 ] || bad "page base [$b] is longer than 40 for [$n]"
        rest=${name#"$b"-}
        case ${rest%.html} in
            ''|*[!0-9]*) bad "page crc in [$name] is not a number for [$n]" ;;
        esac
    done
    cache_page "$d/w \"q\" \$(touch PWNED)/$long.md"
    case ${cpage##*/} in
        nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn-*.html) ;;
        *) bad "long name not cut to 40: ${cpage##*/}" ;;
    esac
    cache_page "$d/w \"q\" \$(touch PWNED)/q\"<script>.md"
    case ${cpage##*/} in
        q__script_.md-*.html) ;;
        *) bad "name not changed to _: ${cpage##*/}" ;;
    esac
    # Every page is in the cache folder, and nothing else was made.
    n=$(find "$XDG_CACHE_HOME" -type f | wc -l | tr -d ' ')
    [ "$n" = 8 ] || bad "want 8 pages in the cache, got $n: $(find "$XDG_CACHE_HOME" -type f)"
    no_pwned
    no_tmp
    exit 0
}

# --out keeps the old direct write: an existing file keeps its mode, a link is
# written through, and a device works. No temporary file is made.
case_out_direct_write() {
    fresh
    cp "$sample" s.md || bad "cp"
    printf 'old\n' > "$page" || bad "write"
    chmod 640 "$page" || bad "chmod"
    md --no-open --out "$page" s.md
    [ "$rc" = 0 ] || bad "exit $rc for an existing file: $(cat "$d/err")"
    grep -qF '<meta name="far2l-md-key"' "$page" || bad "the existing file was not written"
    case $(ls -l "$page") in
        -rw-r-----*) ;;
        *) bad "the existing file lost its mode: $(ls -l "$page")" ;;
    esac
    rm -f "$page"
    : > "$d/target"
    ln -s "$d/target" "$page" || bad "ln"
    md --no-open --out "$page" s.md
    [ "$rc" = 0 ] || bad "exit $rc for a link: $(cat "$d/err")"
    [ -L "$page" ] || bad "the --out link was replaced"
    grep -qF '<meta name="far2l-md-key"' "$d/target" || bad "the page was not written through the link"
    md --no-open --out /dev/stdout s.md
    [ "$rc" = 0 ] || bad "exit $rc for --out /dev/stdout: $(cat "$d/err")"
    grep -qF '<meta name="far2l-md-key"' "$d/out" || bad "--out /dev/stdout printed no page"
    [ ! -d "$XDG_CACHE_HOME/far2l" ] || bad "--out made the cache folder"
    no_tmp
    exit 0
}

# The browser gets the umask of the caller, after a build and after a reuse.
case_cache_umask_restored() {
    fresh
    umask 022
    cp "$sample" s.md || bad "cp"
    old s.md
    mkdir "$d/bin" || bad "mkdir"
    printf '#!/bin/sh\numask > "%s"\n' "$d/umask" > "$d/bin/um" || bad "write"
    chmod 755 "$d/bin/um" || bad "chmod"
    for step in build reuse; do
        rm -f "$d/umask"
        md --browser "$d/bin/um" s.md
        [ "$rc" = 0 ] || bad "exit $rc on $step: $(cat "$d/err")"
        [ -f "$d/umask" ] || bad "the browser did not run on $step"
        [ "$(cat "$d/umask")" = 0022 ] || bad "the browser got umask $(cat "$d/umask") on $step, want 0022"
    done
    cache_page s.md
    case $(ls -l "$cpage") in
        -rw-------*) ;;
        *) bad "page mode: $(ls -l "$cpage")" ;;
    esac
    exit 0
}

# A build that stops after its temporary file is open (a TERM while marked.min.js
# is read from a fifo) leaves no temporary file and no page, and exits 1.
case_cache_tmp_removed_on_error() {
    fresh
    cp "$sample" s.md || bad "cp"
    cache_page s.md
    mkfifo "$d/fifo" || bad "mkfifo"
    MARKED_JS=$d/fifo sh "$md_open" --no-open s.md > "$d/out" 2> "$d/err" &
    pid=$!
    i=0
    while [ -z "$(find "$d/cache" -name '*.tmp.*' 2>/dev/null)" ]; do
        i=$((i + 1))
        [ "$i" -le 20 ] || { kill -9 "$pid" 2>/dev/null; bad "no temporary file was made"; }
        sleep 1
    done
    kill -TERM "$pid" || bad "kill"
    # Open the fifo for writing, so that the reader gets end of file and the shell runs its trap.
    ( : > "$d/fifo" ) &
    w=$!
    wait "$pid"
    rc=$?
    kill "$w" 2>/dev/null
    [ "$rc" = 1 ] || bad "exit $rc after TERM, want 1"
    [ ! -e "$cpage" ] || bad "a page was made"
    no_tmp
    exit 0
}

for c in flags_ok flag_width_bad flag_theme_bad flag_width_missing \
         ini_valid ini_hostile ini_missing ini_browser_bad ini_browser_app_dir \
         init_config_writes init_config_keeps print_association_ok \
         browser_no_shell browser_env_dash file_name_hostile \
         page_has_github_css page_width_rules page_start_values page_policy_unchanged \
         page_narrow_rule page_hljs_after_marked page_mermaid_theme_hook \
         page_gear_markup page_print_hidden page_storage_try_catch page_keys_guard page_paste_line \
         vendor_no_close_tag vendor_sha384 \
         cache_path_stable cache_two_files cache_reuse cache_rebuild_on_edit cache_rebuild_on_flag \
         cache_rebuild_on_ini cache_no_cache_flag cache_dir_mode cache_dir_fallback cache_name_safe \
         out_direct_write cache_umask_restored cache_tmp_removed_on_error; do
    run "$c"
done

exit "$fail"
