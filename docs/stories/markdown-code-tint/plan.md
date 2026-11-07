# Plan: markdown-code-tint

Tasks for the build. The gate page holds the deal; spec.md holds the contract.

## Global Constraints

- Code style: `CODESTYLE.md`; the Markdown rules in `docs/architecture/features/markdown/markdown.invariants.md`.
- The tint is one row-level field; no new parse work.
- True color gates the tint; `--norgb` keeps the plain background.
- Contrast: one verdict on (code foreground, tint), cached on (viewer background, code color).
- Commit per task: message = what changed, and why.
- Check: the gate's Run-it command, plus the story's Done list.

## T1: The field, the blend, the printer

**Depends on:** none
**Role:** implementer — review task-reviewer

Files: `far2l/src/markdown/MarkdownDoc.hpp`, `far2l/src/markdown/MarkdownRender.cpp`,
`far2l/src/markdown/MdPalette.{hpp,cpp}`, `far2l/src/ViewerPrinter.cpp`,
`testing/markdown/look_driver.cpp`.

`MdLine` gains `int64_t tint = -1`. The fence builder sets it on the fence's code rows and the
hint row. `MdCodeTint(viewer_bg)` blends the viewer background with the code role's resolved
foreground, the pattern of `BlendRgb` in `far2l/src/filediff.cpp`. True color gates the tint, and
3:1 on (code foreground, tint) gates it, with `wcagComputeContrast`. One verdict, cached on
(viewer background, code color). `Print` paints the fill for the full row width, before the runs.

Driver: `tint`: a code row's background equals the blend. A row outside the block is plain. A
low-contrast pair draws no tint. A background change redraws with the new blend.

**Done check:** `sh testing/markdown/look-driver.sh --case tint` exits 0, the whole look driver
is green, and the full build exits 0.

## T2: Smoke, `--norgb`

**Depends on:** T1
**Role:** implementer-fix-late — review task-reviewer

Files: `testing/tests/000d-view-md-look/test.js`.

`000d` reads the javascript fence. A code row's background is the tint. One cell past the end of
the text is the tint, so the fill reaches the right edge. The row after the block is plain. Read
the values from a sweep run, not by guess. The harness runs 16 colors, so the tint shows on the
4-bit path; assert the code cell's background as the run prints it.

**Done check:** smoke `000d` PASS with the new rows, and the gate's Run-it command prints
`PASS` for every test (the two known Mac failures allowed).

## T3: Durable docs

**Depends on:** T2
**Role:** implementer-fix-late — review task-reviewer

Files: `docs/architecture/decisions/adr-004-palette-roles.md`,
`docs/architecture/features/markdown/markdown-viewer.spec.md`,
`docs/architecture/features/markdown/markdown.data.md`,
`docs/architecture/features/markdown/markdown.invariants.md`.

ADR-004 gains the exception row: the block background is the tint, not the viewer background.
The spec's fence row names the tint. The data view names the `MdLine` field. The invariants gain
the tint rule with its contrast gate.

**Done check:** the four files show the new rows in `git diff`, and `check_ste.py` prints nothing.

## T4: Close-out

**Depends on:** T3
**Role:** implementer — review final

The orchestrator writes the close-out draft (the records commit): the story ledger rows and the
gate's At-merge rows. The main session fills the gate close-out rows and
commits the durable docs. It opens the pull request into `next/2.9.1-ovr.1`, merges, and pulls
into the main checkout. The check runs once more on the merge.

**Done check:** the records commit exists, the final review's findings have fixes, and the merge
into `next/2.9.1-ovr.1` shows the story's tip.
