#!/bin/sh
# Check for the markdown-ascii-diagrams story. T1: the bundle golden test only.
# T3 adds the far2l build and smoke test 0007.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
sh "$root/testing/markdown/ascii-golden.sh"
