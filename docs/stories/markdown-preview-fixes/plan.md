# Plan: F3 Markdown preview — mermaid and colors

Spec: `spec.md` (governs exact values).

## Global Constraints

- C++ in `far2l/src/markdown/*` and `far2l/src/ViewerPrinter.*`, `far2l/src/viewer.cpp` only where a task says so. No new dependency. No network.
- Do not edit `story.md` (the handover was merged into it). Branch: `markdown-preview/fixes`, in place (no worktree: the feature is uncommitted on master).
- Spike findings already in the working tree (review them, keep them if sound): `MermaidEngine.cpp` now has `window.addEventListener` stubs, an extended DOM shim (`ownerDocument`, `namespaceURI`, siblings, `getBBox`, `getComputedTextLength`, `CSSStyleSheet`, `DOMParser`, `document.implementation`), and stacks in error text. Remaining failure at the last run: `not a function` at `mermaid.min.js:395:27878` (called from `draw`, `renderDiagram`). Find the missing function and add it to `kDomShim`.
- Standalone driver (no TUI needed): a `headers.hpp` stub in an include dir, then `clang++ -std=c++17 -I<stubdir> -Ifar2l/src/markdown -Ifar2l/third_party/quickjs driver.cpp far2l/src/markdown/MermaidEngine.cpp ../far2l.build/far2l/third_party/quickjs/libqjs.a`; `mermaid.min.js` must be in the cwd or beside the exe. Keep the driver as `testing/markdown/mermaid_driver.cpp` with `testing/markdown/run.sh`.
- Contingencies from the original plan: sync `setTimeout`, `htmlLabels:false`, `flowchart.useMaxWidth:false`, fixed `theme` and no `securityLevel` DOM purify when it fails.
- Run the real tool: for each shim fix, rerun the driver and read the real error. Do not guess.
- Colors: runs carry foreground-only attributes (`F_*` in `MarkdownRender.cpp:26-34`). `MarkdownPrinter::Print` calls `SetFarColor(run.attr)`. The viewer text color is `FarColorToReal(COL_VIEWERTEXT)`. The symptom is black background (tables, box glyphs) and red background (everywhere). Find the cause by reading how `SetFarColor` and `FarColorToReal` treat the value, and how `PlainViewerPrinter` does it. Fix by combining each run's foreground with the viewer background.
- Settled choices: keep `MermaidEngine::Render` as declared in `MermaidEngine.hpp` (SVG in `svg_out`, error text in `err`, bool result). Keep the cache key (fence file position, width) and the 8x16 px per cell mapping in `SvgCanvas`. Cell rule: a shape edge becomes the box glyph for its direction; text overwrites glyphs. Put the color fix where the run attribute is built or printed (`MarkdownRender.cpp` or `MarkdownPrinter::Print`), not in `viewer.cpp`, unless the cause is proved to be there; if so, log it in the closeout. Engine load happens once per process; a failed load is not retried.
- Commit message: a short imperative summary line, a blank line, then `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. One commit per task.
- Check: `cd /Users/Viachaslau_Kudzinau/Projects/far2l && cmake --build ../far2l.build --target far2l -j8 && cd testing && ./far2l-smoke -t 30 ../../far2l.build/install/far2l.app/Contents/MacOS/far2l tests/0007-view-file-markdown` (retry on `operation not permitted`). Owner-checked paths: `stories/markdown-preview-fixes/story.md`.

## T1: Mermaid renders SVG in the engine

**Depends on:** none
**Done check:** `sh testing/markdown/run.sh flowchart` exits 0 and prints `ok=1` and an SVG that contains `Start` and `Finish`; `sh testing/markdown/run.sh swimlane` the same; `sh testing/markdown/run.sh invalid` prints `ok=0` with a non-empty error and exits 0
**Role:** implementer — review task-reviewer
Make `MermaidEngine::Render` pass the three spec rows. Add the driver and `run.sh`. Keep stack text in errors. Remove no diagnostics.

## T2: SvgCanvas turns the real SVG into box art

**Depends on:** T1
**Done check:** `sh testing/markdown/run.sh cells` exits 0 and prints a grid with `Start`, `Finish` and a box glyph, every row at most 80 cells
**Role:** implementer — review task-reviewer
Feed the real mermaid SVG (flowchart and swimlane) to `SvgCanvas::Parse` and `ToCells`. Fix what the real SVG needs (CSS `style` attributes, `<g transform>`, `<foreignObject>` or `<text>` labels, markers, `<path d>` commands). Extend `run.sh` with a `cells` mode.

## T3: Viewer shows the box art (integration)

**Depends on:** T1, T2
**Done check:** the Check command in Global Constraints exits 0 with 0007 asserting `Start`, `Finish` and a box glyph after the scroll
**Role:** implementer — review final
Trace `MarkdownDoc::Render` mermaid splice and the cache for the real result. Make sure the asset is found in the built app (`Contents/Resources`). Extend `testing/tests/0007-view-file-markdown/test.js`.

## T4: Colors use the viewer background

**Depends on:** none
**Done check:** the Check command exits 0 with 0007 asserting that the background of a table cell, a box glyph cell and a text cell equals the background of a plain viewer cell (use `READ_CELL` attrs through the smoke API)
**Role:** implementer — review task-reviewer
Owner ruling (2026-11-05): use the usual far theme colors, not rainbow colors. Set every `ATTR_*` constant in `MarkdownRender.cpp:26-34` to the viewer text color of the theme (read at render time from `COL_VIEWERTEXT`, so theme changes apply), and keep structure visible by layout only (heading underline rule, `│ ` quote prefix, indented code, bullets, table borders). Then fix the cause named in Global Constraints. Selection still uses `_sel_color`. Do not change hex or text modes.
