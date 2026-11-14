#!/bin/sh
# Golden test for far2l/bootstrap/mermaid-ascii.js (story markdown-ascii-diagrams).
# Each golden/<name>.mmd is rendered by the bundle in Node and, when the repo's quickjs
# build is there, in QuickJS; each output must equal golden/<name>.txt byte for byte.
# The goldens come from the beautiful-mermaid package (golden/make-golden.mjs), not the bundle.
# Also checks the size limit, that elkjs is not bundled, and the unsupported and parse-error rows.
# usage: sh testing/markdown/ascii-golden.sh
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
bundle=$root/far2l/bootstrap/mermaid-ascii.js
gold=$here/golden
runner=$gold/run-bundle.js
fail=0

fail_msg() {
    echo "FAIL: $*" >&2
    fail=1
}

if [ ! -f "$bundle" ]; then
    echo "ascii-golden: bundle missing: $bundle (build it with sh far2l/bootstrap/mermaid-ascii/build.sh)" >&2
    exit 1
fi

size=$(wc -c < "$bundle" | tr -d ' ')
if [ "$size" -gt 307200 ]; then
    fail_msg "bundle is $size bytes, limit is 307200 (300 KB)"
fi
if grep -q elkjs "$bundle"; then
    fail_msg "bundle contains elkjs"
fi

tmp=$(mktemp -d "${TMPDIR:-/tmp}/ascii-golden.XXXXXX")
trap 'rm -rf "$tmp"' EXIT

# leg LABEL COMMAND...: runs COMMAND with each sample appended and compares the output to its golden.
leg() {
    label=$1
    shift
    for mmd in "$gold"/*.mmd; do
        name=$(basename "$mmd" .mmd)
        out=$tmp/$label-$name.txt
        if ! "$@" "$mmd" > "$out" 2> "$tmp/err.txt"; then
            fail_msg "$label $name: $(cat "$tmp/err.txt")"
            continue
        fi
        if ! cmp -s "$out" "$gold/$name.txt"; then
            fail_msg "$label $name differs from golden/$name.txt"
            diff "$gold/$name.txt" "$out" >&2 || true
        fi
    done
}

if ! command -v node > /dev/null 2>&1; then
    echo "ascii-golden: node missing (needed for the Node leg)" >&2
    exit 1
fi
leg node node "$gold/node-host.mjs" "$bundle" "$runner"

# The QuickJS leg needs the repo's quickjs library (libqjs.a) from the main checkout's build dir.
main=$root
if common=$(cd "$root" && git rev-parse --git-common-dir 2> /dev/null); then
    main=$(cd "$root" && cd "$common/.." && pwd)
fi
qjs_build=$main/out/far2l/third_party/quickjs
if [ -f "$qjs_build/libqjs.a" ]; then
    ${CC:-cc} -std=c11 -O1 -I"$root/far2l/third_party/quickjs" "$gold/qjs-host.c" \
        "$qjs_build/libqjs.a" -lm -lpthread -o "$tmp/qjs-host"
    leg qjs "$tmp/qjs-host" "$bundle" "$runner"
else
    echo "skip qjs: no $qjs_build/libqjs.a (build far2l first); the Node leg still ran"
fi

for mmd in "$gold"/*.mmd; do
    name=$(basename "$mmd" .mmd)
    txt=$gold/$name.txt
    if [ ! -s "$txt" ]; then
        fail_msg "golden/$name.txt is missing or empty"
        continue
    fi
    if grep -q "$(printf '\033')" "$txt"; then
        fail_msg "golden/$name.txt has ANSI codes"
    fi
    if grep -q ' $' "$txt"; then
        fail_msg "golden/$name.txt has trailing spaces"
    fi
done

if ! grep -qx 'unsupported: swimlane-beta' "$gold/swimlane.txt"; then
    fail_msg "swimlane row is not 'unsupported: swimlane-beta'"
fi
bad_lines=$(wc -l < "$gold/bad.txt" | tr -d ' ')
if [ "$bad_lines" != 1 ] || grep -q '^unsupported:' "$gold/bad.txt"; then
    fail_msg "bad row must be one parse-error line, not an unsupported row"
fi

if [ "$fail" -ne 0 ]; then
    echo "ascii-golden: FAIL" >&2
    exit 1
fi
echo "ascii-golden: ok"
