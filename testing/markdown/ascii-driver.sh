#!/bin/sh
# Builds the standalone AsciiMermaid driver (testing/markdown/ascii_driver.cpp) against libqjs.a
# and runs every mode in its own process: the AsciiMermaid rows of spec.md (story
# markdown-ascii-diagrams). No far2l build is needed.
# usage: sh testing/markdown/ascii-driver.sh
# Env: ASCII_BUILD overrides the build dir (default: far2l.ascii.build next to the main checkout).
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
main=$root
if common=$(cd "$root" && git rev-parse --git-common-dir 2> /dev/null); then
    main=$(cd "$root" && cd "$common/.." && pwd)
fi
ascii_build=${ASCII_BUILD:-$main/../far2l.ascii.build}
build=$ascii_build/markdown-ascii-driver
qjs=$ascii_build/far2l/third_party/quickjs/libqjs.a
if [ ! -f "$qjs" ]; then
    qjs=$main/../far2l.build/far2l/third_party/quickjs/libqjs.a
fi
bundle=$root/far2l/bootstrap/mermaid-ascii.js
golden=$here/golden

if [ ! -f "$qjs" ]; then
    echo "missing libqjs.a (build quickjs first; looked in $ascii_build and the far2l.build dir)" >&2
    exit 2
fi
if [ ! -f "$bundle" ]; then
    echo "ascii-driver: bundle missing: $bundle (build it with sh far2l/bootstrap/mermaid-ascii/build.sh)" >&2
    exit 2
fi

mkdir -p "$build/stub" "$build/scratch"
# AsciiMermaid.cpp includes "headers.hpp"; the driver supplies only the std headers it needs.
cat > "$build/stub/headers.hpp" <<'EOF'
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
EOF

"${CXX:-clang++}" -std=c++17 -O1 -g -Wall -Wextra \
    -I"$build/stub" \
    -I"$root/far2l/src/markdown" \
    -I"$root/far2l/third_party/quickjs" \
    "$here/ascii_driver.cpp" \
    "$root/far2l/src/markdown/AsciiMermaid.cpp" \
    "$qjs" \
    -o "$build/ascii_driver"

fail=0
for mode in flowchart unsupported broken missing timeout speed; do
    if ! "$build/ascii_driver" "$mode" "$bundle" "$golden" "$build/scratch"; then
        echo "ascii-driver: $mode FAILED" >&2
        fail=1
    fi
done

if [ "$fail" -ne 0 ]; then
    echo "ascii-driver: FAIL" >&2
    exit 1
fi
echo "ascii-driver: ok"
