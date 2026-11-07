---
description: Give code blocks in the F3 Markdown view a full-width background tint, so that a block stands apart from the text.
status: done
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
- Ledger, 2026-11-07: docs recorded: `gate.md`, `spec.md`, `plan.md` (4 tasks; design=no).
- Ledger, 2026-11-07: status in-progress. `flow tier=full docs=spec,plan design=no
  orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=none`. Check: "The check" in
  `AGENTS.md` plus a full build (all plugins) and the smoke run of all tests. Budget $8, hard
  stop $12. Base `feat/markdown-colorer-fences` at `782d918ad` (`next/2.9.1-ovr.1` after the
  fences merge), branch `feat/markdown-code-tint`, worktree `.worktrees/markdown-code-tint`.
  started: 2026-11-07T11:05:12Z
- Ledger, 2026-11-07: status done. Merged in PR #15, `2a032e3f8` (2026-11-07). Check
  requests: 4 (docs gate, gate close-out, post-merge suite, final suite). Mid-build `next`
  moved (fences V2 export): printer, palette, driver and 000d ported to V2 ranges, merged
  in `c92a5cbba`, `0adbf77a5`, `0495c85bb`, `eaf27cf12`. Gate check: all PASS (two known
  Mac failures), drivers 0. done: 2026-11-07T21:10:00Z
