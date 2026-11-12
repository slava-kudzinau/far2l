---
description: Move the Alt-F3 Markdown browser view out of far2l into a standalone public repo md-view, with its history, its own docs and its own install.
status: in-review
size: M
risk: contract, migration
refs:
  - docs/architecture/decisions/adr-005-browser-view-script.md
  - docs/architecture/features/markdown/markdown-browser-view.spec.md
---

# Story: md-view-repo

## Goal

The browser view must be a tool that a user installs separately. Other tools then call it like any other external viewer. The owner's words: "let's keep it in a separate repo md-view, far2l wouldn't have any knowldge of it, user installs md-view separately". This story makes the new repo. Story md-view-remove then removes the browser view from far2l.

## Context

- [ADR-005](../../architecture/decisions/adr-005-browser-view-script.md) and [markdown-browser-view.spec.md](../../architecture/features/markdown/markdown-browser-view.spec.md) describe the script as it is now.
- far2l runs an external tool on Alt-F3 through File associations (`ProcessLocalFileTypes(..., FILETYPE_ALTVIEW, ...)`, `far2l/src/panels/filelist.cpp:1520`). Midnight Commander does the same through `mc.ext`.
- The source tree is `origin/next/2.9.1-ovr.3`.

## Scope

**In:**

| Part | What changes |
|---|---|
| Repo | A new git repo at `~/Projects/md-view`, made with `git-filter-repo` from a fresh clone of far2l. It keeps the original commits of the paths below. |
| Paths kept | `far2l/bootstrap/md-open.sh`, `marked.min.js`, `highlight.min.js`, `github-markdown-light.css`, `github-markdown-dark.css`, `hljs-github.min.css`, `hljs-github-dark.min.css`, `marked.LICENSE.md`, `highlight.LICENSE.md`, `github-markdown.LICENSE.md`, `testing/markdown/browser-page.sh`, `testing/markdown/browser-links-dom.sh`, `testing/markdown/browser-page/`. `git filter-repo --path-rename` moves them to `lib/` and `tests/` in all of the history. |
| Command | The command is `md-view`. The flags stay the same. |
| Config and cache | The config is `${XDG_CONFIG_HOME:-$HOME/.config}/md-view/config.ini`. The cache is `${XDG_CACHE_HOME:-$HOME/.cache}/md-view/`. There is no read of the old far2l paths. |
| Strings | No file in the repo names far2l, except: the README, `AGENTS.md` and `docs/`, the far2l text of `--print-association`, the tests of that text, the test of the old config path, and the history. |
| Install | A `Makefile`: `make install PREFIX=...` (default `/usr/local`), `make uninstall`, `make check`. The vendored files go beside the script, for example `$PREFIX/share/md-view/`, and `$PREFIX/bin/md-view` starts it. |
| Docs | `README.md` (install, usage, far2l Alt-F3 association, `mc.ext` rule, config keys). `LICENSE`. `AGENTS.md` for people and agents: build, test, layout, rules, and the place of the docs. `docs/` with an ADR that carries ADR-005 forward, a spec moved from `markdown-browser-view.spec.md`, and invariants. `docs/stories/md-view-repo/` holds a copy of `story.md` and `closeout.md`, with no absolute home path. The structure follows far2l ADR-001, so that the owner can install `fde-delivery` in the repo after this story. |
| Remote | A public GitHub repo `slava-kudzinau/md-view`. Push `main`. The owner approved this on 2026-11-12. |

**Out:**

- Removal of the files from far2l, the CMake block, the hint row and the far2l docs. Story md-view-remove takes these.
- A Homebrew formula or tap. That is a later story in md-view.
- Offline mermaid. It stays on the CDN, as ADR-005 says.
- The installation of `fde-delivery` in md-view. The owner does this.

## Size

M. One repo extract, one rename of paths, a Makefile, a README, an AGENTS.md and the docs. The contract changes: command name, config path, cache path. There is no new behavior in the page.

## Unknowns

- Does `make check` need Chrome? `browser-links-dom.sh` prints skip with no Chrome. The first task decides if a skip is a pass and writes the rule into AGENTS.md.
- `docs/stories/markdown-browser-view/check.sh` and its fixtures (`plain`, `escapes`, `hostile`, `remote`, `diagram`, `broken-diagram`) test the security rules, but they point at a far2l build folder. The first task decides if they move into `testing/` of md-view. They are the only hostile-input fixtures, so the default is to move them.
- `git-filter-repo` is not installed. The first task installs it with `brew install git-filter-repo`. The owner approved this on 2026-11-12.

## Acceptance criteria

- `git log --oneline` in md-view shows the original commits of `md-open.sh`, including `92a76a055` "Add md-open.sh to show Markdown in the browser" (its new hash), with the original authors and dates.
- `make check` passes. It runs `browser-page.sh`, `browser-links-dom.sh` (or a stated skip) and the hostile-input fixtures.
- `make install PREFIX=$tmp` and then `$tmp/bin/md-view sample.md --no-open --out $tmp/p.html` writes a page. This works from any current folder.
- `md-view --print-association` prints a far2l File associations command that uses `md-view`, not a far2l path.
- With `config.ini` in the new path, the width, theme and browser keys take effect. A file in the old far2l path has no effect.
- `git grep -i far2l` finds only the places that the Strings row allows.
- A manual check: Alt-F3 on a `.md` file in far2l, with the new association, opens the page in the browser, and a mermaid diagram draws.
- `slava-kudzinau/md-view` exists, is public, and has `main` with the same head as the local repo.
- `AGENTS.md`, the README, the ADR, the spec and the invariants exist. Each doc has `description` frontmatter and passes `check_ste.py`.

## Notes

- Owner rulings, 2026-11-12:
  - A separate repo. far2l has no knowledge of it.
  - History through `git-filter-repo`.
  - Own config and cache paths, with no fallback.
  - A public GitHub repo.
  - md-view has its own AGENTS.md and docs. The owner installs `fde-delivery` there.
- The work happens in `~/Projects/md-view`, not in a far2l worktree. The far2l check does not apply. The check is `make check` in md-view.
- far2l record: this brief and its close-out go on far2l branch `feat/md-view-repo`, based on the newest `next/` branch (`origin/next/2.9.1-ovr.3` on 2026-11-12). That branch holds only story records. A copy goes in `docs/stories/md-view-repo/` of md-view.
- Build order: this story first. Story md-view-remove depends on it, so that users have md-view before far2l drops the browser view.
- budget: 15, hard stop: 20 (size M)
- flow tier=full docs=spec,plan,design design=yes orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=plan-verifier-critical
- started: 2026-11-12T19:01:22Z
- BASE: `614e702c9` (origin/next/2.9.1-ovr.3), worktree `.worktrees/md-view-repo`. The code goes in `~/Projects/md-view`.
- Check: `make -C ~/Projects/md-view check`, plus the install row of `spec.md`.
- Hooks: preflight none, verify = the check, status none, cleanup none.
- G2-design waived by owner, 2026-11-12: "go in auto mode".
- Main-session rulings, 2026-11-12, from the plan verifier:
  - History paths are rewritten to `lib/` and `tests/` with `--path-rename`. No final move commit. The brief wording was the main session's, and the owner's ruling was "history through git-filter-repo".
  - The Strings row lists the exact far2l mentions that the other criteria need.
  - `--print-association` prints only the far2l entry. The `mc.ext` rule is only in the README. No `DESTDIR` (YAGNI).
- Plan verifier: pass 1 ASK (ruled above), pass 2 FIX with 5 findings and no ASK. The main session fixed all 5 from the docs and started the build with no third pass, under the auto-mode waiver.
- `--print-association` prints the absolute launcher path, because far2l started from the Dock has a short `PATH`. Main-session ruling, 2026-11-12.
- `git-filter-repo` 2.47.0 installed with Homebrew on 2026-11-12, as the owner approved.
