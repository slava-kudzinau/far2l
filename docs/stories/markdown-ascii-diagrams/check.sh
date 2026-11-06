#!/bin/sh
# Check for the markdown-ascii-diagrams story: the bundle golden test, the AsciiMermaid driver,
# the far2l build in its own build dir, and smoke test 0007 (the Markdown view of sample.md).
# usage: sh docs/stories/markdown-ascii-diagrams/check.sh
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
# The main checkout (the git common dir's parent); the build dirs sit next to it.
main=$root
if common=$(cd "$root" && git rev-parse --git-common-dir 2> /dev/null); then
    main=$(cd "$root" && cd "$common/.." && pwd)
fi
build=${BUILD:-$(cd "$main/.." && pwd)/far2l.ascii.build}

sh "$root/testing/markdown/ascii-golden.sh"
sh "$root/testing/markdown/ascii-driver.sh"

# The far2l build, with the flags of the main build dir (far2l.build); the smoke test needs TESTING.
cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTESTING=Yes -DUSEWX=No \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
cmake --build "$build" --target far2l -j8

case "$(uname -s)" in
    Darwin) app=$build/install/far2l.app/Contents/MacOS/far2l ;;
    *) app=$build/install/far2l ;;
esac
# A smoke run leaves a unix socket in testing/tests, and the runner takes it for a test
# directory, so the sockets go first. The runner builds the test driver and refreshes each
# test's workdir from its initdir.
find "$root/testing/tests" -maxdepth 1 -type s -exec rm -f {} +
(cd "$root/testing" && sh far2l-smoke-run.sh "$app" 0007-view-file-markdown)

echo "markdown-ascii-diagrams check: ok"
