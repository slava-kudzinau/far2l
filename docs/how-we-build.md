---
description: Guide for people who work on this far2l fork - how to build, test, format and deliver a change, and what CI does.
refs:
  - README.md
  - HACKING.md
  - CODESTYLE.md
  - testing/README.md
---

# How we build far2l (the fork)

This guide shows what the repo files do not show. For the general build and the code rules read [README.md](../README.md), [HACKING.md](../HACKING.md) and [CODESTYLE.md](../CODESTYLE.md). Every command here was run on macOS 26 (arm64) on branch `next/2.9.1-ovr.1`. The docs layout and the file naming rule are in [ADR-001](architecture/decisions/adr-001-docs-structure.md). The Markdown feature docs start at [markdown.overview.md](architecture/features/markdown/markdown.overview.md).

## Prerequisites

- macOS: `brew bundle` reads `Brewfile` (cmake, ninja, pkg-config, openssl, libarchive, libssh, libnfs, neon, uchardet, libxml2, samba, wxwidgets, wget). Linux: the packages in `dependencies.txt` (`dependencies-minimal.txt` for a smaller set).
- Go is needed for the smoke tests (`testing/src/Makefile` builds `testing/far2l-smoke`). Node is needed for the Markdown golden test and to rebuild the mermaid bundle. QuickJS needs no install - the source is in `far2l/third_party/quickjs`.
- The Markdown feature needs no cmake option. The `far2l_md` library (`md4c`, `MarkdownRender.cpp`, `AsciiMermaid.cpp`) always builds. `-DTESTING=Yes` is only for the smoke tests.

| Option | Use |
|---|---|
| `-DTESTING=Yes` | Builds in the test hooks that `far2l-smoke` needs. Without it the tests hang and fail. |
| `-DUSEWX=No` | TTY-only build with no wxWidgets. We use it. |
| `-DPYTHON=yes` | Python plugin (CI uses it). We do not build it. |
| `-DCMAKE_BUILD_TYPE=RelWithDebInfo` | What `../far2l.build` uses. CI uses `Release`. |
| `-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0` | macOS only. |

## Build

`../far2l.build` is next to the checkout and is out of tree. Its `CMakeCache.txt` has `CMAKE_HOME_DIRECTORY` set to this checkout, so it belongs to this tree. Its flags are `-DTESTING=Yes -DUSEWX=No -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0`. The cache also holds `FAR2L_MD_NO_QJS=1`; no file in the tree reads it (unverified that it has any effect).

Rule: always build out of tree, in a folder next to the repo (`../far2l.<name>.build`). Never run cmake inside the checkout. Each worktree uses its own build folder.

```sh
# new build folder (once)
cmake -S . -B ../far2l.build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTESTING=Yes -DUSEWX=No -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
# far2l only - the fast loop
cmake --build ../far2l.build --target far2l -j8
# full build - the plugins too
cmake --build ../far2l.build -j8
```

The incremental full build took 42 s on an 18-core Mac. The binary is `../far2l.build/install/far2l.app/Contents/MacOS/far2l` (on Linux: `install/far2l`). Resources go to `far2l.app/Contents/Resources`.

## Tests

There is no `make test`, and `ctest` at the build root finds nothing, because the root `CMakeLists.txt` has no `enable_testing`. Two plugins have CTest tests of their own. `plugins/transformer/CMakeLists.txt` adds `transformer_config_test` when `TESTING` is on. `plugins/jumpword/tests/CMakeLists.txt` is stand-alone (the root build does not use it), and the file says some of its tests are expected to fail. We did not run either one. The main tests are the smoke tests plus the Markdown scripts below.

**Smoke tests.** `testing/far2l-smoke-run.sh APP [prefix]` builds the Go runner, then clears every `tests/*/workdir`, copies `initdir/*` into `workdir/` and runs the tests whose folder name starts with `prefix`. A test is `tests/NNNN-name/test.js`; it gets `WorkDir()`. The syntax is in [testing/README.md](../testing/README.md).

```sh
cd testing
./far2l-smoke-run.sh "$PWD/../../far2l.build/install/far2l.app/Contents/MacOS/far2l" 0007-view-file-markdown
```

- Without a prefix the run goes in name order and stops at the first failure (a Go panic). On this Mac this stops at 0004, so use one run for each test.
- Pass a full folder name. A number such as `0007` runs both tests that have it.
- The exit code is 0 even when a test returns a non-zero code. That case only logs `[FAILED] Error`. Look for `[DONE]` in the output.
- `./far2l-smoke -t 30 APP tests/NAME` runs the runner directly. Make the workdir first: `rm -rf` it, `mkdir`, then copy `initdir/*`.
- A run can fail with `operation not permitted` (EPERM). Run it again. We did not see it in the last runs (cause unverified).
- A crashed run can leave a unix socket in `testing/tests`. The runner takes it for a test, so delete it.

Numbers 0007, 0008 and 0009 each hold two tests (an upstream test and one of ours). This is not a problem: the runner uses the folder name, and each folder has its own `workdir`. Only the prefix rule above needs care. Use a new unused number for a new test.

Result on this Mac, `next/2.9.1-ovr.1`: 12 of 14 pass. `0004-vt-shell` fails (`ping` output, `Nothing at ... of [localhost]`) and `000b-find-file` fails (expected `Found files: 101, folders: 3`). We did not find out why (unverified, probably the macOS environment).

**Markdown tests.**

| Script | What | Needs |
|---|---|---|
| `0007-view-file-markdown`, `0008-view-md-default`, `0009-view-md-setting-off` | The viewer in Markdown mode and its default and off setting | `-DTESTING=Yes` build |
| `sh testing/markdown/ascii-golden.sh` | `mermaid-ascii.js` against `testing/markdown/golden/*.mmd` and `*.txt`, in Node and in QuickJS | `node`; the QuickJS leg needs `../far2l.build/far2l/third_party/quickjs/libqjs.a` |
| `sh testing/markdown/ascii-driver.sh` | The `AsciiMermaid` C++ code in a stand-alone driver (timeout, speed) | `libqjs.a`, no far2l build |

All three run with no network. Do not regenerate `golden/*.txt` to make a failing test pass: they come from the `beautiful-mermaid` package (`golden/make-golden.mjs`), not from the bundle. The test for `md-open.sh` (Alt-F3 browser view) exists only in a story folder, not in `testing/` (see Risks).

**The check for a story** and the command for the full suite are in [AGENTS.md](../AGENTS.md). The check takes about 12 s after a build. The full suite takes about 32 s, and two tests fail today (see above).

## Format and style

`.clang-format` (column limit 110, indent 4, `SortIncludes: Never`), `.editorconfig` (UTF-8, LF, tabs of width 4; spaces in `*.yml` and `CMakeLists.txt`) and [CODESTYLE.md](../CODESTYLE.md). Nothing runs them for you. In old code, copy the style of the file. Run `clang-format` only on lines you change.

## CI

| File | Runs on | What |
|---|---|---|
| `.github/workflows/build.yml` | Linux (docker, `_ci/Dockerfile.*`; Ubuntu 22.04, 24.04), macOS 14 and 15 | Build. Only one Ubuntu job uses `-DTESTING=yes` and runs `far2l-smoke-run.sh`. macOS jobs build only. |
| `.github/workflows/artifacts.yml` | Push to `master` | Release artifacts. |
| `.github/workflows/clean_workflow.yml` | Monthly | Deletes old runs. |
| `.cirrus.yml` | FreeBSD 14.3, macOS | Build only (Cirrus: `brew bundle`, Ninja). |
| `.travis.yml` | Old Linux and macOS | Old; do not rely on it (unverified that it still runs). |

`build.yml` starts on a push to `master` and on a pull request to `master`, or by hand. A push to `next/*` or `markdown-preview/*` does not start it. A PR into `master` starts it. A PR into a story branch (a stacked PR) does not. A push to the fork's `master` is skipped by the `if` guard (`github.repository != 'elfmz/far2l'`). The fork has Actions enabled and 0 workflow runs (`gh api`, checked 2026-11-06). CI does not run the Markdown golden tests.

## Regenerate things

- `far2l/bootstrap` holds scripts and resources that cmake copies to the install folder (`far2l/bootstrap/CMakeLists.txt`). Language and help files come from `farlang.templ.m4`, `FarEng.hlf.m4` and `farlng.pl` in `far2l/bootstrap/scripts`. The build runs `farlng.pl` through perl, and no CMake file adds the `farlng` tool. A normal build does this; there is no step to run by hand.
- The mermaid bundle `far2l/bootstrap/mermaid-ascii.js` (limit 300 KB, no `elkjs`) is built by `sh far2l/bootstrap/mermaid-ascii/build.sh`. It needs `npm` and the network: `npm install` of the pinned `beautiful-mermaid` and `npx esbuild@0.25.8`. After it, run `ascii-golden.sh` and `ascii-driver.sh`.
- `mermaid.min.js` (5.5 MB) in `far2l/bootstrap` is git-ignored and cmake no longer installs it. Nothing on this branch reads it. Only `mermaid-png.sh` on the unmerged branch `markdown-preview/image-poc` and the check of story markdown-default-on use it. It can be deleted. The copy in `Contents/Resources` of an old build folder is a leftover of that build.
- `marked.min.js` and its license are checked in. No procedure is recorded.

## Branches and delivery

- `origin` is the fork `slava-kudzinau/far2l`. `upstream` is `elfmz/far2l`. `master` is the upstream base. `next/2.9.1-ovr.1` is our integration branch.
- Name each new branch `{type}/{slug}`. The type is `feat`, `fix`, `refactor`, `test`, `doc` or `chore`. A story branch is `feat/{story}` or `fix/{story}`. Each worktree needs its own build folder (`../far2l.<name>.build`).
- The first stories used `markdown-preview/<short>`, and these branches keep their names. The short name drops the prefix `markdown-` or `markdown-preview-`. Only some of these stories had a worktree in `.worktrees/`.
- Stories are stacked: a story branch is based on the previous one (`fixes`, then `reading`, then `ascii-diagrams`, `default-on` and `browser-view` in parallel). The PR for each goes into `next/2.9.1-ovr.1` as a merge commit (PRs #1 to #4). The owner says when to push and merge.
- Delivery uses `fde-delivery:deliver-story` (see `docs/how-it-works.md` in the `fde-delivery` plugin):
  1. The owner approves `story.md` and sets the check command, the budget and the owner-checked paths.
  2. The skill writes `spec.md`, `plan.md` and `gate.md`; for a full story the owner reads the gate first.
  3. Implementer and reviewer subagents build the story in a worktree.
  4. The skill runs the check again, writes `closeout.md` and opens a draft PR.
  5. The owner says "merge". The skill does not merge by itself.

## Risks and open questions

- The three story `check.sh` files (stories markdown-ascii-diagrams, markdown-default-on, markdown-browser-view) are broken since the move from `stories/`. They find the repo root with `../..` from their own folder. Now that is `docs/`, so they look for `docs/testing` and `docs/far2l`. We ran `markdown-ascii-diagrams/check.sh` (exit 127) and `markdown-browser-view/check.sh` (exit 1, 47 checks failed). We did not run `markdown-default-on/check.sh`, because it would copy files into the wrong folder. The fix is `../../..`.
- The `md-open.sh` checks exist only in the `check.sh` of story markdown-browser-view, with its fixtures. Nothing in `testing/` covers it.
- `0004-vt-shell` and `000b-find-file` fail on this Mac. We do not know if they also fail on `master`.
- Branch `markdown-preview/fixes` exists only locally (no `origin` copy). The story markdown-preview-fixes has no `check.sh`.
