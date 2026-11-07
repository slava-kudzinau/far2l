# Spec: Exact Colorer colors in fenced code

Contract for the fence colors. The gate page holds the deal; the plan holds the tasks.

## Intent

**Problem:** a fence in F3 shows four colors (keyword, string, comment, number). F4 with the same
scheme colors functions, types, constants, operators and tags.

**Approach:** a second export, `ColorerTokenizeV2`, returns the real region's foreground and
style per token. The core's token group carries a color and an underline bit, not a role.
`PrintCodeRuns` paints it.

## States

- Colorer present, a known language, a passing contrast verdict: each token shows the scheme's
  foreground, and the scheme's underline when the style carries bit 4.
- A token with no range (the tail, the padding): the plain code role of the palette.
- Colorer missing, an unknown language, a fatal parse, a contrast failure: the plain code role,
  as today.
- A scheme change: the next render refetches; a resize does not refetch.

## Parts

### Plugin export `ColorerTokenizeV2`

`plugins/colorer/src/pcolorer2/ColorerExport.cpp`, beside `ColorerTokenizeV1`, as a
`SHAREDSYMBOL` export. The core binds it with `dlsym`, as V1.

```c++
int ColorerTokenizeV2(int abi, const wchar_t* language, const wchar_t* const* lines,
                      int nLines, int budget_ms,
                      uint64_t* cells, int capacity, uint64_t* generation);
```

- Same input and limits as V1: a line cap of 32 lines, a cell cap of 256 cells. The deadline is
  in milliseconds, 100 ms in the viewer.
- Cell stride: 8 words. `[0]` start column, `[1]` end column (1-based, inclusive). `[2]` the
  region's foreground as `0xRRGGBB`, `[3]` the scheme's 4-bit foreground (0..15). `[4]` the
  Colorer style word (bit 4 = underline). `[5]..[7]` zero.
- The colors resolve through `HrcLibrary::getRegionDefine` for the active scheme, with the F4
  fallback chain: the region, then its parents, then `def:Text`.
- Return codes as V1: a count of zero or more in the return (zero for a fence with no tokens),
  -1 (no catalog), -2 (unknown language),
  -3 (fatal parse). `*generation` carries the scheme generation of the answer, and 0 on a code
  other than a count. A deadline stop returns the cells written: a partial group, painted and
  cached as-is, like V1's code 1 path.
- `abi == 1` for this shape; a larger abi answers -2.
- The export never creates `FarEditorSet`: the same three-call shape as the look export (count
  the catalog, count the lines, load the scheme, tokenize).

### The core bridge

`far2l/src/markdown/MdColorer.{hpp,cpp}`.

- `TokenizeV2(language, lines, cells, &generation)`: dlsym `ColorerTokenizeV2` once, beside the
  V1 binding; the same deadline and caps. A missing symbol falls back to V1's classes: the cell
  keeps the four alias rows as the 24-bit and 4-bit pair from the class table.
- The binding loads lazily, on the first fence.

### The token group and the cache

`far2l/src/markdown/MarkdownRender.{hpp,cpp}` (the class `MarkdownDoc`).

- A cached range: `{start, end, fg (0xRRGGBB), fg4 (0..15), underline (bool)}`, not a class. The
  class survives only as the fallback path's alias color.
- The cache key gains the scheme generation: (normalized language, fence source hash,
  generation). A scheme change refetches each visible fence; a resize does not.
- The render stores the ranges on the fence's lines, as today.

### `PrintCodeRuns`

`far2l/src/ViewerPrinter.cpp`.

- A range paints its foreground: the 24-bit value when `RgbDefaultsAllowed`, else `fg4`.
- An underline bit sets `COMMON_LVB_UNDERSCORE` on the run, and the underline re-apply of the
  look story carries it (the span pass in `MarkdownPrinter::Print`).
- The background is the viewer text background, as today.
- A cell with no range, and the padding after the line's last token: the plain code role.

### Contrast

`far2l/src/markdown/MdPalette.cpp`.

- The look's batch verdict (keyed (viewer background, scheme generation)) decides for the whole
  view. On a failure: fences take the plain code role, and prose the fork table. One verdict,
  one recompute on a theme change.
- A 4-bit scheme (a console `far2l.hrd` style scheme) answers the 4-bit path. Fences take the
  scheme's `fg4`, prose the scheme's foreground. No 3:1 check.

### Docs

- `docs/architecture/decisions/adr-010-colorer-tokenize-v2.md`: V2 returns colors, the 8-word
  cell, the generation key, V1 stays.
- `markdown-viewer.spec.md`: the fence row: tokens carry the scheme's foreground and underline.
- `markdown.data.md`: the export row gains V2; the token cache key gains the generation.
- `markdown.invariants.md`: the cache key rule, and the 8-word stride.

Out: the block background tint (story markdown-code-tint); new language aliases; bold, italic and
strikeout bits.

## Contracts

The V2 export is a data contract: the signature and the 8-word stride are fixed. A change needs a
new version name (ADR-008 precedent). The cache key (language, hash, generation) is a data
contract for the render path.

## Done

- `sh testing/markdown/look-driver.sh --case tokens_v2`: a `javascript` fence through the real
  plugin: keyword, function, string, number, constant, operator, tail. The per-word values equal
  F4. On `far2l.hrd`: keyword `0xffffff`, function `0x6060f0`, string `0x88ee88`, number
  (`def:NumberDec`) `0x00e020`, constant (`def:BooleanConstant`) `0xa030ff`, operator
  (`def:Symbol`) `0xe0e0c0`.
  On `console.hrd`: the scheme's 4-bit rows. An unset color takes the F4 chain to `def:Text`.
- `tokens_v2_limits`: 32 lines, 256 cells, the deadline, as V1's `look_codes`.
- `tokens_v2_cost`: a 20k-line fence renders inside the budget; the report prints the mean and
  the worst.
- `tokens_v2_v1_callers`: V1 answers the javascript fence as before (the four classes, the
  4-word emit callback), so the old callers keep working.
- `look_fence_contrast`: a fake scheme whose foregrounds fail 3:1: fences take the plain code
  role, prose the fork table, one verdict.
- `look_fence_generation`: two generations: the second render refetches the ranges.
- Smoke `000d`: the javascript fence asserts a keyword cell, a function cell, a string cell, and
  the tail. The colors are the console.hrd rows, as the harness runs 16 colors, and the underline
  rides where the scheme underlines. The truecolor rows are asserted by `tokens_v2`.
- Colorer missing: every fence is the plain code role (the `look_bridge` case stays).
- The hostile fixture (a 1 MB python fence, many fences, a long line) renders inside the budget.
- Check: the gate's Run-it command exits 0, all PASS, and the two known Mac failures.
- Docs: ADR-010, the spec, `markdown.data.md`, the invariants, and this story's gate, updated in
  this branch.

## Non-goals

- The block background tint, new language aliases, bold/italic/strikeout.
