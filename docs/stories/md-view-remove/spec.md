---
description: The exact contract of md-view-remove - the undrawn mermaid fence, the build output, the removed files and the doc edits.
---

# Spec: md-view-remove

The base is `b51146802` on branch `feat/md-view-remove`. The story removes the Alt-F3 browser view from far2l. far2l keeps no knowledge of md-view. The tool now lives in its own repo and the owner installs it apart from far2l.

## The viewer, `far2l/src/markdown/MarkdownRender.cpp`

| Input | Result |
|---|---|
| A mermaid fence that `AsciiMermaid::Render` cannot draw (an unsupported first word such as `swimlane-beta`, or a failed render) | The fence shows as its code rows only. There is one row for each source line. Each row has the code indent, the code role and the code tint (`tint` is 0). No row follows the code rows. |
| `MarkdownDoc::Render(L"```mermaid\ngraph XX\nA\n```\n", 80)` | Two rows in `Lines()`. Before the story: three rows. Both code rows have `tint` 0 or more. |
| An undrawn fence inside a list item or a quote | The code rows keep the lead that any fenced code row has in that place. The fence does not store its own lead. |
| The `nFilePos` of the rows of an undrawn fence | The code rows keep their own offsets. No row takes the offset of the last code row. |
| A drawn fence (`graph`, `flowchart`, `stateDiagram`, `stateDiagram-v2`, `sequenceDiagram`, `classDiagram`, `erDiagram`, `xychart-beta`) | No change. Art rows replace the code rows and carry the fence offset. |
| A failed render | No change to the stderr line `AsciiMermaid: render failed: <err>`. The viewer stays open. |
| The text `diagram not drawn` and the name `MERMAID_HINT` | Not in `far2l/` and not in `testing/`. |

## Smoke test `testing/tests/0007-view-file-markdown/test.js`

| Step | Result |
|---|---|
| The swimlane fence of `sample.md` after PageDown | The row of `a1[Start] --> a2[Finish]` is on screen. The row below it holds `end`. |
| `ExpectNoString("not drawn here")` after those rows | Passes. |
| Background of the code row of the swimlane fence | Equals the background of the `Code Fence Body` cell and of column 70 in the same row. This is the code tint. |
| The same test on the binary from before the story | Fails on `ExpectNoString("not drawn here")`. The plan records this run. |

## Look driver `testing/markdown/look_driver.cpp`, case `tint`

| Step | Result |
|---|---|
| The mermaid fixture `graph XX` and `A` | `Lines().size()` is 2. Each row has `tint` 0 or more. The driver prints `ok tint: the undrawn fence rows carry the tint`. |

## Build output

| Call | Result |
|---|---|
| `cmake --build .build/out -j8` | Exits 0. `far2l.app/Contents/Resources/` holds `mermaid-ascii.js` and `mermaid-ascii.LICENSE.md`. It holds no `md-open.sh`, no `marked.min.js` and no other vendored browser file. `far2l.app/Contents/MacOS/` has no `md-open.sh`. |
| `far2l/bootstrap/CMakeLists.txt` | The scripts are copied by the one upstream line `COMMAND "cp" "${CMAKE_CURRENT_SOURCE_DIR}/*.sh" "${INSTALL_DIR}"`. There is no `md-open` filter, no `MD_OPEN_*` variable and no `install(` call for the browser files. The mermaid-ascii rules stay as they are. |

## Removed files

| Folder | Files |
|---|---|
| `far2l/bootstrap/` | `md-open.sh`, `marked.min.js`, `highlight.min.js`, `github-markdown-light.css`, `github-markdown-dark.css`, `hljs-github.min.css`, `hljs-github-dark.min.css`, `marked.LICENSE.md`, `highlight.LICENSE.md`, `github-markdown.LICENSE.md` |
| `testing/markdown/` | `browser-page.sh`, `browser-links-dom.sh`, `browser-page/` |

## Docs

| File | Result |
|---|---|
| `docs/architecture/decisions/adr-005-browser-view-script.md` | Frontmatter `status: superseded`. One new line in the body names md-view and its URL `https://github.com/slava-kudzinau/md-view` as the replacement. The source of the URL is the main-session ruling of 2026-11-12. This is the only far2l file outside `docs/stories/` that names md-view. Line 38 no longer quotes the hint text, so the grep for `diagram not drawn` has no exception. The rest of the ADR stays as history. |
| `docs/architecture/features/markdown/markdown-browser-view.spec.md` | Deleted. No file links to it. `markdown.overview.md` says this in one sentence. The HTML page view and its spec are not part of far2l. The spec is deleted, and the view moved out of far2l. The sentence names no tool. |
| ADR-004 line 21 | No `the hint row` in the body role. The edit is in the scope because the acceptance grep of `story.md` requires it. |
| `AGENTS.md`, `docs/how-we-build.md`, `docs/architecture/blueprint.md`, `docs/architecture/views/context.md`, ADR-002, ADR-006, `markdown.overview.md`, `markdown.runtime.md`, `markdown.data.md`, `markdown.invariants.md`, `markdown-viewer.spec.md` | No text about the browser view, the hint row, `md-open`, `md-view.ini`, the vendored browser files or Alt-F3 for Markdown. The `markdown-viewer.spec.md` row list says that an undrawn fence shows as code rows and nothing else. The `[mermaid]` invariant says the same. `docs/how-we-build.md` has no line about the checked-in `marked.min.js`. The "Browser view" section of `markdown.invariants.md` is gone. |
| Each changed doc | Keeps `description` frontmatter. Passes `check_ste.py`. |

## Done

`sh .build/check.sh` prints `check ok`. `LOOK_BUILD=.build/out sh testing/markdown/look-driver.sh --case tint` exits 0. The all-smoke-tests loop of `AGENTS.md` shows PASS for each test except the known failures `0004-vt-shell` and `000b-find-file`. Test 0007 is shown to fail on the old binary before the C++ change. The acceptance greps of `story.md` give the results in the Docs table. Each row above has a task in `plan.md` that covers it.

## Out of scope

- The md-view repo. Story md-view-repo made it.
- The story folders of the done browser stories. `retire-story` removes them later. Some of them link to the deleted spec. These links are history.
- The upstream Alt-F3 help text in the `.hlf.m4` files.
- A File associations entry. far2l ships none before and none after.
- The release notes of the next far2l package. The main session writes them.
