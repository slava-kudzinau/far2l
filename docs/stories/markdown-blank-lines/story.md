---
description: Show a blank row for each empty source line in the F3 Markdown view, with no rows added or removed by the renderer.
status: done
size: M
risk: contract
---

# Story: Source blank lines in the F3 Markdown view

## Goal

A reader opens a Markdown file in F3 and sees the empty lines of the file, row for row, as F4 shows them. Today the renderer makes its own spacing. A heading of level 2 to 6 pulls the next block up tight, so `### Q1` sits directly on `**Answer:**`. But the file has one empty line between them. This story removes the made-up spacing and shows each empty source line as one blank row.

## Context

- [markdown-viewer.spec.md](../../architecture/features/markdown/markdown-viewer.spec.md), section Spacing (lines 112 to 125) - the contract this story replaces.
- [markdown.invariants.md](../../architecture/features/markdown/markdown.invariants.md) - the rules that stay.
- [ADR-002](../../architecture/decisions/adr-002-md4c-parser.md) - md4c gives no block line offsets, but text callbacks point into the input.
- [ADR-003](../../architecture/decisions/adr-003-line-index-scrolling.md) - rows keep a file position, and the positions never decrease.
- `far2l/src/markdown/MarkdownRender.cpp` - the spacing machinery. `Gap` enum and the gap helpers: lines 293 to 335. `open_block`: line 310. The gap calls in `cb_enter_block` and `cb_leave_block`: lines 464 to 703.
- [markdown-compact-layout](../markdown-compact-layout/story.md) - the story whose spacing rules this story reverses.
- `testing/tests/0007-view-file-markdown/` - the smoke test and the fixture `spacing.md` that pin the current spacing.

## Scope

**In:**
- `far2l/src/markdown/MarkdownRender.cpp`: delete the gap queue (`Gap`, `gap`, `spacer_row`, `tight_heading_row`, `force_gap`, `drop_soft_gap`, `close_block`, and the gap logic in `open_block`). Emit blank rows from the source instead. Track the end offset of the last source text (`b.pos`, set in `cb_text`). The count runs at the first text of the next block, not at `enter_block`: md4c gives no offset for a block before its text. The count starts at the start of that text's source line (`line_start_of`), not at the text itself, because the first text often sits mid-line. Count the empty source lines between the two points. Emit one blank row per empty line. The `row_lead()` of a blank row inside a list or a quote stays.
- The spacing assertions of `testing/tests/0007-view-file-markdown/test.js` and, if a case is missing, the fixture `spacing.md`.
- The Spacing section of [markdown-viewer.spec.md](../../architecture/features/markdown/markdown-viewer.spec.md) at close-out.

**Out:**
- The Alt-F3 browser view.
- The content rendering of tables, code fences and mermaid art. Only the rows between blocks change.
- The palette, the settings keys and the golden files.

## Size

One source file, one contract section, one large smoke-test rework. Most 0007 spacing assertions move by one row: the forced blanks around code blocks, headings and lists go away, and the source blank lines appear. Size M. Risk `contract`: the story replaces the Spacing section of the spec. It reverses the spacing ruling of markdown-compact-layout (2026-11-06) with the owner's ruling of 2026-11-08.

## Unknowns

- A thematic break (`---`) gets no text callback, so it has no first-text offset. The builder must anchor its blank rows by a scan of the source. The scan starts at the last text end and stops at the first line that is not empty. Low risk. The build verifies it with a case in `spacing.md`.

## Acceptance criteria

- Each empty line of the source gives exactly one blank row. Two empty lines in a row give two blank rows. Today the test pins the collapse at `test.js` line 410; that assertion flips to two rows.
- A paragraph after a heading of level 2 to 6 has the source blank row between them. Today `test.js` line 407 pins the tight row; that assertion flips to one blank row between.
- No blank row comes from the renderer. No block adds a blank row before or after itself, and no block removes one. The file start and the file end keep their empty lines.
- A blank row inside a list or a quote keeps its lead.
- A blank row takes the file position of the row before it, so the positions never decrease (ADR-003).
- `check ok` from the build command in [AGENTS.md](../../../AGENTS.md), with the 0007 spacing assertions rewritten.
- At close-out, the Spacing section of the spec states the source-driven rule, and the Tests table row for `spacing.md` stays.

## Notes

- Owner ruling, 2026-11-08: "I think colorer display text lines as-is not making any assumptions and not adding anything to compact/expand". The F3 Markdown view shows the text lines of the file as they are.

- Owner ask, 2026-11-08: "lets keep empty lines as exactly as they are in F4 mode, this is for F3 markdown mode". The trigger: `~/react-native-interview-questions.md`, where `### Q1` sits directly on `**Answer:**` in F3, but F4 shows one empty line between them.
- This story reverses the spacing half of markdown-compact-layout. The removed rule rows under headings and the lighter table borders of that story stay.
- Assumptions under the ruling: an empty list item (`-` with no text) still shows no row of its own. Leading and trailing empty lines of the file show as blank rows. A fence marker line, a table rule line and a thematic break line are not empty lines, so they add no blank rows. No new settings keys.

## Build record

- started: 2026-11-08T12:06:06Z
- flow tier=full docs=spec,plan design=no orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=none (risk contract)
- Budget $15, hard stop $20 (size M).
- BASE: `feat/markdown-blank-lines` at `4bdffebbe`, off `master` (the brief commit). Worktree `.worktrees/markdown-blank-lines`, build `.worktrees/far2l.markdown-blank-lines.build`.
- Gate page: stop before build (G2-design, full tier, risk contract).

- G2-design waived by owner, 2026-11-08: "go in auto mod". Autonomous run started.
