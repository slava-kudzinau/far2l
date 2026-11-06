# Close-out: markdown palette

Status: build complete on `feat/markdown-palette`, stacked on the unmerged `feat/markdown-compact-layout` (a7b61f26f).
Check: `sh docs/stories/markdown-palette/check.sh` -> exit 0, `check ok`, verified by the main session at 48cb801c7.
Manual checks: `manual-checks.md`, two items, results pending from the owner.

## What was built against the plan

| Task | Commit | Result |
|---|---|---|
| T1 probe and test hooks | 8ac6c428a | `COLORTERM=truecolor` in `testing/far2l-smoke-run.sh`; RGB path reachable |
| T2 roles in the renderer | e99382fce + 0449ddea7 | `MdRole` split into body/h1/h2/h3/strong/link/code/quote/rule/art |
| T3 palette module and resolve | c8d162cc2 + 616bc7c82 + 131105939 | `MdPalette.hpp/.cpp`, draw-time resolve, ini overrides, contrast, mtime cache |
| T4 smoke coverage | be44a2bcc + 48cb801c7 | 0007 assertions and fixtures, `--norgb`, override and hostile-ini cases |

Verified role colors (ForeTC, RGB path):

| Role | ForeTC |
|---|---|
| body | ffff00 |
| h1 | ffff5f |
| h2 | ffaf5f |
| h3-h6 | 87afff |
| strong | ffffff |
| labeled link | 87ff87 |
| code | ff87ff |
| quote | afafd7 |
| rule/border | 8787af |
| art | c0c0c0 |
Layout, text and row count are unchanged; the check includes the golden and ascii-driver runs.

## Decisions changed during the build

- Rule color: the story gives one role to horizontal rules and table borders. The plan first split the today's-look entry by glyph kind; that needs two roles, so the ruling is one role with `COL_VIEWERTEXT` for both. Vertical `│` bars moved from art to rule to match the story wording. The RGB path keeps the rule color `8787af` for both.
- Autolink color is parked. `Viewer::UnderlineLinks` recolors autolink cells to `#ff8833` after
  the printer, so `MD_ROLE_LINK` never shows on them. `viewer.cpp` and `LinkHighlighter.cpp` are
  outside the story's file list. The test asserts only that the autolink cell differs from body.
  Labeled links that do not match a URL prefix keep the link role.
- Contrast is all or none, not per role. The spec was right; the first T3 pass checked each role alone.
  The verdict is now cached per batch and keyed on the viewer background's real color.
- `--norgb` asserts the fallback of palette-driven roles, not the body cell. The body fore is the
  viewer's own `COL_VIEWERTEXT` true color, so it is not a palette fallback.
- The `link=@Help.Topic` override case was rewritten so it can fail (T4 review, Important #2).

## What the reviews caught

- T2 (critical): stale "heading role" comment, a 133-char line, C-style casts. All fixed in 0449ddea7.
- T3 (critical): per-role contrast instead of all-or-none, and a vacuous h4 assertion. Fixed in 616bc7c82.
  The re-review found a process-lifetime verdict cache that ignored theme changes. Fixed in 131105939.
- T4 (task review): missing rule-color assertions, the unfalsifiable link override, a near-vacuous
  norgb pair, H6 uncovered, missing `CheckBack`, a duplicate h2/h3 relation. All fixed in 48cb801c7.
- Final review: 1 Critical, 4 Important and 3 Minor. The Critical: an `@Name` override was frozen at ini load. The Important: no contrast assertion, an unfalsifiable link override, the rule fallback split, a partial `--norgb` set.
  All are fixed in `2fe21e34d`. The contrast case now runs a light `Viewer.Text` theme and asserts the fallback.
- Parked on purpose: reparse on a failed load, unknown role mapping to body override, duplicated
  `kBackBits`, mtime-only cache key. Recorded for the final review.

## Verification of the new assertions

Each new assertion was run against the regression it guards. The mutation was built, 0007 was run, the mutation was reverted, and the suite went green again.

| Mutation | Result |
|---|---|
| The contrast verdict forced to pass | FAILED: "light background: h1 kept its RGB default" |
| The h1 fallback changed to `F_CYAN` | FAILED: "norgb h1: fore 3, expected 14" |
| The `@Name` value resolved to the body color | FAILED: "link override vs body foreground is the same as the viewer foreground" |
| The `link` key skipped at load | FAILED: "link override vs default link foreground is the same as the default link foreground" |

## Rulings

1. G2-design waived by the owner, 2026-11-06: "go in auto mode, nothing is build yet".
2. Spec wins over the plan for today's look: horizontal rules use `COL_VIEWERTEXT`, table borders `COL_PANELHIGHLIGHTTEXT`.
3. `viewer.cpp` and `LinkHighlighter.cpp` are out of scope. A labeled link keeps the link role; an autolink cell asserts only differs-from-body.
4. `--norgb` asserts the palette-driven roles' 4-bit fallbacks, not the body cell, whose fore is the viewer's own true color.
5. Spend metering: this run's model has no entry in `scripts/prices.json`. The build subtree shows $0.00, and the main session enforces the dollar stop from its own usage.
6. T4 fix round 1 carried all six review findings. The rule color, the link override and the `--norgb` pair can each fail now.

## Parked findings

- Autolink cells show the viewer link highlight `#ff8833` instead of the link role (`Viewer::UnderlineLinks` recolors after the printer). Fix needs `viewer.cpp` or `LinkHighlighter.cpp`.
- A failed ini load is re-parsed on every call instead of cached (cheap; no user-visible effect).
- An unknown role value maps to body through the override path rather than being rejected.
- `kBackBits` is duplicated between `ViewerPrinter.cpp` and `MdPalette.cpp`.
- The ini cache key is the file mtime only, so a same-size rewrite inside one mtime tick is missed.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| task-reviewer-critical | opus | 2 | 1 | 2 | 6 |
| re-reviewer | opus | 1 | 0 | 0 | 0 |
| task-reviewer | haiku | 1 | 0 | 2 | 4 |
| final-reviewer | current model | 1 | see final-review.md | | |

## Lessons

- A subagent that receives "run the check" without a coordinate table re-probes the screen for an hour. The brief must carry the measured cell values and forbid scratch probes.
- `review-yield.mjs` counts findings only under `## Issues`; a review file without that heading records C0 I0 M0. The dispatch template should say so.

## The three close-out checks

1. Contract -> `docs/architecture/decisions/adr-004-palette-roles.md` and
   `docs/architecture/features/markdown/markdown.invariants.md` (`[colors]` rules).
   These are owner-checked paths; the build did not edit them. Status: to be checked at close-out.
2. Setting -> `markdown.data.md` settings list must name `settings/markdown-colors.ini`.
   Target: `docs/architecture/features/markdown/markdown.data.md`. Content: to be checked at close-out.
3. Rule -> the `AGENTS.md` color rule ("use palette roles, no fixed ANSI colors") needs the new
   role/table wording. Owner-checked path; not edited. Status: to be checked at close-out.

Owner-checked docs (`AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`, golden fixtures) are untouched by this build.
