# Close-out draft: markdown-ascii-diagrams

Commits: b9c8a92f5 (T1 bundle and goldens), fab0b73cb (T2 AsciiMermaid), eb4b3c287 (T3 viewer, old path removed), 65acd2903 (T4 unspaced-edge pre-pass), plus a docs commit (spec rows 12 and 24, plan T4). Check: `sh docs/stories/markdown-ascii-diagrams/check.sh` exit 0 (golden, driver, far2l build, smoke 0007).

## Rulings

1. A broken-flowchart test source is the header error `graph XX\n  A --> B`. Package 1.1.3 does not throw on `A -->`. Spec row 12 needs rewording to match.
2. Spec row 24 ("within 2 s") means: `Render` returns false with `timeout` after the 2 s deadline. Reword the spec.
3. The build dir is the absolute `/Users/Viachaslau_Kudzinau/Projects/far2l.ascii.build`. The libqjs.a for the Node/qjs tests comes from the main checkout's `far2l.build`.
4. The T2 review was raised to task-reviewer-critical (timeouts and shared runtime state).
6. Spec rows 12 and 24 reworded; plan.md gained T4 after the final review (main session ruled the fix in scope).
5. The commit trailer is Sonnet 5.5 on all commits (ordered by the main session).

## Open items for the owner (disclose in the PR text and the gate)

- FIXED in T4 (65acd2903): `A-->B` without spaces drew one wrong box. `entry.js` now spaces edge operators for `graph`/`flowchart` sources, outside label text. Goldens: nospace, spaced (equal output), label. Open Minor: circle/cross/edge-id endpoints may be split by the pre-pass; `A--B` becomes `A -- B`.
- Large diagrams: a 26-node `graph TD` fails at once (stack overflow, hint shown); a 26-node `graph LR` chain can take the full 2 s and show `timeout`. Render blocks the UI thread for up to 2 s.
- A fence that starts with a `%%` comment or directive is sent to the hint (`unsupported: %%`), though the package draws it. Fix: skip leading `%%` lines in the first-word check.
- Story B (`browser-view`) also edits `far2l/bootstrap/CMakeLists.txt`; `git merge-tree` merges it without conflict, result consistent. `story.md` says B does not touch that file: owner-checked, not edited.
- The hint names Alt-F3, a key story B adds.
- The smoke harness leaves a stale unix socket in `testing/tests/` after a run; `check.sh` deletes all sockets there first. Root cause is outside this story.

## Parked findings

- T2 Minor 1-6: timing window 1900-2500 ms and thresholds may flake; package-internal JS errors have no `js:` prefix; runtime kept after a non-timeout fatal state; small string-helper gaps; no test that a failed render leaves the runtime usable.
- T3 Minor 1-8: hint row nFilePos is the last code row's (keeps offsets non-decreasing); driver runs before the build that creates libqjs.a; swimlane code role not asserted; art rows lose the list/quote lead; wide and over-wide art untested; check.sh removes all sockets under testing/tests; lookup copied three times; stale wording.
- Final Minor 2-7: spec rewording (rows 12, 24), `%%` first-word gate, mid-size diagram cliff, check.sh ordering, thin viewer tests, story B doc wording.

## Lessons

- A haiku implementer finished all three tasks with no fix round. Haiku task reviewers and the sonnet final reviewer found no Critical issues; the one Important finding is a package limit that no task test could see. Spec rows that describe a third-party package's behavior (such as "throws on a dangling edge") need a check against the package when the plan is written.


## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| task-reviewer-critical | opus | 1 | 0 | 0 | 6 |
| task-reviewer | haiku | 2 | 0 | 0 | 12 |
| final-reviewer-light | sonnet | 1 | 0 | 1 | 6 |
