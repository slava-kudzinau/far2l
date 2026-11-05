#!/bin/sh
# Builds and runs the standalone MermaidEngine driver (no TUI).
# usage: sh testing/markdown/run.sh flowchart|swimlane|invalid
# Env overrides: MERMAID_JS (path to mermaid.min.js), MD_DRIVER_BUILD (build dir).
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
build=${MD_DRIVER_BUILD:-$root/../far2l.build/markdown-driver}
qjs=$root/../far2l.build/far2l/third_party/quickjs/libqjs.a
mermaid_js=${MERMAID_JS:-$root/../far2l.build/install/far2l.app/Contents/Resources/mermaid.min.js}
sample=$root/testing/tests/0007-view-file-markdown/initdir/left/sample.md

case "${1:-}" in
    flowchart|swimlane|invalid) ;;
    *) echo "usage: sh $0 flowchart|swimlane|invalid" >&2; exit 2 ;;
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

clang++ -std=c++17 -O1 -g \
    -I"$build/stub" \
    -I"$root/far2l/src/markdown" \
    -I"$root/far2l/third_party/quickjs" \
    "$here/mermaid_driver.cpp" \
    "$root/far2l/src/markdown/MermaidEngine.cpp" \
    "$qjs" \
    -o "$build/mermaid_driver"

# MermaidEngine looks for mermaid.min.js beside the executable first.
cp -f "$mermaid_js" "$build/mermaid.min.js"

cd "$build"
./mermaid_driver "$1" "$sample"
