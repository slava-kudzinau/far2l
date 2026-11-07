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
# The tree attributes (from data-tree on) are not part of this check.
expect_tag() {
    want="<html lang=\"en\" data-start-width=\"$1\" data-start-theme=\"$2\">"
    [ "$3" = 1 ] && want="<html lang=\"en\" data-start-width=\"$1\" data-start-theme=\"$2\" data-forced=\"1\">"
    got=$(html_tag | sed 's/ data-tree=.*>$/>/')
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
    grep -qxF '<meta name="far2l-md-key" content="w=70;t=auto;f=0;tr=on;r=dir;d=6;n=500">' "$page" \
        || bad "no page key for the defaults"
    rm -f "$page"
    md --width 60 --theme light --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc for a flag run"
    expect_tag 60 light 1
    grep -qxF '<meta name="far2l-md-key" content="w=60;t=light;f=1;tr=on;r=dir;d=6;n=500">' "$page" \
        || bad "no page key for a forced run"
    rm -f "$page"
    write_ini '[View]' 'width=80' 'theme=dark'
    md --no-open --out "$page" "$sample"
    [ "$rc" = 0 ] || bad "exit $rc with ini"
    expect_tag 80 dark 0
    grep -qxF '<meta name="far2l-md-key" content="w=80;t=dark;f=0;tr=on;r=dir;d=6;n=500">' "$page" \
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
    # The one-file page; tree_page_markup checks the elements of a tree page.
    md --tree off --no-open --out "$page" "$sample"
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
# The real folder keeps a newline at the end of its name (see real).
cache_page() {
    case $1 in
        */*) cdir=${1%/*} ;;
        *) cdir=. ;;
    esac
    case $cdir in
        /*) ;;
        *) cdir=./$cdir ;;
    esac
    real "$cdir"
    creal=${rpath%/}/${1##*/}
    ccrc=$(printf '%s' "$creal" | cksum | sed 's/ .*//')
    cbase=$(printf '%s\n' "${1##*/}" | sed 's/[^A-Za-z0-9._-]/_/g' | cut -c1-40 | sed 1q)
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
    grep -qxF '<meta name="far2l-md-key" content="w=70;t=auto;f=0;tr=on;r=dir;d=6;n=500">' "$cpage" || bad "no page key"
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
    # A folder whose name ends in a newline does not share a page with its sibling.
    mkdir "a$tnl" || bad "mkdir a<newline>"
    printf '# gamma-text\n' > "a$tnl/note.md" || bad "write"
    md --no-open "a$tnl/note.md"
    [ "$rc" = 0 ] || bad "exit $rc for a<newline>/note.md: $(cat "$d/err")"
    cache_page "a$tnl/note.md"; pn=$cpage
    [ "$pn" != "$pa" ] || bad "a<newline>/note.md and a/note.md have one page name"
    [ -f "$pn" ] || bad "no page at $pn"
    grep -qF 'gamma-text' "$pn" || bad "the page of a<newline>/note.md does not hold its text"
    grep -qF 'alpha-text' "$pa" || bad "the page of a/note.md was replaced"
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
    grep -qxF '<meta name="far2l-md-key" content="w=80;t=auto;f=1;tr=on;r=dir;d=6;n=500">' "$cpage" || bad "no key for --width 80"
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
    grep -qxF '<meta name="far2l-md-key" content="w=70;t=auto;f=0;tr=on;r=dir;d=6;n=500">' "$cpage" || bad "no key for no flag"
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
    grep -qxF '<meta name="far2l-md-key" content="w=60;t=auto;f=0;tr=on;r=dir;d=6;n=500">' "$cpage" || bad "no key from the new ini"
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
    # The writer keeps the fifo open for 1 s: on macOS a reader whose open meets a writer
    # that closes at once can block in read for ever.
    ( exec 3> "$d/fifo"; sleep 1 ) &
    w=$!
    wait "$pid"
    rc=$?
    kill "$w" 2>/dev/null
    [ "$rc" = 1 ] || bad "exit $rc after TERM, want 1"
    [ ! -e "$cpage" ] || bad "a page was made"
    no_tmp
    exit 0
}

# --- tree settings, root and scan (story markdown-browser-tree-panel) --------

tnl='
'

# md_wd ARGS...: md under a watchdog. A run that takes 20 s is killed, and the
# case fails. The watchdog writes nothing to the test output.
md_wd() {
    rm -f "$d/killed"
    sh "$md_open" "$@" > "$d/out" 2> "$d/err" &
    p=$!
    ( sleep 20; kill -9 "$p" 2>/dev/null && : > "$d/killed" ) > /dev/null 2>&1 &
    w=$!
    wait "$p"
    rc=$?
    { kill "$w"; wait "$w"; } 2>/dev/null
    [ ! -e "$d/killed" ] || bad "the watchdog killed the run of: $*"
}

# lt FILE [ARGS...]: md-open.sh --list-tree under the watchdog. It must exit 0
# with an empty stderr and write no page.
lt() {
    f=$1
    shift
    md_wd --list-tree "$@" -- "$f"
    [ "$rc" = 0 ] || bad "--list-tree exit $rc: $(cat "$d/err")"
    [ ! -s "$d/err" ] || bad "--list-tree stderr: $(cat "$d/err")"
}

# real DIR: sets rpath to the real path of DIR. The "x" after pwd keeps a
# newline at the end of the path, which command substitution would remove.
# encreal DIR: prints the encoded real path of DIR.
real() {
    rpath=$(cd -P "$1" && pwd -P && echo x) || bad "real $1"
    rpath=${rpath%"$tnl"x}
}
encreal() {
    real "$1"
    enc "$rpath"
}

# enc TEXT: TEXT encoded as the spec says (each byte outside A-Za-z0-9._~/-
# as %XX). Written apart from the script under test, with od.
enc() {
    printf '%s' "$1" | od -An -v -tx1 | tr -s ' \n' '\n\n' | sed '/^$/d' | while read -r h; do
        case $h in
            2d|2e|2f|3[0-9]|4[1-9a-f]|5[0-9a]|5f|6[1-9a-f]|7[0-9a]|7e)
                printf "\\$(printf '%03o' "0x$h")" ;;
            *)
                printf '%%%s' "$(echo "$h" | tr a-f A-F)" ;;
        esac
    done
    echo
}

# line1: the first line of the --list-tree output. body: the lines after it.
line1() {
    sed -n 1p "$d/out"
}
body() {
    sed 1d "$d/out"
}

# has LINE / hasnt PATTERN: a line of the output is LINE / no line matches PATTERN.
has() {
    grep -qxF -- "$1" "$d/out" || bad "no line [$1] in: $(cat "$d/out")"
}
hasnt() {
    ! grep -q -- "$1" "$d/out" || bad "a line matches [$1]: $(grep -- "$1" "$d/out")"
}

# chain DIR N: folders c1/c2/.../cN under DIR, with a file dK.md in folder cK,
# and top.md in DIR. File dK.md has depth K+1.
chain() {
    : > "$1/top.md" || bad "touch top"
    cdir=$1
    k=1
    while [ "$k" -le "$2" ]; do
        cdir=$cdir/c$k
        mkdir "$cdir" || bad "mkdir $cdir"
        : > "$cdir/d$k.md" || bad "touch d$k"
        k=$((k + 1))
    done
}

# deepest: the largest depth of a tree path in the output.
deepest() {
    body | awk '$1 != "cap" { p = substr($0, index($0, " ") + 1); n = split(p, a, "/"); if (n > m) m = n } END { print m + 0 }'
}

# many DIR N: files f001.md to fN.md (three digits) in DIR.
many() {
    awk -v n="$2" 'BEGIN { for (i = 1; i <= n; i++) printf "%03d\n", i }' | while read -r k; do
        : > "$1/f$k.md" || exit 1
    done || bad "many"
}

case_tree_ini_keys() {
    fresh
    mkdir -p proj/.git proj/sub/c1/c2/c3 || bad "mkdir"
    : > proj/a.md; : > proj/b.md; : > proj/c.md
    : > proj/sub/s.md; : > proj/sub/c1/c2/c3/deep.md
    write_ini '[View]' 'tree=off'
    lt proj/sub/s.md
    [ "$(body)" = off ] || bad "tree=off gave: $(cat "$d/out")"
    write_ini '[View]' 'root=repo'
    lt proj/sub/s.md
    [ "$(line1)" = "$(encreal proj)" ] || bad "root=repo gave root [$(line1)]"
    has 'page a.md'
    write_ini '[View]' 'root=repo' 'tree_depth=2'
    lt proj/sub/s.md
    [ "$(deepest)" = 2 ] || bad "tree_depth=2 gave depth $(deepest)"
    hasnt 'deep.md'
    write_ini '[View]' 'root=repo' 'tree_max_files=2'
    lt proj/sub/s.md
    [ "$(body | grep -c '^page ')" = 3 ] || bad "tree_max_files=2: $(cat "$d/out")"
    has 'page a.md'
    has 'page b.md'
    has 'page sub/s.md'
    has 'cap 2'
    [ "$(sed -n '$p' "$d/out")" = 'cap 2' ] || bad "cap is not the last line"
    # The section counts: a key outside [View] is ignored.
    write_ini '[Other]' 'tree=off' '[View]' 'tree_depth = 1 '
    lt proj/sub/s.md
    [ "$(body | sed -n 1p)" != off ] || bad "a key outside [View] was used"
    # The value is trimmed and used: depth 1.
    [ "$(deepest)" = 1 ] || bad "tree_depth = 1 gave depth $(deepest)"
    [ "$(body)" = 'page s.md' ] || bad "tree_depth = 1 lists: $(body)"
    exit 0
}

case_tree_ini_ceiling() {
    fresh
    mkdir proj || bad "mkdir"
    chain proj 14
    write_ini '[View]' 'tree_depth=99'
    lt proj/top.md
    [ "$(deepest)" = 12 ] || bad "tree_depth=99 gave depth $(deepest)"
    write_ini '[View]' 'tree_depth=99999999999999999999999'
    lt proj/top.md
    [ "$(deepest)" = 12 ] || bad "a long tree_depth gave depth $(deepest)"
    mkdir proj/m || bad "mkdir m"
    many proj/m 2010
    write_ini '[View]' 'tree_max_files=99999'
    lt proj/top.md
    has 'cap 2000'
    [ "$(body | grep -c '^page ')" = 2000 ] || bad "cap 2000 lists $(body | grep -c '^page ')"
    exit 0
}

case_tree_ini_hostile() {
    fresh
    mkdir proj || bad "mkdir"
    chain proj 9
    # A .git above proj: a bad root= value that fell back to repo would show.
    mkdir .git || bad "mkdir .git"
    for v in '-1' '<scriptXQX>' '0' '000' '6x' ' ' '' '+3' '1e2' '$(touch PWNED)' '0x3'; do
        write_ini '[View]' "tree_depth=$v" "tree_max_files=$v" "tree=$v" "root=$v"
        lt proj/top.md
        [ "$(deepest)" = 6 ] || bad "tree_depth=[$v] gave depth $(deepest), want 6"
        hasnt '^cap'
        [ "$(line1)" = "$(encreal proj)" ] || bad "root=[$v] gave root [$(line1)]"
    done
    # A leading zero is a decimal number.
    write_ini '[View]' 'tree_depth=007'
    lt proj/top.md
    [ "$(deepest)" = 7 ] || bad "tree_depth=007 gave depth $(deepest)"
    # The page has no text from a bad value.
    write_ini '[View]' 'tree_depth=<scriptXQX>' 'tree_max_files=<scriptXQX>' 'tree=XQX' 'root=XQX'
    md --no-open --out "$page" proj/top.md
    [ "$rc" = 0 ] || bad "page exit $rc"
    ! grep -q XQX "$page" || bad "the page has text from a bad value"
    no_pwned
    exit 0
}

case_tree_flag() {
    fresh
    mkdir proj || bad "mkdir"
    : > proj/a.md; : > proj/b.md
    lt proj/a.md --tree off
    [ "$(body)" = off ] || bad "--tree off gave: $(cat "$d/out")"
    write_ini '[View]' 'tree=off'
    lt proj/a.md --tree on
    has 'page b.md'
    write_ini '[View]' 'tree=on'
    lt proj/a.md --tree off
    [ "$(body)" = off ] || bad "--tree off did not beat tree=on"
    for v in maybe ON '' '-1' '"><script>'; do
        md --no-open --out "$page" --tree "$v" proj/a.md
        expect_usage "--tree [$v]"
    done
    md --no-open --out "$page" proj/a.md --tree
    expect_usage "--tree with no value"
    exit 0
}

case_tree_off_no_scan() {
    fresh
    mkdir -p "$d/proj/sub" "$d/fstub" || bad "mkdir"
    : > "$d/proj/a.md"; : > "$d/proj/sub/b.md"
    realfind=$(command -v find) || bad "no find"
    {
        echo '#!/bin/sh'
        echo 'printf "%s|" "$(pwd -P)" "$@" >> "$FIND_LOG"'
        echo 'echo >> "$FIND_LOG"'
        printf 'exec "%s" "$@"\n' "$realfind"
    } > "$d/fstub/find" || bad "write stub"
    chmod 755 "$d/fstub/find" || bad "chmod stub"
    FIND_LOG=$d/find.log
    export FIND_LOG
    PATH=$d/fstub:$PATH
    real "$d/proj"
    rootr=$rpath
    # The stub logs a call when the tree is on.
    : > "$FIND_LOG"
    lt "$d/proj/a.md"
    grep -q "^$rootr|" "$FIND_LOG" || bad "the stub did not log the scan: $(cat "$FIND_LOG")"
    # tree=off, --tree off and a root of HOME: no call in the root, and no call
    # that names the root folder.
    no_root_call() {
        while IFS= read -r l; do
            case $l in
                "$rootr"/*|"$rootr"\|*) bad "$1: find ran in the root: $l" ;;
                *"|$rootr|"*|*"|$rootr/|"*|*"|$d/proj|"*|*"|.|"*) bad "$1: find got the root: $l" ;;
            esac
        done < "$FIND_LOG"
    }
    write_ini '[View]' 'tree=off'
    : > "$FIND_LOG"
    lt "$d/proj/a.md"
    [ "$(line1)" = "$(enc "$rootr")" ] || bad "tree=off line 1 is [$(line1)]"
    [ "$(body)" = off ] || bad "tree=off gave: $(cat "$d/out")"
    md --no-open "$d/proj/a.md"
    [ "$rc" = 0 ] || bad "page exit $rc"
    md --no-open "$d/proj/a.md"
    [ "$rc" = 0 ] || bad "page reuse exit $rc"
    no_root_call 'tree=off'
    rm -f "$ini"
    : > "$FIND_LOG"
    lt "$d/proj/a.md" --tree off
    [ "$(body)" = off ] || bad "--tree off gave: $(cat "$d/out")"
    md --no-open --tree off "$d/proj/a.md"
    md --no-open --tree off "$d/proj/a.md"
    no_root_call '--tree off'
    HOME=$d/proj
    : > "$FIND_LOG"
    lt "$d/proj/a.md"
    [ "$(body)" = off ] || bad "root of HOME gave: $(cat "$d/out")"
    md --no-open "$d/proj/a.md"
    md --no-open "$d/proj/a.md"
    no_root_call 'HOME'
    exit 0
}

# A folder name that ends in a newline is not its sibling without the newline.
case_root_newline() {
    fresh
    nlname='p
'
    mkdir p "$nlname" || bad "mkdir"
    printf 'SIBLING\n' > p/a.md; : > p/other.md
    printf 'REAL\n' > "${nlname}/a.md"
    lt "${nlname}/a.md"
    case $(line1) in
        */p%0A) ;;
        *) bad "line 1 does not end in p%0A: $(line1)" ;;
    esac
    [ "$(line1)" = "$(encreal "$nlname")" ] || bad "line 1 is [$(line1)]"
    [ "$(body)" = 'page a.md' ] || bad "lists: $(body)"
    hasnt 'other.md'
    exit 0
}

# A file in / gives the root /: one-file mode. It needs a readable plain file in
# /, which slash_file names (found before the cases run).
case_root_slash_off() {
    fresh
    lt "$slash_file"
    [ "$(line1)" = / ] || bad "line 1 is [$(line1)]"
    [ "$(body)" = off ] || bad "a file in / gave: $(cat "$d/out")"
    write_ini '[View]' 'root=repo'
    lt "$slash_file"
    [ "$(body)" = off ] || bad "root=repo for a file in / gave: $(cat "$d/out")"
    exit 0
}

case_root_dir() {
    fresh
    mkdir -p "proj/.git" "proj/sub" || bad "mkdir"
    : > proj/sub/s.md; : > proj/top.md
    ln -s "$d/work/proj/sub" lnk || bad "ln"
    lt proj/sub/s.md
    [ "$(line1)" = "$(encreal proj/sub)" ] || bad "root=dir root is [$(line1)]"
    [ "$(body)" = 'page s.md' ] || bad "root=dir lists: $(body)"
    # A path through a linked folder gives the real folder.
    lt lnk/s.md
    [ "$(line1)" = "$(encreal proj/sub)" ] || bad "root through a link is [$(line1)]"
    write_ini '[View]' 'root=dir'
    (cd proj/sub && lt s.md) || exit 1
    [ "$(line1)" = "$(encreal proj/sub)" ] || bad "a relative file gave root [$(line1)]"
    exit 0
}

case_root_repo() {
    fresh
    mkdir -p proj/.git proj/a/b other/x/y || bad "mkdir"
    : > proj/top.md; : > proj/a/b/f.md
    printf 'gitdir: elsewhere\n' > other/.git || bad "write .git file"
    : > other/o.md; : > other/x/y/g.md
    write_ini '[View]' 'root=repo'
    lt proj/a/b/f.md
    [ "$(line1)" = "$(encreal proj)" ] || bad ".git folder: root is [$(line1)]"
    has 'page top.md'
    has 'page a/b/f.md'
    # .git as a file is a match too.
    lt other/x/y/g.md
    [ "$(line1)" = "$(encreal other)" ] || bad ".git file: root is [$(line1)]"
    has 'page o.md'
    has 'page x/y/g.md'
    # The first match upward wins.
    mkdir proj/a/.git || bad "mkdir inner .git"
    lt proj/a/b/f.md
    [ "$(line1)" = "$(encreal proj/a)" ] || bad "inner .git: root is [$(line1)]"
    has 'page b/f.md'
    exit 0
}

case_root_repo_none() {
    fresh
    # A .git above HOME is not seen: the walk stops after HOME.
    mkdir -p "$d/.git" "$HOME/p/q" || bad "mkdir"
    : > "$HOME/p/q/f.md"; : > "$HOME/p/up.md"
    write_ini '[View]' 'root=repo'
    lt "$HOME/p/q/f.md"
    [ "$(line1)" = "$(encreal "$HOME/p/q")" ] || bad "no .git: root is [$(line1)]"
    [ "$(body)" = 'page f.md' ] || bad "no .git lists: $(body)"
    # A file outside HOME with no .git up to /: the root=dir folder.
    rmdir "$d/.git" || bad "rmdir"
    mkdir -p "$d/out1/r" || bad "mkdir"
    : > "$d/out1/r/g.md"
    lt "$d/out1/r/g.md"
    [ "$(line1)" = "$(encreal "$d/out1/r")" ] || bad "outside HOME: root is [$(line1)]"
    exit 0
}

case_root_home_off() {
    fresh
    HOME=$d/home
    : > "$HOME/notes.md"; : > "$HOME/other.md"
    lt "$HOME/notes.md"
    [ "$(line1)" = "$(encreal "$HOME")" ] || bad "line 1 is [$(line1)]"
    [ "$(body)" = off ] || bad "a file in HOME gave: $(cat "$d/out")"
    # root=repo from a folder under HOME with .git in HOME: the root is HOME.
    mkdir -p "$HOME/.git" "$HOME/p" || bad "mkdir"
    : > "$HOME/p/x.md"
    write_ini '[View]' 'root=repo'
    lt "$HOME/p/x.md"
    [ "$(body)" = off ] || bad "root=repo to HOME gave: $(cat "$d/out")"
    exit 0
}

case_scan_skip_rules() {
    fresh
    mkdir -p proj/.git proj/node_modules proj/.hid proj/sub/node_modules proj/sub/.x \
        "proj/n
l" "proj/t	b" || bad "mkdir"
    for f in .git/x.md node_modules/x.md .hid/x.md sub/node_modules/y.md sub/.x/z.md \
             .hidden.md sub/.h.md "n
l/a.md" "t	b/a.md" "nl
name.md" "tab	name.md" "$(printf 'bell\007.md')" \
             notes.txt README sub/x.mdx sub/y.md.bak; do
        : > "proj/$f" || bad "touch [$f]"
    done
    for f in top.md UP.MD sub/s.Md long.markdown sub/L.MarkDown; do
        : > "proj/$f" || bad "touch [$f]"
    done
    lt proj/top.md
    want='page UP.MD
page long.markdown
page top.md
page sub/L.MarkDown
page sub/s.Md'
    [ "$(body)" = "$want" ] || bad "lists: $(body)"
    exit 0
}

case_scan_symlinks() {
    fresh
    mkdir -p outside/dir proj || bad "mkdir"
    : > outside/o.md; : > outside/dir/od.md; : > proj/a.md
    ln -s "$d/work/outside/o.md" proj/link.md || bad "ln file"
    ln -s "$d/work/outside/dir" proj/linkdir || bad "ln dir"
    ln -s "$d/work/proj" proj/loop || bad "ln loop"
    lt proj/a.md
    [ "$(body)" = 'page a.md' ] || bad "lists: $(body)"
    # A symlink opened file shows under its own name.
    lt proj/link.md
    want='page a.md
page link.md'
    [ "$(body)" = "$want" ] || bad "a symlink opened file: $(body)"
    exit 0
}

case_scan_depth() {
    fresh
    mkdir proj || bad "mkdir"
    chain proj 14
    lt proj/top.md
    [ "$(deepest)" = 6 ] || bad "default depth is $(deepest)"
    has 'page c1/c2/c3/c4/c5/d5.md'
    hasnt 'd6.md'
    write_ini '[View]' 'tree_depth=1'
    lt proj/top.md
    [ "$(body)" = 'page top.md' ] || bad "tree_depth=1 lists: $(body)"
    # The opened file is in the tree at any depth.
    write_ini '[View]' 'tree_depth=3'
    lt proj/c1/c2/c3/c4/c5/c6/c7/c8/d8.md
    [ "$(line1)" = "$(encreal proj/c1/c2/c3/c4/c5/c6/c7/c8)" ] || bad "root=dir for a deep file"
    write_ini '[View]' 'tree_depth=3' 'root=repo'
    mkdir proj/.git || bad "mkdir .git"
    lt proj/c1/c2/c3/c4/c5/c6/c7/c8/d8.md
    want='page top.md
page c1/d1.md
page c1/c2/d2.md
page c1/c2/c3/c4/c5/c6/c7/c8/d8.md'
    [ "$(body)" = "$want" ] || bad "a deep opened file: $(body)"
    exit 0
}

case_scan_cap() {
    fresh
    mkdir proj || bad "mkdir"
    many proj 600
    lt proj/f001.md
    [ "$(body | grep -c '^page ')" = 500 ] || bad "lists $(body | grep -c '^page ') files"
    has 'page f500.md'
    hasnt 'f501.md'
    [ "$(sed -n '$p' "$d/out")" = 'cap 500' ] || bad "last line is $(sed -n '$p' "$d/out")"
    # No cap line when the files fit.
    write_ini '[View]' 'tree_max_files=600'
    lt proj/f001.md
    hasnt '^cap'
    [ "$(body | grep -c '^page ')" = 600 ] || bad "600 files fit, lists $(body | grep -c '^page ')"
    exit 0
}

case_scan_open_past_cap() {
    fresh
    mkdir proj || bad "mkdir"
    many proj 600
    lt proj/f550.md
    [ "$(body | sed -n 500p)" = 'page f500.md' ] || bad "line 500 is $(body | sed -n 500p)"
    [ "$(body | sed -n 501p)" = 'page f550.md' ] || bad "line 501 is $(body | sed -n 501p)"
    [ "$(body | sed -n 502p)" = 'cap 500' ] || bad "line 502 is $(body | sed -n 502p)"
    [ "$(body | wc -l | tr -d ' ')" = 502 ] || bad "$(body | wc -l) lines"
    [ "$(grep -c 'f550.md' "$d/out")" = 1 ] || bad "the opened file is listed twice"
    exit 0
}

# big NAME BYTES: a file of BYTES bytes, lines of "x".
mkbytes() {
    yes x | head -c "$2" > "$1" || bad "write $1"
}

case_scan_big() {
    fresh
    mkdir proj || bad "mkdir"
    mkbytes proj/big.md 1048577
    mkbytes proj/edge.md 1048576
    : > proj/a.md
    lt proj/a.md
    want='page a.md
big big.md
page edge.md'
    [ "$(body)" = "$want" ] || bad "lists: $(body)"
    # The opened file is in the page at any size.
    lt proj/big.md
    has 'page big.md'
    exit 0
}

case_scan_cut() {
    fresh
    mkdir proj || bad "mkdir"
    for n in a b c d e f g h i z; do
        mkbytes "proj/$n.md" 1000000
    done
    printf 'small\n' > proj/j.md
    lt proj/z.md
    # z counts first (1000000), then a to g fit (8000000); h and i pass 8388608.
    want='page a.md
page b.md
page c.md
page d.md
page e.md
page f.md
page g.md
cut h.md
cut i.md
page j.md
page z.md'
    [ "$(body)" = "$want" ] || bad "lists: $(body)"
    exit 0
}

case_scan_order() {
    fresh
    mkdir -p proj/sub proj/Z proj/a proj/_u || bad "mkdir"
    for f in B.md a.md 'a b.md' _x.md '~t.md' sub/a.md Z/z.md a/0.md _u/k.md 'a-b.md'; do
        : > "proj/$f" || bad "touch [$f]"
    done
    lt proj/a.md
    # A.md and a.md would be one file on a disk that ignores case.
    want='page B.md
page _x.md
page a%20b.md
page a-b.md
page a.md
page ~t.md
page Z/z.md
page _u/k.md
page a/0.md
page sub/a.md'
    [ "$(body)" = "$want" ] || bad "lists: $(body)"
    exit 0
}

case_scan_names_hostile() {
    fresh
    mkdir -p 'proj/d "q" $(touch PWNED)' || bad "mkdir"
    for f in '"><script>.md' 'a b.md' '100%.md' "it's.md" '<x>.md' 'a&b.md' '-dash.md' \
             "$(printf '\303\251.md')" '`touch PWNED`.md' '$(touch PWNED).md' 'q?*[x].md' \
             'd "q" $(touch PWNED)/in.md'; do
        : > "proj/$f" || bad "touch [$f]"
    done
    lt 'proj/-dash.md'
    body | while IFS= read -r l; do
        case ${l#* } in
            *[!A-Za-z0-9._~/%-]*) echo "$l" ;;
        esac
    done > "$d/badlines"
    [ ! -s "$d/badlines" ] || bad "lines with a raw byte: $(cat "$d/badlines")"
    for w in 'page %22%3E%3Cscript%3E.md' 'page a%20b.md' 'page 100%25.md' 'page it%27s.md' \
             'page %3Cx%3E.md' 'page a%26b.md' 'page -dash.md' 'page %C3%A9.md' \
             'page %60touch%20PWNED%60.md' 'page %24%28touch%20PWNED%29.md' 'page q%3F%2A%5Bx%5D.md' \
             'page d%20%22q%22%20%24%28touch%20PWNED%29/in.md'; do
        has "$w"
    done
    [ "$(body | wc -l | tr -d ' ')" = 12 ] || bad "$(body | wc -l) lines"
    no_pwned
    exit 0
}

case_list_tree_flag() {
    fresh
    # A root path with pattern characters cannot change a pattern of the scan.
    mkdir -p 'r[a]*? x/sub' 'r[a]*? x/other' || bad "mkdir"
    : > 'r[a]*? x/a.md'; : > 'r[a]*? x/sub/b.md'; : > 'r[a]*? x/other/c.md'; : > 'r[a]*? x/b.txt'
    : > ra.md
    lt 'r[a]*? x/a.md'
    real 'r[a]*? x'
    r=$rpath
    want="$(enc "$r")
page a.md
page other/c.md
page sub/b.md"
    [ "$(cat "$d/out")" = "$want" ] || bad "output: $(cat "$d/out")"
    case $(line1) in
        *'%5Ba%5D%2A%3F%20x') ;;
        *) bad "line 1 does not end in the encoded root name: $(line1)" ;;
    esac
    # Byte-exact: each line ends in a newline, and nothing else is printed.
    printf '%s\n' "$want" > "$d/want"
    cmp -s "$d/want" "$d/out" || bad "output is not byte-exact"
    # No page, no cache file, no opener.
    [ ! -e "$OPEN_LOG" ] || bad "an opener ran"
    [ -z "$(find "$XDG_CACHE_HOME" "$TMPDIR" -type f 2>/dev/null)" ] || bad "a page was made"
    md --list-tree --out "$page" 'r[a]*? x/a.md'
    [ "$rc" = 0 ] || bad "exit $rc with --out"
    [ ! -e "$page" ] || bad "--list-tree wrote the --out page"
    exit 0
}

# --- the page in tree mode, and reuse (story markdown-browser-tree-panel) ----

# tpage ARGS...: a page at $page (--out, --no-open) under the watchdog. It must
# exit 0 with an empty stderr.
tpage() {
    rm -f "$page"
    md_wd --no-open --out "$page" "$@"
    [ "$rc" = 0 ] || bad "exit $rc: $(cat "$d/err")"
    [ ! -s "$d/err" ] || bad "stderr: $(cat "$d/err")"
}

# file_tags [PAGE]: the start tags of the file elements and of the tree index.
file_tags() {
    grep -E '^<script type="text/(markdown|plain)"' "${1:-$page}"
}

# elem_text ENC: the lines of the file element with data-path ENC.
elem_text() {
    awk -v t="data-path=\"$1\">" '
        on && $0 == "</script>" { exit }
        on { print }
        index($0, "<script type=\"text/markdown\"") == 1 && substr($0, length($0) - length(t) + 1) == t { on = 1 }' "$page"
}

# tree_index [PAGE]: the lines of the tree index element.
tree_index() {
    sed -n '/^<script type="text\/plain" id="md-tree">$/,/^<\/script>$/p' "${1:-$page}" | sed -e 1d -e '$d'
}

# head_of PAGE: the head of PAGE, as md-open.sh reads it.
head_of() {
    sed -n '1,/^<\/head>$/p' "$1"
}

# state_line FILE [ARGS...]: sets sline to the tree state line that the spec
# gives for the --list-tree output of FILE: the first field of its cksum and
# the number of tree files up to the cap.
state_line() {
    lt "$@"
    ssum=$(cksum < "$d/out" | sed 's/ .*//')
    scount=$(body | awk '$1 == "cap" { c = $2; next } { n++ } END { print (c != "" ? c : n) + 0 }')
    sline="<!-- far2l-md-tree: $ssum $scount -->"
}

# mklong NAME BYTES: a file of BYTES bytes, in lines of 100 bytes.
mklong() {
    awk 'BEGIN { s = ""; for (i = 0; i < 99; i++) s = s "x"; for (;;) print s }' | head -c "$2" > "$1" || bad "write $1"
}

case_tree_page_markup() {
    fresh
    mkdir -p proj/.git proj/sub proj/z || bad "mkdir"
    printf '# opened-zq\n' > proj/sub/o.md || bad "write"
    printf '\357\273\277# A\r\nx \\ y </script> <!-- c -->\r\nend\n' > proj/a.md || bad "write"
    printf 'bee-zq\n' > proj/z/b.md || bad "write"
    printf 'not markdown\n' > proj/n.txt || bad "write"
    write_ini '[View]' 'root=repo'
    tpage proj/sub/o.md
    # The base is the root, with the escapes of today.
    real proj
    grep -qxF "<base href=\"file://$rpath/\">" "$page" || bad "base is $(grep '^<base' "$page"), want the root $rpath"
    # The opened file first, once; then the other page files in tree order; then the index.
    want='<script type="text/markdown" id="md" data-path="sub/o.md">
<script type="text/markdown" class="md-file" data-path="a.md">
<script type="text/markdown" class="md-file" data-path="z/b.md">
<script type="text/plain" id="md-tree">'
    [ "$(file_tags)" = "$want" ] || bad "file elements: $(file_tags)"
    [ "$(grep -c 'data-path="sub/o.md"' "$page")" = 1 ] || bad "the opened file is not written once"
    [ "$(elem_text sub/o.md)" = "# opened-zq" ] || bad "text of sub/o.md: [$(elem_text sub/o.md)]"
    [ "$(elem_text z/b.md)" = "bee-zq" ] || bad "text of z/b.md: [$(elem_text z/b.md)]"
    # The escapes of today: BOM and CR go, then \ to \\, </ to <\/, <!-- to <\!--.
    elem_text a.md > "$d/got"
    printf '# A\nx \\\\ y <\\/script> <\\!-- c -->\nend\n\n' > "$d/want"
    cmp -s "$d/want" "$d/got" || bad "text of a.md: [$(cat "$d/got")]"
    if grep -qF 'not markdown' "$page"; then bad "a file that is not Markdown is in the page"; fi
    # A file that cannot be read gives an empty element, and the page is made.
    chmod 000 proj/z/b.md || bad "chmod"
    tpage proj/sub/o.md
    chmod 644 proj/z/b.md || bad "chmod back"
    grep -qxF '<script type="text/markdown" class="md-file" data-path="z/b.md">' "$page" || bad "no element for an unreadable file"
    [ -z "$(elem_text z/b.md)" ] || bad "text of an unreadable file: [$(elem_text z/b.md)]"
    grep -qF 'opened-zq' "$page" || bad "no opened text with an unreadable file"
    # One-file mode keeps the element of today: no data-path, no index, the base of the folder.
    tpage --tree off proj/sub/o.md
    [ "$(file_tags)" = '<script type="text/markdown" id="md">' ] || bad "one-file elements: $(file_tags)"
    # The folder as pwd gives it, as today.
    lpath=$(cd proj/sub && pwd) || bad "cd"
    grep -qxF "<base href=\"file://$lpath/\">" "$page" || bad "one-file base is $(grep '^<base' "$page")"
    if grep -qF 'bee-zq' "$page"; then bad "the one-file page has another file"; fi
    if grep -qF 'data-path=' "$page"; then bad "the one-file page has data-path"; fi
    exit 0
}

case_tree_page_index() {
    fresh
    mkdir -p proj/.git proj/sub || bad "mkdir"
    write_ini '[View]' 'root=repo'
    : > proj/a.md; : > proj/sub/o.md; : > 'proj/sub/x y.md'
    mkbytes proj/big.md 1048577
    lt proj/sub/o.md
    body > "$d/want"
    tpage proj/sub/o.md
    tree_index > "$d/got"
    cmp -s "$d/want" "$d/got" || bad "index [$(cat "$d/got")], want [$(cat "$d/want")]"
    grep -qxF 'big big.md' "$d/got" || bad "no big line in the index"
    grep -qxF 'page sub/x%20y.md' "$d/got" || bad "no encoded line in the index"
    # With the cap: the opened file past the cap is last, and no cap line.
    write_ini '[View]' 'root=repo' 'tree_max_files=2'
    lt proj/sub/o.md
    grep -qxF 'cap 2' "$d/out" || bad "no cap in --list-tree: $(cat "$d/out")"
    tpage proj/sub/o.md
    want='page a.md
big big.md
page sub/o.md'
    [ "$(tree_index)" = "$want" ] || bad "capped index: $(tree_index)"
    # Each index line is a state word and an encoded path.
    tree_index | grep -vE '^(page|big|cut) [A-Za-z0-9._~/%-]*$' > "$d/badlines"
    [ ! -s "$d/badlines" ] || bad "bad index lines: $(cat "$d/badlines")"
    exit 0
}

case_tree_page_attrs() {
    fresh
    mkdir -p 'my proj' || bad "mkdir"
    : > 'my proj/a.md'; : > 'my proj/b.md'
    tpage 'my proj/b.md'
    want='<html lang="en" data-start-width="70" data-start-theme="auto" data-tree="on" data-root="my%20proj" data-files="2" data-cap="0">'
    [ "$(html_tag)" = "$want" ] || bad "html is $(html_tag)"
    # 600 files, the opened file is file 550: 500 files and the cap.
    mkdir big || bad "mkdir"
    many big 600
    printf '# file-550-zq\n' > big/f550.md || bad "write"
    tpage big/f550.md
    want='<html lang="en" data-start-width="70" data-start-theme="auto" data-tree="on" data-root="big" data-files="500" data-cap="1">'
    [ "$(html_tag)" = "$want" ] || bad "html is $(html_tag)"
    [ "$(file_tags | sed -n 1p)" = '<script type="text/markdown" id="md" data-path="f550.md">' ] \
        || bad "the opened file is not first: $(file_tags | sed -n 1p)"
    grep -qF 'file-550-zq' "$page" || bad "the opened file text is not in the page"
    [ "$(tree_index | tail -n 1)" = 'page f550.md' ] || bad "the opened file is not last in the index"
    [ "$(tree_index | wc -l | tr -d ' ')" = 501 ] || bad "index has $(tree_index | wc -l) lines"
    # One-file mode: data-tree="off" and the reason.
    tpage --tree off 'my proj/a.md'
    [ "$(html_tag)" = '<html lang="en" data-start-width="70" data-start-theme="auto" data-tree="off" data-tree-why="flag">' ] \
        || bad "--tree off html is $(html_tag)"
    write_ini '[View]' 'tree=off'
    tpage 'my proj/a.md'
    [ "$(html_tag)" = '<html lang="en" data-start-width="70" data-start-theme="auto" data-tree="off" data-tree-why="ini">' ] \
        || bad "tree=off html is $(html_tag)"
    rm -f "$ini"
    printf '# home\n' > "$HOME/h.md" || bad "write"
    tpage "$HOME/h.md"
    [ "$(html_tag)" = '<html lang="en" data-start-width="70" data-start-theme="auto" data-tree="off" data-tree-why="home">' ] \
        || bad "home html is $(html_tag)"
    exit 0
}

case_tree_page_cut() {
    fresh
    mkdir proj || bad "mkdir"
    # z is opened and counts first (1000000). a to g fit (8000000); h and i pass 8388608.
    for n in a b c d e f g h i z; do
        mklong "proj/$n.md" 1000000
        printf 'text-of-%s-zq\n' "$n" >> "proj/$n.md" || bad "write"
    done
    printf 'small-zq\n' > proj/j.md || bad "write"
    mklong proj/big.md 1048577
    printf 'text-of-big-zq\n' >> proj/big.md || bad "write"
    tpage proj/z.md
    want='<script type="text/markdown" id="md" data-path="z.md">'
    for n in a b c d e f g j; do
        want="$want$tnl<script type=\"text/markdown\" class=\"md-file\" data-path=\"$n.md\">"
    done
    want="$want$tnl<script type=\"text/plain\" id=\"md-tree\">"
    [ "$(file_tags)" = "$want" ] || bad "file elements: $(file_tags)"
    for n in z a g; do
        grep -qF "text-of-$n-zq" "$page" || bad "no text of $n.md"
    done
    grep -qF 'small-zq' "$page" || bad "no text of j.md"
    for n in h i big; do
        if grep -qF "text-of-$n-zq" "$page"; then bad "the text of $n.md is in the page"; fi
    done
    for l in 'cut h.md' 'cut i.md' 'big big.md' 'page j.md' 'page z.md'; do
        tree_index | grep -qxF "$l" || bad "no index line [$l]"
    done
    exit 0
}

case_tree_page_names_hostile() {
    fresh
    mkdir -p 'proj/d "q" $(touch PWNED)' || bad "mkdir"
    for f in '"><script>.md' 'a b.md' '100%.md' "it's.md" '<x>.md' 'a&b.md' '-dash.md' \
             "$(printf '\303\251.md')" '`touch PWNED`.md' '$(touch PWNED).md' 'q?*[x].md' \
             'd "q" $(touch PWNED)/in.md'; do
        printf '# hostile-zq\n' > "proj/$f" || bad "write [$f]"
    done
    tpage 'proj/-dash.md'
    file_tags | sed -n 's/.* data-path="\([^"]*\)">$/\1/p' > "$d/paths"
    [ "$(wc -l < "$d/paths" | tr -d ' ')" = 12 ] || bad "$(wc -l < "$d/paths") data-path values"
    grep -vE '^[A-Za-z0-9._~/%-]*$' "$d/paths" > "$d/badpaths"
    [ ! -s "$d/badpaths" ] || bad "raw data-path: $(cat "$d/badpaths")"
    # Each file element tag is one of the two forms; nothing else is in the tag.
    file_tags | grep -vxE '<script type="text/markdown" (id="md"|class="md-file") data-path="[A-Za-z0-9._~/%-]*">|<script type="text/plain" id="md-tree">' > "$d/badtags"
    [ ! -s "$d/badtags" ] || bad "bad tags: $(cat "$d/badtags")"
    tree_index | grep -vE '^(page|big|cut) [A-Za-z0-9._~/%-]*$' > "$d/badlines"
    [ ! -s "$d/badlines" ] || bad "bad index lines: $(cat "$d/badlines")"
    [ "$(tree_index | wc -l | tr -d ' ')" = 12 ] || bad "index has $(tree_index | wc -l) lines"
    [ "$(grep -c 'hostile-zq' "$page")" = 12 ] || bad "want 12 file texts"
    for w in '"><script>' '<x>.md' "it's.md" 'a&b.md' '100%.md' '$(touch PWNED)' '`touch PWNED`' 'q?*[x]'; do
        if grep -qF -- "$w" "$page"; then bad "the raw name [$w] is in the page"; fi
    done
    grep -qxF '<script type="text/markdown" class="md-file" data-path="%22%3E%3Cscript%3E.md">' "$page" \
        || bad "no encoded element for \"><script>.md"
    no_pwned
    exit 0
}

case_tree_off_same_page() {
    fresh
    mkdir -p "$d/base" || bad "mkdir"
    (cd "$root" && git show 7f6523ad5:far2l/bootstrap/md-open.sh) > "$d/base/md-open.sh" || bad "git show"
    for g in $vendored; do
        cp "$vendor/$g" "$d/base/" || bad "cp $g"
    done
    cp "$sample" s.md || bad "cp"
    : > other.md
    sh "$d/base/md-open.sh" --no-open --out "$d/base.html" s.md 2> "$d/err" || bad "the BASE script failed: $(cat "$d/err")"
    tpage --tree off s.md
    # Only the html line, the key line and the page script may differ.
    strip() {
        sed -e '/^<html /d' -e '/^<meta name="far2l-md-key" /d' -e '/^(function () {$/,/^})();$/d' "$1"
    }
    strip "$d/base.html" > "$d/a"
    strip "$page" > "$d/b"
    cmp -s "$d/a" "$d/b" || bad "the pages differ: $(diff "$d/a" "$d/b" | head -20)"
    [ "$(html_tag)" = "$(grep '^<html ' "$d/base.html" | sed 's/>$//') data-tree=\"off\" data-tree-why=\"flag\">" ] \
        || bad "html line is $(html_tag)"
    [ "$(grep '^<meta name="far2l-md-key" ' "$page")" = "$(grep '^<meta name="far2l-md-key" ' "$d/base.html" | sed 's/">$//');tr=off;r=dir;d=6;n=500\">" ] \
        || bad "key line is $(grep '^<meta name="far2l-md-key" ' "$page")"
    # The page script is there in both.
    [ "$(grep -c '^(function () {$' "$page")" = 1 ] || bad "no page script"
    [ "$(head_of "$page" | wc -l)" = "$(head_of "$d/base.html" | wc -l)" ] || bad "the head has new lines"
    exit 0
}

case_tree_key_line() {
    fresh
    mkdir -p proj/.git proj/sub || bad "mkdir"
    : > proj/a.md; : > proj/sub/o.md
    tpage proj/sub/o.md
    key='<meta name="far2l-md-key" content="w=70;t=auto;f=0;tr=on;r=dir;d=6;n=500">'
    grep -qxF "$key" "$page" || bad "key: $(grep far2l-md-key "$page")"
    # The tree state goes after the ini comment, in the head.
    state_line proj/sub/o.md
    [ "$sline" = '<!-- far2l-md-tree: '"$ssum"' 1 -->' ] || bad "count in [$sline]"
    head_of "$page" | grep -A1 -xF '<!-- far2l-md-ini: none -->' | sed -n 2p > "$d/next"
    [ "$(cat "$d/next")" = "$sline" ] || bad "after the ini line: [$(cat "$d/next")], want [$sline]"
    [ "$(grep -c 'far2l-md-tree:' "$page")" = 1 ] || bad "want one tree state line"
    write_ini '[View]' 'root=repo' 'tree_depth=3' 'tree_max_files=7'
    tpage proj/sub/o.md
    grep -qxF '<meta name="far2l-md-key" content="w=70;t=auto;f=0;tr=on;r=repo;d=3;n=7">' "$page" \
        || bad "key with ini: $(grep far2l-md-key "$page")"
    state_line proj/sub/o.md
    [ "$sline" = '<!-- far2l-md-tree: '"$ssum"' 2 -->' ] || bad "count in [$sline]"
    head_of "$page" | grep -qxF "$sline" || bad "no tree state [$sline] for root=repo"
    write_ini '[View]' 'tree_depth=99' 'tree_max_files=99999'
    tpage proj/sub/o.md
    grep -qxF '<meta name="far2l-md-key" content="w=70;t=auto;f=0;tr=on;r=dir;d=12;n=2000">' "$page" \
        || bad "key with ceilings: $(grep far2l-md-key "$page")"
    write_ini '[View]' 'tree_depth=<script>' 'tree_max_files=-1' 'root=x"y'
    tpage proj/sub/o.md
    grep -qxF "$key" "$page" || bad "key with hostile values: $(grep far2l-md-key "$page")"
    if grep -qF 'x"y' "$page"; then bad "a hostile value is in the page"; fi
    rm -f "$ini"
    # With the cap, the count is the files up to the cap.
    write_ini '[View]' 'root=repo' 'tree_max_files=1'
    tpage proj/sub/o.md
    state_line proj/sub/o.md
    [ "$scount" = 1 ] || bad "capped count $scount"
    head_of "$page" | grep -qxF "$sline" || bad "no capped tree state [$sline]"
    rm -f "$ini"
    tpage --tree off proj/sub/o.md
    grep -qxF '<meta name="far2l-md-key" content="w=70;t=auto;f=0;tr=off;r=dir;d=6;n=500">' "$page" \
        || bad "--tree off key: $(grep far2l-md-key "$page")"
    if grep -qF 'far2l-md-tree:' "$page"; then bad "a one-file page has a tree state"; fi
    exit 0
}

# tree_proj: a root proj with three tree files, all old, and cpage for proj/o.md.
tree_proj() {
    mkdir -p proj/sub || bad "mkdir"
    printf '# opened\n' > proj/o.md || bad "write"
    printf '# a\n' > proj/a.md || bad "write"
    printf '# b\n' > proj/sub/b.md || bad "write"
    printf 'text\n' > proj/notes.txt || bad "write"
    old proj/o.md proj/a.md proj/sub/b.md proj/notes.txt
    cache_page proj/o.md
}

case_cache_tree_reuse() {
    fresh
    tree_proj
    md_wd --no-open proj/o.md
    [ "$rc" = 0 ] || bad "exit $rc"
    grep -qF 'far2l-md-tree:' "$cpage" || bad "the cache page is not a tree page"
    mark
    cp "$cpage" "$d/before" || bad "cp"
    chmod 500 "$XDG_CACHE_HOME/far2l/md" || bad "chmod"
    md_wd proj/o.md
    st=$rc
    chmod 700 "$XDG_CACHE_HOME/far2l/md" || bad "chmod back"
    [ "$st" = 0 ] || bad "exit $st for a reuse: $(cat "$d/err")"
    [ ! -s "$d/err" ] || bad "stderr for a reuse: $(cat "$d/err")"
    expect_open "$cpage"
    cmp -s "$cpage" "$d/before" || bad "the page changed on a reuse"
    marked || bad "the page was built again"
    # A change to a file that is not in the tree does not build the page again.
    future proj/notes.txt
    : > proj/.hidden.md
    mkdir -p proj/node_modules && : > proj/node_modules/x.md
    md_wd --no-open proj/o.md
    marked || bad "the page was built again for a file outside the tree"
    [ "$(cache_files)" = 1 ] || bad "want one page, got $(ls -A "$XDG_CACHE_HOME/far2l/md")"
    no_tmp
    exit 0
}

case_cache_tree_changed_file() {
    fresh
    tree_proj
    md_wd --no-open proj/o.md
    mark
    md_wd --no-open proj/o.md
    marked || bad "no reuse before the change"
    printf 'changed-zq\n' >> proj/sub/b.md || bad "edit"
    future proj/sub/b.md
    md_wd --no-open proj/o.md
    [ "$rc" = 0 ] || bad "exit $rc after the change"
    if marked; then bad "the page was not built again after a change of sub/b.md"; fi
    grep -qF 'changed-zq' "$cpage" || bad "the changed text is not in the page"
    # A file of the future is newer than any page; an old one again allows a reuse.
    old proj/sub/b.md
    md_wd --no-open proj/o.md
    mark
    md_wd --no-open proj/o.md
    marked || bad "no reuse after the build"
    no_tmp
    exit 0
}

case_cache_tree_added_file() {
    fresh
    tree_proj
    md_wd --no-open proj/o.md
    mark
    printf '# new-zq\n' > proj/sub/new.md || bad "write"
    old proj/sub/new.md
    md_wd --no-open proj/o.md
    [ "$rc" = 0 ] || bad "exit $rc after the add"
    if marked; then bad "the page was not built again after an old file was added"; fi
    grep -qF 'data-path="sub/new.md"' "$cpage" || bad "the new file is not in the page"
    grep -qF 'new-zq' "$cpage" || bad "the new text is not in the page"
    mark
    md_wd --no-open proj/o.md
    marked || bad "no reuse after the build"
    no_tmp
    exit 0
}

case_cache_tree_deleted_file() {
    fresh
    tree_proj
    md_wd --no-open proj/o.md
    mark
    grep -qF 'data-path="sub/b.md"' "$cpage" || bad "sub/b.md is not in the page"
    rm proj/sub/b.md || bad "rm"
    md_wd --no-open proj/o.md
    [ "$rc" = 0 ] || bad "exit $rc after the delete"
    if marked; then bad "the page was not built again after a delete"; fi
    if grep -qF 'sub/b.md' "$cpage"; then bad "the deleted file is still in the page"; fi
    no_tmp
    exit 0
}

case_cache_tree_head_only() {
    fresh
    tree_proj
    # The tree state after an add, found before the add.
    : > proj/sub/new.md
    old proj/sub/new.md
    state_line proj/o.md
    rm proj/sub/new.md || bad "rm"
    # The opened file holds that line, and so does the end of the page.
    printf '# opened\n\n%s\n' "$sline" > proj/o.md || bad "write"
    old proj/o.md
    md_wd --no-open proj/o.md
    [ "$rc" = 0 ] || bad "exit $rc"
    if head_of "$cpage" | grep -qxF "$sline"; then bad "the head has the state after the add"; fi
    printf '%s\n' "$sline" >> "$cpage" || bad "append"
    mark
    : > proj/sub/new.md
    old proj/sub/new.md
    md_wd --no-open proj/o.md
    [ "$rc" = 0 ] || bad "exit $rc after the add"
    if marked; then bad "a tree state outside the head made a reuse"; fi
    head_of "$cpage" | grep -qxF "$sline" || bad "the new head has no state [$sline]"
    no_tmp
    exit 0
}

case_cache_tree_no_cache() {
    fresh
    tree_proj
    md_wd --no-open proj/o.md
    mark
    md_wd --no-open proj/o.md
    marked || bad "a normal call did not reuse the page"
    md_wd --no-cache --no-open proj/o.md
    [ "$rc" = 0 ] || bad "exit $rc for --no-cache"
    [ -f "$cpage" ] || bad "no page at the cache path after --no-cache"
    if marked; then bad "--no-cache did not build the page again"; fi
    grep -qF 'far2l-md-tree:' "$cpage" || bad "the --no-cache page is not a tree page"
    [ "$(cache_files)" = 1 ] || bad "want one page, got $(ls -A "$XDG_CACHE_HOME/far2l/md")"
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
         out_direct_write cache_umask_restored cache_tmp_removed_on_error \
         tree_ini_keys tree_ini_ceiling tree_ini_hostile tree_flag tree_off_no_scan \
         root_dir root_repo root_repo_none root_home_off \
         scan_skip_rules scan_symlinks scan_depth scan_cap scan_open_past_cap scan_big scan_cut \
         scan_order scan_names_hostile list_tree_flag root_newline \
         tree_page_markup tree_page_index tree_page_attrs tree_page_cut tree_page_names_hostile \
         tree_off_same_page tree_key_line cache_tree_reuse cache_tree_changed_file \
         cache_tree_added_file cache_tree_deleted_file cache_tree_head_only cache_tree_no_cache; do
    run "$c"
done

# root_slash_off runs only when / has a readable plain file. Else it is not run,
# and a note goes to stderr (not a skip line, so the run stays skip-free).
slash_file=
for f in /.file /*; do
    if [ -f "$f" ] && [ -r "$f" ]; then
        slash_file=$f
        break
    fi
done
if [ -n "$slash_file" ]; then
    run root_slash_off
else
    echo "  root_slash_off: not run, / has no readable plain file" >&2
fi

exit "$fail"
