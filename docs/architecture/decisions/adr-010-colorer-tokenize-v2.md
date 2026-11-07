---
description: Fence tokens come from the Colorer plugin as colored ranges, a V2 export with an 8-word stride, cached under the scheme generation.
status: accepted
date: 2026-11-07
---

# ADR-010: The V2 tokenize export with real scheme colors

## Context

- ADR-008 gave fenced code four token classes (keyword, string, comment, number) and one flat
  code color. F4 with Colorer paints a javascript fence with the scheme's regions: a function
  name, a boolean constant, an operator, a decimal number. The owner asked for the exact F4
  colors in F3 (story markdown-colorer-fences).
- ADR-009 added the scheme look for prose, keyed on a generation counter. ADR-004 keeps the run
  a role, never a color.
- The token cache from ADR-008 is keyed on (language, fence source hash). A scheme change left a
  cached fence with the old colors.

## Decision

The plugin exports `ColorerTokenizeV2(int abi, const wchar_t* language, const wchar_t* const*
lines, int nLines, int budget_ms, uint64_t* cells, int capacity, uint64_t* generation)`
(`plugins/colorer/src/pcolorer2/ColorerExport.cpp`). It keeps V1 unchanged for the plain-v1
fallback.

- A cell is 8 words, in source order. An enclosing scheme cell comes before its leaves.
  Words: start column, end column (1-based, inclusive), the region's 24-bit foreground
  `0xRRGGBB`, the scheme's 4-bit foreground, the Colorer style word, then three zero words.
  Bit 4 of the style word sets the underline.
  Colors resolve through `RegionMapper::getRegionDefine(const Region*)`, the F4 chain:
  region, parents, `def:Text`.
- Return is the count of cells (zero for a fence with no tokens), -1 no catalog, -2 unknown
  language, -3 fatal parse. `*generation` is the scheme generation, 0 on a code. The export never
  creates `FarEditorSet`.
- The export parses 32 lines and 256 cells per call. One deadline is shared across a fence.
  A deadline stop returns the cells written: a partial range set, painted and cached as-is.
  That matches V1's code 1 path.
- The bridge calls the export once per line. The 8-word stride carries no line word, so per-line
  calls keep line attribution exact. The loss: a cross-line region (an open block comment) colors
  its opening line only.
- The token cache key gains the generation: (normalized language, fence source hash, scheme
  generation), fetched once per render and updated from each answer. A scheme change refetches
  every fence; a resize does not re-tokenize.
- Fences ride the one view-wide Colorer contrast verdict of ADR-009, keyed (viewer background,
  generation). A failing scheme sends fences to the plain code role, and prose to the fork table.

## Consequences

- F3 fences show the scheme's colors and underline, matching F4 on the same scheme; a scheme
  change repaints fences and prose together.
- The stride is a data contract: a change needs a new version name (ADR-008 precedent).
- One fetch serves each (language, hash, generation) pair. A 20k-line fence renders in 9.5 ms
  mean and 10.9 ms worst. A 1 MB hostile python file renders in 12.7 ms. The deadline is
  100 ms. Story markdown-colorer-fences measured these.
- On macOS the executable needs `-Wl,-force_load far2l_md`: the markdown library is the sole
  compile of the Markdown sources, and `dlsym` lookups do not pin static members.
- String edges (quotes) take the block region's color, not `def:StringEdge`. The handler writes
  leaves over the block span, and F4 paints the body as the contract.
