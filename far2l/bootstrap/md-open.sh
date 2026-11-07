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
#     --tree on|off       show the file tree of the root folder, or one file
#     --list-tree         print the root and the tree files, write no page
#   md-open.sh --init-config
#   md-open.sh --print-association
#
# Writes one page (the Markdown text and marked inline; mermaid from a pinned
# CDN script with an SRI hash) and opens it with the system opener. The page
# goes to ${XDG_CACHE_HOME:-$HOME/.cache}/far2l/md/<base>-<crc>.html unless --out
# names another file, and is used again while no input changed. If that folder
# cannot be used, the page goes to ${TMPDIR:-/tmp}/far2l-md-<pid>.html.
# marked.min.js is read from this script's folder, or from MARKED_JS. The GitHub
# CSS and the highlight.js files are read from this script's folder too.
#
# Defaults come from ${XDG_CONFIG_HOME:-$HOME/.config}/far2l/settings/md-view.ini,
# section [View], keys width, theme, browser, tree, root, tree_depth and
# tree_max_files. A flag beats the file. No value
# from the file or a flag is run as code; it is used only when it equals an
# accepted word, or (browser only) when it is an absolute path to a program.

LC_ALL=C
export LC_ALL

self_dir=$(cd "$(dirname -- "$0")" && pwd) || exit 2
ini=${XDG_CONFIG_HOME:-$HOME/.config}/far2l/settings/md-view.ini
cr=$(printf '\r')
nl='
'
# The umask of the caller. The cache page is made under 077; the browser gets this one.
old_umask=$(umask)

usage() {
    echo 'usage: md-open.sh [--out <page.html>] [--no-open] [--width 60|70|80]' >&2
    echo '                  [--theme light|dark|auto] [--browser B] [--no-cache]' >&2
    echo '                  [--tree on|off] [--list-tree] <file.md>' >&2
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

# num_val VALUE CEILING: sets num to VALUE, or to CEILING when VALUE is larger.
# VALUE must be a decimal number of 1 or more; else num is empty and the return
# is 1. A long number is not given to the shell arithmetic, so it cannot overflow.
num_val() {
    num=
    case $1 in
        ''|*[!0-9]*) return 1 ;;
    esac
    v=$1
    while :; do
        case $v in
            0?*) v=${v#0} ;;
            *) break ;;
        esac
    done
    [ "$v" = 0 ] && return 1
    if [ "${#v}" -gt 5 ] || [ "$v" -gt "$2" ]; then
        num=$2
    else
        num=$v
    fi
}

# read_ini: reads md-view.ini, section [View]. Sets cfg_width, cfg_theme,
# cfg_browser, cfg_tree, cfg_root, cfg_depth and cfg_max to the last valid value
# of each key; sets cfg_browser_bad=1 if a browser= line has a value that is not
# accepted. Other values are ignored.
read_ini() {
    cfg_width=
    cfg_theme=
    cfg_browser=
    cfg_browser_bad=0
    cfg_tree=
    cfg_root=
    cfg_depth=
    cfg_max=
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
            tree)
                case $val in
                    on|off) cfg_tree=$val ;;
                esac ;;
            root)
                case $val in
                    dir|repo) cfg_root=$val ;;
                esac ;;
            tree_depth)
                num_val "$val" 12 && cfg_depth=$num ;;
            tree_max_files)
                num_val "$val" 2000 && cfg_max=$num ;;
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
# tree: on shows the files under the root in one page, with a file tree on
# the left; off shows one file, as a page without the tree.
# root: dir takes the folder of the opened file; repo takes the nearest
# folder upward with .git, else the folder of the file.
# tree_depth: 1 to 12 (default 6). tree_max_files: 1 to 2000 (default 500).
# A number over the ceiling gives the ceiling.

[View]
width=70
theme=auto
browser=auto
tree=on
root=dir
tree_depth=6
tree_max_files=500
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

# real_of DIR: sets rp to the real path of DIR (cd -P, then pwd -P). The "x"
# after pwd keeps a newline at the end of the path, which command substitution
# would remove. DIR must be absolute or start with "./". Returns 1 on failure.
real_of() {
    rp=$(cd -P "$1" && pwd -P && echo x) || return 1
    rp=${rp%"$nl"x}
}

# find_repo_root START: sets repo_root to the first folder from START upward
# that has .git (a folder or a file). START is a real path. The walk checks
# $HOME (its real path, in home_real) and stops there; it never checks /.
# Returns 1 when no folder matches.
find_repo_root() {
    repo_root=
    d=$1
    while [ "$d" != / ]; do
        if [ -e "$d/.git" ]; then
            repo_root=$d
            return 0
        fi
        [ -n "$home_real" ] && [ "$d" = "$home_real" ] && break
        real_of "$d/.." || break
        d=$rp
    done
    return 1
}

# The awk function enc(s): s with each byte outside A-Za-z0-9._~/- written as
# %XX, two upper-case hex digits. Only A-Za-z0-9._~/%- come out.
enc_fn='
BEGIN {
    for (i = 1; i < 256; i++) ord[sprintf("%c", i)] = i
    hex = "0123456789ABCDEF"
    safe = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._~/-"
}
function enc(s,    r, i, c, n) {
    r = ""
    for (i = 1; i <= length(s); i++) {
        c = substr(s, i, 1)
        if (index(safe, c) > 0) {
            r = r c
        } else {
            n = ord[c]
            r = r "%" substr(hex, int(n / 16) + 1, 1) substr(hex, n % 16 + 1, 1)
        }
    }
    return r
}
'

# enc_path TEXT: prints TEXT encoded. A newline in TEXT becomes %0A. The added
# newline ends the last record, so a newline at the end of TEXT is kept too.
enc_path() {
    printf '%s\n' "$1" | awk "$enc_fn"'
        { s = s (NR > 1 ? "%0A" : "") enc($0) }
        END { print s }'
}

# scan_find [PRIMARY...]: lists the tree files under the root as ./<tree path>,
# one per line. It runs in the root, so a pattern character in the root path
# cannot change a pattern. It skips .git, node_modules and hidden folders,
# hidden files, a name with a byte below 0x20 and a path deeper than
# tree_depth. find does not follow a symlink, and -type f skips a symlink.
# The extra primaries (for example -size) go before -print.
scan_find() {
    deep='./*'
    i=0
    while [ "$i" -lt "$tree_depth" ]; do
        deep=$deep/*
        i=$((i + 1))
    done
    ctl=$(printf '*[\001-\037]*')
    (
        cd "$tree_root" || exit 1
        # scan-rule
        find . \( -name "$ctl" -o -type d \( -name '.?*' -o -name node_modules \) \) -prune -o -path "$deep" -prune -o -type f ! -name '.*' \( -name '*.[mM][dD]' -o -name '*.[mM][aA][rR][kK][dD][oO][wW][nN]' \) "$@" -print 2>/dev/null
        exit 0
    )
}

# scan_tree: scans the root. Sets tree_list to the "<state> <tree path>" lines
# in tree order, up to the cap (state is page, big or cut), tree_count to their
# number, tree_capped to 1 when more files exist, and opened_in_list to 1 when
# the opened file is one of the lines. The opened file is always "page"; when it
# is not in tree_list, list_tree_text adds it after the list. Its bytes count
# first toward the page limit.
scan_tree() {
    [ -d "$tree_root" ] || return 1
    files=$(scan_find) || return 1
    big=$(scan_find -size +1048576c) || return 1
    # The opened file joins the scan unless its tree path has a byte below 0x20.
    ctlb=$(printf '\001-\037')
    case $tree_opened in
        *[$ctlb]*) extra= ;;
        *) extra=./$tree_opened ;;
    esac
    sorted=$(
        { [ -n "$files" ] && printf '%s\n' "$files"; [ -n "$extra" ] && printf '%s\n' "$extra"; } |
        awk '!seen[$0]++ { p = substr($0, 3); n = split(p, a, "/"); print n " " p }' |
        LC_ALL=C sort -k1,1n -k2
    )
    res=$(
        cd "$tree_root" || exit 1
        { [ -n "$big" ] && printf '%s\n' "$big"; echo; [ -n "$sorted" ] && printf '%s\n' "$sorted"; } |
        MD_OPENED=$tree_opened MD_MAX=$tree_max awk '
        # size(f, room): bytes of f as lines with a newline each, read with
        # getline; the read stops once the count passes room.
        function size(f, room,    n, line) {
            n = 0
            while ((getline line < f) > 0) {
                n += length(line) + 1
                if (n > room) break
            }
            close(f)
            return n
        }
        BEGIN {
            limit = 8388608
            opened = ENVIRON["MD_OPENED"]
            max = ENVIRON["MD_MAX"] + 0
            total = size("./" opened, limit + 1)
            phase = 0
            count = 0
            capped = 0
            inlist = 0
        }
        phase == 0 {
            if ($0 == "") phase = 1
            else isbig[$0] = 1
            next
        }
        $0 == "" { next }
        {
            p = substr($0, index($0, " ") + 1)
            if (count >= max) { capped = 1; next }
            count++
            if (p == opened) {
                state = "page"
                inlist = 1
            } else if (("./" p) in isbig) {
                state = "big"
            } else {
                n = size("./" p, limit - total)
                if (total + n > limit) state = "cut"
                else { total += n; state = "page" }
            }
            print state " " p
        }
        END { print "#meta " count " " capped " " inlist }'
    ) || return 1
    meta=${res##*"$nl"}
    case $res in
        *"$nl"*) tree_list=${res%"$nl"*} ;;
        *) tree_list= ;;
    esac
    meta=${meta#\#meta }
    tree_count=${meta%% *}
    meta=${meta#* }
    tree_capped=${meta%% *}
    opened_in_list=${meta#* }
}

# list_tree_text: the --list-tree output. Line 1 is the root, encoded. In
# one-file mode line 2 is "off". Else one "<state> <ENC>" line per tree file,
# the opened file after them when it is not in the list, and "cap <N>" last
# when the cap stopped the scan.
list_tree_text() {
    enc_path "$tree_root"
    if [ "$tree_mode" != 1 ]; then
        echo off
        return 0
    fi
    if [ -n "$tree_list" ]; then
        printf '%s\n' "$tree_list" | awk "$enc_fn"'
            { i = index($0, " "); print substr($0, 1, i - 1) " " enc(substr($0, i + 1)) }'
    fi
    if [ "$opened_in_list" != 1 ]; then
        printf 'page %s\n' "$(enc_path "$tree_opened")"
    fi
    if [ "$tree_capped" = 1 ]; then
        printf 'cap %s\n' "$tree_count"
    fi
}

# page_files: prints the file elements of a tree page. The opened file goes first
# in the script element "md", then each other "page" file of tree_list in a
# script element of the class "md-file". Each has data-path, the encoded tree
# path. Then the tree index "md-tree" has the "<state> <ENC>" lines of
# list_tree_text, without the root line and the cap line. One awk pass reads
# all files from the root, as "./<tree path>", and removes a BOM on line 1 and
# a CR at a line end. One sed pass then gives the text the escapes of the
# one-file page: "\" becomes "\\", "</" becomes "<\/" and "<!--" becomes "<\!--".
# A file that cannot be read gives an empty element.
page_files() {
    (
        cd "$tree_root" || exit 1
        { [ -n "$tree_list" ] && printf '%s\n' "$tree_list"; } |
        MD_OPENED=$tree_opened awk "$enc_fn"'
        # Each output line starts with a mark for the sed below: "T" for a tag
        # line, "|" for a text line, which gets the escapes.
        function emit(p, attrs,    f, line, n) {
            printf "T<script type=\"text/markdown\" %s data-path=\"%s\">\n", attrs, enc(p)
            f = "./" p
            n = 0
            while ((getline line < f) > 0) {
                if (n++ == 0 && substr(line, 1, 3) == "\357\273\277") line = substr(line, 4)
                if (substr(line, length(line)) == "\r") line = substr(line, 1, length(line) - 1)
                print "|" line
            }
            close(f)
            printf "T\nT</script>\n"
        }
        BEGIN {
            opened = ENVIRON["MD_OPENED"]
            emit(opened, "id=\"md\"")
        }
        substr($0, 1, 5) == "page " {
            p = substr($0, 6)
            if (p != opened) emit(p, "class=\"md-file\"")
        }' |
        # The escapes of the one-file page, with sed (linear in the line length).
        sed -e '/^T/{' -e 's/^T//' -e b -e '}' -e 's/^|//' \
            -e 's/\\/\\\\/g' -e 's|</|<\\/|g' -e 's|<!--|<\\!--|g'
    ) || return 1
    printf '<script type="text/plain" id="md-tree">\n'
    list_tree_text | sed -e 1d -e '/^cap /d'
    printf '</script>\n'
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
flag_tree=
list_tree=0
while [ $# -gt 0 ]; do
    case $1 in
        --print-association)
            print_association
            exit 0 ;;
        --init-config)
            do_init=1 ;;
        --list-tree)
            list_tree=1 ;;
        --tree)
            [ $# -ge 2 ] || usage
            case $2 in
                on|off) flag_tree=$2 ;;
                *) usage ;;
            esac
            shift ;;
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

# The tree settings: a flag, else the file, else the default. Only accepted
# words and numbers are kept.
tree=${flag_tree:-${cfg_tree:-on}}
root_mode=${cfg_root:-dir}
tree_depth=${cfg_depth:-6}
tree_max=${cfg_max:-500}
tree_why=
if [ "$tree" = off ]; then
    if [ -n "$flag_tree" ]; then tree_why=flag; else tree_why=ini; fi
fi

# The real folder of the opened file and its name. The shell parameter forms
# keep a name that ends in a newline; "./" keeps cd from using CDPATH.
case $file in
    */*) file_dir=${file%/*}; [ -n "$file_dir" ] || file_dir=/ ;;
    *) file_dir=. ;;
esac
case $file_dir in
    /*) ;;
    *) file_dir=./$file_dir ;;
esac
file_name=${file##*/}
real_of "$file_dir" || { echo "md-open: cannot read $file" >&2; exit 2; }
real_dir=$rp
home_real=
case ${HOME:-} in
    /*) real_of "$HOME" 2>/dev/null && home_real=$rp ;;
esac

# The root: the real folder of the opened file, or with root=repo the first
# folder upward that has .git. The walk does not check / and stops after $HOME.
tree_root=$real_dir
if [ "$root_mode" = repo ] && find_repo_root "$real_dir"; then
    tree_root=$repo_root
fi
if [ -z "$tree_why" ]; then
    if [ "$tree_root" = / ]; then
        tree_why=slash
    elif [ -n "$home_real" ] && [ "$tree_root" = "$home_real" ]; then
        tree_why=home
    fi
fi
[ -z "$tree_why" ] && tree_mode=1 || tree_mode=0

# The tree path of the opened file: its real folder under the root, then its own
# name (a symlink keeps its name).
rel=${real_dir#"$tree_root"}
rel=${rel#/}
tree_opened=${rel:+$rel/}$file_name

tree_list=
tree_count=0
tree_capped=0
opened_in_list=0
if [ "$tree_mode" = 1 ]; then
    scan_tree || { echo 'md-open: cannot read the file tree' >&2; exit 2; }
fi

if [ "$list_tree" = 1 ]; then
    list_tree_text
    exit 0
fi

# The page path. --out names it. Else it is <base>-<crc>.html in the cache
# folder: <base> is the file name with each character outside A-Za-z0-9._- made
# "_" and cut to 40 characters, <crc> is the cksum of the real path of the file.
# A cache folder that cannot be made or used, or a page path that is a link or
# not a plain file, gives one stderr line and the old temporary page.
cached=0
if [ -n "$out" ]; then
    page=$out
else
    page=
    umask 077
    cache_home=${XDG_CACHE_HOME:-}
    case $cache_home in
        /*) ;;
        *) cache_home=${HOME:-}/.cache ;;
    esac
    cache_dir=${cache_home%/}/far2l/md
    if (umask 077; mkdir -p "$cache_dir") 2>/dev/null && [ -d "$cache_dir" ] && [ ! -L "$cache_dir" ] \
        && chmod 700 "$cache_dir" 2>/dev/null && [ -w "$cache_dir" ]; then
        # real_dir and file_name keep a newline at the end of a name, so a
        # folder "a<newline>" and a folder "a" get two page names.
        real=${real_dir%/}/$file_name
        crc=$(printf '%s' "$real" | cksum | sed 's/ .*//')
        base=$(printf '%s\n' "$file_name" | sed 's/[^A-Za-z0-9._-]/_/g' | cut -c1-40 | sed 1q)
        case $crc in
            ''|*[!0-9]*)
                echo 'md-open: cannot name the cache page; using a temporary page' >&2 ;;
            *)
                page=$cache_dir/$base-$crc.html
                if [ -L "$page" ] || { [ -e "$page" ] && [ ! -f "$page" ]; }; then
                    echo "md-open: $page is not a plain file; using a temporary page" >&2
                    page=
                else
                    cached=1
                fi ;;
        esac
    else
        echo "md-open: cannot use the cache folder $cache_dir; using a temporary page" >&2
    fi
    if [ -z "$page" ]; then
        tmp_dir=${TMPDIR:-/tmp}
        tmp_dir=${tmp_dir%/}
        page=$tmp_dir/far2l-md-$$.html
    fi
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

# The page key and the state of md-view.ini go in the page head. The state is
# the size and the modification time that ls shows, or "none".
if [ "$tree_mode" = 1 ]; then tree_word=on; else tree_word=off; fi
key_line="<meta name=\"far2l-md-key\" content=\"w=$width;t=$theme;f=$forced;tr=$tree_word;r=$root_mode;d=$tree_depth;n=$tree_max\">"
ini_state=none
if [ -f "$ini" ]; then
    ini_state=$(ls -lnLd -- "$ini" 2>/dev/null \
        | sed -n 's/^[^ ]*  *[^ ]*  *[^ ]*  *[^ ]*  *\([0-9][0-9]*\)  *\([^ ]*\)  *\([^ ]*\)  *\([^ ]*\) .*$/\1 \2 \3 \4/p' \
        | sed -e 's/[^A-Za-z0-9 :.]/_/g' -e 1q)
    [ -n "$ini_state" ] || ini_state=unknown
fi
ini_line="<!-- far2l-md-ini: $ini_state -->"

# In tree mode the head has the tree state: the cksum of the --list-tree output
# and the number of tree files up to the cap. The html element gets the tree
# attributes; each value is an encoded path, a number or an accepted word.
tree_line=
if [ "$tree_mode" = 1 ]; then
    tree_sum=$(list_tree_text | cksum | sed 's/ .*//')
    case $tree_sum$tree_count in
        ''|*[!0-9]*) echo 'md-open: cannot read the file tree' >&2; exit 2 ;;
    esac
    [ "$tree_capped" = 1 ] || tree_capped=0
    tree_line="<!-- far2l-md-tree: $tree_sum $tree_count -->"
    root_enc=$(enc_path "${tree_root##*/}")
    html_attrs="$html_attrs data-tree=\"on\" data-root=\"$root_enc\" data-files=\"$tree_count\" data-cap=\"$tree_capped\""
else
    html_attrs="$html_attrs data-tree=\"off\" data-tree-why=\"$tree_why\""
fi

# page_fresh: true if the cache page can be used as it is. No input is newer than
# the page, and the head of the page has the key and the ini state of this run.
# In tree mode the head also has the tree state of this scan, and no tree file
# is newer than the page.
page_fresh() {
    [ -f "$page" ] && [ ! -L "$page" ] || return 1
    abs_marked=$marked_js
    case $abs_marked in
        /*) ;;
        *) abs_marked=$(pwd)/$abs_marked ;;
    esac
    set -- "$real" "$self_dir/$(basename -- "$0")" "$abs_marked"
    for f in github-markdown-light.css github-markdown-dark.css hljs-github.min.css \
             hljs-github-dark.min.css highlight.min.js; do
        set -- "$@" "$self_dir/$f"
    done
    [ -f "$ini" ] && set -- "$@" "$ini"
    newer=$(find -H "$@" -prune -newer "$page" -print 2>/dev/null) || return 1
    [ -z "$newer" ] || return 1
    sed -n '1,/^<\/head>$/p' "$page" | grep -qxF -- "$key_line" || return 1
    sed -n '1,/^<\/head>$/p' "$page" | grep -qxF -- "$ini_line" || return 1
    if [ "$tree_mode" = 1 ]; then
        sed -n '1,/^<\/head>$/p' "$page" | grep -qxF -- "$tree_line" || return 1
        case $page in
            /*) abs_page=$page ;;
            *) abs_page=$(pwd)/$page ;;
        esac
        newer=$(scan_find -newer "$abs_page") || return 1
        [ -z "$newer" ] || return 1
    fi
}

if [ "$cached" = 1 ] && [ "$no_cache" = 0 ] && page_fresh; then
    umask "$old_umask"
    if [ "$do_open" = 1 ]; then
        open_page "$page"
    fi
    exit 0
fi

# The cache page and the temporary page are written to a temporary file in their
# folder, then moved over the page, so a reader never sees a half page. The file
# goes on error or a signal. noclobber: the shell does not write through a file
# or link made at that path. --out keeps the old direct write to its path.
tmpf=
dest=$page
if [ -z "$out" ]; then
    tmpf=$page.tmp.$$
    trap 'rm -f "$tmpf"' EXIT
    trap 'exit 1' INT TERM HUP
    rm -f "$tmpf"
    set -C
    dest=$tmpf
fi

# Folder of the Markdown file as a file:// URL for <base href>. In tree mode it
# is the root.
if [ "$tree_mode" = 1 ]; then
    dir=$tree_root
else
    dir=$(cd "$(dirname -- "$file")" && pwd) || exit 2
fi
case $dir in
    */) ;;
    *) dir=$dir/ ;;
esac
# A newline in the path becomes %0A, so the base stays on one line.
base_url=file://$(printf '%s' "$dir" | sed -e 's/%/%25/g' -e 's/ /%20/g' -e 's/#/%23/g' \
    -e 's/?/%3F/g' -e 's/"/%22/g' -e "s/'/%27/g" -e 's/</%3C/g' -e 's/>/%3E/g' -e 's/&/%26/g' \
    | awk 'NR > 1 { printf "%%0A" } { printf "%s", $0 }')

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
    printf '%s\n%s\n' "$key_line" "$ini_line"
    [ -n "$tree_line" ] && printf '%s\n' "$tree_line"
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
  #md-gear { display: none }
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
HTML
    # The panel CSS and markup are in tree mode only. In one-file mode the page
    # stays the page of story 1.
    if [ "$tree_mode" = 1 ]; then
        cat <<'CSS'
html[data-panel="open"] body { padding-left: calc(260px + 16px) }
#md-tree-panel { position: fixed; top: 0; left: 0; bottom: 0; z-index: 10; box-sizing: border-box; width: 260px; padding: 8px; overflow: auto; border-right: 1px solid GrayText; background: Canvas; color: CanvasText; font: 13px/1.4 sans-serif }
html[data-panel="closed"] #md-tree-panel { display: none }
#md-tree-toggle { position: fixed; top: 8px; left: 268px; z-index: 20; box-sizing: border-box; width: 28px; height: 28px; margin: 0; padding: 0; border: 1px solid GrayText; border-radius: 6px; background: Canvas; color: CanvasText; font: 14px/1 sans-serif; cursor: pointer }
html[data-panel="closed"] #md-tree-toggle { left: 8px }
#md-tree-panel .md-root { margin: 0 0 6px; font-weight: 600; overflow-wrap: anywhere }
#md-tree-panel details { margin: 0 }
#md-tree-panel summary { cursor: pointer }
#md-tree-panel a { display: block; padding-left: 12px; color: inherit; text-decoration: none; overflow-wrap: anywhere }
#md-tree-panel a:hover, #md-tree-panel a:focus { text-decoration: underline }
#md-tree-panel a[aria-current="page"] { font-weight: 600; background: Highlight; color: HighlightText }
#md-tree-panel a.md-off { opacity: .5 }
#md-tree-panel .md-cap { margin-top: 6px; font-style: italic; opacity: .7 }
@media (max-width: 699px) {
  html[data-panel] body { padding-left: 16px }
  #md-tree-panel, #md-tree-toggle { display: none }
}
@media print { #md-gear, #md-pop, #md-tree-panel, #md-tree-toggle { display: none !important } }
CSS
    else
        cat <<'CSS'
@media print { #md-gear, #md-pop { display: none !important } }
CSS
    fi
    cat <<'HTML'
</style>
</head>
<body class="markdown-body">
<button id="md-gear" type="button" aria-label="Page settings" aria-expanded="false" aria-controls="md-pop">&#9881;</button>
<div id="md-pop" hidden></div>
HTML
    if [ "$tree_mode" = 1 ]; then
        printf '<button id="md-tree-toggle" type="button" aria-label="Files" aria-controls="md-tree-panel" aria-expanded="true">&#9776;</button>\n'
        printf '<nav id="md-tree-panel" aria-label="Files"></nav>\n'
    fi
    printf '<main id="content" class="markdown-body"></main>\n'
    # The text cannot end its script element or open a comment there. Backslashes
    # are doubled first, then "</" becomes "<\/" and "<!--" becomes "<\!--"; the
    # page undoes the steps in reverse order. A BOM and CRs go too.
    if [ "$tree_mode" = 1 ]; then
        page_files || exit 1
    else
        printf '<script type="text/markdown" id="md">\n'
        sed -e "1s/^$bom//" -e "s/$cr\$//" -e 's/\\/\\\\/g' -e 's|</|<\\/|g' -e 's|<!--|<\\!--|g' < "$file"
        printf '\n</script>\n'
    fi
    printf '<script>\n'
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
  // drawnTheme is the theme of the last batch that started. A batch that does not
  // settle in MERMAID_TIMEOUT ms shows its source, and the chain goes on.
  var drawChain = Promise.resolve();
  var drawQueued = false;
  var drawnTheme = null;
  var MERMAID_TIMEOUT = 10000;
  var storageBlocked = false;
  var forced = root.getAttribute('data-forced') === '1';
  var WIDTHS = ['60', '70', '80'];
  var THEMES = ['light', 'dark', 'auto'];
  var KEY_WIDTH = 'far2l-md-width';
  var KEY_THEME = 'far2l-md-theme';
  var KEY_TREE = 'far2l-md-tree';
  var narrow = window.matchMedia('(max-width: 699px)');
  var treePanel = document.getElementById('md-tree-panel');
  var treeToggle = document.getElementById('md-tree-toggle');
  var treeLinks = Object.create(null);
  var treeFolders = Object.create(null);
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
  function unescapeText(t) {
    return t.split('<\\!--').join('<!--').split('<\\/').join('</').split('\\\\').join('\\');
  }
  var treeMode = root.getAttribute('data-tree') === 'on';
  var ENC_RE = /^[A-Za-z0-9._~\/%-]+$/;
  var files = Object.create(null);
  var tree = Object.create(null);
  var order = [];
  var openedEnc = '';
  var openedEl = document.getElementById('md');
  var content = document.getElementById('content');
  function has(obj, k) {
    return Object.prototype.hasOwnProperty.call(obj, k);
  }
  // The files and the tree index are data. An ENC that does not match the
  // pattern is ignored, and a state word is used only if it is page, big or cut.
  (function readPage() {
    var els = document.querySelectorAll('script[type="text/markdown"]');
    if (treeMode) {
      Array.prototype.forEach.call(els, function (el) {
        var e = el.getAttribute('data-path');
        if (e === null || !ENC_RE.test(e)) return;
        files[e] = unescapeText(el.textContent);
      });
      if (openedEl) openedEnc = openedEl.getAttribute('data-path') || '';
      var idx = document.getElementById('md-tree');
      if (idx) {
        idx.textContent.split('\n').forEach(function (line) {
          var m = /^(page|big|cut) (\S+)$/.exec(line);
          if (!m || !ENC_RE.test(m[2]) || has(tree, m[2])) return;
          tree[m[2]] = m[1];
          order.push(m[2]);
        });
      }
    } else if (openedEl) {
      files[''] = unescapeText(openedEl.textContent);
    }
  })();
  // A decode error keeps the text as it is. decodeURIComponent throws on a bad
  // escape, and the value is shown only with textContent.
  function decodeText(s) {
    try { return decodeURIComponent(s); } catch (e) { return s; }
  }
  // The base name of an encoded tree path, decoded for display.
  function decName(enc) {
    var parts = enc.split('/');
    return decodeText(parts[parts.length - 1] || '');
  }
  // The page URL without its hash: the target of a tree link, then #/ENC.
  function pageUrl() {
    return window.location.href.split('#')[0];
  }
  // buildTree makes the panel from the tree index: the root name, one details
  // element per folder, one link per file, in tree order. Every name goes in
  // with textContent, so a name cannot make an element.
  function buildTree() {
    if (!treeMode || !treePanel) return;
    treePanel.textContent = '';
    var head = document.createElement('div');
    head.className = 'md-root';
    head.textContent = decodeText(root.getAttribute('data-root') || '');
    treePanel.appendChild(head);
    var body = document.createElement('div');
    treePanel.appendChild(body);
    var boxes = Object.create(null);
    boxes[''] = body;
    order.forEach(function (enc) {
      var parts = enc.split('/');
      var key = '';
      for (var i = 0; i < parts.length - 1; i++) {
        var next = key ? key + '/' + parts[i] : parts[i];
        if (!boxes[next]) {
          var det = document.createElement('details');
          var sum = document.createElement('summary');
          sum.textContent = decodeText(parts[i]);
          det.appendChild(sum);
          var box = document.createElement('div');
          det.appendChild(box);
          boxes[key].appendChild(det);
          boxes[next] = box;
          treeFolders[next] = det;
        }
        key = next;
      }
      var a = document.createElement('a');
      a.setAttribute('href', pageUrl() + '#/' + enc);
      a.textContent = decName(enc);
      if (tree[enc] !== 'page') a.className = 'md-off';
      boxes[key].appendChild(a);
      treeLinks[enc] = a;
    });
    if (root.getAttribute('data-cap') === '1') {
      var cap = document.createElement('div');
      cap.className = 'md-cap';
      cap.textContent = 'Showing the first ' + root.getAttribute('data-files') + ' files.';
      body.appendChild(cap);
    }
  }
  // markTree marks the shown file and opens its folders. A folder that the
  // reader opened stays open.
  function markTree(enc) {
    if (!treeMode || !treePanel) return;
    var parts = enc.split('/');
    var key = '';
    for (var i = 0; i < parts.length - 1; i++) {
      key = key ? key + '/' + parts[i] : parts[i];
      if (treeFolders[key]) treeFolders[key].open = true;
    }
    Object.keys(treeLinks).forEach(function (e) {
      var a = treeLinks[e];
      if (e === enc) a.setAttribute('aria-current', 'page');
      else a.removeAttribute('aria-current');
    });
    var cur = treeLinks[enc];
    if (cur && cur.scrollIntoView) cur.scrollIntoView({ block: 'nearest' });
  }
  // treeOpen: the panel is open unless data-panel says closed.
  function treeOpen() {
    return root.getAttribute('data-panel') !== 'closed';
  }
  function setTree(open, keep) {
    root.setAttribute('data-panel', open ? 'open' : 'closed');
    if (treeToggle) treeToggle.setAttribute('aria-expanded', open ? 'true' : 'false');
    if (keep) save(KEY_TREE, open ? 'open' : 'closed');
  }
  // The narrow window hides the panel, and b does nothing there.
  function toggleTree() {
    if (!treeMode || narrow.matches) return;
    setTree(!treeOpen(), true);
  }
  // The saved state, else open. tree=off makes no panel, so no state is read.
  function loadTree() {
    if (!treeMode) return;
    setTree(storageGet(KEY_TREE) !== 'closed', false);
  }
  // The focus goes to the reading pane after a click in the tree or on a link.
  function focusContent() {
    if (!content) return;
    content.setAttribute('tabindex', '-1');
    try { content.focus({ preventScroll: true }); } catch (e) { content.focus(); }
  }
  // The encoded form of a tree path: each part is encoded, and each byte
  // outside A-Za-z0-9._~- becomes %XX in upper case. "/" stays.
  function encPath(path) {
    return String(path).split('/').map(function (p) {
      return encodeURIComponent(p).replace(/[!'()*]/g, function (c) {
        return '%' + c.charCodeAt(0).toString(16).toUpperCase();
      });
    }).join('/');
  }
  // The folder parts of an encoded tree path, decoded.
  function dirOfEnc(enc) {
    var parts = enc.split('/');
    parts.pop();
    return parts.map(decodeText);
  }
  // The URL of the folder of a file, for images and for links that are not
  // Markdown. The encoded parts of the ENC are safe in a URL.
  function fileBaseFor(enc) {
    var parts = enc.split('/');
    parts.pop();
    var rel = parts.join('/');
    try { return new URL(rel ? rel + '/' : '', document.baseURI).href; } catch (e) { return document.baseURI; }
  }
  // The heading slug: lower case, then only letters, digits, space, "-" and "_".
  function slug(text) {
    return String(text).toLowerCase().replace(/[^\p{L}\p{N} _-]/gu, '').replace(/ /g, '-');
  }
  function badBytes(s) {
    return /[\\?\x00-\x1f\x7f]/.test(s);
  }
  // inRoot PARTS UP: the root rule of step 8. UP is true when a `..` had no
  // part before it. An empty result is out too.
  function inRoot(parts, up) { // root-rule
    return !up && parts.length > 0;
  }
  // target HREF DIR: the text rules 1 to 9. DIR is the folder of the shown file,
  // as decoded parts. The kind is keep, anchor, drop, out, other or candidate.
  function target(href, dir) {
    if (href === '') return { kind: 'keep' };
    if (href.charAt(0) === '#') return { kind: 'anchor', frag: href.slice(1) };
    var clean = href.replace(/[\x00-\x20]/g, '');
    if (/^(javascript|vbscript|data):/i.test(clean)) return { kind: 'drop' };
    var m = /^[A-Za-z][A-Za-z0-9+.-]*:/.exec(href);
    if (m) {
      var s = m[0].toLowerCase();
      if (s === 'http:' || s === 'https:' || s === 'mailto:') return { kind: 'other' };
      return { kind: 'out' };
    }
    if (href.charAt(0) === '/') return { kind: 'out' };
    if (badBytes(href)) return { kind: 'out' };
    var i = href.indexOf('#');
    var path = i < 0 ? href : href.slice(0, i);
    var frag = i < 0 ? '' : href.slice(i + 1);
    var dec;
    try { dec = decodeURIComponent(path); } catch (e) { return { kind: 'out' }; }
    if (badBytes(dec)) return { kind: 'out' };
    var parts = dir.slice();
    var up = false;
    var segs = dec.split('/');
    for (var k = 0; k < segs.length; k++) {
      var seg = segs[k];
      if (seg === '' || seg === '.') continue;
      if (seg === '..') {
        if (!parts.length) { up = true; break; }
        parts.pop();
        continue;
      }
      parts.push(seg);
    }
    if (!inRoot(parts, up)) return { kind: 'out' };
    if (!/\.(md|markdown)$/i.test(parts[parts.length - 1])) return { kind: 'other' };
    return { kind: 'candidate', enc: encPath(parts.join('/')), frag: frag };
  }
  // The note after a link that does not navigate.
  function noteAfter(a) {
    var s = document.createElement('span');
    s.className = 'md-note';
    s.textContent = ' (not in this tree)';
    if (a.parentNode) a.parentNode.insertBefore(s, a.nextSibling);
  }
  // applyLink A DIR FILEBASE PAGEBASE ENC: the link rules of the spec. A link
  // in the tree points to the page and a route. A link that does not go in the
  // tree keeps its text, loses its href and gets the note.
  function applyLink(a, dir, fileBase, pageBase, enc) {
    var href = a.getAttribute('href');
    if (href === null) return;
    var t = target(href, dir);
    if (t.kind === 'keep') return;
    if (t.kind === 'anchor') {
      a.setAttribute('href', pageBase + (treeMode ? '#/' + enc + '#' + t.frag : '#' + t.frag));
      return;
    }
    if (t.kind === 'drop') { a.removeAttribute('href'); return; }
    if (t.kind === 'out') { a.removeAttribute('href'); noteAfter(a); return; }
    if (t.kind === 'other') {
      if (!/^[A-Za-z][A-Za-z0-9+.-]*:/.test(href)) {
        try { a.setAttribute('href', new URL(href, fileBase).href); } catch (e) { a.removeAttribute('href'); }
      }
      return;
    }
    if (has(tree, t.enc)) {
      a.setAttribute('href', pageBase + '#/' + t.enc + (t.frag ? '#' + t.frag : ''));
      if (tree[t.enc] === 'big' || tree[t.enc] === 'cut') a.setAttribute('data-md-note', tree[t.enc]);
      return;
    }
    a.removeAttribute('href');
    noteAfter(a);
  }
  // A relative image gets an absolute URL in the folder of the shown file.
  function applyImage(img, fileBase) {
    var v = img.getAttribute('src');
    if (v === null || v === '' || v.charAt(0) === '/' || /^[A-Za-z][A-Za-z0-9+.-]*:/.test(v)) return;
    try { img.setAttribute('src', new URL(v, fileBase).href); } catch (e) { /* keep the value */ }
  }
  // The heading ids of the spec, with a counter for a repeated slug.
  function headingIds(scope) {
    var seen = Object.create(null);
    Array.prototype.forEach.call(scope.querySelectorAll('h1,h2,h3,h4,h5,h6'), function (h) {
      var s = slug(h.textContent);
      if (has(seen, s)) {
        seen[s] = seen[s] + 1;
        s = s + '-' + seen[s];
      } else {
        seen[s] = 0;
      }
      h.id = 'user-content-' + s;
    });
  }
  // Scrolls to the heading of FRAG. A decode error keeps the raw text. A
  // missing heading goes to the top.
  function scrollToFrag(frag) {
    if (!frag) { window.scrollTo(0, 0); return; }
    var f = decodeText(frag);
    var el = document.getElementById('user-content-' + f);
    if (!el) el = document.getElementById('user-content-' + f.toLowerCase());
    if (el && el.scrollIntoView) el.scrollIntoView();
    else window.scrollTo(0, 0);
  }
  // route: the route of the hash. ENC is null for a hash that is not "#/ENC".
  function route() {
    var h = window.location.hash || '';
    if (h.indexOf('#/') === 0) {
      var rest = h.slice(2);
      var i = rest.indexOf('#');
      return { enc: i < 0 ? rest : rest.slice(0, i), frag: i < 0 ? '' : rest.slice(i + 1) };
    }
    return { enc: null, frag: h.length > 1 ? h.slice(1) : '' };
  }
  // currentRoute: the file to show. An unknown route shows the opened file.
  function currentRoute() {
    var r = route();
    if (!treeMode) return { enc: '', frag: r.frag };
    if (r.enc === null || !has(tree, r.enc)) return { enc: openedEnc, frag: r.enc === null ? r.frag : '' };
    return { enc: r.enc, frag: r.frag };
  }
  function showLimitNote(state) {
    content.textContent = '';
    var p = document.createElement('p');
    p.className = 'md-file-note';
    p.textContent = state === 'big' ? 'This file is too large for the page.' : 'This file is over the page limit.';
    content.appendChild(p);
    items = [];
  }
  // show ENC FRAG: renders one file into #content. The steps are the order of
  // the spec: parse, remove script schemes, ids, links and images, move,
  // highlight, mermaid, title and scroll.
  function show(enc, frag) {
    if (!treeMode) enc = '';
    var state = tree[enc];
    if (state === 'big' || state === 'cut') {
      shownEnc = enc;
      showLimitNote(state);
      document.title = decName(enc);
      markTree(enc);
      return;
    }
    if (typeof files[enc] !== 'string') {
      enc = openedEnc;
      frag = '';
      if (typeof files[enc] !== 'string') return;
    }
    var tpl = document.createElement('template');
    tpl.innerHTML = marked.parse(files[enc]);
    var nodes = tpl.content;
    Array.prototype.forEach.call(nodes.querySelectorAll('a[href]'), function (a) {
      if (/^(javascript|vbscript|data):/i.test(a.getAttribute('href').replace(/[\x00-\x20]/g, ''))) {
        a.removeAttribute('href');
      }
    });
    headingIds(nodes);
    var dir = dirOfEnc(enc);
    var fileBase = fileBaseFor(enc);
    var pageBase = window.location.href.split('#')[0];
    Array.prototype.forEach.call(nodes.querySelectorAll('a[href]'), function (a) {
      applyLink(a, dir, fileBase, pageBase, enc);
    });
    Array.prototype.forEach.call(nodes.querySelectorAll('img[src]'), function (img) {
      applyImage(img, fileBase);
    });
    content.textContent = '';
    content.appendChild(nodes);
    highlightAll(content);
    items = Array.prototype.map.call(content.querySelectorAll('pre.mermaid'), function (pre) {
      return { pre: pre, src: pre.textContent };
    });
    var mine = items;
    if (mine.length) {
      ensureMermaid(function (ok) {
        if (!ok || items !== mine) return;
        redraw(mine);
      });
    }
    shownEnc = enc;
    if (treeMode) document.title = decName(enc);
    markTree(enc);
    scrollToFrag(frag);
  }
  // A start value is used only if it is an accepted word.
  function accepted(v, words, dflt) {
    return words.indexOf(v) >= 0 ? v : dflt;
  }
  // Each storage call is inside try. A failed read or write sets storageBlocked,
  // and the popover then shows a notice. The page works without storage.
  function storageGet(key) {
    try { return window.localStorage.getItem(key); } catch (e) { storageBlocked = true; return null; }
  }
  function save(key, value) {
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
    if (mermaidReady && usedTheme() !== drawnTheme) redraw(items);
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
  // Code with a known language gets highlight.js. A block with no language, with
  // an unknown one, or a mermaid block stays plain. An error leaves it plain.
  function highlightAll(scope) {
    Array.prototype.forEach.call(scope.querySelectorAll('pre > code'), function (code) {
      var m = /(?:^|\s)language-(\S+)/.exec(code.className || '');
      if (!m || m[1] === 'mermaid') return;
      try {
        if (window.hljs && hljs.getLanguage(m[1])) hljs.highlightElement(code);
      } catch (e) {
        // The plain block stays.
      }
    });
  }
  // Mermaid is added by a script element created here, and only at the first
  // file with a diagram, so a slow or blocked CDN cannot hide the text. Its load
  // or its error decides how the diagrams are shown. mermaidState: 0 not
  // loaded, 1 loading, 2 ready, 3 failed.
  var mermaidState = 0;
  var mermaidWaiting = [];
  function ensureMermaid(cb) {
    if (mermaidState === 2) { cb(true); return; }
    if (mermaidState === 3) { cb(false); return; }
    mermaidWaiting.push(cb);
    if (mermaidState === 1) return;
    mermaidState = 1;
    function done() {
      mermaidReady = mermaidState === 2;
      var list = mermaidWaiting;
      mermaidWaiting = [];
      list.forEach(function (f) { f(mermaidState === 2); });
    }
    var s = document.createElement('script');
    s.src = MERMAID_URL;
    s.integrity = MERMAID_SRI;
    s.crossOrigin = 'anonymous';
    s.onload = function () {
      mermaidState = typeof mermaid === 'undefined' ? 3 : 2;
      done();
    };
    s.onerror = function () {
      mermaidState = 3;
      done();
    };
    document.head.appendChild(s);
  }
  var shownEnc = null;
  function start() {
    var r = currentRoute();
    show(r.enc, r.frag);
  }
  applyTheme();
  buildPopover();
  buildTree();
  loadTree();
  wireControls();
  start();
  // A new route renders its file. The same file scrolls to its anchor only.
  window.addEventListener('hashchange', function () {
    var r = currentRoute();
    if (r.enc === shownEnc) {
      scrollToFrag(r.frag);
      return;
    }
    show(r.enc, r.frag);
  });

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

  function drawDiagrams(list, batch) {
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
      if (!good.length || batch.cancelled) return;
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
      drawnTheme = usedTheme();
      var batch = { cancelled: false };
      return withTimeout(drawDiagrams(list, batch), MERMAID_TIMEOUT, function () {
        // A batch that is late does not draw any more; its source is shown.
        batch.cancelled = true;
        list.forEach(function (it) {
          showCode(it.pre, it.src, new Error('it did not draw in ' + (MERMAID_TIMEOUT / 1000) + ' s'));
        });
      });
    }).catch(function (e) { console.error(e); });
  }
  // Resolves with p, or calls onTimeout and resolves after ms when p has not settled.
  function withTimeout(p, ms, onTimeout) {
    var timer = null;
    var late = new Promise(function (resolve) {
      timer = setTimeout(function () { onTimeout(); resolve(); }, ms);
    });
    return Promise.race([p, late]).then(function (v) {
      clearTimeout(timer);
      return v;
    }, function (e) {
      clearTimeout(timer);
      throw e;
    });
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
    save(KEY_WIDTH, w);
    refresh();
  }
  function setTheme(t) {
    root.setAttribute('data-theme', t);
    save(KEY_THEME, t);
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
    help.textContent = treeMode ? 'Keys: t theme, w width, b tree, ? help' : 'Keys: t theme, w width, ? help';
    pop.appendChild(help);
    notice = document.createElement('p');
    notice.className = 'md-notice';
    notice.textContent = 'Storage is blocked. Your choice is not kept.';
    notice.hidden = true;
    pop.appendChild(notice);
    if (!treeMode) {
      var off = document.createElement('p');
      off.className = 'md-notice';
      off.textContent = treeOffText();
      pop.appendChild(off);
    }
    var label = document.createElement('p');
    label.textContent = 'Paste into md-view.ini:';
    pop.appendChild(label);
    paste = document.createElement('pre');
    paste.className = 'md-paste';
    pop.appendChild(paste);
    pop.appendChild(makeButton('Reset', resetChoice));
    refresh();
  }
  // The notice of a one-file page, from data-tree-why.
  function treeOffText() {
    var why = root.getAttribute('data-tree-why');
    if (why === 'flag') return 'File tree: off (--tree off)';
    if (why === 'home') return 'File tree: off (the root is the home folder)';
    if (why === 'slash') return 'File tree: off (the root is /)';
    return 'File tree: off (tree=off in md-view.ini)';
  }
  // The gear, a click outside the popover, and the keys.
  function wireControls() {
    gear.addEventListener('click', togglePop);
    if (treeToggle) treeToggle.addEventListener('click', toggleTree);
    // A click in the tree, or on a link, leaves the focus in the reading pane.
    document.addEventListener('click', function (e) {
      var t = e.target;
      while (t && t !== document) {
        if (t.nodeName && String(t.nodeName).toLowerCase() === 'a') { focusContent(); return; }
        t = t.parentNode;
      }
    });
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
      if (e.key === 'Escape') {
        if (!pop.hidden) { closePop(); gear.focus(); }
      }
      else if (e.key === 't') setTheme(nextWord(THEMES, themeSetting()));
      else if (e.key === 'w') setWidth(nextWord(WIDTHS, root.getAttribute('data-width')));
      else if (e.key === 'b') toggleTree();
      else if (e.key === '?') openPop();
    });
  }
})();
JS
    printf '</script>\n</body>\n</html>\n'
} > "$dest" || { echo "md-open: cannot write $page" >&2; exit 1; }
if [ -n "$tmpf" ]; then
    set +C
    mv -f "$tmpf" "$page" || { echo "md-open: cannot write $page" >&2; exit 1; }
    trap - EXIT
fi
umask "$old_umask"

if [ "$do_open" = 1 ]; then
    open_page "$page"
fi
exit 0
