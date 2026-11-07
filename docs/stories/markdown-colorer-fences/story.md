---
description: Paint fenced code in the F3 Markdown view with the exact F4 colors, one color and underline bit for each token from the active Colorer scheme.
status: done
size: M
risk: contract,security
depends-on: markdown-colorer-look
---

# Story: Exact Colorer colors in fenced code

## Goal

A fenced block with a known language shows the same colors as the same code in the F4 editor. Functions, types, constants, operators and tags have their own colors, not only keyword, string, comment and number.

## Context

- Story markdown-code-highlight: `ColorerTokenizeV1` returns four token classes. Fence cells get a role of the fork table. ADR-008 records the export and the input limits.
- Story markdown-colorer-look: the Colorer look is a resolver step, and `MdColorer` has a generation counter.
- [markdown-viewer.spec.md](../../architecture/features/markdown/markdown-viewer.spec.md) and [markdown.data.md](../../architecture/features/markdown/markdown.data.md): the token group and `PrintCodeRuns` in `far2l/src/ViewerPrinter.cpp`.
- `plugins/colorer/src/pcolorer2/FarViewer.cpp` and `FarEditor.cpp`: how F4 turns a region into a color and an attribute, and the fallback to `def:Text`.

## Scope

**In:**

| Part | Rule |
|---|---|
| Plugin export `ColorerTokenizeV2`, beside V1 | Same input and the same limits as V1 (the deadline for each render, a line cap and a size cap). Each cell range returns foreground RGB and style bits of the real region, with the F4 fallback for an unset color. V1 stays for the old callers. |
| Token group in the core | A range holds a color and an underline bit, taken from the export, not a role. The 16-color index and the 4-bit fallback keep the same rule as the look from markdown-colorer-look. |
| `PrintCodeRuns` | Paints each range with the foreground and the underline bit. A cell with no range keeps the plain code role. The background is the viewer background. |
| Contrast | The 3:1 rule of markdown-colorer-look decides for the whole Markdown view. On a failure, fences use the plain code role and prose uses the fork table. |
| Docs | Update ADR-008 or add ADR-010 for V2. Update the spec, `markdown.data.md` and the invariants file. |

**Out:**

- The tint of the block background: story markdown-code-tint.
- New language aliases. The map of language tags to Colorer types stays as in markdown-code-highlight.
- Bold, italic and strikeout bits.

## Size

M. One export, one change of the token group, one print path, three docs. One input limit that stays, with a hostile fixture.

## Unknowns

- Whether ranges for many token classes make a render slower than the 40 ms budget on a large fence. The first task measures it with a 20k-line fence.
- Whether V1 can go. The first task counts its callers after markdown-code-highlight merges.

## Acceptance criteria

- With a fake token source, each range paints its own foreground and underline bit.
- With the real plugin, a `javascript` fence has the same foreground per word as the same text in F4 with `far2l.hrd`.
- A hostile fixture (a Python fence of 1 MB, many fences, a very long line) ends inside the render budget, as in markdown-code-highlight.
- Colorer missing or disabled: the plain code role, as before.
- A failing contrast scheme gives the plain code role for fences.
- The spec, `markdown.data.md`, the invariants file and the ADR are updated in the same branch.
- Check: "The check" in `AGENTS.md`, plus a plugin build and the smoke run of all tests.

## Notes

- Owner's words, 2026-11-07: "code should be colored as in colorer too".
- Owner's choice, 2026-11-07: "Exact F4 colors". The four classes of V1 are not enough.
- Branch `feat/markdown-colorer-fences`. Base `next/2.9.1-ovr.1`, after markdown-colorer-look merges. Budget and hard stop M $15/$20.
- Build order: after markdown-colorer-look, before markdown-code-tint. They share `ViewerPrinter.cpp`.
- Ledger, 2026-11-07: status in-progress. `flow tier=full docs=spec,plan design=no
  orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=none`. Check: "The check"
  in `AGENTS.md` plus a full build (all plugins) and the smoke run of all tests. Budget $15,
  hard stop $20. Base `next/2.9.1-ovr.1` at `5049c6aa8`, branch `feat/markdown-colorer-fences`,
  worktree `.worktrees/markdown-colorer-fences`. started: 2026-11-07T07:03:54Z
- Ledger, 2026-11-07: gate, spec, plan written (`782d918ad`, `215367a07`), owner approved
  G2-design with the four answers recorded as rulings at the gate foot.
- Ledger, 2026-11-07: T1 done: `0c2dc3174`, review 1 Important fixed (`0691ddf33`, the export
  never creates `FarEditorSet`).
- Ledger, 2026-11-07: T2 done: `fa75d065b`, review clean (generation rides every V2 answer).
- Ledger, 2026-11-07: T3 done: `035952c1c`, review passes (5 P3 nits, folded in T4).
- Ledger, 2026-11-07: T4 done: `ed99f16e1`, review passes (comments folded in T5); cost mean
  9.5 ms worst 10.9 ms, hostile 12.7 ms, fetches 8 for 7 fences.
- Ledger, 2026-11-07: T5 done: `5c68cc8ad` (main session: ADR-010, viewer spec, data view,
  invariants).
- Ledger, 2026-11-07: final review: ship; five doc-precision nits folded (`5501844ee`).
- Ledger, 2026-11-07: close-out: gate At-merge and Spend filled, index row done, merged in
  PR #14 `52ad4d440`, merge check green (drivers + 0007/0008/0009/000c/000d, spend $2.99).
