# Plan: Markdown preview — reading experience

Spec: `spec.md` (governs exact values).

## Global Constraints

- C++ in `far2l/src/markdown/*`, `far2l/src/ViewerPrinter.*`, `far2l/src/viewer.cpp`, `far2l/src/viewer.hpp`; the test in `testing/tests/0007-view-file-markdown/`. No new dependency. Do not touch hex or text mode behavior.
- Scrolling design (settled): in Markdown mode the document is already one vector of rendered lines (`MarkdownDoc`, `m_md_screen`). Scroll over that vector by line index. Keep the top line index in the viewer; get the file position for the status line, percent and position cache from the line's source offset. Do not rely on `Builder::pos` as a file offset unless it is made one.
- Existing tests must keep passing. Existing 0007 asserts (`Start`, `Finish`, box glyph, backgrounds) stay.
- Mermaid layout and speed belong to story B: do not change `MermaidEngine.cpp` or `SvgCanvas.cpp` here, except to read the lines they return.
- Colors: no `F_*` or `B_*` constants as final colors. Read palette entries through the existing color functions (`FarColorToReal(COL_...)`) at render time so theme changes apply; the background always comes from `COL_VIEWERTEXT`.
- Settled choices: scroll step, page step, stop rules, percent and resize follow the Scrolling table in `spec.md` exactly. Rendering and caching stay as they are: the whole file is rendered once per width into one vector of screen lines (re-rendered on width change or mode toggle). The top line index is kept across a re-render by finding the line with the same source offset. Blank-line rules are the Spacing table; an empty source line inside a code fence is kept as is. The three palette roles are heading, code, art; pick entries so that all three differ from `COL_VIEWERTEXT` and from each other. Wheel scrolling uses the viewer's existing wheel step if it has one, else 1 line.
- Commit message: a short imperative summary line, a blank line, then the exact trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. One commit per task. Put the trailer in every dispatch.
- Check: see `story.md` Notes. Owner-checked paths: `stories/*/story.md`.
- Test input: add a second markdown fixture `long.md` (at least 3 screens: headings, paragraphs, a list, code, table) to `testing/tests/0007-view-file-markdown/initdir/left/` for scroll tests. `git add -f` files under `testing/` (root `.gitignore` matches `Testing`).

## T1: Scrolling by line index

**Depends on:** none
**Done check:** the Check command exits 0 with 0007 asserting the Scrolling rows of the spec on `long.md` (Down, PgDn not blank, PgUp, End, Home)
**Role:** implementer — review task-reviewer-critical
Replace the SHOW_UP/SHOW_DOWN/PgUp/PgDn/mouse path for Markdown mode with line-index scrolling as settled above. Keep status percent and resize behavior.

## T2: Spacing

**Depends on:** none
**Done check:** the Check command exits 0 with 0007 asserting a blank row after a heading and between two paragraphs in `long.md`
**Role:** implementer — review task-reviewer
Add the blank lines of the Spacing table in `MarkdownDoc::Render` callbacks. Collapse repeated blanks.

## T3: Theme palette colors

**Depends on:** T2
**Done check:** the Check command exits 0 with 0007 asserting three different foreground colors (heading, code, body) and one background
**Role:** implementer — review task-reviewer
Apply the Colors table. Write the chosen palette entries and where they come from into `closeout.md`.
