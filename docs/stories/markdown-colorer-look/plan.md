# Plan: Colorer look for Markdown prose

Spec: `spec.md` (governs exact values). No `design.md`: the design is here, in the tasks and
the constraints.

## Global Constraints

- C++17, the code style of `CODESTYLE.md`: tabs for indent, `snake_case` for locals, `CamelCase`
  for functions, `m_` prefix for members. Keep the fork's changes to upstream files small:
  three new files (`MdExport.cpp`, `look_driver.cpp`, `look-driver.sh`), the rest are edits.
- Files this story may touch:
  `plugins/colorer/src/pcolorer2/MdExport.cpp` (new),
  `plugins/colorer/src/pcolorer2/pcolorer2.cpp`,
  `plugins/colorer/src/pcolorer2/FarEditorSet.{h,cpp}`,
  `plugins/colorer/CMakeLists.txt`,
  `docs/stories/markdown-colorer-look/manual-checks.md` (new),
  `far2l/src/markdown/MdColorer.{hpp,cpp}`,
  `far2l/src/markdown/MdPalette.{hpp,cpp}`,
  `far2l/src/markdown/MarkdownRender.{hpp,cpp}`,
  `far2l/src/ViewerPrinter.cpp`,
  `testing/markdown/look-driver.sh`, `testing/markdown/look_driver.cpp` (new),
  `testing/tests/0007-view-file-markdown/test.js`,
  `testing/tests/000c-view-md-highlight/test.js` (a comment only),
  `testing/tests/000d-view-md-look/` (new),
  `docs/architecture/features/markdown/markdown-viewer.spec.md`,
  `docs/architecture/features/markdown/markdown.invariants.md`,
  `docs/architecture/features/markdown/markdown.data.md`,
  `docs/architecture/features/markdown/markdown.runtime.md`,
  `docs/architecture/features/markdown/markdown-browser-view.spec.md`,
  `docs/architecture/decisions/adr-009-colorer-look-step.md` (new).
- Export ABI, exact (spec "Export"): `SHAREDSYMBOL int ColorerRegionLookV1(int abi, const
  wchar_t* const* names, int count, unsigned int* look, int capacity, unsigned int*
  generation)`. `abi` is 1. `look[2*i]` is the foreground as `0xRRGGBB`, `look[2*i+1]` is the
  Colorer style word. Codes: 0 filled, 1 not loaded, 2 disabled, 3 Colorer `Exception`, 4 bad
  `abi` or `2*count > capacity`. The export reads the global `editorSet` (`pcolorer.cpp:6`)
  and must not create it.
- The export resolves a name with `HrcLibrary::getRegion(&name)` then
  `regionMapper->getRegionDefine(const Region*)`, the overload that climbs the HRC `parent=`
  chain (`colorer/handlers/RegionMapper.h:46`). The name overload does not climb; do not use it.
  Cast with `StyledRegion::cast(rd)` and read `fore`, `style`, `isForeSet`.
- New `FarEditorSet` members, exact: `public: StyledHRDMapper* GetRegionMapper() { return
  regionMapper.get(); }`, `unsigned int GetBaseGeneration() const { return m_baseGeneration; }`,
  `private: unsigned int m_baseGeneration = 0;`, and `++m_baseGeneration;` as the last statement
  of `FarEditorSet::ReloadBase()`.
- Bridge API, exact (spec "Bridge"): `struct MdRegionLook { uint32_t fore; uint32_t style; };`,
  `class MdLookSource { virtual unsigned long long Fetch(const char* const* names, int count,
  MdRegionLook* out) = 0; };`, `MdLookSource& MdGetLookSource()`,
  `void MdSetLookSourceForTest(std::unique_ptr<MdLookSource>)`. `Fetch` returns 0 for "no look".
  The plugin bridge reuses the module handle and the lookup of `PluginTokenSource::Open()`
  (`MdColorer.cpp:151-173`): one `FindPlugin(SYSID_COLORER)` → `Plugin::Load()` →
  `dlopen(RTLD_LAZY|RTLD_NOLOAD)` → two `dlsym`.
- Region map, the names in this order (spec "Region map"):
  `def:Text`, `markdown:HeadText`, `markdown:LinkText`, `markdown:StrongText`,
  `markdown:EmText`, `markdown:Code`, `markdown:QuoteText`, `markdown:ListMark`,
  `markdown:Rule`, `markdown:Text`. The last name (`markdown:Text`) is the body text of a
  Markdown file; the resolver takes its row when its `fore` is non-zero, else `def:Text`.
- Resolver order (spec "Resolver order"): user ini → Colorer look → fork RGB default → today's
  look / 4-bit fallback. The Colorer step runs only when `WINPORT(GetConsoleColorPalette)(NULL)
  >= 8` and the bridge answered for this generation and the 3:1 batch verdict passes.
  The 3:1 threshold is 3.0 with `wcagComputeContrast`, all or none over the scheme rows,
  against the real viewer background; the verdict is cached on `(viewer background, generation)`.
  An underline from the scheme sets `COMMON_LVB_UNDERSCORE` (0x8000) inside the returned
  `uint64_t`. Style bits 1, 2 and 8 are ignored.
- New roles: `MD_ROLE_EM` and `MD_ROLE_LISTMARK`, appended after `MD_ROLE_TOKEN_NUMBER` and
  before `MD_ROLE_COUNT`, so existing numbers do not move. `MD_ROLE_EM` fork default
  `0xd7d7af`, `def256` `0xd7d7af`, 4-bit `F_LIGHTMAGENTA`, today's look `COL_VIEWERTEXT`.
  `MD_ROLE_LISTMARK` fork default `0x8787af`, `def256` `0x8787af`, 4-bit `F_CYAN`, today's
  look `COL_VIEWERTEXT`. Both need a `kRoleDefaults` row, a `kRoleKeys` key (`emphasis`,
  `listmark`) and a `TodayLookEntry` case: `TodayLookEntry` has no `default:` label, so a role
  without a case returns an undefined value.
- Underline draw: `Console::Text` drops `COMMON_LVB_UNDERSCORE` for one-cell characters
  (`far2l/src/console/interf.cpp:609`), so the bit must be re-applied after the print with
  `GetText` + `PutText`, as `Viewer::ApplyUnderlineSegment` does (`viewer.cpp:4145`).
- A Markdown file and a Colorer scheme are untrusted input. The bridge copies the look into
  plain structs. No plugin call per run: at most one ten-name fetch per generation.
- Tests use `ReadCellRaw(x,y).Attributes & 0x8000` for the underline (`testing/README.md`:
  `Attributes uint64 - value from corresponding CHAR_INFO::Attributes`).
- Commit message: a short imperative summary line, a blank line, then the trailer
  `Co-Authored-By: Claude <noreply@anthropic.com>`. One commit per task.
- Check: the check from `AGENTS.md`, run with `BUILD=../far2l.markdown-colorer-look.build`,
  plus the full plugin build, `sh testing/markdown/look-driver.sh` and the smoke run of all
  tests. Clean stale sockets before every smoke run:
  `(cd testing && pkill -9 -f -- '--test=/tmp/far2l'; sleep 1; find tests -maxdepth 1 -type s -delete; for s in tests/*; do [ -S "$s" ] && rm -f "$s"; done)`.
  Owner-checked paths: `AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`,
  `testing/markdown/golden/*.txt`, `far2l/src/markdown/md4c/`, `far2l/third_party/quickjs/`,
  `.github/workflows/`.

## T1: The plugin export, and the three measurements

**Depends on:** none
**Done check:** `cmake --build ../far2l.markdown-colorer-look.build --target colorer -j8 && LOOK_BUILD=../far2l.markdown-colorer-look.build sh testing/markdown/look-driver.sh --case look_first_call && LOOK_BUILD=../far2l.markdown-colorer-look.build sh testing/markdown/look-driver.sh --case look_real_hrd && LOOK_BUILD=../far2l.markdown-colorer-look.build sh testing/markdown/look-driver.sh --case look_cost` exits 0
**Role:** implementer-risky — review task-reviewer-critical

New file `plugins/colorer/src/pcolorer2/MdExport.cpp`, one line in `SOURCES`
(`plugins/colorer/CMakeLists.txt:24`, next to `ColorerExport.cpp`), and the three
`FarEditorSet` additions from Global Constraints. The export body, in this order:

1. `if (abi != 1 || capacity < 2 * count) return 4;` `look` may be null when `count` is 0.
2. `if (editorSet == nullptr) return 1;` — do not create it.
3. `if (!editorSet->GetPluginStatus() || !editorSet->GetParserFactory() ||
   !editorSet->GetRegionMapper()) return 2;` (`GetPluginStatus` reports `Enabled`;
   `ReloadBase` leaves `parserFactory` and `regionMapper` null when `SyntaxDraw=0`.)
4. In a `try`: for each name, `HrcLibrary::getRegion(&u)` from
   `editorSet->GetParserFactory()->getHrcLibrary()`, then
   `regionMapper->getRegionDefine(region)`, then `StyledRegion::cast`; write `fore` and
   `style`, and write 0/0 when the name, the define or the cast is null.
   `*generation = editorSet->GetBaseGeneration();` `return 0;`
5. `catch (Exception& ex)`: `ex.announce(); return 3;`

New `testing/markdown/look-driver.sh` and `testing/markdown/look_driver.cpp`, the same shape
as `highlight-driver.sh` and `highlight_driver.cpp`: the driver links the far2l core through
`link.txt`, owns `ControlObject`, sets `FAR2L_PLUGIN`, dlopen()s the plugin module, and runs a
`{name, fn}` case table with `--case <name>`; it prints `ok <case>`, `FAILED <case>: <why>`
and `DONE`. It needs the full build. `LOOK_BUILD` names the build dir, default
`../far2l.markdown-colorer-look.build`, falling back to `../far2l.build`.

Three cases in this task:

- `look_first_call`: with a fresh plugin, `ColorerRegionLookV1(1, names, 10, ...)` returns 1
  (not loaded) and takes under 2 ms; then `ProcessViewerEventW(0, nullptr)` (the real
  signature, `pcolorer.cpp:142`); then the same call returns 0, and the count-0 probe
  `ColorerRegionLookV1(1, names, 0, nullptr, 0, &gen)` returns 0 with `gen == 1`
  (`ReloadBase` increments once in the constructor, so a real generation is never 0).
  Print both times. This answers the story's first unknown: the export does not create
  `FarEditorSet`, and the viewer's own `ProcessViewerEventW` does.
- `look_real_hrd`: the ten names of the region map, with `far2l.hrd` (the default rgb scheme,
  `catalog-rgb.xml:43-44`), give the exact ten pairs of the spec table: `0x99EEEE/0`,
  `0x8888ff/4`, `0x8888ff/4`, `0x40a000/0`, `0xff20a0/0`, `0xFFE977/0`, `0x6699ff/0`,
  `0xFFFFFF/0`, `0xFFFFFF/0`, and `markdown:Text` present (non-zero `fore`). A mismatch fails
  with the name, the expected word and the given word.
- `look_cost`: one ten-name fetch on a warm scheme, 1000 times; print the mean in
  microseconds. The bridge cache in T4 is on either way; this number goes to `closeout.md`.

Also write `docs/stories/markdown-colorer-look/manual-checks.md`: the owner's list for the
underline in macOS Terminal and Ghostty. The owner opens
`testing/tests/000d-view-md-look/initdir/left/fixture.md` (from T5, copied into the test
profile) in F3, looks at a heading and a link, and writes the result in the file.

## T2: The core bridge

**Depends on:** T1
**Done check:** `cmake --build ../far2l.markdown-colorer-look.build -j8 && LOOK_BUILD=../far2l.markdown-colorer-look.build sh testing/markdown/look-driver.sh --case look_bridge && LOOK_BUILD=../far2l.markdown-colorer-look.build sh testing/markdown/look-driver.sh --case look_codes && LOOK_BUILD=../far2l.markdown-colorer-look.build sh testing/markdown/look-driver.sh --case look_bridge_real` exits 0
**Role:** implementer — review task-reviewer

Add to `MdColorer.hpp` the `MdRegionLook` struct, the `MdLookSource` class,
`MdGetLookSource()` and `MdSetLookSourceForTest()`, with `MD_COLORER_API` on the getter, as
`MdNormalizeLangName` has. In `MdColorer.cpp`:

- `PluginLookSource : public MdLookSource`, which takes the module handle from the fence-token
  source: move the body of `PluginTokenSource::Open()` into a shared `PluginModule` helper with
  `void* Handle()` and two `dlsym` lookups (`ColorerTokenizeV1`, `ColorerRegionLookV1`).
  `PluginTokenSource` keeps its behaviour, including `m_broken` and `Ready()`.
- `Fetch` converts each `const char*` name to `std::wstring` with `utf8_to_wide`, calls the
  export, and on 0 copies `2*count` words into `out` and returns `*generation`. Codes 1 to 4
  return 0, and set no "broken for good" flag: a later `ReloadBase` can answer.
- `Generation` calls the export with `count` 0 and a null `look`, and returns `*generation`;
  codes 1 to 4 give 0.
- `MdRegionNames(int* count)` returns the static ten-name table of Global Constraints, in
  order; `MdColorer.cpp` owns it, `MdPalette.cpp` reads it (T4).
- A process-lifetime `static PluginLookSource` behind `MdGetLookSource()`, and the test hook,
  the same shape as `MdGetTokenSource` / `MdSetTokenSourceForTest` (`MdColorer.cpp:263-277`).

Driver cases:

- `look_bridge`: `MdRegionNames(&count)` gives the ten names of Global Constraints, in order;
  install a fake source, record the name list it is called with, answer ten rows through the
  same call, and read them back through `MdGetLookSource().Fetch(...)`: assert the copy and
  the generation.
- `look_codes`: drive the four codes with a fake that returns them in turn (codes 1 to 4 map
  to `Fetch` 0, and `Generation` 0), and one case where `dlsym` finds no `ColorerRegionLookV1`
  (a fake that answers 0, standing for the missing symbol): `Fetch` returns 0 and
  `MdGetTokenSource().Tokenize` (the real source, untouched by the hook) still works.
- `look_bridge_real`: the real `MdGetLookSource()`, the real plugin, the full build, no fake.
  Before any viewer event, `Generation()` returns 0 (the real code-1 path: dlopen, dlsym,
  export, mapping). Then `ProcessViewerEventW(0, nullptr)`; then `Generation()` returns 1,
  and `Fetch(MdRegionNames(&count), count, rows)` returns 1 with the ten `far2l.hrd` rows of
  the spec table. This covers the bridge's own dlopen/dlsym, the name conversion, the code
  mapping, and the row copy, on the plugin T1 built.

## T3: The two new roles on the fork path

**Depends on:** none
**Done check:** `cmake --build ../far2l.markdown-colorer-look.build --target far2l -j8 && LOOK_BUILD=../far2l.markdown-colorer-look.build sh testing/markdown/look-driver.sh --case look_new_roles` exits 0
**Role:** implementer — review final

- `MarkdownRender.hpp`: `MD_ROLE_EM` and `MD_ROLE_LISTMARK` after `MD_ROLE_TOKEN_NUMBER`,
  before `MD_ROLE_COUNT`.
- `MarkdownRender.cpp`: a `span_em` counter on the builder, `++` in the `MD_SPAN_EM` case of
  `span_open` and `--` in `span_close` (next to `span_strong`, `:734` and `:757`). `cur_role()`
  (`:238-251`) keeps its order: code, link, strong, h1, h2, h3. The last line's quote/body
  default becomes: quote inside a quote, else `MD_ROLE_EM` when `span_em > 0`, else body.
  So `***x***` prints strong, and `_x_` inside a quote prints quote.
  In `flush_lead` (`:218-226`), split the lead into two runs:
  the indent and the quote bars keep `lead_role()`, and `li_prefix` (the bullet `• `, the number
  `1. `, the task box) takes `MD_ROLE_LISTMARK`.
- `MdPalette.cpp`: a `kRoleDefaults` row and a `kRoleKeys` key for each new role, and the two
  `TodayLookEntry` cases, with the values from Global Constraints.
- `look_new_roles` driver case: `MdRunColor(MD_ROLE_EM)` under `--norgb` gives `0xd7d7af` in
  the truecolor bits and `F_LIGHTMAGENTA` in the low bits; `MdRunColor(MD_ROLE_LISTMARK)` gives
  `0x8787af` and `F_CYAN`; a `markdown-colors.ini` with `emphasis = #123456` gives `0x123456`;
  and every role id below `MD_ROLE_COUNT` resolves to a readable 16-color color, as the
  existing `print_contrast_fail` case does.
- `testing/tests/0007-view-file-markdown/test.js`: it uses the same `sample.md` and `initdir`
  as today. The emphasis line of `sample.md` now shows `0xafd7d7` (`#d7d7af` as `0xBBGGRR`)
  and the bullet `• ` of a list item shows `0xaf8787` (`#8787af`) with the truecolor flag,
  where today both take the body color. Rewrite the rows that read those cells; keep the
  count rows. Add the comment "the check builds the far2l target only, so no plugin loads and
  these are the fork colors" at the top of the file.

## T4: The Colorer step in the resolver

**Depends on:** T2, T3
**Done check:** `cmake --build ../far2l.markdown-colorer-look.build --target far2l -j8 && LOOK_BUILD=../far2l.markdown-colorer-look.build sh testing/markdown/look-driver.sh --case look_roles && LOOK_BUILD=../far2l.markdown-colorer-look.build sh testing/markdown/look-driver.sh --case look_contrast && LOOK_BUILD=../far2l.markdown-colorer-look.build sh testing/markdown/look-driver.sh --case look_ini && LOOK_BUILD=../far2l.markdown-colorer-look.build sh testing/markdown/look-driver.sh --case look_generation` exits 0
**Role:** implementer-risky — review task-reviewer-critical

In `MdPalette.cpp`, between the user ini and the RGB defaults, the step of spec "Resolver
order". A static cache, mutex-guarded, like `ReadOverrides` and `RgbDefaultsAllowed`:

```
struct LookCache { unsigned long long gen = 0; MdRegionLook rows[10]; bool fetched = false; };
```

- `ReadLook()`: when the cached `gen` differs from `MdGetLookSource().Generation()` (a
  count-0 probe, safe per run), fetch `MdRegionNames(&count)` from the source into the
  cache; a generation of 0 means "no look". A redraw of a whole screen costs probes and
  one ten-name fetch.
- `ColorerLookAllowed()`: the 3:1 batch verdict over the scheme rows, all or none at 3.0 with
  `wcagComputeContrast` against the real viewer background, cached on
  `(viewer background, generation)`. A row whose `fore` is 0 counts as a pass (the region is
  absent, and that role uses its fork default).
- The step, per role: `if (GetConsoleColorPalette(NULL) >= 8 && ColorerLookAllowed())` take
  `TrueForeColor(row.fore)`, and add `COMMON_LVB_UNDERSCORE` when `row.style & 4`; a row with
  `fore` 0 falls through to the fork default of that role. The `body` role takes the
  `markdown:Text` row when its `fore` is non-zero, else the `def:Text` row. The `art` role and
  the four `token-*` roles skip the step. Return with the viewer background, as today:
  `(entry & ~kBackBits) | (body & kBackBits)`.
- This task covers the palette through a fake source; the real plugin is T1's `look_real_hrd`.

Driver cases, each with a fake source that answers a ten-row table:

- `look_roles`: with the `far2l.hrd` values, `MdRunColor` per role gives the truecolor word
  of that row, the viewer background, and `COMMON_LVB_UNDERSCORE` for h1, h2, h3 and link only.
  Check the bit through the returned `uint64_t` (`& 0x8000`), not through a cell. The fake's
  name list is `MdRegionNames`: assert the ten names arrive in the spec's order.
- `look_contrast`: one row with a foreground below 3:1 against the background of the case;
  every role then takes its fork row (h1 `0xffff5f`, code `0xff87ff`), not the scheme value.
  Also the cache case: the same background and the same generation give the same verdict with
  one fetch; a new generation refetches, and the count of `Fetch` calls proves it.
- `look_ini`: `emphasis = #123456` wins over the scheme row for `emphasis`, and the other roles
  keep the scheme values; a bad value on `emphasis` leaves the scheme row (no contrast check
  on an override, as today).
- `look_generation`: the source answers generation 5 twice, then 6; `Fetch` runs twice in
  total, and the roles follow the new rows after generation 6. `Generation` calls run per
  `MdRunColor`; `Fetch` calls stay the proof of the cache.

## T5: The underline on screen

**Depends on:** T4
**Done check:** `cmake --build ../far2l.markdown-colorer-look.build --target far2l -j8 && (cd testing && pkill -9 -f -- '--test=/tmp/far2l'; sleep 1; find tests -maxdepth 1 -type s -delete; for s in tests/*; do [ -S "$s" ] && rm -f "$s"; done; ./far2l-smoke-run.sh "$PWD/../far2l.markdown-colorer-look.build/install/far2l.app/Contents/MacOS/far2l" 000d-view-md-look) > /tmp/000d.log 2>&1 && grep -q '\[DONE\]' /tmp/000d.log && ! grep -q FAILED /tmp/000d.log` exits 0
**Role:** implementer — review final

`MarkdownPrinter::Print` (`far2l/src/ViewerPrinter.cpp:158`) prints a row run by run with
`SetColor(MarkdownRunColor(run.role))` and `Text(...)`. `Console::Text` drops the underline bit
for one-cell characters, so:

- Keep the bit in the color word, and record the column span of every run whose color carries
  `COMMON_LVB_UNDERSCORE` while printing.
- At the end of the row, read the row with `GetText` (the columns from `_screenX1` to
  `_screenX2`), set bit `0x8000` on the recorded columns, and write the row back with `PutText`
  — one pair of calls per row, not per run, as `Viewer::ApplyUnderlineSegment` does.
- Skip the pass when no run of the row is underlined: the common case costs nothing.
- The token path (`PrintCodeRuns`) needs no pass: the token roles keep the fork look.

New smoke test `testing/tests/000d-view-md-look/` (`initdir`, `test.js`, `workdir`), the shape
of `000c-view-md-highlight`: `initdir/left/fixture.md` holds a `# Heading`, a link, a strong
span, an emphasis span, an inline code span, a quote, a list and a table; `test.js` opens F3
with the plugin present and asserts with `ReadCell(x,y).ForeTC` the values of the spec table,
and with
`ReadCellRaw(x,y).Attributes & 0x8000` the underline on the heading and the link, and no
underline on strong, emphasis, code, quote, list mark and body. The test needs the full build,
so say so in a comment at the top of `test.js`.

## T6: The F4/F6 parity sessions in 000d

**Depends on:** T5
**Done check:** `(cd testing && pkill -9 -f -- '--test=/tmp/far2l'; sleep 1; find tests -maxdepth 1 -type s -delete; for s in tests/*; do [ -S "$s" ] && rm -f "$s"; done; ./far2l-smoke-run.sh "$PWD/../far2l.markdown-colorer-look.build/install/far2l.app/Contents/MacOS/far2l" 000d-view-md-look) > /tmp/000d.log 2>&1 && grep -q '\[DONE\]' /tmp/000d.log && ! grep -q FAILED /tmp/000d.log` exits 0
**Role:** implementer — review final

Extend `000d-view-md-look/test.js` with two more sessions in the same file, the way `000c`
runs three:

- F4 then F6 on the same file: press F4, `ExpectString` a heading word, read its cell, press
  F6 (Enter back to the panel, then F3), read the same word in the viewer, and assert the two
  `ForeTC` values are equal, and that the editor cell's `Attributes & 0x8000` equals the
  viewer cell's. The plugin loads on the F4 press, so this session proves the parity in both
  directions.
- A scheme change: write `plugins/colorer/config.ini` in the test profile with
  `[Settings] HrdNameTm = solarized-dark` (the file `hrd/solarized-dark.hrd`, the catalog
  entry "Solarized Dark"), open F3, and assert a heading's `ForeTC` differs from the
  `far2l.hrd` value of the first session, and that the viewer still shows the rows of the
  file, so the new scheme did not break the render. The plugin reads `HrdNameTm` at load,
  so start far2l with the ini in place; do not change the scheme inside one session.

Both sessions need the full build, like the first session. A run on the far2l-only build (the
check) shows the fork colors, and the fork-color rows of T5 stay green.

The durable docs, for the main session at close-out (owner-checked paths):
- `markdown-viewer.spec.md`: rewrite the row "Emphasis, strikethrough, underline" to say that
  emphasis takes the `em` role and that the scheme can underline a role; add `em` and `listmark`
  to the role table; add the Colorer step as step 2 of "The resolver order" and renumber the
  rest; add the export to "Contracts"; add `000d-view-md-look` to the Tests table; add the
  export to the feature's `refs`.
- `markdown.invariants.md`: under `[colors]`, one row for the Colorer look: a run holds a role,
  the look is a resolver step, the plugin gives a foreground and a style word, and the style
  word carries the underline bit; one row for the one-fetch rule (at most one fetch per
  generation).
- `markdown.data.md`: add the `em` and `listmark` roles to the `MdRun` row and to the ini row,
  add the `LookCache` row (owner, key, life) next to `m_token_cache`, and add a row for the
  export in the plugin section.
- New `docs/architecture/decisions/adr-009-colorer-look-step.md`: the Colorer look is a resolver
  step between the user file and the fork table, the run keeps its role, and the scheme's style
  word decides the underline; name the alternatives (a color in the run, a fixed per-role
  underline table, a fork scheme copy) and why they lost. Frontmatter `description` and
  `status: accepted`.
- Fix the module name in `markdown.runtime.md:37` and `markdown-browser-view.spec.md:60`: the
  module is `colorer.far-plug-wide`, not the old name.
