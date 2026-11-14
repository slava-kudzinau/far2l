---
description: Tasks that cut md-view out of far2l into its own repo, rename it, add install and tests, and write its docs.
---

# Plan: md-view-repo

Spec: `spec.md` (governs exact values). Design: `design.md`.

## Global Constraints

- MV is `~/Projects/md-view`. The far2l source repo is `~/Projects/far2l`. The story worktree is `~/Projects/far2l/.worktrees/md-view-repo`. Never run `git filter-repo` in the far2l repo or in any of its worktrees.
- Write only inside MV. Do not change any file in the far2l repo or its worktrees.
- POSIX `sh` only for scripts. No new tool, package or network install. `git-filter-repo` 2.47.0 is installed.
- Every exact name, path and key is in the tables of `spec.md`. Do not invent other names.
- The script reads its vendored files from its own folder (`self_dir`). Keep that.
- Treat Markdown input as untrusted. Do not weaken the CSP, the mermaid `securityLevel:'strict'`, the SRI hash, the escape of raw HTML, or the block of remote images.
- The commit trailer below is exact. Do not use the name of your own model in it.
- Commit message: a short imperative summary line, a blank line, then the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. One commit per task, in MV on branch `main`.
- Check: `make -C ~/Projects/md-view check` (from T4 on). Before T4, the done check of each task is the proof.
- Owner-checked paths: none in MV. In far2l: all of the far2l repo.
- Do not push and do not create a GitHub repo. The main session does that.

## T1: Extract the history

**Depends on:** none
**Done check:** `cd ~/Projects/md-view && git log --format='%an|%ad|%s' | grep -qxF 'Viachaslau Kudzinau|Fri Nov 6 00:03:53 2026 +0100|Add md-open.sh to show Markdown in the browser' && test -f lib/md-view && test -f tests/fixtures/hostile.md && test -z "$(git ls-files | grep -v -E '^(lib|tests)/')" && test "$(git branch --format='%(refname:short)')" = main && test -z "$(git tag)" && test -z "$(git remote)" && test "$(git log --format=%h -- tests/fixtures/hostile.md | tail -1)" = "$(git log --format=%h --grep='^Add md-open.sh to show Markdown in the browser$' | tail -1)"`
**Role:** implementer — review task-reviewer-critical

1. `git clone --no-local ~/Projects/far2l ~/Projects/md-view`, then `git -C <MV> checkout -B main 614e702c9`. MV must not exist before. The local far2l repo has no `next/2.9.1-ovr.3` branch, only the remote-tracking ref, so do not use `--branch`. Give `git filter-repo --force`, because the checkout makes the clone "not fresh".
2. In MV, run one `git filter-repo` call. Give one `--path` for each source path in the spec table. Give one `--path-rename` for each move. The source paths are the 10 files in `far2l/bootstrap/`, `testing/markdown/browser-page.sh`, `testing/markdown/browser-links-dom.sh`, `testing/markdown/browser-page/`, `docs/stories/markdown-browser-view/fixtures/` and its older path `stories/markdown-browser-view/fixtures/`. Both fixture paths rename to `tests/fixtures/`. Put `far2l/bootstrap/md-open.sh:lib/md-view` before `far2l/bootstrap/:lib/`.
3. Rename the branch to `main`. Remove the `origin` remote. Remove all other branches and tags.
4. Record in the task report: the commit count, `git log --format='%h %an %ad %s' | tail -3`, and the new hash of far2l commit `7f6523ad5` from `.git/filter-repo/commit-map`.

## T2: Rename to md-view in the script and the tests

**Depends on:** T1
**Done check:** `cd ~/Projects/md-view && MD_OPEN=lib/md-view sh tests/browser-page.sh && MD_OPEN=lib/md-view sh tests/browser-links-dom.sh && ! grep -n -E 'far2l/settings|far2l-md|/far2l/md|md-open|md-view\.ini|mc\.ext' lib/md-view tests/browser-links-dom.sh && test "$(grep -c 'far2l/settings' tests/browser-page.sh)" -le 2 && grep -q 'old config path is ignored' tests/browser-page.sh`
**Role:** implementer — review task-reviewer-critical

1. In `lib/md-view`, apply each row of the "The command" table in `spec.md`. Remove each other far2l mention in `lib/md-view` (for example the line 2 header), except the text of `--print-association`. The `far2l/settings` path appears only in the new old-path case of `tests/browser-page.sh`, at most on two lines. This includes the localStorage keys in the page script inside the file.
2. `--print-association`: print the far2l entry as the spec says. Do not add an `mc.ext` block. The command path is absolute: the path that `command -v md-view` gives when it is absolute. Else the first file that exists of `$self_dir/../../bin/md-view` (installed), `$self_dir/../bin/md-view` (checkout), `$self_dir/md-view`. Quote it only when the shell needs it, as today.
3. In `tests/browser-page.sh` and `tests/browser-links-dom.sh`: the default `MD_OPEN` is `$root/lib/md-view`, where `root` is the repo root. Change every assertion on an old value to the new value.
4. Add the case of the spec. Write `width=80` in section `[View]` of `$XDG_CONFIG_HOME/far2l/settings/md-view.ini`: the page key has the default width 70. Write the same text in `$XDG_CONFIG_HOME/md-view/config.ini`: the page key has 80. The test name is `old config path is ignored`.
5. The headers of the test files describe md-view, not far2l stories.
6. `case_tree_off_same_page` in `tests/browser-page.sh` runs `git show 7f6523ad5:far2l/bootstrap/md-open.sh`. Use the new hash from the T1 report and the path `lib/md-view`. Pipe the old page through `sed 's/far2l-md-/md-view-/g'` before the compare, and make `strip` remove the `md-view-key` line.
7. Put the full output of `git grep -n -i far2l -- lib tests` in the task report. Each hit must be in the Strings list of the story.

## T3: Port the hostile-input story check

**Depends on:** T2
**Done check:** `cd ~/Projects/md-view && sh tests/hostile-dom.sh && CHROME=/nonexistent sh tests/hostile-dom.sh | grep -q '^skip all'`
**Role:** implementer — review task-reviewer-critical

1. Read the old check with `git -C ~/Projects/far2l show 614e702c9:docs/stories/markdown-browser-view/check.sh`.
2. Write `tests/hostile-dom.sh`. Keep each row that tests the page from the fixtures in `tests/fixtures/`. Keep the `association:` rows and the far2l name-escape `sed`, with the command name `md-view`.
3. Chrome lookup: the same as `tests/browser-links-dom.sh`. With no Chrome, print `skip all` and exit 0. When `CHROME` is set but is not an executable file, also print `skip all` and exit 0.
4. Rows that need the mermaid CDN: probe `https://cdn.jsdelivr.net/npm/mermaid@12.3.0/dist/mermaid.min.js` with `curl -sfI --max-time 10`. If it fails, print `skip <label>` for each such row and go on.
5. Drop a row only when it reads `far2l/src`, runs `cmake`, or checks an install path. List each dropped row by label in the task report.
6. Each case runs with a fresh `HOME`, `XDG_CONFIG_HOME`, `XDG_CACHE_HOME` and `TMPDIR` in a temporary folder that the script removes. Exit 0 only when no row fails.

## T4: Launcher and Makefile

**Depends on:** T3
**Done check:** `cd ~/Projects/md-view && make check && p=$(mktemp -d) && make install PREFIX=$p && (cd / && "$p/bin/md-view" ~/Projects/md-view/tests/browser-page/sample.md --no-open --out "$p/o.html") && test -s "$p/o.html" && make uninstall PREFIX=$p && test ! -e "$p/bin/md-view" && test ! -e "$p/share/md-view"`
**Role:** implementer — review task-reviewer-critical

1. `bin/md-view` in the repo is not the installed file. The install map:
   - `lib/md-view` goes to `$(PREFIX)/share/md-view/md-view`, mode 755.
   - The 9 vendored files of `lib/` go to `$(PREFIX)/share/md-view/`, mode 644.
   - `$(PREFIX)/bin/md-view`, mode 755, is two lines: `#!/bin/sh` and `exec '<PREFIX>/share/md-view/md-view' "$@"`. The path is in single quotes. `make install` stops with an error when `PREFIX` holds a single quote.
2. `uninstall` removes the 11 named files, then removes `share/md-view` with `rmdir` (it stays if it holds other files). `Makefile` targets: `install`, `uninstall`, `check`. Variable `PREFIX ?= /usr/local`. No `DESTDIR`. Use `install -m 755` and `install -m 644`. `check` runs the four tests of the spec in sequence and stops on the first failure.
3. In the repo, `bin/md-view` is a launcher for a checkout. It finds its folder with `dirname -- "$0"` and runs `exec "$dir/../lib/md-view" "$@"`. It does not resolve symlinks. Then a user can put the checkout `bin/` on `PATH`.
4. A name with a space reaches the script unchanged through both launchers. Test this in `tests/launcher.sh`, which installs to a temporary `PREFIX`. `make check` runs it after the other three tests.

## T5: Docs

**Depends on:** T4
**Done check:** `cd ~/Projects/md-view && for f in docs/architecture/decisions/adr-001-standalone-tool.md docs/architecture/md-view.spec.md docs/architecture/md-view.invariants.md docs/.templates/story.md docs/.templates/adr.md docs/stories/md-view-repo/story.md; do test -f "$f" || exit 1; done && ! grep -rn '/Users/' docs README.md AGENTS.md && for h in Install Usage Configuration 'Use from far2l' 'Use from Midnight Commander' 'Move from far2l' License; do grep -q "^## $h" README.md || exit 1; done && for f in README.md AGENTS.md docs/architecture/*.md docs/architecture/decisions/*.md; do grep -q '^description:' "$f" && python3 ~/Projects/fde-kit/plugins/fde-delivery/kb/scripts/check_ste.py "$f" || exit 1; done && head -1 LICENSE | grep -q 'GNU GENERAL PUBLIC LICENSE'`
**Role:** drafter — review final

1. `LICENSE`: copy `~/Projects/far2l/LICENSE.txt` unchanged.
2. `README.md`, `AGENTS.md` and the files under `docs/architecture/`, as the "Docs" table of `spec.md` says. Sources: far2l `docs/architecture/decisions/adr-005-browser-view-script.md`, `docs/architecture/features/markdown/markdown-browser-view.spec.md`, and the "Browser view" section of `docs/architecture/features/markdown/markdown.invariants.md`, each at `614e702c9`. Apply the new names of the spec to every value you copy.
3. `AGENTS.md` follows the shape of the far2l `AGENTS.md`, but short. It has these sections:
   - "Build and test": `make check` and the install row. Also the rule: with no Chrome or no network, a test prints `skip` lines and passes; read the `skip` lines before you trust a pass.
   - "Where things are": a table of the docs.
   - "Global policies": untrusted input, the CSP and SRI rules, vendored files and their licenses, ASD-STE100 docs with `check_ste.py`, branches `{type}/{slug}`, no push without the owner, and story work through `fde-delivery:deliver-story` with folders in `docs/stories/{story}/`.
4. A link in a copied far2l doc that points to a far2l file becomes plain text that names far2l and the file. Links inside md-view stay links.
5. README: a first paragraph says what md-view does. Then these `##` headings, in this order: `Install`, `Usage`, `Configuration`, `Use from far2l`, `Use from Midnight Commander`, `Move from far2l`, `License`.
6. `docs/stories/md-view-repo/story.md`: a copy of far2l `docs/stories/md-view-repo/story.md` from the story worktree, with each absolute home path replaced by `~`. The main session adds `closeout.md` at close-out.
7. `docs/.templates/`: copy `story.md` and `adr.md` from far2l `docs/.templates/`.
8. The README section "Move from far2l" tells a user to move `~/.config/far2l/settings/md-view.ini` to `~/.config/md-view/config.ini`. It also says that the user can delete the old cache folder `~/.cache/far2l/md`.
9. Each doc passes `check_ste.py` and has `description` frontmatter. A frontmatter value does not contain `: ` or ` #`.
