---
status: in-progress
size: M
risk: none
depends-on: markdown-preview-reading
---

# Story C: ASCII art for diagrams in the terminal Markdown view

## Why
The terminal view draws mermaid diagrams with the real mermaid library and a pixel rasterizer. It is slow (seconds), large (a 5 MB library and 1,400 lines of rasterizer) and the art looks poor. The owner chose: draw the diagram types that `beautiful-mermaid` supports as clean box art, and send every other type to the browser (story B, Alt-F3).

## What
- Bundle the ASCII part of the npm package `beautiful-mermaid` (v1.1.3, MIT) into one small JS file, `far2l/bootstrap/mermaid-ascii.js`. It runs in the quickjs engine that is already in the tree.
- Flowchart, state, sequence, class, ER and XY chart fences show as box art in the viewer's art color (far theme palette role).
- Any other diagram type (for example `swimlane-beta`), or a fence that fails to draw, shows as a code block plus one hint line: `(diagram not drawn here - Alt-F3 opens it in the browser)`.
- The old path is removed from the terminal view: `MermaidEngine`, `SvgCanvas` and the use of `mermaid.min.js`.

## Not in this story
- A C++ port of `beautiful-mermaid` (parked).
- Per-part colors inside one diagram (one art color only).
- Wrapping or scaling art wider than the screen (it is cut at the screen edge, like code lines).

## Notes
- Branch `markdown-preview/ascii-diagrams`, worktree `.worktrees/markdown-ascii-diagrams`, based on `markdown-preview/reading` (unmerged). Runs in parallel with story B. B touches only `far2l/bootstrap/md-open.sh`, `marked.min.js` and `marked.LICENSE.md`, so the two do not share files.
- Budget/hard stop: M $15/$20.
- Owner rules: no PR, no push until the owner says so. Commit trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- Owner-checked paths: stories/*/story.md.
- started: 2026-11-06T00:00:00Z
