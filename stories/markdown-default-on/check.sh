#!/bin/sh
# Done check for story markdown-default-on (T1).
# Configures far2l in the story build dir when it has no CMakeCache.txt, builds the far2l target,
# builds the smoke runner, refreshes the workdirs of tests 0007 and 0008, and runs both.
# Any failure exits non-zero.
set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD=/Users/Viachaslau_Kudzinau/Projects/far2l.default-on.build
APP="$BUILD/install/far2l.app/Contents/MacOS/far2l"
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

# The bootstrap step copies the vendored mermaid.min.js. It is git-ignored, so a fresh worktree
# lacks it; take it from the main checkout.
if [ ! -f "$ROOT/far2l/bootstrap/mermaid.min.js" ]; then
	cp -f /Users/Viachaslau_Kudzinau/Projects/far2l/far2l/bootstrap/mermaid.min.js "$ROOT/far2l/bootstrap/mermaid.min.js"
fi

if [ ! -f "$BUILD/CMakeCache.txt" ]; then
	cmake -S "$ROOT" -B "$BUILD" -Wno-dev -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTESTING=Yes \
		-DUSEWX=No -DFAR2L_MD_NO_QJS=1 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
fi
cmake --build "$BUILD" --target far2l -j "$JOBS"

make -C "$ROOT/testing/src"

for t in 0007-view-file-markdown 0008-view-md-default; do
	rm -rf "$ROOT/testing/tests/$t/workdir"
	mkdir -p "$ROOT/testing/tests/$t/workdir"
	cp -R "$ROOT/testing/tests/$t/initdir/." "$ROOT/testing/tests/$t/workdir/"
done

cd "$ROOT/testing"
./far2l-smoke "$APP" tests/0007-view-file-markdown tests/0008-view-md-default
