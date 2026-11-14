---
description: Three tasks that drop the hint row and fix its tests, delete the browser view files and CMake rules, and clean the docs.
---

# Plan: md-view-remove

Spec: `spec.md` (governs exact values). No `design.md`: the story has three tasks and no new design.

## Global Constraints

- Work only in the worktree `~/Projects/far2l/.worktrees/md-view-remove`, on branch `feat/md-view-remove`. Do not switch branch. Do not push. Do not touch `main` or any `next/*` branch.
- Build folder: `.build/out`. Binary: `.build/out/install/far2l.app/Contents/MacOS/far2l`. Do not create another build folder. The story Notes set this folder, and `.build/check.sh` uses it. The text in `AGENTS.md` that says `.build` as the build folder does not apply here.
- Line numbers in this plan are the line numbers of the files at the base `b51146802`, before any edit. Find a line by its text, or edit from the bottom of a file to the top.
- T1 changes only `far2l/src/markdown/MarkdownRender.cpp`, `far2l/src/markdown/MarkdownRender.hpp` (two comments), `testing/tests/0007-view-file-markdown/test.js` and `testing/markdown/look_driver.cpp`. T2 changes only the files in the spec table "Removed files" and `far2l/bootstrap/CMakeLists.txt`. T3 changes only `AGENTS.md` and `docs/`, except `docs/stories/`.
- Never edit `testing/markdown/golden/*.txt`, `far2l/src/markdown/md4c/`, `far2l/third_party/quickjs/` or `.github/workflows/`. The upstream Alt-F3 text in `far2l/bootstrap/scripts/*.hlf.m4` stays.
- Exact strings. Removed hint: `(diagram not drawn here - Alt-F3 opens it in the browser)`. Removed name: `MERMAID_HINT`. New test text: `ExpectNoString("not drawn here")`. ADR-005 status: `superseded`. The replacement line in ADR-005 names `md-view` and the URL `https://github.com/slava-kudzinau/md-view`. No other file outside `docs/stories/` names md-view.
- Source of the URL and of the rule that far2l does not know md-view (main-session ruling, 2026-11-12): md-view is public at `https://github.com/slava-kudzinau/md-view`. The owner ruled that far2l has no knowledge of md-view. The one line in ADR-005 is the only far2l mention that the acceptance grep allows outside `docs/stories/`.
- Wording: where this plan gives exact text, use that text. Elsewhere the plan gives the meaning, and the implementer writes short new wording. The reviewer checks the meaning, not the words. Docs follow the STE rules.
- Alt-F3: after T3, no doc outside `docs/stories/` and ADR-005 names Alt-F3. The grep today finds only uses for the browser view in docs, so no doc needs the upstream key. If the drafter finds another use, it stops and reports.
- Rules of `docs/architecture/features/markdown/markdown.invariants.md` that apply (the file is the source of each rule):
  - `[mermaid]`: a fence that cannot be drawn keeps its source as code rows and never stops the viewer. T1 keeps this and drops only the hint row. Render limits (deadline, stack, memory) stay as they are.
  - `[scroll]`: the `nFilePos` of the rows never decreases down the document. The code rows of an undrawn fence keep their own offsets.
  - `[layout]`: fences and art rows keep one row for each row and scroll. Wrap does not split them.
  - `[colors]`: a code row paints the block tint as its background. A run holds a role, never a color. T1 adds no color value.
  - `AGENTS.md` Tests: run a test that must catch a regression against that regression before you trust it. Do not regenerate golden files.
- Style: follow `CODESTYLE.md`. Above a function or a struct, a short comment says what it does. When you change code, change its comment. Docs use ASD-STE100: run `python3 ~/Projects/fde-kit/plugins/fde-delivery/kb/scripts/check_ste.py <file>` on each changed doc and fix what it lists. The script exits 0 even when it lists findings, so read its output.
- Owner-checked paths (`AGENTS.md`): `AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`, `testing/markdown/golden/*.txt`, vendored code and `.github/workflows/`. Main-session ruling, 2026-11-12: the approved story scope is the owner's approval to edit the doc files that the Docs row of `story.md` names. The ruling also covers ADR-004 line 21 (`the hint row`), because the acceptance grep of `story.md` requires that edit. Only T3 edits owner-checked paths, and only these doc files. No task edits `testing/markdown/golden/*.txt`.
- Commit message: `type(scope): subject` in the conventional form, a blank line, then the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not use the name of your own model in it, and do not use the attribution line of the session. The trailer above is the main-session value. One commit per task, on branch `feat/md-view-remove` only. No push.
- Check: `sh .build/check.sh` from the worktree root. It prints `check ok` and exits 0. It does not run the look driver, so T1 runs that too.
- Verify hook: the main session runs it after the last task. It runs the check. Then it runs the all-smoke-tests loop of `AGENTS.md`, with `APP` set to the binary above and a socket clean before each test. The known failures are `0004-vt-shell` and `000b-find-file`. Any other failure is new.

## What the scout found (facts for the tasks)

Hits of `git grep -n -i -E "md-open|md-view\.ini|browser view|MERMAID_HINT|diagram not drawn"` outside `docs/stories/`, and who covers them:

| Place | Covered by |
|---|---|
| `far2l/bootstrap/CMakeLists.txt` lines 71-79 (the glob, the `md-open` filter and the loop), 83 (`${BOOTSTRAP_COPIED_SCRIPTS}`), 105-126 (the "Markdown in the browser" block, to `DESTINATION ... ${MD_OPEN_OPTIONS})` on line 126) | T2. The brief says lines 105-125. The block ends on line 126. |
| `far2l/bootstrap/md-open.sh` (43 hits), `testing/markdown/browser-page.sh` (26), `testing/markdown/browser-links-dom.sh` (8), the license files of `marked` and `highlight` | T2, delete |
| `far2l/src/markdown/MarkdownRender.cpp:31` (`MERMAID_HINT`) and `:1329` (the only use) | T1 |
| `testing/tests/0007-view-file-markdown/test.js:165` | T1 |
| `docs/architecture/**`, `AGENTS.md`, `docs/how-we-build.md` (about 60 hits in 17 files) | T3 |
| `docs/stories/**` (for example `index.md:63`, `markdown-ascii-diagrams/story.md:16`) | Out. They are story history. `retire-story` removes them. |

Hits that the brief did not list, found by the scout:

- `testing/markdown/look_driver.cpp:1618-1634` (case `tint`) checks that an undrawn mermaid fence makes three rows, "two code rows and the hint". It has no hint text, so the grep misses it. The check in `AGENTS.md` does not run it. The case passes today with exit 0 (run in `.build/out`). After the hint row goes, it must say two rows. T1 covers it. The brief note "no golden file or driver has it" is wrong for the row count.
- `far2l/src/markdown/MarkdownRender.cpp:192-199` (the `Fence::lead` member and its comment), `:778` (`Builder::Fence{... b.row_lead()}`), `:750-751` (a comment about the hint row) and `far2l/src/markdown/MarkdownRender.hpp:59-61` (a comment about the hint row). `lead` is read only at `:1329`. After T1 it is dead, so T1 removes it. This makes the code smaller.
- Docs that the brief did not name: ADR-004 line 21 (`the hint row` in the body role). Also `markdown-viewer.spec.md` (about 13 hits) and `AGENTS.md` lines 3, 81, 82 and 93. T3 covers them.
- `docs/architecture/views/containers.md:82` says "file structure browser" for `inside`. It is not the browser view, so no task changes it.

Hits of the second grep (`marked.min`, `highlight.min`, `github-markdown`, `hljs-github`, `browser-page`, `browser-links`, `Alt-F3`, `Alt+F3`) outside `docs/stories/`:

- The 10 vendored files and the two test scripts: T2 deletes them.
- `far2l/bootstrap/CMakeLists.txt`: T2.
- `docs/architecture/**`: T3. ADR-005 keeps its text as history.
- `far2l/bootstrap/scripts/*.hlf.m4`, `far2l/bootstrap/scripts/farlang.templ.m4`, `far2l/src/filetype.hpp:42` and `far2l/src/panels/filelist.cpp:1518` name Alt-F3 as the upstream key for the viewer association. They do not name the browser view. Out.
- `highlight_driver.cpp` and `highlight-driver.sh` in `testing/markdown/` match `highlight` only. They test the Colorer fence tokens, not `highlight.min.js`. They stay.

Packaging and install rules that name the deleted files: none. `git grep` over `packaging/`, `.github/`, the root `CMakeLists.txt`, every `*.cmake` and every other `CMakeLists.txt` finds `md-open` and `marked` only in `far2l/bootstrap/CMakeLists.txt`. The two other `marked` hits are license text in `packaging/debian/copyright/licenses/`. The install rules sit in `far2l/bootstrap/CMakeLists.txt` lines 105-126, and T2 deletes them. `.build/out/install` holds no `md-open.sh` or `marked.min.js` today, because those rules are `EXCLUDE_FROM_ALL` on macOS.

## T1: Drop the hint row, and fix test 0007 and the look driver

**Depends on:** none
**Done check:** `sh .build/check.sh && LOOK_BUILD=.build/out sh testing/markdown/look-driver.sh --case tint && test -z "$(git grep -n -E 'MERMAID_HINT|diagram not drawn here|row_lead\(\)\}' -- far2l testing)" && test -z "$(git grep -n -i 'hint row' -- far2l/src/markdown testing/markdown testing/tests/0007-view-file-markdown)"` exits 0
**Fails before the task:** yes. The last `git grep` finds `MERMAID_HINT` and `row_lead()}` today. Test 0007 with its new text also fails on the old binary (step 2).
**Role:** implementer — review task-reviewer-critical

This task changes the viewer contract: an undrawn fence has no extra row. Do the steps in this order. The binary in `.build/out` is from the old source until step 4.

1. In `testing/tests/0007-view-file-markdown/test.js`, replace lines 163-171. The file `testing/tests/0007-view-file-markdown/initdir/left/sample.md` (lines 30-34) holds the swimlane fence: `swimlane-beta`, `subgraph Alpha`, `a1[Start] --> a2[Finish]`, `end`. Edit the test only. The `workdir` copy is generated. `code_cell` is `ReadCell(code.X, code.Y)` at line 127 of the same test, and it stays valid after the page change. Today they find `hint` with `ExpectString("(diagram not drawn here - Alt-F3 opens it in the browser)")`, require `hint.Y == swim_last.Y + 2`, and call `CheckForeSame("diagram hint", ..., body_cell, "body")`. Keep `var swim_last = ExpectString("a1[Start] --> a2[Finish]")`. After it assert:
   - `ExpectString("end", 0, swim_last.Y + 1, 80, 1)`: the row below is the last code row.
   - `ExpectNoString("not drawn here")`.
   - `CheckSameBack("undrawn fence code row", ReadCell(swim_last.X, swim_last.Y), code_cell, "code")` and the same with `ReadCell(70, swim_last.Y)`. The rows of an undrawn fence keep the code tint.
   - Keep `TypeEnd()` and the lines after it. Change the comment on line 163 to say that the code of the undrawn swimlane block shows and nothing follows it.
2. Run the new test on the old binary. Do not build first. Clean the sockets, then run:
   `APP=$(pwd)/.build/out/install/far2l.app/Contents/MacOS/far2l; (cd testing && pkill -9 -f -- '--test=/tmp/far2l'; sleep 1; find tests -maxdepth 1 -type s -delete; for s in tests/*; do [ -S "$s" ] && rm -f "$s"; done; ./far2l-smoke-run.sh "$APP" 0007-view-file-markdown 2>&1 | tail -20)`
   The output must show `FAILED` for `ExpectNoString("not drawn here")` and no `[DONE]`. Put the last lines of the output in the task report. If the test passes on the old binary, stop and report it. The test is then wrong.
3. In `far2l/src/markdown/MarkdownRender.cpp`:
   - Delete `MERMAID_HINT` (line 31) and the comment above it (line 30).
   - In the splice loop (about lines 1322-1334), the `if (art.empty())` branch becomes `if (art.empty()) continue;` with a one-line comment: the code rows stay as they are. Delete the `MdLine hint` block, its `nFilePos` and `tint` lines and the `insert`.
   - Delete the `lead` member of `Builder::Fence` (line 199) and its mention in the comment on lines 192-193. Delete the last argument `b.row_lead()` of `Builder::Fence{...}` at line 778. `row_lead()` itself stays: other code calls it.
   - Change the comment at lines 750-751. It says that `Render()` replaces the fence body with the box art. It keeps the body when the fence is not drawn.
   - Keep the stderr line `AsciiMermaid: render failed`.
   - After the change the loop body ends at the `continue` for an undrawn fence. The rest of the body is the `erase` and `insert` of the art rows. It is for a drawn fence only. No later code needs the undrawn case.
4. In `far2l/src/markdown/MarkdownRender.hpp`, change the comment on lines 59-61: the fence builder sets `tint` on the fence code rows. Remove the words about the hint row. Also change the `MarkdownDoc` class comment (lines 76-77): a fence it cannot draw keeps its code rows. Remove "and gets a hint row under them".
   - Every `hint row` mention in `far2l/src/markdown`, `testing/markdown` and test 0007 goes in T1. At the base, `MarkdownRender.cpp` has them on lines 193, 750 and 1326, and `MarkdownRender.hpp` on lines 58 and 77. `look_driver.cpp` has them on lines 1567, 1618 and 1634, and `test.js` on lines 163 and 170. In `MarkdownRender.cpp` line 193, remove "or add the hint row under the code". In `look_driver.cpp` line 1567, remove "and the hint row" from the comment.
5. In `testing/markdown/look_driver.cpp`, case `tint` (lines 1618-1634), make three changes.
   - The comment says that the code rows of an undrawn mermaid fence carry the tint.
   - The size check becomes `lines.size() != 2`. Its message says "want the two code rows".
   - The final `printf` says `ok tint: the undrawn fence rows carry the tint`.
6. Build with `cmake --build .build/out --target far2l -j8`, then run the done check. Rebuild only through the check or this command.
7. Commit: `refactor(markdown): drop the hint row under an undrawn mermaid fence`.

## T2: Delete the browser view files and the CMake rules

**Depends on:** none
**Done check:** `cmake --build .build/out -j8 && R=.build/out/install/far2l.app/Contents && test -f $R/Resources/mermaid-ascii.js && test -f $R/Resources/mermaid-ascii.LICENSE.md && test ! -e $R/Resources/md-open.sh && test ! -e $R/Resources/marked.min.js && test ! -e $R/MacOS/md-open.sh && test -z "$(git ls-files far2l testing | grep -E 'md-open|marked|highlight\.|github-markdown|hljs|browser-')" && test -z "$(git grep -n -i -E 'md-open|marked\.min|highlight\.min|browser-page|browser-links' -- far2l testing)" && test -z "$(git grep -n -E 'MD_OPEN_|BOOTSTRAP_COPIED' -- far2l testing)"` exits 0
**Fails before the task:** yes. The `git ls-files` and `git grep` parts find the 13 paths and the CMake text today. On macOS the four `test ! -e` parts pass before the task: the old install rules are `EXCLUDE_FROM_ALL`, so a plain build never installs those files. Those parts only guard against a new copy rule.
**Role:** implementer — review task-reviewer-critical

The review is critical because the task removes installed files that a File associations entry of a user can call (`share/far2l/md-open.sh` on Linux).

1. `git rm` these 13 paths. The spec table "Removed files" is the source.
   - In `far2l/bootstrap/`: `md-open.sh`, `marked.min.js`, `highlight.min.js`, `github-markdown-light.css`, `github-markdown-dark.css`, `hljs-github.min.css` and `hljs-github-dark.min.css`.
   - In `far2l/bootstrap/`: `marked.LICENSE.md`, `highlight.LICENSE.md` and `github-markdown.LICENSE.md`.
   - In `testing/markdown/`: `browser-page.sh`, `browser-links-dom.sh` and the folder `browser-page/` (one file, `sample.md`).
2. In `far2l/bootstrap/CMakeLists.txt`:
   - Delete lines 71-79 (the comment, `file(GLOB BOOTSTRAP_SCRIPTS ...)`, `set(BOOTSTRAP_COPIED_SCRIPTS ...)` and the `foreach` loop) and the blank line after them.
   - The block that holds line 83 then reads as below. The `*.vbs` and `*.ini` lines do not change. It is the upstream text (`git show upstream/master:far2l/bootstrap/CMakeLists.txt`).
     ```
     add_custom_command(TARGET bootstrap
         POST_BUILD
         COMMAND "cp" "${CMAKE_CURRENT_SOURCE_DIR}/*.sh" "${INSTALL_DIR}"
         COMMAND "cp" "${CMAKE_CURRENT_SOURCE_DIR}/*.vbs" "${INSTALL_DIR}"
         COMMAND "cp" "${CMAKE_CURRENT_SOURCE_DIR}/*.ini" "${INSTALL_DIR}"
     )
     ```
   - Delete the "Markdown in the browser" block (original lines 105-126) and the blank line before it. Keep the mermaid-ascii rules (original lines 88-103). Those rules are `POST_BUILD` of the target `bootstrap`, not install rules, so a plain build runs them. `Resources/mermaid-ascii.js` exists in `.build/out` today.
   - Do not add a line for any other file.
3. `cmake --build .build/out -j8` runs configure again because the CMake file changed. It must exit 0.
4. Commit: `chore(bootstrap): remove the Markdown browser view files and install rules`.

## T3: Docs

**Depends on:** T1, T2
**Done check:** `test -z "$(git grep -n -i -E 'md-open|md-view\.ini|browser view|MERMAID_HINT|diagram not drawn|marked\.min|hljs|Alt-F3|hint row' -- AGENTS.md docs ':!docs/stories/' ':!docs/architecture/decisions/adr-005-browser-view-script.md')" && test -z "$(git grep -n -E 'MERMAID_HINT|diagram not drawn' -- . ':!docs/stories/')" && test "$(git grep -l -i 'md-view' -- . ':!docs/stories/')" = docs/architecture/decisions/adr-005-browser-view-script.md && grep -q '^status: superseded$' docs/architecture/decisions/adr-005-browser-view-script.md && test ! -e docs/architecture/features/markdown/markdown-browser-view.spec.md && test -z "$(git grep -n 'markdown-browser-view' -- . ':!docs/stories/')"` exits 0
**Fails before the task:** yes. The first `git grep` finds about 50 hits today, and the status line and the spec file are not changed.
**Role:** drafter — review final

This task edits owner-checked paths under the ruling in Global Constraints. Edit only the files below. Write in ASD-STE100. Keep each file's frontmatter, and change a `description` only where this plan gives new text. A frontmatter value has no `: ` and no ` #`.

1. `docs/architecture/decisions/adr-005-browser-view-script.md`:
   - Set `status: superseded`.
   - Add one line under the title: `Superseded. The browser view moved out of far2l. md-view replaces it: https://github.com/slava-kudzinau/md-view.`
   - Line 38: replace the quoted hint with `The terminal hint row pointed at this key. It did nothing until the owner added the association.` The second acceptance grep (`diagram not drawn`) then has no exception.
   - Change nothing else in the ADR. The first grep still excludes this file.
2. `docs/architecture/features/markdown/markdown-browser-view.spec.md`: `git rm` it.
   - Reason: the acceptance grep finds 22 hits in it, and the contract now belongs to md-view.
   - Remove it from each `refs:` list, table and link.
   - These are `AGENTS.md` line 81, `markdown-viewer.spec.md` line 7 and `markdown.data.md` line 6.
   - Also `markdown.overview.md` lines 7 and 26, and `markdown.runtime.md` line 6.
3. `AGENTS.md`:
   - Line 3: drop the sentence about the HTML view in an external browser.
   - Line 81: delete the row.
   - Line 82: remove `open in browser` from the runtime row.
   - Line 93: the ADR-005 row says `Superseded. The Markdown page for the browser moved out of far2l.` It does not name the tool.
   - Do not name md-view in `AGENTS.md`.
4. `docs/how-we-build.md`:
   - Line 94: the sentence about the `md-open.sh` test.
   - Line 119: `marked.min.js and its license are checked in`. Delete the line.
   - Line 136: the browser check of story markdown-browser-view.
   - Line 137: the `md-open.sh` checks.
   - Keep line 126. It is story history and has no hit.
5. `docs/architecture/blueprint.md` line 20 and `docs/architecture/views/context.md` lines 21 and 33: remove the browser view and `md-open.sh`.
   - Line 21 becomes `clipboard, notify, sudo`. Line 33 loses `the system browser` and the `md-open.sh` sentence.
   - far2l opens no web browser itself, so the word `browser` goes from the list of external programs.
6. Decisions:
   - ADR-002 line 25: delete it.
   - ADR-004 line 21: remove `the hint row`.
   - ADR-006 `description` (line 2): `The terminal Markdown view draws six mermaid types as box art with a committed JS bundle in the vendored quickjs-ng, and shows other types as code.`
   - ADR-006 line 13: delete it. Line 21: `Any other type, or a failed render, shows the fence as a code block.`
   - ADR-006 line 32 (`%%` goes to the hint): `A fence that starts with %% shows as a code block.`
7. `markdown.overview.md`:
   - In the sentence on line 14, change `adds Markdown in two places` to `adds Markdown to the F3 viewer`.
   - Replace the browser bullet (line 17) with this sentence: `The HTML page view and its spec are not part of far2l. The spec is deleted, and the view moved out of far2l.` It names no tool.
   - Delete the table row on line 26. Keep the row on line 47 (`far2l/bootstrap/` files), and name only `mermaid-ascii.js` in it. On line 27 remove `open in browser`.
   - Remove the strategy bullets on lines 37 (`Other types go to the browser`) and 38.
   - Line 53: keep the first sentence (quickjs with no `std`, `os` or DOM). Delete the sentence about the browser page.
   - Line 56: keep "the terminal view uses no network". Delete the CDN sentence.
   - Delete the risk lines 61, 65 and 67.
8. `markdown.runtime.md`:
   - `description` (line 2): `Runtime view - open a Markdown file in F3, render and scroll, and draw one mermaid fence as ASCII art.`
   - Line 13: remove the sentence about the browser flow, and change the flow count.
   - Line 119 `code rows stay, hint row follows` becomes `code rows stay`.
   - Delete the section "Open in the browser", from line 130 to its last table (line 160 is in it).
9. `markdown.data.md`:
   - `description` (line 2): `Data view - screen lines, color runs, source offsets, the position cache and settings keys.`
   - Line 12: end the sentence after `one defaults file`. Drop `and one page file for each browser view`.
   - Diagram: delete the five nodes on lines 25-29 (`file --> page`, `marked`, `vend`, `ini`, `cdn`).
   - Delete the rows for `md-view.ini` and `localStorage` (lines 65 and 66) and the sentence on line 68.
   - Delete the whole section "Generated files" (lines 70-77), because all its rows are for the page.
   - Delete the bundled rows for `marked`, `github-markdown`, `highlight`, `hljs` and the mermaid CDN (lines 86-90) and line 93.
   - Delete the environment rows `MARKED_JS`, `XDG_CACHE_HOME`, `XDG_CONFIG_HOME`, `BROWSER` and `TMPDIR`. Before you delete one, run `git grep -n <NAME> -- far2l/src` and keep the row if code reads it.
   - Line 46: delete the sentence `The hint row takes the offset of the last code row.`
10. `markdown.invariants.md`:
   - Delete the section "Browser view" (line 50 to the end).
   - Line 36 (`[mermaid]`): `A fence that cannot be drawn keeps its source as code rows, with no extra row. It never stops the viewer.` Keep the `Learned from` part.
   - `description` (line 2): `Rules that always hold for the Markdown feature - viewer and diagrams - each with where it was learned.`
11. `markdown-viewer.spec.md`:
   - Remove the browser spec from `refs` (line 7).
   - Line 23: remove `the browser view is the fallback for that fence`.
   - Lines 131 and 168: remove the hint row from the body and tint rows.
   - Line 210 (step 4): `If the call fails, the code rows stay and no row follows them.`
   - Lines 217 and 218: delete the `Hint row` row. In `Art lead` drop `The hint row keeps its lead.`
   - Line 241: `shows the code rows`, and nothing else.
   - Line 288: `Art and the undrawn fence in the viewer`.
   - Lines 301 and 319: remove the hint. The `%%` row says that the fence shows as code rows.
   - Lines 331 and 336: delete the defect rows about the BOM in the browser view and the Alt-F3 hint.
12. Run `check_ste.py` on each changed doc and fix the findings. Run the done check.
13. Commit: `docs(markdown): remove the browser view from the architecture docs`.
