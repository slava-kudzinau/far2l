#!/bin/sh
# The check from AGENTS.md for this worktree. Run it from the worktree root.
# Clears stale unix sockets before each test (open owner item 11).
BUILD=${BUILD:-../../../far2l.palette.build}
APP=$(cd "$BUILD" && pwd)/install/far2l.app/Contents/MacOS/far2l
TESTS=${TESTS:-0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off}
cmake --build "$BUILD" --target far2l -j8 >/dev/null || exit 1
sh testing/markdown/ascii-golden.sh || exit 1
sh testing/markdown/ascii-driver.sh || exit 1
cd testing || exit 1
for t in $TESTS; do
	find tests -maxdepth 1 -type s -delete
	o=$(./far2l-smoke-run.sh "$APP" "$t" 2>&1)
	if [ $? -ne 0 ] || ! echo "$o" | grep -q '\[DONE\]' || echo "$o" | grep -q FAILED; then
		echo "FAILED: $t"
		echo "$o" | tail -15
		exit 1
	fi
done
find tests -maxdepth 1 -type s -delete
echo "check ok"
