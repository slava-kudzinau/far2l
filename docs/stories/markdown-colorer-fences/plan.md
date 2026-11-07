# Plan: markdown-colorer-fences

Tasks for the build. The gate page holds the deal; spec.md holds the contract.

## Global Constraints

- Code style: `CODESTYLE.md`; the Markdown rules in `docs/architecture/features/markdown/markdown.invariants.md`.
- Keep the fork's changes to upstream files small; new code in `far2l/src/markdown/`.
- Treat Markdown and mermaid text as untrusted: a render has a time limit and a stack limit.
- Cell caps stay: 32 lines, 256 cells, the deadline in microseconds (100 ms in the viewer).
- V1 keeps its stride and its codes; V2 is additive: 8 words per cell, codes -1..-3, `abi` 1.
- The export never creates `FarEditorSet`: the same three-call shape as the look export.
- Commit per task: message = what changed, and why.
- Check: the gate's Run-it command, plus the story's Done list.

## T1: The V2 export

**Depends on:** none
**Role:** implementer-risky — review task-reviewer

Files: `plugins/colorer/src/pcolorer2/MdExport.cpp`, `plugins/colorer/src/pcolorer2/MdExport.h`,
`testing/markdown/look_driver.cpp`.

Add `ColorerTokenizeV2` beside V1 in the export table. Take the same input path as V1: catalog,
language, load scheme, tokenize, deadline. For each token, resolve the real region's foreground
and style through `HrcLibrary::getRegionDefine(const Region*)` for the active scheme. An unset
color takes the F4 chain: the region, its parents, then `def:Text`. Write 8-word cells: start,
end, `0xRRGGBB` foreground, the scheme's 4-bit foreground, the Colorer style word, three zeros.
`*generation` carries the scheme generation, 0 on codes -1..-3. `abi == 1`.

Driver: `tokens_v2`: a javascript fence through the real plugin. Assert a keyword, a function, a
string, a number, a constant, an operator, and the tail. The 24-bit values come from
`far2l.hrd`, the 4-bit values from console.hrd. An unset color falls to `def:Text`.
`tokens_v2_limits`: 32 lines, 256 cells, the deadline. `tokens_v2_v1_callers`: V1 answers as
before, with its own stride.

**Done check:** `sh testing/markdown/look-driver.sh --case tokens_v2` exits 0; the same for
`tokens_v2_limits` and `tokens_v2_v1_callers`; `cmake --build ../far2l.markdown-colorer-fences.build -j8` exits 0.

## T2: The bridge, the cache key, the token group

**Depends on:** T1
**Role:** implementer-risky — review task-reviewer

Files: `far2l/src/markdown/MdColorer.{hpp,cpp}`, `far2l/src/markdown/MarkdownDoc.{hpp,cpp}`,
`far2l/src/markdown/MarkdownRender.cpp`, `testing/markdown/look_driver.cpp`.

Bind `ColorerTokenizeV2` once, beside the V1 binding. A missing symbol falls back to V1's
classes through the alias table. A cached range carries `{start, end, fg, fg4, underline}`, not
a class. The cache key gains the scheme generation, the generation the look export answers:
(language, hash, generation). The render stores the ranges on the fence's lines, as today.

Driver: `look_fence_generation`: a fake source answers two generations. The second render
refetches. The report prints the fetch count.

**Done check:** `sh testing/markdown/look-driver.sh --case look_fence_generation` exits 0; the
whole look driver is green; the full build exits 0.

## T3: PrintCodeRuns and the view contrast verdict

**Depends on:** T2
**Role:** implementer-risky — review task-reviewer

Files: `far2l/src/ViewerPrinter.cpp`, `far2l/src/markdown/MdPalette.{hpp,cpp}`,
`testing/markdown/look_driver.cpp`.

`PrintCodeRuns` paints each range. `RgbDefaultsAllowed` takes `fg`, else `fg4`. The underline
bit sets `COMMON_LVB_UNDERSCORE`, and the look story's span re-apply carries it. A cell with no
range keeps the plain code role. The background is the viewer text background.

The look's batch verdict is keyed (viewer background, generation). It decides for the whole
view: on a failure, fences take the plain code role and prose the fork table.

Driver: `look_fence_contrast`: a fake scheme that fails 3:1 sends fences to the plain code role
and prose to the fork table, one verdict, printed. A passing scheme sends the scheme colors to
the fences.

**Done check:** `sh testing/markdown/look-driver.sh --case look_fence_contrast` exits 0. The
whole look driver is green. The full build exits 0. Smokes `0007`, `000c`, `000d` PASS: the
fence rows stay unchanged until T4.

## T4: Smoke, hostile, budget

**Depends on:** T3
**Role:** implementer-fix-late — review task-reviewer

Files: `testing/tests/000d-view-md-look/test.js`,
`testing/tests/000d-view-md-look/workdir/left/fixture.md` (if the fence needs a function word),
`testing/markdown/look_driver.cpp`.

`000d` asserts the javascript fence: a keyword cell, a function cell, a string cell, and the
tail. The colors are the console.hrd rows, and the underline rides where the scheme underlines.
Run the sweep first; read the cells from the run, not by guess.

`tokens_v2_cost`: a 20k-line fence renders inside the budget. Print the mean and the worst. The
hostile fixture (`fixture.py`, 1 MB) stays inside the budget.

Finish: the gate's full Run-it command; `0004-vt-shell` and `000b-find-file` are the known Mac
failures.

**Done check:** the gate's Run-it command prints `check ok` (all PASS, the two known Mac
failures allowed), and the cost line is printed.

## T5: Durable docs

**Depends on:** T4
**Role:** implementer-fix-late — review task-reviewer
Files: `docs/architecture/decisions/adr-010-colorer-tokenize-v2.md`,
`docs/architecture/features/markdown/markdown-viewer.spec.md`,
`docs/architecture/features/markdown/markdown.data.md`,
`docs/architecture/features/markdown/markdown.invariants.md`.

ADR-010: V2 returns colors, the 8-word stride, the generation key, V1 stays. The spec's fence
row: tokens carry the scheme's foreground and underline. The data page: the export row gains V2,
the token cache key gains the generation. The invariants: the cache key rule, the 8-word stride,
the one view-wide contrast verdict.

**Done check:** the four files show the new rows in `git diff`, and `check_ste.py` on the four
files prints nothing.

## T6: Close-out

**Depends on:** T5
**Role:** implementer — review final

The orchestrator writes the close-out draft (the records commit): the story ledger rows, the T1
measurements, and the gate's At-merge rows. The main session fills the gate close-out rows and
commits the durable docs. It opens the pull request into `next/2.9.1-ovr.1`, merges, and pulls
into the main checkout. The check runs once more on the merge.

**Done check:** the records commit exists, the final review's findings have fixes, and the merge
into `next/2.9.1-ovr.1` shows the story's tip.
