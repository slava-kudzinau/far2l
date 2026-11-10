---
description: Width-aware Markdown tables that wrap cell content so the F3 viewer never requires horizontal scrolling.
status: in-progress
size: M
depends-on: markdown-horizontal-scroll
risk: contract
---

# Story: markdown-table-fit

## Goal

An end user reading a Markdown file in F3 can see every table cell within the current viewer width. A table grows vertically when its cells need more lines, but it never requires Right or Left to reveal table content.

## Context

- `docs/architecture/features/markdown/markdown-viewer.spec.md` defines the current table and wrap contract.
- `docs/architecture/features/markdown/markdown.runtime.md` describes width-dependent rendering and line-index scrolling.
- `docs/architecture/features/markdown/markdown.invariants.md` defines styled runs, source offsets, wrapping and cache rules.
- `far2l/src/markdown/MarkdownRender.cpp` currently computes natural column widths and emits one fixed line per table row.
- `testing/tests/0007-view-file-markdown/` already checks table borders, spacing and palette roles.
- `testing/tests/000f-view-md-wrap/` owns the F2 wrap and horizontal-scroll behavior introduced by `markdown-horizontal-scroll`.

## Scope

**In:**

- Replace natural-width-only table layout with width-aware layout in `MarkdownRender.cpp`, retaining natural widths when they fit the viewer.
- Allocate column widths within the available viewer width, including borders and cell padding.
- Wrap each cell at whitespace where possible and split an overlong word when required.
- Render continuation lines with table borders and padding, preserving `MdRun` roles for body, header, code and link content.
- Preserve separator-row behavior, header styling, source offsets, blank-row rules and resize re-rendering.
- Add smoke coverage for a table wider than the terminal, including a long cell, multiple rows and styled inline content.
- Update the Markdown viewer spec, runtime description and layout invariants from horizontal table scrolling to vertical cell wrapping.

**Out:**

- No changes to prose, quote, list, fence or mermaid wrapping beyond interactions already covered by `markdown-horizontal-scroll`.
- No truncation or ellipsis policy. Table content remains recoverable in the rendered rows.
- No table alignment support. Source alignment markers remain ignored.
- No browser-view HTML table changes.
- No changes to the md4c parser or vendored code.

## Size

M. One renderer layout algorithm, one existing smoke fixture, and three durable Markdown contract documents. The renderer, fixture, and contract change together. Hard stop at natural-width allocation, styled cell wrapping, and regression coverage. Do not add table alignment, truncation, selection, or parser changes.

- The narrow-viewer policy is fixed. Keep the original columns, give each column the smallest deterministic width that fits the available table area, and wrap aggressively. Do not stack or transpose columns, and do not keep horizontal scrolling as an exception.
- Continuation lines repeat the complete vertical border structure and cell padding. This keeps column boundaries visible while a cell occupies multiple visual rows.
- The table has no top border. The header starts with its left and right borders, followed by the separator row. Continuation lines repeat the vertical borders and padding, and the table has no bottom border.
- The test harness's most stable assertion for “no horizontal scroll” must be confirmed while implementing the fixture. Prefer checking visible continuation text and bounded line contents over inspecting private layout state.

## Acceptance criteria

- At an 80-column viewer width, a table whose natural width fits uses that width. A wider table renders all cell content in rows bounded by the viewer width. Right and Left do not reveal additional table content.
- Cell content wraps vertically at the allocated column width. Whitespace is a preferred break point, and a single word longer than the column is split rather than clipped.
- Each continuation line retains the table's vertical borders, cell padding and column boundaries. The separator row remains directly below the complete header row.
- Header text uses the strong role, table borders use the rule role, and inline code or link runs retain their own roles after wrapping.
- Empty cells, short rows, missing cells and cells with multiple styled runs do not misalign columns or create trailing spaces.
- A resize or F2/Shift-F2 re-render recalculates table widths and keeps scrolling by rendered line index without decreasing `nFilePos`.
- Existing table spacing, palette, default-on and Markdown wrap smoke tests pass. The Markdown golden and driver checks pass.
- `markdown-viewer.spec.md`, `markdown.runtime.md` and `markdown.invariants.md` state that tables fit the viewer width and grow vertically instead of horizontally scrolling.

## Notes

The owner chose wrapping because truncation hides content. The table remains a structured `rows × cells × styled runs` model until layout. Width allocation and wrapping preserve palette roles. This story follows `markdown-horizontal-scroll`; both change `MarkdownRender.cpp` and the Markdown smoke tests.
started: 2026-11-09T00:00:00Z
flow: tier=full docs=spec,plan design=no orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=none
G2-design waived by owner, 2026-11-09: "work on your own". The owner asked to keep status in-progress until the corrected implementation had a fresh review. Final reviewer approved the display-cell splitter, padding, and wrap tests on 2026-11-09. The full build and Markdown check passed on 2026-11-09.
