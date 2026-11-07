---
description: The Alt-F3 browser page gets a docked file-tree panel on the left, and a click on a file in the tree or on a local link shows that Markdown file in the same page.
status: in-progress
size: M
risk: security
depends-on: markdown-browser-github-look
---

# Story: File tree and local links in the browser page

## Goal

The owner keeps notes in several Markdown files in one folder or one repository. Today the Alt-F3 page shows one file, and a relative link opens the other file as raw text. The owner wants a file tree on the left of the page, like the project panel of Zed. A click on a file in the tree, or on a link in the text, shows that file in the same look. There is no trip back to far2l, and Back returns.

## Context

- [ADR-005](../../architecture/decisions/adr-005-browser-view-script.md): the page is made by `far2l/bootstrap/md-open.sh`. Raw HTML in the Markdown shows as text. The page policy allows inline scripts, the jsdelivr CDN and local images only.
- Story `markdown-browser-github-look` made the page look, the gear, `md-view.ini`, the saved choice and the cache page. This story edits the same files.
- A page opened from `file://` cannot fetch another local file. The page must hold the files it shows.
- far2l runs the Alt-F3 command in the folder of the active panel. The script gets the file name.

## Scope

**In:**

| Part | Rule |
|---|---|
| Root | `root=dir` (default): the real folder of the opened file. `root=repo`: the nearest folder upward that has `.git`, else the folder of the file. The walk stops at `$HOME` or `/`. |
| Scan | The script lists the Markdown files under the root. It skips `.git`, `node_modules` and hidden folders. It does not follow symlinks. |
| Limits | `tree_depth` (default 6) and `tree_max_files` (default 500) in `md-view.ini`. Each has a ceiling in the script that the file cannot pass. At the cap, the panel says "showing the first N files". A size limit for each file and for the page. |
| Panel | Docked on the left and collapsible. The root name at the top. The current file is marked. A toggle button at the top-left edge of the panel, and one key. Hidden under 700 px. The gear stays at the top right. With the panel open, the width setting (60, 70, 80) applies to the space on its right. |
| Show a file | A click on a tree file or on a link to a file in the page shows it in the reading pane. The focus stays on the reading pane. Back and Forward work. |
| Links | A link to a Markdown file in the root shows that file. A link that leaves the root shows as plain text with the note "not in this tree". `%XX` is decoded once before the root check. `file:` and `javascript:` targets do not navigate. |
| Settings | `tree=on\|off` (default `on`) and flag `--tree on\|off`. `off` gives the page of story 1: no panel and no scan. The saved open or closed state of the panel wins over the file, as for width and theme. If the file sets `off`, the popover says so. `root=dir\|repo`. |
| Untrusted input | File names, folder names, link targets and the text are data. A name with spaces, `%`, quotes, `<`, `>`, `&` or a leading `-` cannot break out of the page. Raw HTML still shows as text. |
| Cache | An unchanged tree opens from the cached page. A changed, added or removed file builds the page again. `--no-cache` forces a build. |

**Out:**

- A local server, live reload, and files that change while the page is open (run Alt-F3 again).
- Non-Markdown files in the tree.
- A new source in the page policy, and any C++ change.
- The terminal Markdown view.

## Size

M. One script, one page change, two test scripts with fixtures. Risk `security`: the script reads files named by untrusted text and the disk. Hard stop: the scan uses only POSIX `sh`, `sed`, `awk`, `grep`, `find`, `ls`, `sort` and `cksum`. If it needs another tool, stop and ask the owner.

## Unknowns

- The load time of a page with 500 files. The first task measures it.
- Whether `find` with POSIX options only can apply the depth limit and the skip rules on macOS and Linux. The first task proves it with a fixture tree.

## Acceptance criteria

- Alt-F3 on a file in a folder of notes shows the page with the tree on the left. The current file is marked. A click on another file shows it, and Back returns.
- A link to a file in the tree shows it in the page. A link to `../outside.md`, an absolute path, a `file:` URL or `%2e%2e/x.md` shows as text with "not in this tree".
- `root=repo` finds the folder with `.git`. With no `.git` up to `$HOME`, the root is the folder of the file.
- `.git`, `node_modules`, hidden folders and symlinks are not in the tree. A tree over `tree_max_files` or `tree_depth` stops at the limit, and the panel says "showing the first N files". An ini value over a ceiling gives the ceiling.
- `tree=off` or `--tree off` gives the page of story 1, with no scan. The popover says that the tree is off.
- The panel toggle and its key open and close the panel. The state stays after a reload. Under 700 px the panel is hidden. With the panel open, the text column has the chosen width of the space on its right.
- File names with spaces, `%`, quotes, `<`, `>`, `&`, a leading `-`, and a file named `"><script>.md` cannot inject into the page.
- An unchanged tree opens from the cache. A changed, added or deleted file builds again.
- The check in `AGENTS.md` passes, and a new test was run against a copy with the root rule removed and failed.
- Close-out: update `markdown-browser-view.spec.md` with the tree, the link rules, the settings and the limits. Update `markdown.data.md` with the new settings keys and the page contents. Add a note to ADR-005.

## Notes

- Owner words, 2026-11-07: "what if the MD has a link to another (local) file is it possible to follow?" and "we need to have several files". Decision: bundle, as a separate story.
- Owner change, 2026-11-07: the link crawl is replaced by a docked, collapsible file tree on the left, like Zed's project panel. Links inside the root navigate in the page, links outside show as text with a note. New keys `root=dir|repo`, `tree=on|off`, `tree_depth`, `tree_max_files`, flag `--tree`. The first gate (crawl, breadth first, symlink files) is void.
- Branch `feat/markdown-browser-tree-panel`, base `next/2.9.1-ovr.1`, started after `markdown-browser-github-look` is merged. The brief is not committed until this branch exists.
- started: 2026-11-07T16:38:41Z
- flow tier=full docs=spec,plan design=no orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=plan-verifier-critical
- Budget/hard stop: M $15/$20 (deliver-story Budgets). BASE: `next/2.9.1-ovr.1` at 7f6523ad5 (story 1 merged, PR #13).
- Check: the AGENTS.md check with `BUILD=../far2l.markdown-browser-tree-panel.build` and `sh testing/markdown/browser-page.sh &&` added before `echo "check ok"`. Exact text in `plan.md`, Global Constraints.
- Owner-checked paths: the AGENTS.md list. No task edits them. The close-out edits `markdown-browser-view.spec.md`, `markdown.data.md` and ADR-005.
- G2-design waived by owner, 2026-11-07: "go in auto model with the tree panel story, the mock look great!". The seven gate rulings stand as recommended in gate.md: one page with every file inline; find, sort and cksum allowed; ini ceilings 12 and 2000, caps 1 MiB per file and 8 MiB per page, defaults 6 and 500; home or `/` as root falls back to the one-file page; toggle key `b`; the DOM test stays out of the AGENTS.md check; size stays M ($15/$20).
- Plan verifier, 2026-11-07: pass 1 FIX (5 Important), pass 2 FIX (1 Important), pass 3 FIX (2 Important). Each was corrected from its findings; the owner approved the extra rounds and then the build without a fourth pass: "#1 go," and "autnoumus mode". Remaining risk is left to the task reviews and the final review. Autonomous run: G3-spend and G4-merge still stop.
