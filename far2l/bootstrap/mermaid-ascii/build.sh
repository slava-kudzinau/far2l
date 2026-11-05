#!/bin/sh
# Builds ../mermaid-ascii.js (far2l/bootstrap/mermaid-ascii.js) from the pinned
# beautiful-mermaid package, and copies the package's LICENSE to ../mermaid-ascii.LICENSE.md.
# Node and npm are build-time tools only; the bundle needs neither at run time.
# usage: sh far2l/bootstrap/mermaid-ascii/build.sh
set -eu

here=$(cd "$(dirname "$0")" && pwd)
cd "$here"
npm install --no-audit --no-fund --no-package-lock
npx --yes esbuild@0.25.8 entry.js --bundle --minify --format=iife --global-name=__bm \
    --platform=neutral --outfile=../mermaid-ascii.js
cp node_modules/beautiful-mermaid/LICENSE ../mermaid-ascii.LICENSE.md
