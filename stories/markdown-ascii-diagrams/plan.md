# Plan: ASCII art for diagrams in the terminal Markdown view

Spec: `spec.md` (governs exact values).

## Global Constraints

- Languages: JS (the bundle, built with esbuild through `npx esbuild@0.25.x`; Node is a build-time tool only, never needed at run time), C++ (the repo's C++ standard), POSIX `sh`.
- The bundle is built from the npm package `beautiful-mermaid@1.1.3` (MIT). Import only its ASCII entry (`src/ascii/*`, `src/parser.ts`, the class, ER and sequence parsers). Do not import `elkjs`. Keep the package's LICENSE text in `far2l/bootstrap/mermaid-ascii.LICENSE.md`. Build files live in `far2l/bootstrap/mermaid-ascii/` (`package.json`, `entry.js`, `build.sh`). The built `far2l/bootstrap/mermaid-ascii.js` is committed with `git add -f` (the root `.gitignore` matches `far2l/bootstrap` and `testing/`).
- Render options: `colorMode: 'none'`, `useAscii: false` (box-drawing characters). Strip trailing spaces from every row.
- The bundle exposes `renderAscii(src)` as in `spec.md`. A diagram type is unsupported when the package's own type detection throws for it, or the first word is not one of: `graph`, `flowchart`, `stateDiagram`, `stateDiagram-v2`, `sequenceDiagram`, `classDiagram`, `erDiagram`, `xychart-beta`.
- `AsciiMermaid` (`far2l/src/markdown/AsciiMermaid.{hpp,cpp}`) keeps one quickjs runtime for the process, loaded lazily. It copies the bounds that `MermaidEngine.cpp` already has: a 2 s wall-clock deadline per call (interrupt handler), a 1 MB JS stack, and a runtime rebuild after a timeout. It finds `mermaid-ascii.js` in the same folder where `MermaidEngine` finds `mermaid.min.js` today (same lookup, env override `FAR2L_MERMAID_ASCII_JS`). It needs no DOM shim.
- Viewer: use palette role art (`MD_ROLE_ART`) for the art rows and the existing role for code and body. Keep the cache key of fence source plus width. Keep the behavior that every spliced row carries the fence's `nFilePos`.
- The hint line text is exactly `(diagram not drawn here - Alt-F3 opens it in the browser)`.
- Files this story may touch: `far2l/bootstrap/mermaid-ascii*`, `far2l/src/markdown/` (add `AsciiMermaid.*`; delete `MermaidEngine.*`, `SvgCanvas.*`; edit `MarkdownRender.*`), `far2l/CMakeLists.txt`, the install rules for the bootstrap files, `testing/markdown/` (golden files, Node test, remove the old driver), `testing/tests/0007-view-file-markdown/`, `stories/markdown-ascii-diagrams/`. Do not touch viewer scrolling or palette code.
- Do not delete `far2l/bootstrap/mermaid.min.js` from disk (story B uses it); stop installing it and stop referencing it from `far2l/src`.
- Commit message: a short imperative summary line, a blank line, then the trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. One commit per task (a fix is its own commit).
- Build dir for this worktree: `/Users/Viachaslau_Kudzinau/Projects/far2l.ascii.build` (configure it once like `../far2l.build`; do not reuse the other build dirs).
- Rulings (settle the choices the plan left open):
  - Unsupported: the first-word list decides, checked in the bundle before the package is called. A package parse error is a parse failure, not unsupported.
  - esbuild: `npx --yes esbuild@0.25.8 --bundle --minify --format=iife --global-name=__bm --platform=neutral`; `entry.js` sets `globalThis.renderAscii`. Pin the exact versions in `package.json`.
  - Lookup: `AsciiMermaid` reads `mermaid-ascii.js` where `MermaidEngine.cpp` looks for `mermaid.min.js` today (read that code and copy the lookup, including the Resources folder in the app bundle); env `FAR2L_MERMAID_ASCII_JS` overrides.
  - Hint: ship it even before story B lands; the text names a key that story B adds.
  - Goldens: exact byte match. A small Node script `testing/markdown/golden/make-golden.mjs` writes them from `beautiful-mermaid` (not from the bundle). Samples are 4-8 line diagrams, one per type.
  - `qjs` for the golden test: use `qjs` from the repo's quickjs build at `../far2l.build/far2l/third_party/quickjs/` if it exists. If it is missing the script prints `skip qjs` and still checks Node; exit 0.
  - Error rows: unsupported is `unsupported: <type>`; parse error is the package's message on one line; timeout is `timeout`; bundle missing is `bundle missing`; other JS exceptions are `js: <message>`.
  - Timeout: an interrupt handler with a deadline (default 2000 ms, env `FAR2L_MD_DEADLINE_MS` overrides, used by the endless-loop test with a source that the driver replaces by a test script string). The deadline covers bundle load and the call. After a timeout, free the runtime and rebuild it (the bundle is read again from disk). `Render` runs on the calling thread and may block up to the deadline; a mutex guards the runtime.
  - `lines`: split on `\n`, drop trailing empty rows.
  - Driver: copy the build approach of `testing/markdown/run.sh` (stub headers, `libqjs.a` from `../far2l.ascii.build`).
  - Viewer: art rows go right after the previous block with the same spacing the old art had. The hint goes on its own row after the code block, in role body. Width: use the same width function as code lines. Art wider than the view is cut at the screen edge, like code. Only fences whose first info word is `mermaid` (case-insensitive) are handled; other fences are unchanged. The cache key stays; the cache lives in memory only, so no version bump.
  - CMake: install `mermaid-ascii.js` and its LICENSE file exactly like the bootstrap files that are installed today; find that rule and edit it. Drop `mermaid.min.js` from it.
  - Test 0007: edit `sample.md` and the expectations by hand: assert the box-drawing characters of the flowchart and the hint text on screen.
  - `check.sh` build steps: `cmake -S . -B ../far2l.ascii.build` with the same flags as `../far2l.build` (read its `CMakeCache.txt`), then `cmake --build ../far2l.ascii.build --target far2l -j8`; refresh `workdir` from `initdir` before the smoke run, as `testing/far2l-smoke-run.sh` does.
  - Commit trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>` (put in every dispatch; verify).
- Check: `sh stories/markdown-ascii-diagrams/check.sh` (written in T1, extended in T3). Owner-checked paths: `stories/*/story.md`.

## T1: The bundle and golden files

**Depends on:** none
**Done check:** `sh testing/markdown/ascii-golden.sh` exits 0
**Role:** implementer — review final
Add `far2l/bootstrap/mermaid-ascii/{package.json,entry.js,build.sh}` and the built `mermaid-ascii.js` and LICENSE file. Add samples `testing/markdown/golden/{flowchart,sequence,class,state,er,xychart}.mmd` and `bad.mmd`, `swimlane.mmd`. Generate `*.txt` goldens from the package in Node (not from the bundle). `ascii-golden.sh` runs the bundle in Node and in `qjs` (from the repo's quickjs build, if built; skip with a printed note if not) and compares to the goldens, checks the `unsupported:` and parse-error rows, the size limit and the absence of `elkjs`. Also write `stories/markdown-ascii-diagrams/check.sh` that, for now, runs `ascii-golden.sh`.

## T2: `AsciiMermaid` engine class

**Depends on:** T1
**Done check:** `sh testing/markdown/ascii-driver.sh` exits 0 (builds a small standalone driver against `libqjs.a`, as `testing/markdown/run.sh` does today, and runs every `AsciiMermaid` row of the spec: flowchart, unsupported, broken, missing bundle, endless loop, speed of the 2nd and 3rd call)
**Role:** implementer — review task-reviewer
Signature: `static bool Render(const std::string &src, std::vector<std::string> &lines, std::string &err);`. Add it to `MD_SOURCES` in `far2l/CMakeLists.txt`. The endless-loop sample is a source that makes the bundle loop (the driver may call a test-only entry in the bundle, or set a very short deadline through an env variable `FAR2L_MD_DEADLINE_MS`).

## T3: Use it in the viewer, remove the old path

**Depends on:** T2
**Done check:** `sh stories/markdown-ascii-diagrams/check.sh` exits 0 (golden test, driver test, far2l build in the build dir, smoke test 0007)
**Role:** implementer — review task-reviewer
In `MarkdownRender.cpp` replace the `MermaidEngine`/`SvgCanvas` block with `AsciiMermaid::Render`; build `MdLine` rows in role art from the returned lines (wide-character widths as the existing code does for code lines). On failure or unsupported type keep the fence text (code role) and add the hint line. Delete the old sources and the old driver, remove them from `far2l/CMakeLists.txt`, update the install rules so the bundle is installed and `mermaid.min.js` is not. Update test 0007: refresh `sample.md` expectations (flowchart shows box art; the `swimlane-beta` block shows the hint). Extend `check.sh` to build and run the smoke test (refresh `workdir` from `initdir` first; see `testing/far2l-smoke-run.sh`).
