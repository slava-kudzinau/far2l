# Spec: Code block tint

Contract for the block background. The gate page holds the deal; the plan holds the tasks.

## Intent

**Problem:** a code block in F3 sits on the plain viewer background; its start and end are hard
to see.

**Approach:** a row-level tint on the block's rows, blended from the viewer background, drawn to
the full row width.

## States

- True color, a passing contrast: the block's rows show the tint to the right edge.
- `--norgb`, or a failing contrast: the plain viewer background, the text readable.
- A theme change: the next redraw uses the new background.

## Parts

### Row tint on `MdLine`

`far2l/src/markdown/MarkdownDoc.hpp`, `MarkdownRender.cpp`.

- `MdLine` gains `int64_t tint = -1`: a packed `0xBBGGRR` background, or -1 for none. The fence
  builder sets it on the fence's rows (the code rows and the hint row); art rows keep their own
  background.
- The builder needs no new parse work; the fence rows are already built as a group.

### The blend and the gate

`far2l/src/markdown/MdPalette.{hpp,cpp}`.

- `MdCodeTint(viewer_bg)`: the blend of the viewer background and the code role's resolved
  foreground, at a fixed ratio (the pattern is `BlendRgb` in `far2l/src/filediff.cpp`).
- The tint is drawn only when `CanUseTrueColorFallback`-style true color is on, and the pair
  (code foreground, tint) reaches 3:1 via `wcagComputeContrast` in
  `utils/include/Colorspace.h`. One verdict, cached on the pair (viewer background, code color).

### The printer

`far2l/src/ViewerPrinter.cpp`.

- `Print` paints the row fill before the runs: the tint replaces the viewer background for the
  full row width. Runs keep their own foregrounds, the Colorer ranges of
  markdown-colorer-fences included.
- The selection overlay rides the same fill, as today.

### Docs

- ADR-004 gains the exception row: the block background is the tint, not the viewer background.
- `markdown-viewer.spec.md`: the fence row names the tint. `markdown.data.md`: the `MdLine`
  field. The invariants: the tint rule and its contrast gate.

## Contracts

`MdLine.tint` is a core-internal field. The smoke reads cell backgrounds through `ReadCellRaw`.

## Done

- Driver case `tint`: a code row's cell background equals the blend; a row outside the block is
  plain; a low-contrast pair draws no tint.
- Smoke `000d`: the javascript fence's code rows read the tinted background. One cell past the
  end of the text reads the tint, so the fill reaches the right edge. The block's last row and
  the row after it differ.
- `--norgb`: the plain codes, the text readable (the 000d run shows the 4-bit codes, background
  plain).
- A theme change alters the tint at the next redraw (the driver changes the background, the next
  paint answers the new blend).
- Check: the gate's Run-it command exits 0, all PASS, the two known Mac failures.
- Docs: ADR-004's exception, the spec, the data view, the invariants, and this story's gate, in
  this branch.

## Non-goals

- A tint on inline code spans; bold/italic/strikeout; new languages.
