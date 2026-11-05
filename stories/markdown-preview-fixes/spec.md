# Spec: F3 Markdown preview — mermaid and colors

## Mermaid engine (`far2l/src/markdown/MermaidEngine.cpp`)

| Call | Result |
|---|---|
| `MermaidEngine::Available()` with `mermaid.min.js` beside the binary | `true`; stderr has no `eval failed` line |
| `Render("flowchart LR\n A[Start] --> B[Finish]\n", "m1", 800, svg, err)` | returns `true`; `svg` starts with `<svg`; contains `Start` and `Finish`; `err` is empty |
| `Render` of the `swimlane-alpha` block in `sample.md` | returns `true`; `svg` contains `Start` and `Finish` |
| `Render` of invalid mermaid text | returns `false`; `err` is not empty; no crash |

## SVG to cells (`far2l/src/markdown/SvgCanvas.cpp`)

| Call | Result |
|---|---|
| `ToCells` of the flowchart SVG above | grid has the text `Start` and `Finish`; has at least one of `┌ ┐ └ ┘ │ ─` |
| Same SVG, width 80 cells | every grid row is at most 80 cells wide |

## Colors in Markdown mode

| Case | Result |
|---|---|
| Any markdown text cell, table cell, box glyph | background equals the viewer background (`COL_VIEWERTEXT`) |
| Foreground of heading, strong, emphasis, code, link, quote | stays as in `MarkdownRender.cpp` constants |
| Any cell | no black and no red background unless the viewer theme itself uses it |
| Selected text | uses the viewer selection color |

## Integration

| Case | Result |
|---|---|
| `sample.md`, F3, Shift-F4, scroll to the mermaid fence | box art with `Start` and `Finish` on screen; the raw text ```` ```mermaid ```` is gone |
| Same, in macOS Terminal and ghostty | same result (the owner checks this by hand) |
| Mermaid engine fails | the fence shows as raw text, as before; the viewer does not crash |

## Done

The check (see `story.md` Notes) exits 0: the build passes and `tests/0007-view-file-markdown` passes. The 0007 test asserts `Start`, `Finish` and a box glyph after the scroll, and a cell background check. The mermaid driver (T1) prints `ok=1` for the flowchart.

## Out of scope

New diagram types beyond what mermaid renders. Italic or bold text. Changes to hex or text mode. Fixing the smoke tool EPERM problem (retry only).
