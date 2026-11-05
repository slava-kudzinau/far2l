---
status: in-review
size: M
risk: none
---

# Story: F3 Markdown preview — fix mermaid rendering and colors

Owner brief (2026-11-05, after manual testing in macOS Terminal and ghostty):
1. Mermaid fences are not rendered as box art (major concern).
2. Colors are bad: black background behind pseudo-graphics and tables.
3. Red background everywhere in the markdown view.

Acceptance: in both terminals, `sample.md` in Markdown mode shows the mermaid swimlane as box art,
and tables/box glyphs/text use the viewer's normal background (no black or red blocks).

## Notes

- Check: `cmake --build ../far2l.build --target far2l -j8` then
  `cd testing && ./far2l-smoke -t 30 ../../far2l.build/install/far2l.app/Contents/MacOS/far2l tests/0007-view-file-markdown`
- Owner-checked paths: none stated; the handover file was merged into this story.md at the owner's word.
- Budget/hard stop: M $15/$20.
- Branch `markdown-preview/fixes` made in place (no worktree): the feature is still uncommitted on master.
- flow tier=standard docs=spec,plan design=no orchestrator=yes final=final-reviewer-light gate=read-later scan=no verifier=none
- started: 2026-11-05T17:13:37Z

---

# Original handover (context)

Original plan: `local://viewer-markdown-preview-plan.md` (steps 1–6). Date: 2026-11-05.

## Task

Add a **Markdown preview mode** to far2l's F3 viewer (toggle Shift-F4), rendering markdown natively
(headings/lists/tables/code/links/quotes via md4c) **and** fenced ```mermaid blocks as terminal box art:
`mermaid.min.js 12.3.0` runs in embedded **quickjs-ng** → SVG string → in-house SVG→cell-grid
rasterizer (`SvgCanvas`) → Unicode box glyphs. Swimlane-beta (mermaid 11.16+/12.x) is the hard case;
mermaid.js was chosen over 3rd-party ASCII renderers because it is the only one covering all diagram
types incl. swimlane.

## Chosen approach (from plan, implemented)

- Mode plumbing mirrors **hex mode** (`VM.Markdown` + `SHOW_MARKDOWN` + `ProcessMarkdownMode` +
  `VSMT_MARKDOWN`; hex wins over markdown in `DisplayObject`, markdown/text/... per plan).
- md4c event API → `MarkdownDoc` (whole-file render pass → `std::vector<MdLine>` with per-run attrs +
  `nFilePos` source offsets) + `MarkdownPrinter : ViewerPrinter` + `Viewer::ReadStringMarkdown` glue.
- Mermaid: singleton quickjs runtime, embedded DOM shim (JS string, monospace 0.6*fontSize metrics),
  `mermaid.render()` promise drained via `JS_ExecutePendingJob` loop, SVG off global; SvgCanvas
  rasterizes rects/paths/lines/text to box glyphs; results cached per (fence file_pos, width) in
  `MarkdownDoc::m_mermaid_cache`.

## What is done

1. **Vendored deps + CMake** — `far2l/src/markdown/md4c/` (md4c 0.5.2: md4c.h/.c, md4c-html.h/.c,
   LICENSE.md); `far2l/third_party/quickjs/` (quickjs-ng v0.14.0; root `CMakeLists.txt` adds
   `add_subdirectory(far2l/third_party/quickjs)`, produces `libqjs.a` in build dir, NOT in source tree);
   `far2l/CMakeLists.txt` links `qjs` + `z`, compiles `src/markdown/*.cpp`; asset
   `far2l/bootstrap/mermaid.min.js` (12.3.0, 5,537,838 B) installed to
   `Contents/Resources/mermaid.min.js` (macOS bundle) / share dir.
2. **Mode plumbing** — `farplug-wide.h` (`ViewerMode.Markdown`, `VSMT_MARKDOWN`), `viewer.hpp`
   (`SHOW_MARKDOWN`, `GetMarkdownMode`, `ProcessMarkdownMode`, `ShowMarkdown`), `viewer.cpp`
   (`InitMarkdown`, ctor restore, `KeepInitParameters`, `VCTL_SETMODE`, `DisplayObject`, `ShowPage`,
   `ProcessKey` KEY_SHIFTF4, `MCODE_V_VIEWERSTATE` bit 0x40, poscache Param[3], color gate),
   menu (`fileview2options.cpp` `ViewerMenuMarkdown`) + keybar (`ChangeViewKeyBar` ~:2795-2709:
   markdown→F4 "Mark"/Shift→"Text"; text/hex→Shift→"Mark") + `qview.cpp` mirror, lang
   (`farlang.templ.m4`: `ViewF4Markdown` = "Mark", `ViewerMenuMarkdown`, `SingleView*`), global
   persistence `ViewerOptions::ViewerMarkdownMode` ("Viewer"/"Markdown", `ConfigOpt.cpp`, config.cpp).
3. **Markdown render** — `far2l/src/markdown/MarkdownRender.{hpp,cpp}` (MarkdownDoc, md4c callbacks,
   tables via BoxSymbols, fences incl. mermaid placeholder + splice), `MarkdownPrinter` in
   `ViewerPrinter.{hpp,cpp}`, `Viewer::ReadStringMarkdown` in viewer.cpp. Colors
   (`MarkdownRender.cpp:26-34`): H1 `F_WHITE`, H2-3 `F_LIGHTCYAN`, H4-6 `F_LIGHTMAGENTA`, strong
   `F_LIGHTCYAN`, emphasis `F_LIGHTGREEN`, **code `F_LIGHTGRAY`** (was F_LIGHTRED — user feedback
   "terrible red"), link `F_CYAN`, del `F_DARKGRAY`, quote `F_LIGHTGRAY`.
4. **MermaidEngine** — `far2l/src/markdown/MermaidEngine.{hpp,cpp}` (quickjs + DOM shim + render glue).
   Diagnostics added: `fprintf(stderr, "MermaidEngine: asset=...")` etc. Asset lookup now
   **exe-relative** (`_NSGetExecutablePath` / `/proc/self/exe` → `../Resources/`, `../share/far2l/`,
   exe dir; then /usr/... ; then cwd).
5. **SvgCanvas** — `far2l/src/markdown/SvgCanvas.{hpp,cpp}` (XML subset parse, transforms, Bresenham
   box-glyph merge, text-over-grid, 8×16 px/cell).
6. **Test 0007** — `testing/tests/0007-view-file-markdown/{test.js,initdir/left/sample.md}`
   (sample.md: H1, emphasis/strong/code span, bullets, numbered list, 2-col table, ```cpp fence
   "Code Fence Body", ```mermaid `swimlane-alpha` with Start/Finish, blockquote).

Build: `cd /Users/Viachaslau_Kudzinau/Projects/far2l && cmake --build ../far2l.build --target far2l -j8`
(build dir `/Users/Viachaslau_Kudzinau/Projects/far2l.build`, configured `-DTESTING=Yes`; last build OK,
binary `../far2l.build/install/far2l.app/Contents/MacOS/far2l`, 13.4 MB).

## Current state / open issues (pick up here)

- **User-visible status (latest, ghostty + macOS Terminal):** markdown renders (headings cyan, code
  spans gray on latest binary; screenshots confirmed headings/lists/tables), Shift-F4 keybar hint OK.
  **Mermaid fences render as raw text, no box art** — open issue #1: verify engine init. Run the binary
  with `FAR2L_STD=/tmp/f.log` (stderr capture; e.g. `... --test=... --tty --nodetect --mortal`), open
  `sample.md` F3, Shift-F4, scroll to the ```mermaid fence, then
  `grep Mermaid /tmp/f.log`:
  - `asset=` empty → install/bundle path (check `Contents/Resources/mermaid.min.js` exists).
  - `mermaid.min.js eval failed: ...` → extend DOM shim in `MermaidEngine.cpp` (`kDomShim`: node API,
    `getBoundingClientRect`, `getComputedTextLength`, `classList`, `style`, DOMPurify walk);
    contingencies from plan: sync `setTimeout=fn=>{fn();return 0}`, drop `htmlLabels`.
  - `asset=` ok + no eval error, still raw → `Render` path: check `__far2l_done`/`__far2l_err` glue
    (`kRenderGlue`), microtask drain loop, then `SvgCanvas::Parse` (log svg size).
- **Issue #2 (minor):** "also terrible red color" — code spans switched to `F_LIGHTGRAY`; needs a
  visual confirm in ghostty after the exe-relative rebuild (user last tested a pre-rebuild binary).
- **Issue #3 (test infra):** `testing/far2l-smoke` (Go) intermittently `fork/exec ...: operation not
  permitted` (EPERM) on macOS 26.2 (pty pool ~76/128 used: zsh 33, omp 14, 2.1.318 14). Suite run:
  `cd testing && ./far2l-smoke -t 30 ../../far2l.build/install/far2l.app/Contents/MacOS/far2l tests/0007-view-file-markdown`
  (path MUST include `tests/` prefix; wrapper: `./far2l-smoke-run.sh <far2l> [N]`). 0005/0006/0007
  all pass when spawn succeeds.
- **Python harness (works, use for E2E):** AF_UNIX DGRAM bound e.g. `/tmp/py.sock`, spawn
  `far2l --test=/tmp/py.sock --tty --nodetect --mortal -u <workdir>/profile -cd <left> -cd <right>`
  with stdin/out/err = pty slave (size via `TIOCSWINSZ` on slave, 120x36), env
  `FAR2L_TESTCTL=<sock>` `FAR2L_STD=<logfile>`. Protocol (`WinPort/src/Backend/TestProtocol.h`):
  first 2068-B frame's addr = peer (far2l's ipc_client); cmds: 1 STATUS (2068: u16 ver, u8 h, u8 vis,
  u32 cur_x, cur_y, w, h, title[2048]), 2 READ_CELL (12-B req u32 cmd,x,y; 2056-B reply: u64 attrs,
  str[2048]), 3 WAIT_STRING (24+2048: cmd,timeout,left,top,width,height; str = double-NULL list; 12-B
  reply i,x,y), 5 SEND_KEY (24-B: cmd,controls,chr,key_code,scan_code,pressed+3), 6 SYNC (8-B req,
  1-B reply). Keys via pty master (`\x1b` ESC closes help; `\x1bOS` F3; `\x1b[1;2S` Shift-F4;
  `\x1b[1;B` F1). Drain master after each key; WAIT_STRING region = status w,h. Proven: panel shows
  `sample.md` at (2,3).
- **Unverified in harness:** F3 opening viewer via pty bytes (`\x1bOS`) returned -1 for
  `Code Fence Body` in last runs (title stayed panel) — check ESC-sequencing/timing, then re-run.

## Key anchors

- `far2l/src/viewer.cpp`: `ShowPage` SHOW_MARKDOWN, `ReadStringMarkdown`, printer-choose
  (MarkdownPrinter), `ChangeViewKeyBar` (~:2795), `ProcessMarkdownMode`, `ProcessKey` KEY_SHIFTF4.
- `far2l/src/markdown/MarkdownRender.cpp`: constants :26-34; `MarkdownDoc::Render` :493-563
  (mermaid fences + cache + splice :536-562).
- `far2l/src/markdown/MermaidEngine.cpp`: `kDomShim`, `kRenderGlue`, `locate_asset` (exe-relative),
  `init` (stderr logs), `Render` (escape+`__far2l_render`, microtask drain, `__far2l_svg`/`__far2l_err`).
- `far2l/src/markdown/SvgCanvas.{hpp,cpp}`: Parse/ToCells (8×16 px/cell, box-glyph joins, text wins).

## Next steps (in order)

1. Run binary with `FAR2L_STD=/tmp/f.log`; grep `Mermaid` → fix init/eval/shim per symptom above.
2. Confirm in ghostty: code spans light-gray; mermaid fence → box art (swimlane).
3. `testing/far2l-smoke ... tests/0007-view-file-markdown` (retry until EPERM clears); extend 0007
   asserts: `Heading` (no `#`), `Code Fence Body`, mermaid `Start`/`Finish` labels + `┌` after F1 scroll.
4. Changelog + man note for markdown mode.
