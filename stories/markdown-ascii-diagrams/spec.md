# Spec: ASCII art for diagrams in the terminal Markdown view

## Bundle `far2l/bootstrap/mermaid-ascii.js`

The bundle defines one global function `renderAscii(src)`. It returns a string. It throws an `Error` whose message starts with `unsupported:` for a diagram type the package does not draw, and any other `Error` for a parse failure.

| Call | Result |
|---|---|
| `renderAscii("graph LR\n  A --> B")` | a string of box-drawing art with the labels `A` and `B`, no ANSI codes, no trailing spaces per line |
| `renderAscii` on a sequence, class, state, ER and XY chart sample | a non-empty string for each |
| `renderAscii("swimlane-beta\n  a")` | throws, message starts with `unsupported:` |
| `renderAscii("graph LR\n  A -->")` | throws, message does not start with `unsupported:` |
| file size of `mermaid-ascii.js` | at most 300 KB |
| the bundle contains `elkjs` | no |

## Class `AsciiMermaid` (`far2l/src/markdown/AsciiMermaid.hpp`)

| Call | Result |
|---|---|
| `AsciiMermaid::Render(src, lines, err)` on a flowchart | true; `lines` holds the art rows (UTF-8, one per row) |
| `Render` on `swimlane-beta` text | false; `err` starts with `unsupported:` |
| `Render` on broken flowchart text | false; `err` holds the parse message |
| `Render` when `mermaid-ascii.js` is missing | false; `err` is `bundle missing` |
| `Render` on an endless-loop source | returns false within 2 s (`err` is `timeout`); later calls work |
| three `Render` calls on a flowchart | the 2nd and 3rd take less than 50 ms each |

## Viewer (Markdown mode)

| Case | Result |
|---|---|
| a `mermaid` flowchart fence | art rows in role art (`COL_PANELHIGHLIGHTTEXT`), no raw fence text |
| a `swimlane-beta` fence | the fence as a code block (role code), then the hint line in role body |
| a failed fence | same as the previous row |
| source files `MermaidEngine.*` and `SvgCanvas.*` | deleted; no reference to `mermaid.min.js` in `far2l/src` |
| smoke test `0007-view-file-markdown` | passes, with the screen showing box art for the flowchart and the hint for the swimlane |

## Done

`sh stories/markdown-ascii-diagrams/check.sh` exits 0. It builds the bundle test (Node, golden files in `testing/markdown/golden/`), builds far2l and runs smoke test 0007.

## Out of scope

C++ port of the renderer; per-part colors; width scaling; the browser view (story B).
