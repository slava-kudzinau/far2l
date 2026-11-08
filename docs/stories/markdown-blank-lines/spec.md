# Spec: Source blank lines in the F3 Markdown view

Story: [story.md](story.md). Replaces the Spacing section of
[markdown-viewer.spec.md](../../architecture/features/markdown/markdown-viewer.spec.md) at close-out.

## `far2l/src/markdown/MarkdownRender.cpp`, the spacing of rendered rows

| Source | Rows |
|---|---|
| One empty source line between two blocks | Exactly one blank row between the rows of the two blocks. |
| Two empty source lines in a row | Exactly two blank rows. N empty lines give N blank rows. |
| A paragraph after a heading of level 2 to 6, one empty source line between them | One blank row between the heading row and the paragraph row. |
| No empty line between two blocks | No blank row between them. The renderer adds no blank row anywhere. |
| A leading empty line of the file | One blank row before the first content row. |
| A trailing empty line of the file | One blank row after the last content row. |
| An empty line inside a list or a quote | One blank row that keeps the `row_lead()` of that list or quote. |
| An empty list item (`-` with no text) | No row of its own. |
| A fence marker line, a table rule line, a thematic break line | No blank row. These lines are not empty lines. |
| An empty line inside a code fence | A code row (unchanged; the fence body is verbatim). |

## Positions

Each blank row takes the file position of the row before it, so the `nFilePos` of the
rows never decreases down the document (ADR-003). A blank row before the first content
row takes position 0.

## Done

The check command of [AGENTS.md](../../../AGENTS.md) prints `check ok` (exit 0), run from
the worktree with `BUILD=../far2l.markdown-blank-lines.build`, with:

- The gap queue gone from `MarkdownRender.cpp`: `Gap`, `gap`, `spacer_row`, `tight_heading_row`,
  `force_gap`, `drop_soft_gap`, `close_block`, `last_row_is_spacer` and `after_tight_heading`
  are deleted. The gap logic of `open_block` is deleted too. No other file changes its spacing.
- `testing/tests/0007-view-file-markdown/test.js` spacing assertions rewritten to the table
  above; `spacing.md` gains cases for a leading empty line and a blank row after a thematic
  break. The rest of the file (0008, 0009, golden, driver) unchanged and green.

## Out of scope

The Alt-F3 browser view. Table, code fence and mermaid content rendering. The palette,
the settings keys, the golden files. The removed rule rows under headings and the lighter
table borders of markdown-compact-layout stay.
