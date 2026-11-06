---
description: Make the F3 Markdown view more compact - fewer blank rows, no heading rule rows and lighter table borders.
status: done
size: S
risk: none
---

# Story: Compact layout in the F3 Markdown view

## Goal

A reader of a Markdown file in F3 sees more text on one screen. Today a heading can take four rows, and a table has borders on all sides. This story removes rows that carry no content.

## Context

- [markdown-viewer.spec.md](../../architecture/features/markdown/markdown-viewer.spec.md), sections on layout and Spacing
- [markdown.invariants.md](../../architecture/features/markdown/markdown.invariants.md)
- Code: `Builder::open_block`, `Builder::close_block`, `cb_enter_block` and `cb_leave_block` in `far2l/src/markdown/MarkdownRender.cpp`

## Scope

**In:** the layout rules below, in `far2l/src/markdown/MarkdownRender.cpp`, the smoke test `0007-view-file-markdown` and its fixtures.

| Block | Rule |
|---|---|
| Any heading | No rule row under it. One blank row before it, except at the top of the document. |
| H1 | One blank row after it. |
| H2 to H6 | No blank row after it. |
| List (outermost) | No blank row before it or after it. A heading or a code block next to a list keeps its own blank row. |
| Code block | One blank row before it and after it. |
| Paragraph, quote | One blank row between blocks, as today. |
| Table | Vertical bars and the one line under the header stay. The top and bottom borders go. |
| Thematic break (`---`, `***`, `___`) | The full-width line stays. This is the only full-width line. |
| Mermaid art | No change. |

**Out:** colors (story markdown-theme-colors), long-line wrap, the browser view.

## Size

S. One source file, one test file, no new contract and no ADR. The spec changes at close-out.

## Unknowns

None.

## Acceptance criteria

- Each row in the Scope table has a check in `0007-view-file-markdown`, with a fixture in `spacing.md` or `sample.md`.
- A setext heading (`Title` over `---`) renders as a heading, with no line under it. Only a thematic break draws a line.
- A heading directly after a list has one blank row before it.
- Row offsets stay non-decreasing. Scroll, Shift-F4 and the saved position work as before.
- The check in `AGENTS.md` prints `check ok`.
- `markdown-viewer.spec.md` shows the new rules after close-out.

## Notes

- Decided by the owner on 2026-11-06, in these words: "tighten lists, keep one line after #, no empty lines after ## and ###". The H4 to H6 rule follows H2 and H3, and the table and code block rules come from the owner's answers.
- Branch `feat/markdown-compact-layout`. Base `next/2.9.1-ovr.1`. Budget and hard stop S $8/$10.
- The check is the command in `AGENTS.md`, with `BUILD` set for the worktree.
- This story removes the rule rows that show heading level, so `markdown-theme-colors` must follow it before the next release.
- started: 2026-11-06T13:15:35Z
- flow tier=lite docs=plan design=no orchestrator=no final=final-reviewer-light gate=read-later scan=no verifier=none
- Gate page: read later in the PR
- Check: `sh docs/stories/markdown-compact-layout/check.sh` from the worktree root. It is the check in `AGENTS.md`, and it also deletes stale sockets before each test (open owner item 11 stops the harness in this long worktree path).
- Owner-checked paths: as in `AGENTS.md`.
- Budget raised by the owner on 2026-11-06 to $12, hard stop $14: "try a local model again with hints channel we discussed, raise budget, build far2l let me verify UI". The final spend passed the hard stop. The figure also counts the planning of later stories in the same session.
- Owner's UI check and close order, 2026-11-06: "I checked compact layout UI -- looks good, close status & commit & merge PR".
