---
description: Give code blocks in the F3 Markdown view a full-width background tint, so that a block stands apart from the text.
status: planned
size: S
risk: contract
depends-on: markdown-palette
---

# Story: Code block tint

## Goal

A reader of a Markdown file in F3 sees where a code block starts and ends. The block has a background tint to the right edge of the screen.

## Scope

**In:**
- A row-level fill that `MdLine` carries, drawn to the full row width.
- A tint derived from the viewer background, with `BlendRgb` in `far2l/src/filediff.cpp` as the pattern, and a true color check as in `CanUseTrueColorFallback`.
- A contrast check with `utils/include/Colorspace.h`. A tint with poor contrast is not drawn.

**Out:**
- A tint on code spans.
- Syntax colors (story markdown-code-highlight).

## Size

S. One row-level field, one draw change and one test. The story changes one point of the rule that the background is always the viewer background, so the gate must name it.

## Unknowns

- The blend ratio. It needs a look on one dark theme and one light theme.
- Is the fill compatible with Colorer token colors in fences? Answer this at the gate, after story markdown-code-highlight has its new route.

## Acceptance criteria

- All rows of a code block have the tint to the right edge, also past the end of the text.
- With `--norgb`, no tint shows, and the text stays readable.
- A theme change alters the tint at the next redraw.
- The smoke test reads the background of code cells and checks it.
- ADR-004 and `markdown.invariants.md` state the exception at close-out.

## Notes

- Decided by the owner on 2026-11-06: a tint on code blocks, as part of markdown-theme-colors. On the same day the owner moved it to this later story when markdown-palette replaced that story.
- Branch `feat/markdown-code-tint`. Base `next/2.9.1-ovr.1`, after `markdown-palette` merges.
