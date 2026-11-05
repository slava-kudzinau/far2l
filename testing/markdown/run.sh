#!/bin/sh
# Builds and runs the standalone MermaidEngine driver (no TUI).
# usage: sh testing/markdown/run.sh flowchart|swimlane|invalid|cells
# Env overrides: MERMAID_JS (path to mermaid.min.js), MD_DRIVER_BUILD (build dir).
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
build=${MD_DRIVER_BUILD:-$root/../far2l.build/markdown-driver}
qjs=$root/../far2l.build/far2l/third_party/quickjs/libqjs.a
mermaid_js=${MERMAID_JS:-$root/../far2l.build/install/far2l.app/Contents/Resources/mermaid.min.js}
sample=$root/testing/tests/0007-view-file-markdown/initdir/left/sample.md

case "${1:-}" in
    flowchart|swimlane|invalid|cells) ;;
    *) echo "usage: sh $0 flowchart|swimlane|invalid|cells" >&2; exit 2 ;;
esac

if [ ! -f "$qjs" ]; then
    echo "missing $qjs (build far2l first)" >&2
    exit 2
fi
if [ ! -f "$mermaid_js" ]; then
    echo "missing mermaid.min.js: $mermaid_js (set MERMAID_JS)" >&2
    exit 2
fi

mkdir -p "$build/stub"
# MermaidEngine.cpp includes "headers.hpp"; the driver supplies only the std headers it needs.
cat > "$build/stub/headers.hpp" <<'EOF'
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
EOF
# SvgCanvas.cpp and MarkdownRender.hpp (MdLine, MdRun) need far2l's color and box-symbol
# definitions.  The stubs copy the BOX_DEF_SYMBOLS enum and the BoxSymbols table verbatim
# from the sources, so the glyphs are the ones far2l draws.
cat > "$build/stub/colors.hpp" <<'EOF'
#pragma once
// The two color constants MarkdownRender.hpp uses (values of console/colors.hpp).
enum { F_LIGHTGRAY = 0x07, B_BLACK = 0x00 };
EOF
{
    echo '#pragma once'
    echo '#include <cstdint>'
    sed -n '/^enum BOX_DEF_SYMBOLS/,/^};/p' "$root/far2l/far2sdk/farplug-wide.h"
} > "$build/stub/farplug-wide.h"
{
    echo '#pragma once'
    echo '#include <cwchar>'
    echo 'typedef wchar_t WCHAR;'
    sed -n '/static WCHAR _BoxSymbols\[48\] = {/,/^[[:space:]]*};/p' "$root/far2l/src/console/interf.cpp" \
        | sed 's/static WCHAR _BoxSymbols\[48\]/inline WCHAR BoxSymbols[48]/'
} > "$build/stub/interf.hpp"

clang++ -std=c++17 -O1 -g \
    -I"$build/stub" \
    -I"$root/far2l/src/markdown" \
    -I"$root/far2l/third_party/quickjs" \
    "$here/mermaid_driver.cpp" \
    "$root/far2l/src/markdown/MermaidEngine.cpp" \
    "$root/far2l/src/markdown/SvgCanvas.cpp" \
    "$qjs" \
    -o "$build/mermaid_driver"

# MermaidEngine looks for mermaid.min.js beside the executable first.
cp -f "$mermaid_js" "$build/mermaid.min.js"

cd "$build"
./mermaid_driver "$1" "$sample"
