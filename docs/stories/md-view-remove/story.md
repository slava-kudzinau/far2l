---
description: Remove the Alt-F3 Markdown browser view and every mention of it from far2l, now that md-view is a separate tool, and drop the terminal hint row.
status: in-review
size: M
risk: contract
depends-on: md-view-repo
refs:
  - docs/architecture/decisions/adr-005-browser-view-script.md
  - docs/architecture/features/markdown/markdown-viewer.spec.md
---

# Story: md-view-remove

## Goal

far2l must have no knowledge of md-view. The owner said: "far2l wouldn't have any knowldge of it, user installs md-view separately". This also brings the fork closer to `upstream`, because the fork drops a script, nine vendored files and an install block.

## Context

- Story md-view-repo moves the browser view to the md-view repo.
- [ADR-005](../../architecture/decisions/adr-005-browser-view-script.md) and `markdown-browser-view.spec.md` (deleted by this story) describe what goes.
- The hint row: `MERMAID_HINT` (`far2l/src/markdown/MarkdownRender.cpp:31`) and its splice in the mermaid fence loop (`MarkdownRender.cpp`, the `if (art.empty())` branch near line 1324). Smoke test `testing/tests/0007-view-file-markdown/test.js` checks the hint near line 165.

## Scope

**In:**

| Part | What changes |
|---|---|
| Files | Delete `far2l/bootstrap/md-open.sh`, `marked.min.js`, `highlight.min.js`, `github-markdown-light.css`, `github-markdown-dark.css`, `hljs-github.min.css`, `hljs-github-dark.min.css`, `marked.LICENSE.md`, `highlight.LICENSE.md`, `github-markdown.LICENSE.md`, `testing/markdown/browser-page.sh`, `testing/markdown/browser-links-dom.sh`, `testing/markdown/browser-page/`. |
| CMake | Delete the "Markdown in the browser" block in `far2l/bootstrap/CMakeLists.txt` (lines 105-125). Remove the `md-open` filter in the scripts loop (lines 71-81), so that the loop is a plain `list(APPEND)`. Keep the mermaid-ascii rules (lines 87-103). |
| Hint row | Remove `MERMAID_HINT` and the code that inserts the row. A mermaid fence that the terminal cannot draw shows as a plain code block, with the same rows and tint as any fenced code. |
| Test 0007 | Assert that the code rows of the undrawn swimlane fence show, and that no row follows them with the text `diagram not drawn`. |
| Docs | Mark ADR-005 `status: superseded` and add one line that md-view replaces it. Mark `markdown-browser-view.spec.md` `status: superseded`, or delete it and say so in the overview. Remove the browser-view sections from `markdown.data.md`, `markdown.runtime.md`, `markdown.invariants.md`, `markdown.overview.md`, `markdown-viewer.spec.md` (hint lines), `blueprint.md`, `views/context.md`, ADR-002 and ADR-006 mentions, `docs/how-we-build.md` and `AGENTS.md`. |

**Out:**

- The md-view repo itself. Story md-view-repo makes it.
- The story folders of done browser stories (`markdown-browser-view`, `markdown-browser-github-look`, `markdown-browser-tree-panel`, the epic). They are history. `retire-story` removes them later.
- The upstream Alt-F3 help text in the `.hlf.m4` files. It is generic and does not name the browser view.
- A shipped File associations entry. far2l ships none today and ships none after this story.

## Size

M. Most of the change is deletion. One C++ change (the hint row), one smoke test change, one CMake change, and docs in about twelve files.

## Unknowns

- Do docs outside the paths that the scout searched name `md-open`? The acceptance grep answers this.

## Acceptance criteria

- `git grep -n -i -E "md-open|md-view\.ini|browser view"` finds nothing outside `docs/stories/` and ADR-005, except the one line in ADR-005 that names md-view as its replacement.
- `git grep -n "MERMAID_HINT\|diagram not drawn"` finds nothing outside `docs/stories/`.
- A swimlane mermaid fence in F3 shows its code rows as a fenced code block, with no hint row. Test 0007 checks this and fails on the old binary.
- `cmake --build out -j8` installs no `md-open.sh` and no vendored browser file. On macOS, `out/install/far2l.app/Contents/Resources/` has no `md-open.sh` and no `marked.min.js`.
- The check in `AGENTS.md` prints `check ok`. All smoke tests pass, except the two known failures `0004-vt-shell` and `000b-find-file`.
- At close-out: ADR-005 superseded, `markdown-viewer.spec.md` without the hint, the invariants without the "Browser view" section, and each changed doc passes `check_ste.py`.

## Notes

- Owner rulings, 2026-11-12:
  - far2l has no knowledge of md-view.
  - Remove the hint row. Do not keep a neutral text.
  - Merge PR #32 first (merge commit `614e702c9`), because it edits the same markdown docs.
- Only test 0007 (line 165) checks the hint text. No golden file or driver has it (`git grep` on `origin/next/2.9.1-ovr.3`, 2026-11-12).
- Branch: `feat/md-view-remove`, based on the newest `next/` branch (`origin/next/2.9.1-ovr.3` on 2026-11-12), worktree `.worktrees/md-view-remove`.
- Start this story only after md-view-repo is done and `slava-kudzinau/md-view` is public, so that users can install md-view before far2l drops the browser view. The release notes of the next far2l package say that Alt-F3 for Markdown now needs md-view.
- Check: the check command in `AGENTS.md`, with `BUILD` set to the worktree build folder.
- budget: 15, hard stop: 20 (size M)
- flow tier=full docs=spec,plan design=no orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=plan-verifier-critical
- started: 2026-11-12T20:31:27Z
- G2-design waived by owner, 2026-11-12: "go in auto mode".
- BASE: `b51146802` (`feat/md-view-repo`, stacked). md-view-repo is done, and its records wait in draft PR #34. After #34 merges as a merge commit, rebase this branch on `origin/next/2.9.1-ovr.3`.
- Worktree `.worktrees/md-view-remove`. Build folder `.build/out` (git-ignored on this base). The workspace is `.build/`.
- Check: `sh .build/check.sh` in the worktree. It runs the AGENTS.md check with `BUILD=.build/out`, `ASCII_BUILD=$BUILD`, the three smoke tests named one by one, and a socket clean before each smoke test.
- Plan verifier: pass 1 FIX (9 findings, ruled and fixed), pass 2 FIX (0 Critical, 1 Important, 2 Minor, no ASK). The main session fixed pass 2 from the docs and started the build with no third pass, under the auto-mode waiver. Acceptance 1 uses reading (a): ADR-005 is excluded as a whole; it keeps its history and adds one md-view line.
- Hooks: preflight none, verify = the check plus all smoke tests (known failures `0004-vt-shell`, `000b-find-file`), status none, cleanup none.
