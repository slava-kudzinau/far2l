# Spec: Colorer look for Markdown prose

The F3 Markdown view takes the foreground and the underline of each prose role from the
active Colorer scheme. F4 with Colorer shows the same foreground for the same word.
Story: `story.md`. Base contract: ADR-004 (a run holds a role) and ADR-008 (one small
plugin export).

## Parts

| Part | File | Job |
|---|---|---|
| Plugin export | `plugins/colorer/src/pcolorer2/MdExport.cpp` (new) | Answers `ColorerRegionLookV1`: the look of a list of region names under the active scheme, plus a generation counter. |
| Plugin accessors | `plugins/colorer/src/pcolorer2/FarEditorSet.{h,cpp}` | Expose `regionMapper`, and a counter that changes in `ReloadBase`. |
| Core bridge | `far2l/src/markdown/MdColorer.{hpp,cpp}` | One call that fills plain structs from the export; a fake source for tests. |
| Resolver step | `far2l/src/markdown/MdPalette.cpp` | New step 2: the Colorer look, under the 3:1 rule. |
| New roles | `far2l/src/markdown/MarkdownRender.{hpp,cpp}`, `MdPalette.cpp` | `MD_ROLE_EM` for emphasis, `MD_ROLE_LISTMARK` for the list lead. |
| Underline | `far2l/src/ViewerPrinter.cpp` | Puts `COMMON_LVB_UNDERSCORE` on the cells of an underlined run. |

## Export: `ColorerRegionLookV1`

```c
SHAREDSYMBOL int ColorerRegionLookV1(int abi, const wchar_t* const* names, int count,
    unsigned int* look, int capacity, unsigned int* generation)
```

`abi` is `1`. `names` holds `count` Colorer region names, for example
`L"markdown:HeadText"`. The export writes `2 * count` words to `look`: `look[2*i]` is the
foreground as `0xRRGGBB`, `look[2*i+1]` is the Colorer style. It writes the scheme
generation to `*generation`. A missing region gives `0x00FFFFFF` (white) only when the
scheme itself says white; a name the HRC does not know gives `fore` 0, style 0, and the
core keeps the fork default for that role.

| Code | Meaning | Core action |
|---|---|---|
| 0 | The look is filled. | Use it (step 2). |
| 1 | Not loaded: no `FarEditorSet` yet. | Fork table. |
| 2 | Disabled: `Enabled=0` or `SyntaxDraw=0`. | Fork table. |
| 3 | Error: a Colorer `Exception`. | Fork table. |
| 4 | `abi` is not 1, or `2 * count > capacity`. | Fork table. |

The export does not create `FarEditorSet`, so the first call shows no "Reloading" message.
The generation counter is a new member of `FarEditorSet`, incremented in `ReloadBase`. A
scheme change in the Colorer settings dialog calls `ReloadBase`, so the counter changes and
the core fetches again. With `count` 0 the export checks `abi`, writes `*generation`, and
returns its code without touching `look`: that is the cheap probe the cache uses.

Style bits are the Colorer values: 1 bold, 2 italic, 4 underline, 8 strikeout. The core
uses bit 4 only.

## Bridge: `MdColorer`

```cpp
struct MdRegionLook { uint32_t fore; uint32_t style; };  // fore is 0xRRGGBB

class MdLookSource {
public:
  virtual ~MdLookSource() = default;
  // The current scheme generation; 0 when no look is available. Cheap: the plugin
  // source answers it with a count-0 export call. Safe to call per run.
  virtual unsigned long long Generation() = 0;
  // Fills out[0..count) for names[0..count). Returns the scheme generation,
  // or 0 when no look is available.
  virtual unsigned long long Fetch(const char* const* names, int count,
      MdRegionLook* out) = 0;
};

MD_COLORER_API MdLookSource &MdGetLookSource();
void MdSetLookSourceForTest(std::unique_ptr<MdLookSource> src);

// The ten region names of the Region map, in the fixed order, and their count.
// MdColorer.cpp owns the table; MdPalette.cpp maps role to index.
MD_COLORER_API const char* const* MdRegionNames(int* count);
```

The plugin source reuses the module handle of the fence-token source: one
`FindPlugin(SYSID_COLORER)` → `Plugin::Load` → `dlopen(RTLD_LAZY|RTLD_NOLOAD)` →
`dlsym`, one time per process, now for two symbols: `ColorerTokenizeV1` and
`ColorerRegionLookV1`. A missing `ColorerRegionLookV1` (an old plugin binary) gives the
fork table, and the fence tokens keep working. A `Fetch` that returns 0 means "no look":
the export answered 1, 2, 3, 4, `dlsym` found nothing, or `count` names all came back
empty (`fore` 0).

## Region map

The core asks for ten names, in this order. `MdColorer.cpp` owns the name list
(`MdRegionNames`); `MdPalette.cpp` maps a role to an index.

| Index | Role | Region | `far2l.hrd` fore | style |
|---|---|---|---|---|
| 0 | body | `def:Text` | `#99EEEE` | 0 |
| 1 | h1, h2, h3 | `markdown:HeadText` | `#8888ff` | 4 |
| 2 | link | `markdown:LinkText` | `#8888ff` | 4 |
| 3 | strong | `markdown:StrongText` | `#40a000` | 0 |
| 4 | em | `markdown:EmText` | `#ff20a0` | 0 |
| 5 | code | `markdown:Code` | `#FFE977` | 0 |
| 6 | quote | `markdown:QuoteText` | `#6699ff` | 0 |
| 7 | listmark | `markdown:ListMark` | `#FFFFFF` | 0 |
| 8 | rule | `markdown:Rule` | `#FFFFFF` | 0 |
| 9 | body | `markdown:Text` | `#99EEEE` | 0 |

`markdown:EmText` is the new emphasis region. It climbs to `def:Label`. Headings levels 1
to 6 share `markdown:HeadText`, as in F4. The `art` role has no region: it keeps the fork
look. The four `token-*` roles keep the fence-token path.

## Resolver order

`MdRunColor(role)` keeps its signature and returns a 64-bit far attribute. The order is:

1. A value in `settings/markdown-colors.ini`. New keys: `emphasis`, `listmark`. No
   contrast check, as today.
2. The Colorer look, when the bridge answered and the 3:1 rule passes for the batch. The
   foreground is the scheme's `fore`, and the underline bit is set when the scheme's style
   has bit 4.
3. The fork RGB defaults, under the 3:1 rule, as today.
4. Today's far palette entry, then the 4-bit fallback, as today.

The 3:1 rule for the Colorer look is all or none, at 3.0, against the real viewer
background, like the fork rule. Each of the ten scheme rows above needs 3:1 or more. A
failure sends all roles to step 3. The verdict is cached on the pair (viewer background,
scheme generation), so a theme change and a scheme change both re-evaluate it.

Below 8 bits of color (`GetConsoleColorPalette(NULL) < 8`) the Colorer step does not run:
the look carries 24-bit foregrounds only. The result keeps the 4-bit fallback of the fork
table.

The background of every cell stays the `COL_VIEWERTEXT` background. The scheme background
is not used, not even `def:Text` `#000060`.

## Cache

The look is fetched once per redraw pass, not per run:

| Key | Value | Owner |
|---|---|---|
| Scheme generation | The ten `MdRegionLook` rows, copied into plain structs, plus a per-row "the scheme has this region" flag. | `MdPalette.cpp`, a static cache, mutex-guarded, like `RgbDefaultsAllowed`. |
| (viewer background, generation) | The 3:1 verdict for the Colorer batch. | `MdPalette.cpp`, same shape as the fork verdict cache. |

`MdRunColor` reads the cache. Per call it asks `MdLookSource::Generation()`, a count-0
probe. A new generation refetches all ten rows once; the same generation does not call the
plugin again. The fetch cost is measured in Task 1 and recorded in `closeout.md`.

## New roles

| Role | Element | Fork default (step 3) | 4-bit (step 4) | Today's look (step 3 fail) |
|---|---|---|---|---|
| `MD_ROLE_EM` | Emphasis text, `_text_` and `*text*` | `0xd7d7af` | `F_LIGHTMAGENTA` | `COL_VIEWERTEXT` |
| `MD_ROLE_LISTMARK` | The bullet `• `, the number `1. `, the task box `[ ] ` | `0x8787af` | `F_CYAN` | `COL_VIEWERTEXT` |

`MD_ROLE_EM` and `MD_ROLE_LISTMARK` come after `MD_ROLE_TOKEN_NUMBER`, before
`MD_ROLE_COUNT`, so the existing role numbers do not move. The `MdRunStyle` of emphasis
keeps the italic marker off, as today: only the color changes. `MarkdownRender.cpp` sets
`MD_ROLE_EM` in the `MD_SPAN_EM` callback, and `MD_ROLE_LISTMARK` on the lead run of a
list item.

## Underline

Style bit 4 sets `COMMON_LVB_UNDERSCORE` (0x8000) on every cell of the run. `MdRunColor`
returns that bit inside its `uint64_t`. `Console::Text` drops it for one-cell characters,
so `MarkdownPrinter::Print` applies it again after the print: it records the column span of
each underlined run during the row, and at the end of the row it reads the row with
`GetText`, sets bit 0x8000 on those columns, and writes it back with `PutText`. This is
the same mechanism as `Viewer::ApplyUnderlineSegment`.

Bold, italic and strikeout bits are ignored, as F4 ignores bold and italic. The code holds
no list of which role is underlined: the scheme decides it through the style word.

## Fallback

| Cause | Result |
|---|---|
| Colorer plugin absent | The fork table (steps 3 and 4). |
| `Enabled=0` or `SyntaxDraw=0` (code 2) | The fork table. |
| Colorer `Exception` (code 3) | The fork table. The viewer does not fail and does not wait. |
| Old plugin binary, no `ColorerRegionLookV1` | The fork table; fence tokens still work. |
| One scheme color below 3:1 | The fork table for all roles. |
| Terminal below 8 bits | The 4-bit fallback of the fork table. |
| A region name the HRC lacks | The fork default for that role only. |

## Done

`BUILD=../far2l.markdown-colorer-look.build` and the check from `AGENTS.md` exit 0:

```sh
BUILD=../far2l.markdown-colorer-look.build APP=$PWD/../far2l.markdown-colorer-look.build/install/far2l.app/Contents/MacOS/far2l
cmake --build ../far2l.markdown-colorer-look.build -j8        # the full build: the plugin loads,
                                                               # the first render answers generation
                                                               # 0, later renders answer the scheme
sh testing/markdown/ascii-golden.sh
sh testing/markdown/ascii-driver.sh
sh testing/markdown/highlight-driver.sh
sh testing/markdown/look-driver.sh
(cd testing && ./far2l-smoke-run.sh "$APP" 0007-view-file-markdown
  ./far2l-smoke-run.sh "$APP" 0008-view-md-default
  ./far2l-smoke-run.sh "$APP" 0009-view-md-setting-off
  ./far2l-smoke-run.sh "$APP" 000c-view-md-highlight
  ./far2l-smoke-run.sh "$APP" 000d-view-md-look)
```

`sh testing/markdown/look-driver.sh` runs the driver (same harness shape as the
highlight driver: a case table, `--case <name>` picks one, `ok <case>` /
`FAILED <case>: <why>` lines, `DONE` at the end; an unknown case name exits 1).
It runs with a truecolor backend, so the plugin reads `far2l.hrd`; the cases that
care about the 16-color path switch the backend themselves. Its cases cover every
table row above:

| Case | Proves |
|---|---|
| `look_first_call` | The first viewer call answers generation 0 and starts the scheme load; later calls answer the scheme generation. |
| `look_real_hrd` | The real plugin with `far2l.hrd` gives the ten values of the Region map table. |
| `look_cost` | The cost of one ten-name fetch, printed. Above 1 ms means the bridge cache is on. |
| `look_bridge` | A fake source fills the ten names; the bridge copies into plain structs. |
| `look_codes` | Codes 1, 2, 3, 4 each give the fork table, and no wait, no message. |
| `look_bridge_real` | The real plugin answers the ten names through the core bridge, with its generation. |
| `look_roles` | The resolver order: the user file over the scheme over the fork default; an absent row (`fore` 0) takes the fork default; a style-4 row returns `COMMON_LVB_UNDERSCORE`. |
| `look_contrast` | All rows at 3:1 keep the scheme; one below 3:1 sends every role to the fork table; a new scheme generation and a new viewer background both recompute the verdict. |
| `look_ini` | `markdown-colors.ini` wins over the look, key by key, with no contrast check. |
| `look_generation` | A new generation refetches and replaces the cached rows; the same generation does not. |
| `look_new_roles` | `emphasis` and `listmark` resolve on the fork path: the 256-color look, the 16-color path, the RGB default and today's look. |

The smoke test `testing/tests/000d-view-md-look` covers the real plugin in the viewer,
with `ReadCellRaw(x,y).Attributes & 0x8000` for the underline, and `ReadCell(x,y).ForeTC`
for the foreground. The harness runs a 16-color terminal, so the plugin serves
`console.hrd` and the test asserts its rows: heading `0x5fffff`, link `0x87ff87`, strong
`0xffffff`, emphasis, listmark and body `0xffff00`, code `0xff87ff`, quote `0xd7afaf`,
rule `0xaf8787`, every one of them underlined. The truecolor `far2l.hrd` rows are
asserted by the driver's `look_real_hrd`. The tests that pin the fork colors —
`0007-view-file-markdown`, `000c-view-md-highlight` and
`testing/markdown/highlight-driver.sh` — run with the fork table, because the check
builds the far2l target only and no plugin loads. Each of them says so in a comment.

## Out of scope

- Token colors inside a fence: story markdown-colorer-fences.
- The full-width tint of code blocks: story markdown-code-tint.
- Per-level heading colors on the Colorer path.
- Bold, italic and strikeout attributes, and the scheme background.
- Table borders and the `art` role: they keep the fork look.
