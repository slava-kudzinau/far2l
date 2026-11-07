# Handover: markdown-browser-tree-panel

Written 2026-11-07 for a new session. The owner stopped the build at the budget and said: "no budget raise, write handover doc for another session". This file is not a story deliverable. Delete it at close-out.

## State in one paragraph

Alt-F3 on a `.md` file should open the reading page with a docked, collapsible file tree on the left. Two of four tasks are built, reviewed and committed. The panel is not built, so the story is not shippable yet. Nothing is pushed. No PR exists. Spend was $15.96 against a $15 budget (hard stop $20). The owner refused a raise, so the rest must fit in the money the owner allows, or the story must be cut (see "Decisions for the owner").

## Where things are

| Item | Value |
|---|---|
| Worktree | `/Users/Viachaslau_Kudzinau/Projects/far2l/.worktrees/markdown-browser-tree-panel` |
| Branch | `feat/markdown-browser-tree-panel`, tip `df2811e73`, no upstream set |
| Base | `origin/next/2.9.1-ovr.1` at `7f6523ad5` (story 1 merged as PR #13) |
| Build folder | `../far2l.markdown-browser-tree-panel.build` (configured) |
| Story docs | `docs/stories/markdown-browser-tree-panel/`: `story.md`, `spec.md`, `plan.md`, `gate.md` |
| Build workspace | `docs/stories/markdown-browser-tree-panel/.build/` (gitignored, on disk only): `status.md`, `progress.md` (ledger and 18 rulings), `task-N-brief.md`, `task-N-report.md`, `task-N-review.md`, `check.sh` |
| Mock UI the owner approved | `/private/tmp/claude-501/-Users-Viachaslau-Kudzinau-Projects-far2l/da915daf-5c3b-4047-8d09-582c07323393/scratchpad/tree-mock/mock.html` (may be gone; it is a scratchpad file) |
| Tier and flow | `flow tier=full docs=spec,plan design=no orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=plan-verifier-critical`. Risk `security`. Size M. |
| G2-design | Waived by the owner, with three plan-verifier rounds. See `story.md` Notes. |

Commits on the branch, oldest first:
1. `05b4355de` story docs, index, epic and story 1 slug edits.
2. `f6a5b0e74`, `a459fed26` task 1: settings, root search, scan, `--list-tree`.
3. `65ff501c7`, `df2811e73` task 2: the page in tree mode and its reuse.

The commit trailer in `plan.md` is `Co-Authored-By: Claude Opus 5.5`. The session rule says Sonnet 5.5. The orchestrator used the plan's text. Decide which one before more commits.

## What is done

- **Task 1** (settings, root, scan): ini keys `root`, `tree`, `tree_depth`, `tree_max_files`; flag `--tree`; flag `--list-tree`; the scan with its ceilings and caps. Reviewed, three findings fixed.
- **Task 2** (page in tree mode and reuse): one page with every file inline; page key with `tr`, `r`, `d`, `n`; reuse only when the tree list checksum matches and no file is newer than the page. A Critical finding (quadratic `awk` escape) was fixed with one linear `sed` pass.
- The check from `plan.md` Global Constraints passed with exit 0 and printed `check ok`. The orchestrator ran it as `sh .build/check.sh`. The main session did not re-run it. Re-run it first.

## What is left

| Task | What | Role and review (from `plan.md`) |
|---|---|---|
| T3 | Page script: hash route `#/ENC` and `#/ENC#FRAG`, link rules 1 to 8, images, anchors. New test `testing/markdown/browser-links-dom.sh` (headless Chrome). Includes the cases `dom_link_absolute`, `dom_link_bad_escape`, `dom_link_out_of_root_query`, `dom_link_out_of_root_backslash`, and three regression copies of the script that must make them fail. | implementer, `task-reviewer-critical` |
| T4 | The panel: 260 px left column, folders as `details`, toggle `#md-tree-toggle`, key `b`, `localStorage` key `far2l-md-tree`, hidden under 700 px and in print, the cap note `Showing the first N files.`, gear `#md-gear` unmoved, popover notice from `data-tree-why`. The 500-file measure in `measure.md`. Case `dom_panel_names_hostile`. | implementer-risky, `task-reviewer-critical` |
| Final | `final-reviewer` (opus) on the whole diff from BASE. Then `closeout.md`, gate "At merge" half, `cost.jsonl` row. | |

Read `plan.md` for exact done checks. They are shell commands that grep for `ok <case>`. `spec.md` governs every exact value.

## Rules that bind the next session

- **Budget.** $15 budget, $20 hard stop, G3-spend. The owner said no raise. Never raise it on your own. Check spend with `node /Users/Viachaslau_Kudzinau/Projects/fde-kit/plugins/fde-delivery/scripts/spend.mjs --match .worktrees/markdown-browser-tree-panel --budget 15 --stop 20`. It exits 10 above the budget.
- **Merge.** Never push to `master`. Never send a PR to `upstream`. Use `gh --repo slava-kudzinau/far2l`. The standing rule lets the agent merge its own story PR into `next/2.9.1-ovr.1` when the check is green and the final review has no open Critical or Important finding.
- **Owner-checked paths.** A builder does not edit `AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`, `testing/markdown/golden/*.txt`, vendored code or `.github/workflows/`. Close-out edits to them are proposals for the main session.
- **Security.** Markdown, link targets and file names are untrusted. Names enter the page only as an encoded path (`A-Za-z0-9._~/%-`). No new CSP source. No C++ change. Each security task ends by running its test against a copy with the rule removed, and that run must fail.
- **Files.** New files under `testing/` and `far2l/bootstrap/` need `git add -f`.
- **Smoke tests.** Clean stale sockets before each one (see `AGENTS.md`). Use `'--test=/tmp/far2[l]'` in the `pkill` pattern, because the pattern in `AGENTS.md` kills its own shell.

## Parked findings and notes for close-out

- Linux scan not run. Put this in the gate "At merge" half. The owner decides at merge.
- The root `/` case does not run on macOS.
- `--init-config` sample lacks the new keys (`root`, `tree`, `tree_depth`, `tree_max_files`). Fix it, or list it as a closeout item.
- T2 finding 2: a `find` failure in the `-newer` reuse scan is not handled. The race is small and `cksum` still checks. Parked.
- The `AGENTS.md` `pkill` pattern kills its own shell. Propose `'--test=/tmp/far2[l]'` to the owner.
- Close-out must update `docs/architecture/features/markdown/markdown-browser-view.spec.md` (flags `--tree` and `--list-tree`, keys), `markdown.data.md`, and ADR-005 if the one-page choice outlives the story.
- Gate rows that need the owner at merge: heading ids and anchors now also apply to the one-file page; hidden files and control-byte names are skipped by the scan.
- Manual checks for the owner (gate item 4): Back, Forward, a reload for saved state, focus after a click, a window under 700 px, the scroll to `user-content-FRAG`, in Firefox and Chrome.
- Story 1 follow-ups still open: mark it `done` in `docs/stories/index.md`; the owner's Firefox storage check.

## Decisions for the owner

The owner has not chosen how to finish within $15. The honest estimate is about $30 for the whole story, with $16 spent on two tasks. Options:
1. **Cut the story.** Ship T1 and T2 only. This gives a scan and a cache and no visible feature. It is not recommended.
2. **Ask again with a smaller raise**, for example $24, and take the review cost down: one review round per task, no re-review unless a Critical finding.
3. **Do T3 and T4 in the main session with a leaner process.** This breaks the "main session does not edit code" rule in `AGENTS.md`, so only the owner can allow it.

Until the owner chooses, do not dispatch an implementer.

## How to resume (after the owner picks a budget)

1. `cd` to the worktree. Run `git status` and `git log --oneline 7f6523ad5..HEAD`. Expect five commits and a clean tree except `handover.md`.
2. Run the check: `sh docs/stories/markdown-browser-tree-panel/.build/check.sh`. If `.build/` is gone, copy the check from `plan.md` Global Constraints.
3. Record the new budget in `story.md` Notes, in the owner's words.
4. Start `fde-delivery:story-orchestrator` in the background. Give it the worktree, the story files, the workspace, the script paths under `/Users/Viachaslau_Kudzinau/Projects/fde-kit/plugins/fde-delivery/scripts/`, the check, the flow line, the new budget and "resume from progress.md at T3".
5. Supervise with `supervise.mjs --dir <workspace> --match .worktrees/markdown-browser-tree-panel --budget B --stop S` about every 15 minutes.
6. After T4: final review, `closeout.md`, fill gate "At merge", record cost with `spend.mjs --record`, push with `git push -u origin feat/markdown-browser-tree-panel`, open a draft PR into `next/2.9.1-ovr.1`, then merge on the standing rule.
