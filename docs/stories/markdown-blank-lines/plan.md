# Plan: Source blank lines in the F3 Markdown view

Spec: `spec.md` (governs exact values). No design doc: one source file, and the plan carries the design.

## Global Constraints

- Files this story may touch: `far2l/src/markdown/MarkdownRender.cpp`,
  `testing/tests/0007-view-file-markdown/test.js`,
  `testing/tests/0007-view-file-markdown/initdir/left/spacing.md`,
  and the story folder.
- At close-out: `docs/architecture/features/markdown/markdown-viewer.spec.md`, an owner-checked
  path. Edit only its Spacing section and the Tests table row for `spacing.md`.
  `testing/tests/0007-view-file-markdown/initdir/left/spacing.md`,
  the story folder, and at close-out `docs/architecture/features/markdown/markdown-viewer.spec.md`
  (owner-checked path: edit only at close-out, Spacing section and the Tests table row for `spacing.md`).
- Keep the fork's changes to upstream files small; no new files outside the story folder.
- md4c gives no block line offsets; text callbacks point into the input (ADR-002). Rows keep a file
  position and the positions never decrease (ADR-003). A run holds a role, never a color (ADR-004).
- Blank row rules: N empty source lines give N blank rows. The count runs at the first
  text of the next block. The anchor is the start of that text's source line
  (`line_start_of`), not the text itself.
- A blank row takes the `nFilePos` of the row before it. Before the first content row it
  takes 0. It keeps the `row_lead()` and `lead_role()` of the enclosing list or quote.
- An empty list item shows no row. A fence marker, a table rule and a thematic break are
  not empty lines, so they add no blank rows. The renderer adds no blank row of its own anywhere.
- Line definition: a line is empty when it holds no character other than CR, LF, space and
  tab. `cb_text` fires more than once inside one display line (styled spans). The anchor
  rule makes each call count only whole empty lines, so a same-line text counts nothing.
  The `in_code` path also sets `last_text_end` (a fence's text is source text), so the
  empty lines inside a fence are never counted twice. The helper is never called while
  `in_code` is set.
- Blank row shape: no runs except the lead, no tint, no tokens. `flush()`'s fallback keeps
  the position of the row before it, which is the rule above.
- Delete the whole gap queue: `Gap`, `gap`, `spacer_row`, `tight_heading_row`,
  `last_row_is_spacer`, `after_tight_heading`, `force_gap`, `drop_soft_gap`, `close_block`,
  and the gap logic of `open_block`. `open_block` keeps only the flush of a tight item's text.
  Every `force_gap`/`drop_soft_gap`/`close_block` call site in `cb_enter_block` and
  `cb_leave_block` goes. `flush()` keeps its fallback position logic unchanged.
- The thematic-break unknown (story.md): a `---` block gets no text callback. Anchor its
  blank rows by a source scan from the end offset of the last source text. The scan stops
  at the first line that is not empty. Empty lines strictly between the last text line and
  the break line count. Verify with a case in `spacing.md`.
- Code inside `in_code` is verbatim and unchanged: empty lines of a fence stay fence rows.
  The mermaid splice and hint-row logic, the table render and the fence token cache do not change.
- Do not regenerate `testing/markdown/golden/*.txt` (owner-checked).
- Build out of tree: `cmake --build ../far2l.markdown-blank-lines.build --target far2l -j8`
  from the worktree root. Clean stale sockets before each smoke test in the loop
  (see AGENTS.md, Worktrees): `pkill -9 -f -- '--test=/tmp/far2l'; find tests -maxdepth 1 -type s -delete`
  inside `testing/` before each run.
- Commit message: a short imperative summary line, a blank line, then the body. One commit per task.
  Trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Check: (one command, run from the worktree root; it prints `check ok`)
  `BUILD=../far2l.markdown-blank-lines.build; APP=$(cd "$BUILD" && pwd)/install/far2l.app/Contents/MacOS/far2l; cmake --build "$BUILD" --target far2l -j8 && sh testing/markdown/ascii-golden.sh && sh testing/markdown/ascii-driver.sh && (cd testing && for t in 0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off; do o=$(./far2l-smoke-run.sh "$APP" "$t" 2>&1) && echo "$o" | grep -q '\[DONE\]' && ! echo "$o" | grep -q FAILED || { echo "FAILED: $t"; exit 1; }; done) && echo "check ok"`
  Clean stale sockets before each smoke run in the loop (AGENTS.md, Worktrees).
- Owner-checked paths: `AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`,
  `testing/markdown/golden/*.txt`, vendored `far2l/src/markdown/md4c/`,
  `far2l/third_party/quickjs/`, `.github/workflows/`. The spec section is the close-out
  exception this story plans for.

## T1: Source-driven blank rows in the renderer

**Depends on:** none
**Done check:** the AGENTS.md check with `BUILD=../far2l.markdown-blank-lines.build` exits 0.
The current 0007 test may fail only on spacing assertions. It is an intermediate state: the
check must still build far2l and pass golden, driver, 0008 and 0009. Final proof is T2's done check.
**Role:** implementer — review task-reviewer-critical (public contract: the spec's Spacing section)

Delete the gap queue per Global Constraints. Add to `Builder`:

- `int64_t last_text_end = -1`: the end offset of the last source text. `cb_text` sets it
  from `at + size` when `at >= 0`.
- `void blank_rows(int64_t up_to)`: emits one blank row per empty source line between
  `last_text_end` and the start of the source line holding `up_to`. Each row takes the
  `nFilePos` of the row before it, or 0 when `out` is empty. A row inside a list or a quote
  keeps the current `row_lead()`. The helper then sets `last_text_end` to the end of the
  last counted empty line.
- Call site 1: `cb_text`, non-code and non-break path, before the first append. Call it when
  the text's `line_start_of(at)` is greater than the next line start after `last_text_end`:
  count only whole empty lines strictly between the two blocks.
- Call site 2: `cb_enter_block` for `MD_BLOCK_HR`, with the scan of the thematic-break
  unknown. The count stops at the break's own line.
- Leading empty lines: `last_text_end == -1` counts from offset 0. Trailing empty lines:
  after `md_parse`, in `Render()` after `b.flush()`, the same helper counts the rest of
  the input and emits the rows.
- An empty list item fires no text callback, so it calls nothing and shows no row.

Test names (in `test.js`, T2 rewrites the assertions; T1 changes no test): none.

## T2: Rewrite the 0007 spacing assertions and the fixture

**Depends on:** T1
**Done check:** the AGENTS.md check with `BUILD=../far2l.markdown-blank-lines.build` prints
`check ok` and exits 0.
**Role:** implementer — review final

Rework `spacing.md` and the spacing block of `test.js` (lines ~390 to ~658) to the spec table:

- Two source blanks between paragraphs: two blank rows (`p2` at `p1.Y + 3`; the old collapse
  assertion at test.js:410 flips).
- `## Spacing Heading` then paragraph: one blank row between (the old tight assertion at
  test.js:407 flips to a blank row).
- Every forced renderer blank goes. No blank row comes before a heading, a code block,
  an HTML block or a table. No blank row comes after H1, code, HTML, a thematic break,
  a table or a quote, unless a source empty line sits there.
- Rewrite the C1-C7 and R1-R7 named checks to the source-driven rule. Keep their comment
  tags, so the history stays readable.
- New fixture cases in `spacing.md`. The leading empty line goes on line 1, before
  `# Spacing Title`: a blank row shows above the title. The empty line before the `---`
  thematic break goes into the existing break case. A blank row shows above the break
  and proves the HR scan. Add no other rows to the fixture.
- Keep the file's sort position (after `sample.md`). The two added rows may push the
  paged walk. Recompute the `TypePageDown`/`TypeDown` steps of `test.js` from the new
  row map, and keep the walk inside the 7 pages the test uses today.
- Empty list items, the fence-in-item cases, the table and the review blocks keep their
  source empty lines visible row for row. An item's text and the nested list sit tight
  where the source has no empty line between them.
- Do not touch the T4 `--norgb`/palette block of `test.js`, `0008`, `0009`, the golden files
  or the driver test.
