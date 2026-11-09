---
description: F2 word wrap and Right/Left horizontal scrolling for Markdown rows in the F3 viewer.
status: in-progress
gate: G3
---

# Story: markdown-horizontal-scroll

## Why

The owner reads Markdown in F3 (story markdown-preview-reading, open item: long lines are cut at
the right edge and Right does nothing in the default Wrap mode). Long paragraphs, quotes and
lists need soft wrap; fences, horizontal rules and mermaid art need a horizontal scroll that
keeps their shape.

## Adjacent

- `far2l/src/markdown/MarkdownRender.{hpp,cpp}`: `MdLine.wrap`, the row splitter, the table cell
  splitter, the wrap parameter of `Render`, the cache drops in `Invalidate`.
- `far2l/src/viewer.cpp`: `ProcessWrapMode`, the markdown key guard, the redraw modes,
  `SyncMarkdownTop`, `ReadStringMarkdown`.
- `docs/architecture/features/markdown/`: the wrap contract (spec), the row fields (data), the
  flow (runtime), the rules (invariants).
- `testing/tests/000f-view-md-wrap/`: the F2 toggle, the scroll, the fixed rows under wrap.

## Proven by

- Smoke `000f-view-md-wrap`: F2 → two rows, F2 → one row, Right scrolls the row, the wrap mode
  survives a reopen; table rows, fence rows and mermaid box rows stay one row per row.
- Smoke `0006-view-file-word-wrap`: the plain-viewer wrap path is unchanged.
- Smoke `0007`/`0008`/`0009`: Markdown render, default-on and the settings-off path are unchanged.
- `testing/markdown/ascii-golden.sh`, `ascii-driver.sh`, `highlight-driver.sh`, `look-driver.sh`:
  the render path is unchanged.

## Outcome

`MdLine` has a `wrap` mark. `MarkdownDoc::Render(text, width, wrap)` splits a marked row that is
wider than the width into screen rows (no trailing spaces); the builder marks text, quote, list
and table cell rows; the layout code (fences, rules, art rows) marks its rows with `wrap=false`.
`viewer.cpp` re-renders on F2/Shift-F2 through `ProcessWrapMode` (it sets `VM.Wrap` and
`m_md_ready = false`), adds `SHOW_MARKDOWN_W` for a right-edge reflow, keeps the top row through
the re-render (`SyncMarkdownTop` finds the row by source offset and place), and lets Right, Left,
Home and End move `LeftPos` in Markdown mode in both wrap modes. `Invalidate` drops the render,
the mermaid art cache and the code-block tint cache; the token cache and the Colorer look cache
stay.

The owner rulings that shaped the layout: tables keep one row per table row (the row scrolls when
it is wider than the screen); mermaid art keeps one row per art row; a blank row is one row; a
list continuation starts at the lead indent; the arrows overlay the first and last text column.
