---
description: The exact contract of the standalone md-view repo - layout, command, paths, install, tests and docs.
---

# Spec: md-view-repo

The repo is `~/Projects/md-view` (called `MV` below). The source is far2l `origin/next/2.9.1-ovr.3` at `614e702c9`. MV is made by `git filter-repo` on a fresh clone of far2l.

## Repo layout and history

| Path in MV | Comes from (far2l path, with history) |
|---|---|
| `lib/md-view` | `far2l/bootstrap/md-open.sh` |
| `lib/marked.min.js`, `lib/highlight.min.js` | `far2l/bootstrap/` same names |
| `lib/github-markdown-light.css`, `lib/github-markdown-dark.css`, `lib/hljs-github.min.css`, `lib/hljs-github-dark.min.css` | `far2l/bootstrap/` same names |
| `lib/marked.LICENSE.md`, `lib/highlight.LICENSE.md`, `lib/github-markdown.LICENSE.md` | `far2l/bootstrap/` same names |
| `tests/browser-page.sh`, `tests/browser-links-dom.sh`, `tests/browser-page/` | `testing/markdown/` same names |
| `tests/fixtures/` | `docs/stories/markdown-browser-view/fixtures/`, and before that `stories/markdown-browser-view/fixtures/` |
| `tests/hostile-dom.sh` | new, ported from `docs/stories/markdown-browser-view/check.sh` |
| `bin/md-view`, `Makefile`, `README.md`, `LICENSE`, `AGENTS.md`, `docs/` | new |

- The default branch is `main`.
- `git log --format='%an %ad %s'` keeps the author, the author date and the subject of each original commit. The commit `92a76a055` "Add md-open.sh to show Markdown in the browser" is in the history with a new hash.
- No commit in MV holds a path outside the table.

## The command

| Item | Old value | New value |
|---|---|---|
| Command name in usage, messages and help | `md-open.sh`, `md-open:` | `md-view`, `md-view:` |
| Config file | `${XDG_CONFIG_HOME:-$HOME/.config}/far2l/settings/md-view.ini` | `${XDG_CONFIG_HOME:-$HOME/.config}/md-view/config.ini` |
| Cache folder | `${XDG_CACHE_HOME:-$HOME/.cache}/far2l/md` | `${XDG_CACHE_HOME:-$HOME/.cache}/md-view` |
| Temporary page | `${TMPDIR:-/tmp}/far2l-md-<pid>.html` | `${TMPDIR:-/tmp}/md-view-<pid>.html` |
| The text `md-view.ini` in comments, usage, errors and the sample config | `md-view.ini` | `config.ini` |
| The head line of the sample config | `Defaults for the far2l Markdown browser page (md-open.sh, Alt-F3).` | `Defaults for md-view.` |
| Page key meta | `far2l-md-key` | `md-view-key` |
| Page head comments | `far2l-md-ini`, `far2l-md-tree` | `md-view-ini`, `md-view-tree` |
| localStorage keys | `far2l-md-width`, `far2l-md-theme`, and any other `far2l-md-*` key | the same names with `md-view-` in place of `far2l-md-` |

- The flags, the section `[View]`, the keys in it, the page and its behavior do not change.
- The command reads no file in the old far2l paths.
- `md-view --print-association` prints the far2l File associations entry, as today. The command is the absolute path of the `md-view` launcher, then `!.!`. The mask and the steps do not change. An absolute path works when far2l starts from the Dock with a short `PATH`. The path comes from `command -v md-view`, or else from the installed or checkout `bin/md-view`. It is quoted only when the shell needs it.

## Install

| Call | Result |
|---|---|
| `make install PREFIX=$p` | `$p/share/md-view/` holds `md-view` (mode 755) and the 9 vendored files. `$p/bin/md-view` (mode 755) runs `$p/share/md-view/md-view "$@"` with `exec`. `PREFIX` default is `/usr/local`. |
| `make uninstall PREFIX=$p` | Removes exactly the files that install wrote, and the empty `$p/share/md-view`. |
| `make check` | Runs `tests/browser-page.sh`, `tests/browser-links-dom.sh`, `tests/hostile-dom.sh` and `tests/launcher.sh`. Exit 0 only when no test fails. A test that has no Chrome or no network prints `skip` lines and exits 0. |
| `$p/bin/md-view sample.md --no-open --out $o` from any folder | writes `$o`, exit 0. |

## Tests

- `tests/browser-page.sh` and `tests/browser-links-dom.sh` pass with the new names and paths. The `$ini` path of the tests is the new config path, so the width, theme and browser cases all run against it. Each assertion that names an old value in the table above names the new value.
- `tests/browser-page.sh` has one new case. A config file in the old far2l path has no effect. The same file in the new path sets the width.
- `tests/hostile-dom.sh` keeps each row of the old story check that tests the page. These rows cover escapes, hostile HTML as text, blocked remote images, the source of a broken diagram, and the CSP. It also keeps the `--print-association` rows with the far2l name escape, because they prove that a hostile file name cannot inject a shell command. It drops only the rows that read `far2l/src`, run `cmake` or check install paths. With no Chrome it prints `skip all` and exits 0. The mermaid rows print `skip` with no network.

## Docs

| File | Holds |
|---|---|
| `README.md` | What md-view does, install, usage, flags, config keys, the far2l and the `mc.ext` lines, and the move from the old far2l paths. |
| `LICENSE` | GNU GPL version 2, the license of far2l, where the script came from. |
| `AGENTS.md` | The index for people and agents: build, the check, layout, rules, where the docs are. |
| `docs/architecture/decisions/adr-001-standalone-tool.md` | The decision of far2l ADR-005, carried forward, plus this split. |
| `docs/architecture/md-view.spec.md` | The behavior contract, from far2l `markdown-browser-view.spec.md`, with the new names. |
| `docs/architecture/md-view.invariants.md` | The rules of the "Browser view" section of far2l `markdown.invariants.md`. |
| `docs/stories/md-view-repo/` | A copy of `story.md` and `closeout.md`, with no absolute home path. |

- Each doc has `description` frontmatter and passes `check_ste.py`.

## Publish

- A public GitHub repo `slava-kudzinau/md-view`. Its `main` has the same head as local `main`.

## Done

`make check` in MV exits 0. The install row above works on a temporary `PREFIX`. `git grep -n -i far2l` in MV finds only `README.md`, `docs/`, `AGENTS.md`, the `--print-association` text, the tests of that text, and the old-config-path test.

Manual check by the main session at close-out:

1. Install to a temporary `PREFIX`.
2. Add the association that `--print-association` prints to far2l.
3. Press Alt-F3 on `tests/fixtures/diagram.md`. The page opens in the browser and the mermaid diagram draws.

## Out of scope

- Removal from far2l (story md-view-remove).
- A Homebrew formula, CI, offline mermaid.
