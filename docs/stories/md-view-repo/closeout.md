# Close-out: md-view-repo

Commits in `~/Projects/md-view`, branch `main`:

- T1: the history extract, which ends at 049afea.
- T2: b88c2f0.
- T3: c415a5d.
- T4: 3b0bd1b, and the fix 89f28a8.
- T5: 62d5d95.
- Final-review fixes: 67ebf49 and 88e0e37.

Check: `make -C ~/Projects/md-view check` gives exit 0, with 169 ok, 0 FAIL and 0 skip on this Mac. The main session ran it again after the last commit, together with the T4 and T5 done checks. All gave exit 0.

Manual check by the main session:

1. Install to a temporary `PREFIX`.
2. Run the association command from `--print-association` the same way far2l runs it, with the Dock `PATH` (`/usr/bin:/bin:/usr/sbin:/sbin`), on `tests/fixtures/diagram.md`.
3. Draw the page in headless Chrome.

The page was written, and the mermaid flowchart drew as an SVG. The main session could not press Alt-F3 in a live far2l, because there is no tmux on this Mac. The owner does that step once by hand.

## Rulings

1. T1 named the 10 bootstrap files one by one. The folder also holds files that are not in the spec table.
2. The two tests use the repo root `$here/..`. They need it for `git show` in the tree-off case.
3. The association command runs the launcher with no `sh`. So the installed `bin/md-view` has mode 755. `make install` writes it through a temporary file, so it never writes through a symlink.
4. `tests/hostile-dom.sh` drops three gate.md rows, because they read a far2l story doc that md-view does not have. The remote-image rows of the page stay.
5. Four old assertions were updated. The old check failed the same four on its own base.
6. The final review used the diff base 049afea, because md-view has no `origin/main`.
7. The `%` in `PREFIX` case of `uninstall` has no test. A manual check passed.
8. In the md-view copy of story.md, the far2l links are plain text.

## Parked findings

- T2 Minor 2: the "installed" path that a checkout tries is outside the repo.
- T3 Minor 1: the label of the refused-CDN case does not match its check.
- T3 Minor 2: the empty-file assertion is less strict than before.
- T3 Minor 3: the hostile file name does not hold `;`, `|` or a backtick.
- T4 Minor 5: no test checks the text of the installed launcher.
- T4 re-review Minor: if `$PREFIX/bin/md-view` is a folder, install gives exit 0 and leaves a temporary file.
- Final re-review Minor 2: the upgrade note in `lib/marked.LICENSE.md` names `tests/hostile-dom.sh`. That test skips with no Chrome, and no test checks `<!--`.
- Fix round 2 of the final review had no scoped re-review. The change is small. A mutation probe showed that the new assertion fails when it must.
- The README rule for Midnight Commander (`Regex=\\.md$`) was not run in mc. The reviewer checked it against the header of the system `mc.ext.ini` (mc 4.8.33).
- GNU `mktemp` and GNU `install` were not run. Only macOS was tested.

## Lessons

- A reviewer wrote its findings under `## Critical`, `## Important` and `## Minor`. `review-yield.mjs` counts only `### N. <Severity>:` headings under `## Issues`. The reviewer brief must give that shape.
- The T4 implementer stopped before it committed, and it committed later. Before you send a new implementer, read `git log`.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| task-reviewer-critical | opus | 5 | 0 | 1 | 14 |
| final-reviewer | opus | 1 | 0 | 1 | 5 |
| re-reviewer | haiku | 1 | 0 | 1 | 2 |
