---
description: Lite plan for the compact layout of the F3 Markdown view, with its spec, its exact values and one task.
status: in-progress
---

# Plan: Compact layout in the F3 Markdown view

Tier: lite (`flow tier=lite docs=plan design=no orchestrator=no final=final-reviewer-light gate=read-later scan=no verifier=none`). No `spec.md`: the spec is the section below.

## Spec

The rules below apply to `far2l/src/markdown/MarkdownRender.cpp`. "Blank row" means one row with no text, or with only the lead of a list or a quote.

| # | Block | Rule |
|---|---|---|
| R1 | Any heading | No rule row under it. One blank row before it, except at the top of the document. |
| R2 | H1 | One blank row after it. |
| R3 | H2 to H6 | No blank row after it. |
| R4 | List (outermost) | No blank row before it or after it. A heading or a code block next to a list keeps its own blank row. |
| R5 | Code block, HTML block | One blank row before it and after it. |
| R6 | Paragraph, quote | One blank row between blocks, as today. |
| R7 | Table | Vertical bars and the one line under the header stay. The top and bottom borders go. |
| R8 | Thematic break | The full-width line stays. It is the only full-width line. |
| R9 | Mermaid art | No change. A mermaid fence is a code block, so R5 applies. |

Rules for two blocks that meet:

| # | Blocks | Result | Source |
|---|---|---|---|
| C1 | H2 to H6, then a heading | One blank row. R1 wins. | Decision in `gate.md` |
| C2 | H2 to H6, then a code block | No blank row. R3 wins. | Decision in `gate.md` |
| C3 | H1, then a list | One blank row. | R2 and R4 |
| C4 | H2 to H6, then a list | No blank row. | R3 and R4 |
| C5 | Code block, then a list | One blank row. | R4 and R5 |
| C6 | List, then a code block | One blank row. | R4 and R5 |
| C7 | List, then a heading | One blank row. | R1 and R4 |
| C8 | Paragraph, quote, table or thematic break, then a list, in either order | No blank row. | R4 |
| C9 | List, then a list of the other type | No blank row. | R4 |
| C10 | Code block in a list item, then the next item | One blank row. | R5 |
| C11 | Paragraph, quote, table or thematic break, then one of these four | One blank row, as today. | R6 |
| C12 | H2 to H6, then a paragraph, quote, table or thematic break | No blank row. | R3 |
| C13 | A heading or a code block as the first block of a list item | No blank row. The block stays on the row of the bullet. | Decision in `gate.md` |
| C14 | Any block as the first row of the document | No blank row. `open_block` already does this when `out` is empty. | R1 |

Blocks inside a list item or a quote follow the same rules. This is the rule in `markdown-viewer.spec.md`: a block inside a list item is spaced like a top-level block.

A setext heading (`Title` over `---`, or over `===`) is a heading. Only a thematic break draws a line.

Row offsets (`nFilePos`) stay non-decreasing. Scroll, Shift-F4 and the saved position work as before.

### Done

`sh docs/stories/markdown-compact-layout/check.sh` prints `check ok` and exits 0. Each row of the two tables above has a check in `0007-view-file-markdown`. R6 and R9 keep their present checks.

## Global Constraints

- Language C++ in `MarkdownRender.cpp`, JavaScript in the smoke tests. No new dependency.
- Files this story may touch: `far2l/src/markdown/MarkdownRender.cpp`, `testing/tests/0007-view-file-markdown/test.js`, `testing/tests/0007-view-file-markdown/initdir/left/spacing.md`, `testing/tests/0007-view-file-markdown/initdir/left/sample.md`, `testing/tests/0008-view-md-default/test.js`, `testing/tests/0009-view-md-setting-off/test.js`.
- `long.md` does not change. `0008` and `0009` fixtures do not change.
- Do not touch `far2l/src/ViewerPrinter.cpp`, `far2l/src/Viewer.cpp` or any other viewer file.
- Runs take an `MdRole`. Do not add a role. Table borders keep `MD_ROLE_ART`.
- Offset rule, from `markdown.invariants.md` [scroll]: `nFilePos` never decreases down the document. A blank row takes the offset of the row before it. Keep this code in `open_block`.
- Markdown files are untrusted input. A new branch must not read `out.back()` when `out` is empty.
- Comments: no comment can describe the old layout. Update each comment that names heading rule rows or the Spacing rules. Update the comment in `Builder::flush` too.
- Do not regenerate `testing/markdown/golden/*.txt`.
- Commit message: a short imperative summary line, a blank line, then the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. One commit per task.
- Check: `sh docs/stories/markdown-compact-layout/check.sh`, from the worktree root. Owner-checked paths: `AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`, `testing/markdown/golden/*.txt`, `far2l/src/markdown/md4c/`, `far2l/third_party/quickjs/`, `.github/workflows/`.

## T1: Compact layout and its tests

**Depends on:** none
**Done check:** `sh docs/stories/markdown-compact-layout/check.sh` exits 0
**Role:** implementer — review final

Work test first: change the tests and fixtures, run the check, and record the failure (`FAILED: 0007-view-file-markdown`). Then change the code.

### Code, in `Builder`

1. Replace `bool gap_pending` with `enum Gap { GAP_NONE, GAP_SOFT, GAP_HARD }; Gap gap = GAP_NONE;`. `GAP_SOFT` is a blank row that a list drops. `GAP_HARD` is a blank row that a list keeps.
2. `close_block(Gap kind = GAP_SOFT)`: set `gap` to the larger of `gap` and `kind`, so a hard gap is never lowered. The `cur_started` branch of `open_block` raises `gap` the same way, to at least `GAP_SOFT`.
3. `open_block` emits a blank row for `GAP_SOFT` and `GAP_HARD`, resets `gap` to `GAP_NONE`, and otherwise keeps its present logic.
4. Add `void force_gap()`: set `gap = GAP_HARD`.
5. Add `void drop_soft_gap()`: if `gap == GAP_SOFT`, set `gap = GAP_NONE`.
6. Add `size_t tight_heading_row = (size_t)-1;` and `bool after_tight_heading() const { return !out.empty() && tight_heading_row == out.size() - 1; }`. This uses the same pattern as `spacer_row`.

### Code, in the callbacks

| Event | Change |
|---|---|
| Enter `MD_BLOCK_UL`, `MD_BLOCK_OL`, outermost (`list_stack.empty()`, as today; a list in a quote counts) | `drop_soft_gap()`, then `open_block()`. |
| Enter or leave `MD_BLOCK_UL`, `MD_BLOCK_OL`, nested | No change. A soft gap before a nested list goes at the next `MD_BLOCK_LI`. |
| Leave `MD_BLOCK_UL`, `MD_BLOCK_OL`, outermost | `drop_soft_gap()` in place of `close_block()`. |
| Enter `MD_BLOCK_LI` | `drop_soft_gap()` in place of `gap_pending = false`. |
| Enter `MD_BLOCK_H` | If not `li_prefix_pending`, `force_gap()`. Then `open_block()`. |
| Leave `MD_BLOCK_H` | `flush()`. Delete the rule row. Level 1: `close_block(GAP_HARD)`. Levels 2 to 6: set `tight_heading_row = out.size() - 1` when `out` is not empty, and do not call `close_block`. `heading_level = 0` for all levels. |
| Enter `MD_BLOCK_CODE`, `MD_BLOCK_HTML` | If not `after_tight_heading()` and not `li_prefix_pending`, `force_gap()`. Then `open_block()`. |
| Leave `MD_BLOCK_CODE`, `MD_BLOCK_HTML` | `close_block(GAP_HARD)`. |
| Leave `MD_BLOCK_TABLE` | Delete the first and the last `render_sep` calls. Keep the one after the header row. |

A paragraph, a quote, a table and a thematic break keep `open_block()` and `close_block()` with no change.

### Fixtures

Add one source blank line after `Spacing last paragraph.`, then these blocks, exactly as written, in this order. Each text is unique on the screen. The test finds rows by these texts.

Each line below has an indent of 4 spaces in this plan. Delete the indent when you copy the lines to `spacing.md`.

    Compact para before list.
    - Compact item after para.

    Compact para after list.

    # Compact Top

    - Compact item after top.

    ## Compact Second

    - Compact item after second.

    ## Compact Upper

    ### Compact Lower

    Compact para after lower.

    ## Compact Before Fence

    ```text
    Compact fence after heading.
    ```

    - Compact item after fence.

    ```text
    Compact fence after item.
    ```

    - Compact item before heading.

    ## Compact After Item

    - Compact fence item one:
      ```text
      Compact fence in item.
      ```
    - Compact fence item two.

    1. Compact ordered item.

    ###### Compact Six

    Compact para after six.

    Compact Setext
    ---

    Compact para after setext.

    Compact Setext Top
    ===

    Compact para above break.

    ---

    Compact para below break.

    | Compact A | Compact B |
    |-----------|-----------|
    | compact 1 | compact 2 |

    Compact para after table.

The first block has no source blank row between the paragraph and the list. It is correct Markdown, and md4c starts the list there.

### Tests in `0007-view-file-markdown/test.js`

Use `CheckBlankRow`, `CheckRowOf` and `CheckCellChar`. A row is `found.Y`.

Change these present checks:

1. Heading Four: delete the rule checks. `- bullet one` is on the row after `Heading Four` (R3, C4).
2. Long Start and Long End: the first paragraph is on row heading + 2, and heading + 1 is blank (R1, R2).
3. Spacing Title: the blank row and the lead paragraph move up by one row. `Spacing Heading`: `Paragraph before two source blanks.` is on row heading + 1 (R3).
4. List in `spacing.md`: `Spacing item one` is on the row after `Paragraph after two source blanks.`. The table header is on the row after `Spacing item two` (C8).
5. Table in `spacing.md`: the header, the separator and one body row. The blank row after the table is `table.Y + 3`. The quote comes after it.
6. Fence in an item: the row after the item text is blank (R5). `Spacing paragraph after the list.` keeps a blank row, from the fence (R5).
7. Loose list: `Spacing item with a paragraph.` is on the row after `Spacing paragraph after the list.` (C8). `Spacing last paragraph.` is on the row after `Spacing item paragraph two.` (C8).
8. The comment "after three pages the top row is Item 057" and its expected text: use the item that is at the top row in the new layout. It must be an item of the list, so that the Shift-F4 round trip still tests a list row. Keep 3 pages. If the top row is not a list item, change the page count and say so in the report.

Add these checks, on the new blocks:

| Check | Expected |
|---|---|
| `Compact item after para.` | On the row after `Compact para before list.` (R4) |
| `Compact para after list.` | On the row after `Compact item after para.` (R4) |
| `Compact Top` | Row above is blank (R1). Row below is blank (R2). |
| `Compact item after top.` | On row `Compact Top` + 2 (C3) |
| `Compact Second` | Row above is blank (C7). |
| `Compact item after second.` | On the row after `Compact Second` (C4) |
| `Compact Lower` | On row `Compact Upper` + 2, with a blank row between (C1) |
| `Compact para after lower.` | On the row after `Compact Lower` (R3) |
| `Compact fence after heading.` | On the row after `Compact Before Fence` (C2) |
| `Compact item after fence.` | On row fence + 2, with a blank row between (C5) |
| `Compact fence after item.` | On row item + 2, with a blank row between (C6) |
| `Compact After Item` | On row `Compact item before heading.` + 2, with a blank row between (C7) |
| `Compact fence item two.` | On row `Compact fence in item.` + 2, with a blank row between (C10) |
| `Compact ordered item.` | On the row after `Compact fence item two.` (C9) |
| `Compact para after six.` | On the row after `Compact Six` (R3, level 6) |
| `Compact para after setext.` | On the row after `Compact Setext`. A rule row there fails this check. (setext, level 2) |
| `Compact para above break.` | On row `Compact Setext Top` + 2, with a blank row between (setext, level 1) |
| Thematic break | The row is `Compact para above break.` + 2. `CheckCellChar(0, y, "─")` and `CheckCellChar(70, y, "─")` on it (R8). |
| `Compact A` | Row above is blank (R6, as a table is not next to a list here). `CheckCellChar` of `├` at the header's row + 1, at `X - 2` (R7). |
| `compact 1` | The row above it is the separator, and the row below it is blank (R7). `Compact para after table.` is on row `compact 1` + 2. |
| `- ## ` first block | Add the line `- ## Compact Item Heading` as the last line of `spacing.md`, after `Compact para after table.` and one source blank line. The bullet and the heading text are on one row (C13). |
| Whole file | While each page of `spacing.md` shows, `ExpectNoString("┌")` and `ExpectNoString("└")` (R7). Use a short timeout, as the present calls do. |

The test reaches rows as follows. After `Spacing last paragraph.` the file is longer than one page, so `TypeEnd` no longer shows the present blocks. Replace `TypeEnd` at the present location with `TypeHome`, then `TypePageDown` as many times as each `ExpectString` needs. Then use `TypeEnd` for the last page. Do not add `Sleep` calls.

The check of a row depends on both rows being on the same page. If a pair of rows falls across two pages, press `TypeDown` to bring both on the page. Do not move blocks in the fixture.

### Tests in `0008-view-md-default` and `0009-view-md-setting-off`

Both use `═` as the sign of Markdown mode. After this story no row has `═`, so:

1. `CheckMarkdownSample`: first `var heading = ExpectString("Heading One")`. Then `ExpectNoString("# Heading One")`. Then `CheckCellChar(heading.X, heading.Y + 2, "A")`, because `A paragraph under the heading.` is on row heading + 2. This order matters. `ExpectNoString` before `ExpectString` can pass while the panels are still on the screen.
2. `CheckPlainSample` and `CheckPlainTxt`: keep the `ExpectString` of `# Heading One` or `# plain`. Delete the `CellCharMatches` check of `═`, because it can no longer fail.
3. `CheckMarkdownPlainTxt`: `plain.txt` has only the line `# plain`. Delete `ExpectString("═")`. Use `ExpectNoString("# plain")`, then `var line = ExpectString("plain", 0, 1, 80, 1)`, then check that `line.X == 0`. The call reads only row 1, so the file name in the title row does not match it.
4. Update the comments that name a rule line. After this task, `grep -c '═'` prints 0 for both `test.js` files.

Prove that these two tests still catch a wrong mode. Before you commit, in `0008` find the first `CheckPlainSample()` call and replace it with `CheckMarkdownSample()`. See the test fail, and restore the call. Say so in the report.
