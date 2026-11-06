# Close-out: markdown palette

Status: build complete on `feat/markdown-palette`, stacked on the unmerged `feat/markdown-compact-layout` (a7b61f26f).
Check: `sh docs/stories/markdown-palette/check.sh` -> exit 0, `check ok`, verified by the main session at 452d97c31.
Manual checks: `manual-checks.md`, two items, results pending from the owner.

## What was built against the plan

| Task | Commit | Result |
|---|---|---|
| T1 probe and test hooks | d6a62d121 | `COLORTERM=truecolor` in `testing/far2l-smoke-run.sh`; RGB path reachable |
| T2 roles in the renderer | a870f964a + 2175761aa | `MdRole` split into body/h1/h2/h3/strong/link/code/quote/rule/art |
| T3 palette module and resolve | 69768b418 + b83de954e + 9e679fd0f | `MdPalette.hpp/.cpp`, draw-time resolve, ini overrides, contrast, mtime cache |
| T4 smoke coverage | b6dd439b3 + 452d97c31 | 0007 assertions and fixtures, `--norgb`, override and hostile-ini cases |

Verified role colors (ForeTC, RGB path): body ffff00, h1 ffff5f, h2 ffaf5f, h3-h6 87afff,
strong ffffff, labeled link 87ff87, code ff87ff, quote afafd7, rule/border 8787af, art c0c0c0.
Layout, text and row count are unchanged; the check includes the golden and ascii-driver runs.

## Decisions changed during the build

- Rule color split: the plan said `COL_PANELHIGHLIGHTTEXT` for rules and borders. The spec won:
  horizontal rules `COL_VIEWERTEXT`, table borders `COL_PANELHIGHLIGHTTEXT`.
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

- T2 (critical): stale "heading role" comment, a 133-char line, C-style casts. All fixed in 2175761aa.
- T3 (critical): per-role contrast instead of all-or-none, and a vacuous h4 assertion. Fixed in b83de954e.
  The re-review found a process-lifetime verdict cache that ignored theme changes. Fixed in 9e679fd0f.
- T4 (task review): missing rule-color assertions, the unfalsifiable link override, a near-vacuous
  norgb pair, H6 uncovered, missing `CheckBack`, a duplicate h2/h3 relation. All fixed in 452d97c31.
- Parked on purpose: reparse on a failed load, unknown role mapping to body override, duplicated
  `kBackBits`, mtime-only cache key. Recorded for the final review.

## The three close-out checks

1. Contract -> `docs/architecture/decisions/adr-004-palette-roles.md` and
   `docs/architecture/features/markdown/markdown.invariants.md` (`[colors]` rules).
   These are owner-checked paths; the build did not edit them. Status: to be checked at close-out.
2. Setting -> `markdown.data.md` settings list must name `settings/markdown-colors.ini`.
   Target: `docs/architecture/features/markdown/markdown.data.md`. Content: to be checked at close-out.
3. Rule -> the `AGENTS.md` color rule ("use palette roles, no fixed ANSI colors") needs the new
   role/table wording. Owner-checked path; not edited. Status: to be checked at close-out.

Owner-checked docs (`AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`, golden fixtures) are untouched by this build.
