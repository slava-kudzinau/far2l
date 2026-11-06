---
description: Give each heading level its own color and give code blocks a background tint, with 24-bit colors derived from the active theme.
status: superseded
superseded-by: markdown-palette
size: M
risk: contract
depends-on: markdown-compact-layout
---

> Do not build this story. On 2026-11-06 the owner replaced it with story markdown-palette, and the tint moved to story markdown-code-tint. This file stays as a record.

# Story: Theme-derived heading and code block colors

## Goal

A reader of a Markdown file in F3 sees the document structure from color. Each heading level has its own color, and a code block stands apart from the text. The colors follow the theme of the user.

## Context

- [ADR-004](../../architecture/decisions/adr-004-palette-roles.md): this story changes it
- [markdown.invariants.md](../../architecture/features/markdown/markdown.invariants.md): the `[colors]` rule changes
- Code: `MdRole` and `MdLine` in `far2l/src/markdown/MarkdownRender.hpp`, `MarkdownRunColor` and `MarkdownPrinter::Print` in `far2l/src/ViewerPrinter.cpp`
- Pattern to copy: the true-color tint and its fallback in `far2l/src/filediff.cpp` (`CanUseTrueColorFallback`, `BlendRgb`)

## Scope

**In:**
- Six heading roles, one for each level, in place of the one heading role for heading text. Links, strong text and table header text keep their role.
- Colors derived at draw time from the `COL_HELPTOPIC`, `COL_HELPHIGHLIGHTTEXT` and `COL_VIEWERTEXT` entries of the active palette, with RGB blends. A new theme shows on the next redraw.
- A code block fills the full row width with a tint of the viewer background, in a row-level fill that `MdLine` carries. A code span keeps its foreground and gets no tint.
- RGB only when `GetConsoleColorPalette` reports 24 bits or more. Each RGB color sits on a 16-color index, so a terminal that ignores RGB shows the index.
- A contrast check with the functions in `utils/include/Colorspace.h`. A tint with poor contrast is not drawn.
- New color code goes in `far2l/src/markdown/`. The change in `ViewerPrinter.cpp` stays small.

**Out:**
- New `COL_*` palette entries and Colors menu items.
- Syntax colors (story markdown-code-highlight).
- The fix for equal code and body colors in the black-and-white palette at 16 colors.

## Size

M. One contract change (ADR-004 and the `[colors]` invariant), about four source files, and test changes.

## Unknowns

- The smoke tests run with `--tty --nodetect`. If the palette there reports under 24 bits, the RGB path does not run. Task 1 of the plan must find this out, and give the tests a way to reach the RGB path if needed.
- How many heading levels stay distinct at 16 colors. The gate must propose an index for each level. Levels can share an index, and H1, H2 and H3 must differ from each other.
- What macOS Terminal does with `38;2` sequences. It must be checked by hand.
- The blend ratios. They need a look on at least one dark theme and one light theme.

## Acceptance criteria

- On a 24-bit terminal, the cells of H1 to H6 have six different foreground colors in each bundled theme under test (one dark, one light).
- All rows of a code block have the tint to the right edge, also past the end of the text.
- With `--norgb`, every run shows a readable 16-color index, and no RGB bits remain in effect.
- A theme change alters the colors at the next redraw, with no new render.
- Shift-F4, scroll, selection in plain mode and the Markdown search work as before.
- The smoke test reads the attributes of heading and code cells and checks them.
- ADR-004 and `markdown.invariants.md` state the new rule after close-out: colors derive from the palette, never from fixed values.

## Notes

- Decided by the owner on 2026-11-06: colors derive from the theme, plus a tint on code blocks.
- Bold does not show on a terminal, and no italic attribute exists, so structure comes from color only.
- Branch `feat/markdown-theme-colors`. Base `next/2.9.1-ovr.1`, after `markdown-compact-layout` merges. Budget and hard stop M $15/$20.
- The owner reads the gate before the build, because the story changes a rule.
