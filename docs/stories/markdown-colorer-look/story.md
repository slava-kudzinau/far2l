---
description: Give the prose of the F3 Markdown view the same colors and underline as F4 with Colorer on a Markdown file, from the active HRD scheme, through one small plugin export.
status: in-progress
size: M
risk: contract
depends-on: markdown-code-highlight
---

# Story: Colorer look for Markdown prose

## Goal

A reader who opens a `.md` file in F3 sees the colors of F4 with Colorer, without the Markdown marker characters. Headings and links are underlined when the scheme says so. The reader can press F4 and F6 on the same file and see matching colors. A change of the Colorer color scheme changes F3 too.

## Context

- [markdown.overview.md](../../architecture/features/markdown/markdown.overview.md), [markdown.invariants.md](../../architecture/features/markdown/markdown.invariants.md), [markdown-viewer.spec.md](../../architecture/features/markdown/markdown-viewer.spec.md) (palette roles).
- [ADR-004](../../architecture/decisions/adr-004-palette-roles.md): a run holds a role, not a color.
- ADR-008 (Colorer plugin export), from story markdown-code-highlight. This story builds on its loader `MdColorer` and its rules for the plugin lookup.
- Colorer files: `plugins/colorer/configs/base/hrc/misc/markdown.hrc` (regions), `plugins/colorer/configs/base/hrd/rgb/far2l.hrd` (the default scheme), `plugins/colorer/src/pcolorer2/FarEditorSet.cpp` (`ReloadBase`, scheme choice), `FarViewer.cpp` and `FarEditor.cpp` (style bits to far attributes).

## Scope

**In:**

| Part | Rule |
|---|---|
| New plugin export, new file `plugins/colorer/src/pcolorer2/MdExport.cpp` | `ColorerRegionLookV1` takes a list of region names. It returns foreground RGB and style bits under the active scheme. It also returns a generation counter that changes when the scheme reloads. It does not create `FarEditorSet`. It returns a code for "not loaded", "disabled" and "error". The first argument is the ABI number. |
| Core bridge in `MdColorer.cpp` | One lookup call that reuses the plugin lookup of markdown-code-highlight. All state is copied into plain structs. |
| `MdPalette.cpp`, new resolver step | After the user file `markdown-colors.ini` and before the fork RGB default: the look from Colorer. The result holds the foreground and the underline bit only. The background is always the viewer text background. |
| Region map | Heading levels 1 to 6: `markdown:HeadText`. Link: `markdown:LinkText`. Strong: `markdown:StrongText`. Emphasis: `markdown:EmText` (a new role, today emphasis uses the body role). Inline code: `markdown:Code`. Quote: `markdown:QuoteText`. List mark and rule: `markdown:ListMark` and `markdown:Rule`. Body: `def:Text`. |
| Underline | A style bit 4 in the scheme sets `COMMON_LVB_UNDERSCORE` on the run. Bold, italic and strikeout bits are ignored, as in F4 for bold and italic. The code does not hard-code which role is underlined. |
| Contrast | The 3:1 rule covers the whole Colorer look. If one prose color fails, the resolver uses the fork table for all roles, as today. A user override has no check. |
| Fallback | Colorer absent, disabled (`Enabled=0` or `SyntaxDraw=0`) or in error: the fork table, as today. Below 8 bits of color: the 4-bit fallback of the fork table. |
| Docs | Rewrite the spec row "Emphasis, strikethrough, underline" and the palette-role section. Add ADR-009 (the Colorer look is a resolver step, and the role stays in the run). Update the [colors] rows of `markdown.invariants.md` and `markdown.data.md`. Fix the module name in the docs: `colorer.far-plug-wide`. |

**Out:**

- Token colors in fenced code: story markdown-colorer-fences.
- The full-width tint of code blocks: story markdown-code-tint.
- Heading levels get one look, as in F4. The fork keeps no per-level color on the Colorer path.
- Table borders and the diagram art role keep the fork look. `markdown.hrc` has no region for them.
- Bold and italic attributes.
- The background color of the scheme. F4 paints the far background too, unless `ChangeBgEditor` is set.

## Size

M. One new plugin file, one bridge function, one resolver step, one new role, three docs and the tests. One contract change (the resolver order). One ADR. Two unknowns that measure.

## Unknowns

- First-call cost. `FarEditorSet` creation shows a "Reloading" message. The export must not create it. The first task checks that F3 on a `.md` file already created it (`ProcessViewerEventW`), and what the export does when it has not.
- Underline in macOS Terminal and Ghostty. The first task makes the manual check list. The owner runs it.
- The cost of one lookup of about 10 names per redraw. The first task measures it. If it is above 1 ms, the bridge caches the look by generation.

## Acceptance criteria

- With a fake look source, each prose role takes the foreground from the look. A role with style bit 4 shows `COMMON_LVB_UNDERSCORE` in the cell.
- With the real plugin and `far2l.hrd`, heading and link text in F3 have the foreground `#8888ff` and an underline. Strong text and inline code match the colors of F4 on the same file.
- Press F4, then F6 on a Markdown file: the viewer and the editor show the same foreground for the same word.
- A scheme change in the Colorer settings changes the F3 colors at the next open.
- Colorer missing, disabled or in error: F3 looks as before this story. The viewer does not fail and does not wait.
- A scheme with a failing 3:1 contrast gives the fork table for all roles.
- `markdown-colors.ini` still overrides each role.
- `COMMON_LVB_UNDERSCORE` is tested through `ReadCellRaw().Attributes`, not `ReadCell` (see `testing/README.md`).
- Tests that pin the fork colors still run with the fork table. These are `0007-view-file-markdown`, `000c-view-md-highlight` and the highlight driver. The check builds only the far2l target, so no plugin loads. Each such test says so in a comment.
- The spec, the invariants file, `markdown.data.md` and ADR-009 are updated in the same branch.
- Check: "The check" in `AGENTS.md`, plus a plugin build (full build) and the smoke run of all tests.

## Notes

- Owner's words, 2026-11-07: "let's be consistent with colorer for everything, then we can press F4 -> F6 view and see the same color including the code colors, use underscores, rule of thumb: the markdown looks like in F4 colorer but w/o markdown tags".
- Owner's words, 2026-11-07: "we will base our changes on the markdown-highlight -code story which we are about to merge".
- Owner's choices, 2026-11-07: heading levels get one look "Same as F4"; on a scheme that fails the contrast rule, "Fall back to fork table".
- Research, 2026-11-07, unmeasured in the app:
  - In `far2l.hrd`, `def:URI` is `#8888ff` with `style='4'`. `HeadText` and `LinkText` inherit it. 27 of the rgb schemes set a style on `def:URI`. The console `default.hrd` sets none. So the style comes from the export.
  - `getRegionDefine(name)` does not climb to parents. The export needs the `Region*` and the climbing overload.
  - `FarEditorSet` clears and recreates `regionMapper` on reload. A generation counter tells the core to refetch.
  - The plugin loads on first use. Its module is `colorer.far-plug-wide`.
  - The `highlight` story is built in `.worktrees/markdown-code-highlight`, not merged. Start this story only after it merges.
- Branch `feat/markdown-colorer-look`. Base `next/2.9.1-ovr.1`, after markdown-code-highlight merges. Budget and hard stop M $15/$20.
- Build order: this story, then markdown-colorer-fences, then markdown-code-tint. They share `MdPalette.cpp`, `ViewerPrinter.cpp` and the test `0007-view-file-markdown`.
- Ledger, 2026-11-07: status in-progress. `flow tier=full docs=spec,plan design=no orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=none`. Check: "The check" in `AGENTS.md` plus a full build (all plugins) and the smoke run of all tests. Budget $15, hard stop $20. Base `next/2.9.1-ovr.1` at `6bf36be25`, branch `feat/markdown-colorer-look`, worktree `.worktrees/markdown-colorer-look`. started: 2026-11-07T09:31:54Z
- Ledger, 2026-11-07: docs recorded: `gate.md`, `spec.md`, `plan.md` (6 tasks; design=no).
  Full build `../far2l.markdown-colorer-look.build` (92 s, cache reuse); baseline check green:
  golden, ascii driver, highlight driver (with `HIGH_BUILD` set), smoke 0007/0008/0009/000c
  all PASS. Fresh read (scout): the token-export path creates `FarEditorSet` through
  `ProcessViewerEventW(0,nullptr)` at 0 ms in the driver; the look export's first-call cost is
  open, T1's `look_first_call` measures it; `HrcLibrary::getRegion` returns null for an
  unknown name, no throw; `ReadCellRaw(x,y).Attributes` and `ReadCell(x,y).ForeTC` exist in
  the smoke framework (`testing/README.md`); the generation counter starts at 1, because the
  constructor calls `ReloadBase`. Plan fixes from the fresh read: the probe signature note,
  the emphasis placement in `cur_role()` (below body, above the quote/body default), the 000d
  fixture at `initdir/left/fixture.md` (the shape of 000c), the merged T6 (smoke sessions),
  and the durable docs moved to the main session at close-out. G2-design: stop for the owner
  at `gate.md`.
