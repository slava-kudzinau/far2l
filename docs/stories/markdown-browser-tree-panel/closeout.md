# Close-out: markdown-browser-tree-panel

Commits:
- 05b4355de: the story docs.
- f6a5b0e74 and a459fed26: T1, the settings, the root and the scan.
- 65ff501c7 and df2811e73: T2, the tree page, the reuse and the linear escape.
- c5d1732ae: T3, the route, the links and the anchors.
- 9a556c4ac: T4, the panel and the toggle.
- 93089a732: the durable docs.
Commits after the review: the `--init-config` sample lists the four keys, and the sample test checks them.
Check: the AGENTS.md check with `sh testing/markdown/browser-page.sh` added before `echo "check ok"`, run through `sh`: exit 0, `check ok`. The smoke tests 0007, 0008 and 0009 pass. `browser-page.sh` 80 ok, 0 fail. `browser-links-dom.sh` 36 ok, 0 fail.

## Rulings

1. One page holds every file of the tree as an inline `script` element. A `file://` page cannot read another file, and the page policy allows no `script src` from `file:`. The owner confirmed the mock.
2. The scan uses `find`, `sort` and `cksum` on top of the tools of the hard stop. The owner allowed it with "go in auto model".
3. Ceilings: depth 12, files 2000. Limits: 1 MiB for one file, 8 MiB of Markdown text in one page. The opened file counts first.
4. A root of `$HOME` or `/` gives the one-file page with a notice. `root=repo` stops at `$HOME` and never checks `/`.
5. Only Markdown files are in the tree. Hidden files and folders, `.git`, `node_modules`, names with a control byte and symlinks are skipped. The opened file joins the scan even as a symlink.
6. Order: shallow files first, then `LC_ALL=C` byte order. The cap keeps the files near the root.
7. Folders are `details` elements. The folders on the path of the shown file are open; a folder that the reader opened stays open.
8. Toggle key `b`. Hidden under 700 px. The focus goes to `#content` after a click in the tree or on a link.
9. A link out of the root, an absolute path or a `file:` target becomes text with `not in this tree`. `%XX` is decoded once, then the `..` and real-path checks run.
10. The one-file page keeps the markup and the CSS of story 1. Its popover shows the reason: `data-tree-why` is `ini`, `flag`, `home` or `slash`.
11. The page key adds `tr`, `r`, `d` and `n`. In tree mode a head line holds a `cksum` of the tree list. A new, changed, deleted or renamed file gives a new list and a rebuild.

## Parked findings

- T2 Minor 2: the reuse step of the scan passes when `find` cannot stat the page. The window is small, and the `cksum` line still guards the reuse.
- T1 Minor 3: two assertions prove less than their names say. `case_tree_ini_hostile` cannot tell `dir` from `repo` because the fixture has no `.git`. `case_tree_ini_keys` does not check that a trimmed `tree_depth` value is used.
- T3: `data-md-note` on a link to a `big` or `cut` file is not used by the page. The file itself shows the limit note.
- The Linux scan was not run. The root `/` case was not run: macOS has no readable file in `/` in the test folder.

## Manual checks for the owner

- Chrome and Firefox: the panel shows, `b` and the toggle open and close it, and a reload keeps the state. Under 700 px the panel is hidden.
- A click on another file shows it, Back returns. A link in the text to a file of the tree shows it. A link to `../x.md` shows as text with the note.
- Width 60, 70 and 80 with the panel open. Print hides the panel. A page with a link to a file over 1 MiB shows the limit note.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| task-reviewer-critical | opus | 2 | 1 | 1 | 8 |
| re-reviewer | haiku | 2 | 0 | 0 | 0 |
| final-reviewer | opus | 1 | 0 | 0 | 0 |

## Lessons

- Tree mode is the default. So each one-file case of the page suite names `--tree off`. The one-file page is the contract of story 1.
- `sed` replacement text treats `&` as the match. Inject a script with `awk` and `index`, not `sed`.
- A socket from a stopped smoke run under a long worktree path stays in `testing/tests/` under a cut name. Remove it by name; `find -type s` can miss it.
