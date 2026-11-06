# Plan: Syntax colors in fenced code blocks

Spec: `spec.md` (governs exact values). No design doc: the design is here, in the interfaces and the decisions behind them.

Read before editing: `docs/architecture/features/markdown/markdown.invariants.md`, `docs/architecture/decisions/adr-004-palette-roles.md`, `docs/architecture/features/markdown/markdown.data.md`.

## Global Constraints

- C++, no new third-party dependency. The core reaches Colorer only through the new plugin export and `dlfcn`. The core never links the Colorer library or libxml2.
- Files this story may touch: `plugins/colorer/src/pcolorer2/ColorerExport.cpp` (new) and `plugins/colorer/CMakeLists.txt`, `far2l/src/markdown/MdColorer.hpp` and `.cpp` (new), `far2l/src/markdown/MarkdownRender.hpp` and `.cpp`, `far2l/src/markdown/MdPalette.cpp` (the fork table, and `MdPalette.hpp` gains the helper of the constraint above), `far2l/src/ViewerPrinter.cpp`, `far2l/CMakeLists.txt` (one driver target line), `testing/markdown/highlight-driver.cpp` and `.sh` (new), `testing/tests/000c-view-md-highlight/` (new), and the docs named in T6.
- Keep the fork's changes to upstream files small, so a merge from `upstream` stays cheap. New code goes in `far2l/src/markdown/` where it can. `far2l/src/plug/` does not change.
- A run carries an `MdRole`, never a color (invariant `[colors]`, ADR-004). Pass a 64-bit real color to `FarPrint::SetColor`; the 24-bit attribute color is `0xBBGGRR`, a written color is `0xRRGGBB`, and the low attribute bits of an RGB color hold a 16-color index.
- Markdown is untrusted input: every limit in `spec.md` is a hard bound, and a failed parse leaves the code role, never a hang.
- Colorer parsing runs on the main thread, on the plugin's own `FarEditorSet`. `cregexp.h:44` warns "Do not parse concurrently".
- Export signature, exact:
  `extern "C" int ColorerTokenizeV1(const wchar_t *lang, const wchar_t *const *lines, int count, void (*emit)(void *ctx, int line, int start, int end, int token_class), void *ctx, int deadline_ms);`
  Return codes: 0 all lines emitted, 1 stopped at the deadline, -1 no `FarEditorSet`, -2 unknown language, -3 a Colorer `Exception`. Token classes: keyword 1, string 2, comment 3, number 4.
- Core bridge, exact (in `far2l/src/markdown/MdColorer.hpp`):
  `struct MdToken { uint32_t start; uint32_t end; uint8_t cls; };` // half-open, in cells of the code run's `MdRun::text`, the 4-cell code indent counted
  `class MdTokenSource { public: virtual ~MdTokenSource() = default; virtual bool Tokenize(const std::string &lang, const std::vector<std::string> &lines, int budget_ms, std::vector<MdToken> &out) = 0; };`
  `MdTokenSource &MdGetTokenSource();` and `void MdSetTokenSourceForTest(std::unique_ptr<MdTokenSource> src);`
- Token class to role, exact (in `far2l/src/markdown/MdPalette.hpp`, body in `MdPalette.cpp`): `MdRole MdRoleForTokenClass(int token_class);` maps 1, 2, 3, 4 to the four token roles, and anything else to `MD_ROLE_CODE`. The printer calls it; all color logic stays in `far2l/src/markdown/`.
- Colorer is the syntax-coloring plugin that ships with far2l (`plugins/colorer`), the same parser that the F4 editor uses. Its `ParserFactory` owns the language schemes (the `.scheme` files) and creates the text parser.
- Commit message: a short imperative summary line, a blank line, then the trailer `Co-Authored-By: Claude <noreply@anthropic.com>`. One commit per task.
- Check: `BUILD=../far2l.markdown-code-highlight.build`, then "The check" of `AGENTS.md` with `TESTS="0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off 000c-view-md-highlight"`. The new test needs the plugins, so a task that touches the plugin also runs `cmake --build ../far2l.markdown-code-highlight.build -j8`. Owner-checked paths: `AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`, `testing/markdown/golden/*.txt`, `far2l/src/markdown/md4c/`, `far2l/third_party/quickjs/`, `.github/workflows/`.

Facts the plan relies on, all read in this tree:

- The Colorer plugin exports `PluginModuleOpen`, `SetStartupInfoW`, `GetPluginInfoW`, `ExitFARW`, `OpenPluginW`, `ConfigureW`, `ProcessEditorEventW`, `ProcessViewerEventW`, `ProcessEditorInputW`, `ProcessSynchroEventW`, all `SHAREDSYMBOL` (`extern "C"` with default visibility, `WinPort/WinCompat.h:35`), in `plugins/colorer/src/pcolorer2/pcolorer.cpp:17-199`. A new export follows the same form.
- far2l opens a plugin with `dlopen(path, RTLD_NOW)` (`far2l/src/plug/plugins.cpp:92-106`), so the module is loaded. `Plugin::GetModuleName()` (`far2l/src/plug/plclass.hpp:155`, `PluginW.hpp:177`) returns the full absolute module path. On macOS that is `../far2l.markdown-code-highlight.build/install/far2l.app/Contents/MacOS/Plugins/colorer/plug/colorer.far-plug-wide` (`plugins/colorer/CMakeLists.txt:65-67`, root `CMakeLists.txt:162-163`). `GetModulePFN` is private (`plclass.hpp:66`, body `plclass.cpp:27-33`).
- `Plugin *PluginManager::FindPlugin(DWORD SysID)` loads and returns the plugin, or null (`far2l/src/plug/plugins.cpp:1777-1791`, declared `plugins.hpp:235`). `FAR_PID_COLORER` is in `far2l/plugins/include/plugids.h:8`, value `0xD2F36B62` (`far2l/plugins/include/plugs.h:10`), the same value the plugin sets at `pcolorer.cpp:71`.
- `editorSet` is a file-static that starts null and is created on the first plugin event (`pcolorer.cpp:5,149-150`). `ProcessViewerEventW` handles `VE_READ` (`pcolorer.cpp:285-335`), and `FarViewer::ReadFrom` creates the `FarViewer` and `ViewerColoring` there (`FarViewer.cpp:30-48`), before far2l renders Markdown (`far2l/src/viewer.cpp:1719`). `FarEditorSet::GetPFactory()` gives the `ParserFactory` (`FarEditorSet.h:35`), and `FarEditorSet::getDocCache()` gives the shared document cache (`FarEditorSet.h:37`). `FarEditorSet::ReloadBase` returns early when the plugin is disabled (`FarEditorSet.cpp:957-960`), so a disabled plugin leaves `parserFactory` null.
- Parse chain, in the order `FarEditor::init` uses (`FarEditor.cpp:65-72,125-127`): clear `hrd_name` to `L""`; `ParserFactory::chooseFileType(FarTextSourceType::SRC_FILE, L"x.cpp", &hrd_name)` (`hrc/ParserFactory.h:84`); `TextParser::setParserFactory(factory)`; `TextParser::setDocCache(docCache)`; then `TextParser::parseString(wchar_t*, TextSource*, RegionHandler*)` (`text/TextParser.h:99,82,103`) with `LineSource(const wchar_t*, size_t len = 0)` (`text/LineSource.h:30,36`). `LineSource::nextLine` returns true once, then false, so one parse call covers one line, and the shared document cache carries the state to the next line. `RegionHandler::beginAction(const String &, int, int, bool)` returns true to keep parsing (`handlers/RegionHandler.h:38-42`), and `TextParser::breakParse()` stops the current parse (`text/TextParser.h:88`).
- A region name from `Region::getName()` is `scheme:name`, for example `def:Keyword`, and `Region::isAncestorOf(scheme, L"def", L"Keyword")` tests the mapping. `FarEditorSet.cpp:174` and `ViewerColoring.cpp:57` show the pattern. A region of a scheme other than `def` has no mapping, and `hrd_name` empty means the `def` scheme.
- `chooseFileType` caches by the file name, wide and case-folded, in `FarEditorSet::cache` (`FarEditorSet.cpp:384-441`). A cache hit does not set `hrd_name`, so the caller must clear it to `L""` before each call, the way `FarEditor::init` does (`FarEditor.cpp:65-72,125`).
- One `MdLine` is one screen row. Markdown rows are cut at the screen edge, not wrapped (`WrapsToNext` and `ContinuesFromPrev` stay false; `Viewer::ReadStringMarkdown` at `far2l/src/viewer.cpp:1136-1156`, `Viewer::UpdateWrapFlags` at `viewer.cpp:1338-1344`). Horizontal scroll is `LeftPos`: the draw loop (`viewer.cpp:710-716`) calls `printer->Length` and `printer->Print(LeftPos, Width, …)`, and the Markdown printer drops `skip_len` cells, then prints at most `print_len` cells (`far2l/src/ViewerPrinter.cpp:171-193`), and pads a short row in `COL_VIEWERTEXT` (`ViewerPrinter.cpp:198-202`).
- `MdLine` is `{ std::vector<MdRun> runs; int64_t nFilePos; }` and `MdRun` is `{ std::wstring text; uint64_t role; }` (`far2l/src/markdown/MarkdownRender.hpp:26-37`). A fence line is one code run, with the 4-cell indent baked into its text (`MarkdownRender.cpp:505-521`, `CODE_INDENT = 4` at `:27`). `MdPalette.hpp:10-21` already declares `MD_ROLE_TOKEN_KEYWORD`, `MD_ROLE_TOKEN_STRING`, `MD_ROLE_TOKEN_COMMENT` and `MD_ROLE_TOKEN_NUMBER`. `MdPalette.cpp:15-25` holds the fork table, and `TrueForeColor` (`MdPalette.cpp:54-58`) does the `0xRRGGBB` to `0xBBGGRR` conversion.
- The C++ driver pattern is `testing/markdown/ascii_driver.cpp` with `testing/markdown/ascii-driver.sh`, registered at `far2l/CMakeLists.txt:326-330`. The pure entry is `void MarkdownDoc::Render(const std::wstring &file_text, int width)` (`MarkdownRender.hpp:53`). A fence cache precedent is `MarkdownDoc::m_mermaid_cache`, a `std::map<std::pair<size_t, int>, std::vector<MdLine>>` keyed by a fence source hash and the width, built by `mermaid_key` (`MarkdownRender.cpp:76-79`, used at `:745-762`).
- The smoke harness is a Go driver that runs a `test.js` per test (`testing/src/`, `testing/far2l-smoke-run.sh`). `ReadCell(x, y)` (`testing/src/output.go:132-141`) returns one cell: `Fore` is the 16-color palette index, `IsForeTC` says the cell carries a true color, and `ForeTC` holds that color as `0xBBGGRR`. `testing/tests/0007-view-file-markdown/test.js:74-81` asserts `ForeTC == 0x5fffff` and `Fore == 14` on a heading cell, and `:656-688` runs a second far2l with `--norgb` and asserts the 16-color fallback.

## T1: The Colorer export, proved in the running app

**Depends on:** none
**Done check:** `cmake --build ../far2l.markdown-code-highlight.build -j8` exits 0, `nm -gU ../far2l.markdown-code-highlight.build/install/far2l.app/Contents/MacOS/Plugins/colorer/plug/colorer.far-plug-wide | grep -c ColorerTokenizeV1` prints 1, and `sh testing/markdown/highlight-driver.sh` exits 0, with the probe case `probe_real_plugin`, which loads the plugin module the way T2 does, prints the classes it gets for a `js` snippet, and fails when a keyword, a string, a comment and a number are not all there.
**Role:** implementer — review task-reviewer-critical

New file `plugins/colorer/src/pcolorer2/ColorerExport.cpp`, added to the `SOURCES` list in `plugins/colorer/CMakeLists.txt:11-34` (`add_library(colorer MODULE …)` at `:36`). It implements the export of Global Constraints, in the `SHAREDSYMBOL` form of the plugin's other exports.

Steps in the export:

1. Return -1 when `editorSet` is null, or when `editorSet->GetPFactory()` is null. The second case is a disabled plugin (`FarEditorSet.cpp:957-960`). Create no `FarEditorSet`.
2. For each line, build a `LineSource` over at most 1000 wide characters of that line.
3. Take the `ParserFactory` from `editorSet->GetPFactory()`. Clear `hrd_name` to `L""`, then call `chooseFileType(FarTextSourceType::SRC_FILE, L"x.<alias>", &hrd_name)`. Return -2 when the result is null, and treat the type name `default` as -2.
4. Call `setParserFactory`, then `setDocCache` with a `DocCache` that the export creates for this call (`std::make_shared<colorer::hrc::DocCache>()`), so a multi-line comment keeps its state on the next line and each fence starts clean. Then call `parseString` per line, with a `RegionHandler` that maps every region through `Region::isAncestorOf(scheme, L"def", …)`: `Keyword` gives class 1, `String` and `Character` give class 2, `Comment` gives class 3 and `Number` gives class 4. A region with none of them emits nothing. The events carry the region's start and end in cells of that source line.
5. The `hrd_name` of step 3 is a `wchar_t` buffer of `WC_SIZE`, cleared to `L""` before each call, the way `FarEditor::init` and `ViewerColoring::setScheme` do (`FarEditor.cpp:125`, `ViewerColoring.cpp:147-161`).
6. The alias table is a `static const` array of wide string pairs in `ColorerExport.cpp`, and it maps a fence name to the name that Colorer knows: `c++` to `cpp`, `tsx` to `jsx`, `zsh` to `sh`. The export looks up the fence name; a name that is not in the table is used as it is. The file name for step 3 is `L"x."` plus the result.
7. `parseString` returns the count of parsed lines; a return of 0 is success with no regions, and emits nothing.
8. Compare a monotonic clock with `deadline_ms` in `beginAction`. Call `breakParse()` and return false once the deadline passes. Keep the events of the lines finished before the deadline, and return 1.
9. `deadline_ms` of 0 or less means the budget is already spent: return 1 with no events.
10. Catch `colorer::Exception` and return -3. Events emitted before it stay.

The task also answers two unknowns in its report, each with the code line that shows it: that `dlopen(path, RTLD_LAZY | RTLD_NOLOAD)` finds the module that far2l loaded on this host, and that `editorSet` is not null at the first Markdown render. Record the wall time of the first export call and of a later one in the report.

## T2: The core bridge, with a fake for tests

**Depends on:** T1
**Done check:** `cmake --build ../far2l.markdown-code-highlight.build --target far2l -j8 && sh testing/markdown/highlight-driver.sh` exits 0, with the cases `alias_maps_cpp`, `alias_maps_zsh`, `name_too_long`, `name_bad_char`, `no_source_falls_back`, and `source_call_count`.
**Role:** implementer — review task-reviewer-critical

New `far2l/src/markdown/MdColorer.hpp` and `.cpp`, added to the markdown sources in `far2l/CMakeLists.txt:113`. The `.cpp` holds all knowledge of the plugin, one time per process in a function-local static:

1. `PluginManager::FindPlugin(FAR_PID_COLORER)`, then `Plugin::Load()` on a non-null result.
2. `dlopen(plugin->GetModuleName().c_str(), RTLD_LAZY | RTLD_NOLOAD)`, then `dlsym(handle, "ColorerTokenizeV1")`.
3. A null plugin, a null module name, a `dlopen` failure, a null symbol, and a return of -1, -2 or -3 all set one "no colors" flag. `Tokenize` then returns false with no further call, and the viewer shows the plain code role with no message.

`MdGetTokenSource()` returns the plugin source, or the source installed by `MdSetTokenSourceForTest`. `MdSetTokenSourceForTest` keeps the source for the life of the process; a driver case installs its fake before it runs, and the fake owns the canned tokens. `Tokenize` lower-cases the fence name, then validates it (at most 32 characters of `[A-Za-z0-9+#_-]`, non-empty), applies the alias table of T1, converts the `std::string` lines to `wchar_t`, and calls the export with `budget_ms`. The bridge calls `dlopen` with the exact path string that far2l passed to its own `dlopen`, which is `plugin->GetModuleName()`, so the same module answers. The driver test uses a fake source, so it needs no plugin.

## T3: Token storage in the render, with a per-render budget

**Depends on:** T2
**Done check:** `sh testing/markdown/highlight-driver.sh` exits 0, with the cases `fence_parsed_once`, `line_cap_400`, `byte_cap_65536`, `budget_40ms_skips_later_fences`, `long_line_rest_stays_code`, `scroll_keeps_token_colors`, and `layout_unchanged`.
**Role:** implementer — review task-reviewer-critical

`MdLine` gains `int tokens = -1;`, an index into `MarkdownDoc::m_token_groups`; -1 means no tokens. A group is `struct MdTokenGroup { std::vector<MdToken> tokens; };` — the tokens of one screen row, in cells of that row's single code run, the 4-cell indent counted, so the printer needs no offset math, and a horizontal scroll works on the same coordinates. `MarkdownRender.hpp` declares both.

In `MarkdownRender.cpp`:

1. The fence path is `MD_BLOCK_CODE` in `cb_enter_block` (`MarkdownRender.cpp:397-408`, which sets `b.code_lang` from the md4c fence info string) and in `cb_leave_block` (`MarkdownRender.cpp:491-526`, which splits `b.code_text` on `'\n'` and emits one code run per row). The token call goes in the leave case, after the split, once per fence, with the remaining render budget. The mermaid fences take the same path, and their art rows keep the art role.
2. The render takes the token source once per `Render` call, through `MdGetTokenSource()`. A cache in the document, in the form of `m_mermaid_cache`: `std::map<std::pair<std::string, uint64_t>, std::vector<MdTokenGroup>> m_token_cache`. The key is the lower-case fence language (the bridge of T2 lower-cases it) and a 64-bit FNV-1a hash (a simple, fast 64-bit string hash) of the fence source text; the value is one group per source line, in cell coordinates of the code run. The key hash follows `mermaid_key` (`MarkdownRender.cpp:76-79`). The cache lives in `MarkdownDoc`, so it survives `Invalidate(width)` and a resize; a hit costs no export call. `Render` clears `m_token_groups` at the start and appends groups as it links rows.
3. One 40 ms budget covers all fences of one `Render` call: `Render` sets `m_token_deadline` to `std::chrono::steady_clock::now() + 40ms` at its start, and each fence calls `Tokenize` with the milliseconds left, at least 1. A fence that finds nothing left gets no call and keeps the code role.
4. The caps of `spec.md` apply: 1000 characters per source line, 400 lines per fence, 65536 source bytes per fence.

Text, row count and row offsets must not change: only the token data is added. `0007-view-file-markdown`, `0008-view-md-default` and `0009-view-md-setting-off` must stay green.

## T4: Draw the token roles, and read a cell attribute in a test

**Depends on:** T3
**Done check:** `sh testing/markdown/highlight-driver.sh` exits 0, with the four role cases, the contrast case and the `--norgb` case, and "The check" of `AGENTS.md` with `TESTS="0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off"` exits 0.
**Role:** implementer — review final

1. `MdPalette.cpp:15-25` gets four rows for the roles that `MdPalette.hpp:10-21` already declares, with the values of `spec.md`: 256 index, RGB, 16-color fallback, and the ini key. The palette entry for the contrast-fail case is `COL_HELPHIGHLIGHTTEXT`, the entry of `code`.
2. `MarkdownPrinter::Print` (`far2l/src/ViewerPrinter.cpp:128,165-193`) keeps its run loop. New private helper in the same file: `void PrintCodeRuns(FarPrint &fp, const MdTokenGroup &group, const std::wstring &text, int &pos, int skip_len, int print_len)`, called in place of the plain `AddStr` for a code run on a line with a token group. The helper keeps an index over the tokens of the group and the cell position inside the run: it sets `SetColor(MarkdownRunColor(MdRoleForTokenClass(cls)))` at each token start, prints the run slice up to that token, sets the code role, and prints the slice up to the next token. `skip_len` and `print_len` clip the same slices they clip today. Cells past the last token, and the padding of a short row, keep the code role, as today. The change stays under 40 lines, and all color logic stays in `far2l/src/markdown/`.
3. No new smoke protocol command is needed. A test reads a cell with `ReadCell(x, y)`: `Fore` is the 16-color index, `IsForeTC` says the cell carries a true color, and `ForeTC` holds it as `0xBBGGRR` (`testing/src/output.go:132-141`, used at `testing/tests/0007-view-file-markdown/test.js:74-81`).

## T5: Smoke test and hostile fixtures

**Depends on:** T3, T4
**Done check:** `cmake --build ../far2l.markdown-code-highlight.build -j8 && (cd testing && find tests -maxdepth 1 -type s -delete; APP=$(cd ../*/.. && pwd) ./far2l-smoke-run.sh "$(cd ../far2l.markdown-code-highlight.build && pwd)/install/far2l.app/Contents/MacOS/far2l" 000c-view-md-highlight)` prints `[DONE]` and no `FAILED`
**Role:** implementer — review final

New `testing/tests/000c-view-md-highlight/`, with a `test.js`, an `initdir` and a `workdir`, in the form of `testing/tests/0007-view-file-markdown/`. It needs the colorer plugin, so the run uses the full build, which installs the plugin beside the binary. Its `fixture.md` (in `workdir`) holds, in this order: a `js` fence with the two lines `var s = "x"; // c` and `for (var i = 0; i < 3; i++)`, a `python` fence, a `bash` fence, a `json` fence, a `c++` fence, a `zsh` fence, a fence with an empty language, a fence named `nosuchlang9`, a 401-line fence, a python fence with one line of 1 MB of `'`, an unclosed string, a deeply nested block, and eight small fences.

`test.js` opens the fixture and checks the first screen (rows 0 to 23). It finds a row with `ExpectString(<text>)`, which returns the row and the cell `X` of the match, and reads cells at `X + n`, the way `0007-view-file-markdown` does. The offsets are fixed by the fixture text above: on `var s = "x"; // c` the keyword starts at the run start, the string quote is at offset 6, and the comment is at offset 12; on `for (var i = 0; i < 3; i++)` the number `3` is at offset 17.

`test.js` checks, with `ReadCell`: the `ForeTC` and `Fore` of the keyword, string, comment and number cells of the `js` fence, of the `c++` fence and of the `zsh` fence; the code index of the empty language fence and of `nosuchlang9`; that the fence text is unchanged; and that the long line and the 401th row show the code index. A second app run with `--norgb` checks the 16-color fallback of `spec.md`, the way `testing/tests/0007-view-file-markdown/test.js:656-688` does. The deep fixtures (the 1 MB line, line 401, the budget) are checked by the driver cases of T3.

## T6: The docs the story changes

**Depends on:** T4
**Done check:** `python3 ~/.omp/plugins/cache/plugins/fde-kit___fde-delivery___0.18.0/kb/scripts/check_ste.py` prints nothing for each changed doc, and `node ~/.omp/plugins/cache/plugins/fde-kit___fde-delivery___0.18.0/scripts/lint-story.mjs docs/stories/markdown-code-highlight --tier full` exits 0.
**Role:** implementer — review final

New `docs/architecture/decisions/adr-008-colorer-plugin-export.md` — 008 is the next free number, and the slug is `colorer-plugin-export`, in the form of `adr-004-palette-roles.md`. Record the reasons (no new link to the Colorer library and to libxml2, one catalog copy, no hidden editor) and the rejected routes from `story.md` Notes (link to the core, hidden editor, helper process, HRC data with a new tokenizer), plus the fallback: no plugin gives the plain code role. Update `docs/architecture/features/markdown/markdown-viewer.spec.md` (fence colors), `markdown.data.md` (the four new roles, the token group, the four ini keys, the export as a data contract) and `markdown.invariants.md` (a token group is cached by the fence language and the hash of the fence text, so it survives a resize; the four limits with their values; a token group is in cells of the code run, so a horizontal scroll colors the right cells). `md4c` is the Markdown parser that the viewer already uses.
