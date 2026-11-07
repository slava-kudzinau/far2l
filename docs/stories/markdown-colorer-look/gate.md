# Gate: Colorer look for Markdown prose

_For the owner, who reads this instead of the code. One page per section. The model writes
"Before build" before any task starts, and "At merge" at close-out; both go at the top of the
PR._

## Before build

### What it builds

The F3 Markdown view colors its prose from the active Colorer scheme, the same colors F4
shows. Headings and links are underlined when the scheme says so. F6 after F4 matches.

### Parts it touches

```mermaid
flowchart LR
  HRD[Colorer scheme .hrd] --> EXPORT[New plugin export ColorerRegionLookV1]
  EXPORT --> BRIDGE[MdColorer bridge]
  BRIDGE --> PALETTE[MdPalette resolver]
  PALETTE --> PRINTER[Markdown printer in F3]
  PRINTER --> SCREEN[F3 screen]
```

### Contracts, data and runtime

- New plugin export `ColorerRegionLookV1(abi, names, count, look, capacity, generation)`: the
  look of a list of region names under the active scheme, plus a generation counter. Codes for
  filled, not loaded, disabled, error, bad arguments. The export does not create `FarEditorSet`.
- `FarEditorSet` gains a region-mapper getter and a generation counter, incremented in
  `ReloadBase`. A scheme change in the Colorer settings changes F3 at the next open.
- `MdRunColor` keeps its signature and gains `COMMON_LVB_UNDERSCORE` (0x8000) in the word it
  returns, for the roles the scheme underlines.
- Two new palette roles: `em` (key `emphasis`) and `listmark` (key `listmark`) in
  `settings/markdown-colors.ini`. The resolver order gains one step: user file, Colorer look,
  fork defaults, today's look.
- The core fetches the ten region names at most once per generation, through the same plugin
  module handle the fence tokens use.

### Decisions the model made

| Decision | Alternative rejected | Why |
|---|---|---|
| One new export next to `ColorerRegionLookV1`'s sibling `ColorerTokenizeV1`, four plain arrays. | A shared header struct; a second plugin handle. | Keeps the fork's diff small on both sides. |
| The Colorer look is a resolver step between the user file and the fork table (new ADR-009). | A fixed per-role underline table in the fork. | The scheme decides the underline, so F4 and F6 agree. |
| The underline travels inside the `MdRunColor` word. | A second resolver call per run. | One call site; no extra `stat()` per run. |
| The core asks by region name and maps the name to a role. | Colorer type names, as ADR-008 uses for fences. | Prose regions resolve by name today; no slug work needed. |
| New roles `em` and `listmark`, appended at the end of the role list. | Reuse `body` for both. | Each gets its own ini key; old role numbers do not move. |
| One all-or-none 3:1 batch verdict on the scheme, cached on (background, generation). | Per-role verdicts. | The scheme is chosen as a set; one cache, one rule. |

### How you'll see it working

Open a `.md` file in F3 from the built app: `../far2l.markdown-colorer-look.build/install/
far2l.app/Contents/MacOS/far2l`. Headings and links show `#8888ff` underlined, strong
`#40a000`, emphasis `#ff20a0`, code `#FFE977`, quote `#6699ff`. Press F4 on the same file and
the same words carry the same colors. `sh testing/markdown/look-driver.sh` prints `DONE`.
The underline is visible only in a terminal that draws it; `manual-checks.md` lists the two
terminals to check.

### A fresh reader's paraphrase

Fresh read (scout, 2026-11-07): "T1 builds the plugin export and its driver. The export is
`ColorerRegionLookV1(int abi, const wchar_t* const* names, int count, unsigned int* look,
int capacity, unsigned int* generation)`: `abi` is 1, `look[2*i]` is `0xRRGGBB`, `look[2*i+1]`
is the Colorer style; codes are 0 filled, 1 not loaded, 2 disabled, 3 exception, 4 bad
arguments. Order: check arguments, check `editorSet`, check `GetPluginStatus`/`GetParserFactory`
/`GetRegionMapper`, resolve each name through `getRegion` then the climbing
`getRegionDefine(const Region*)`, write `fore`/`style`, write the generation, return 0; a
Colorer `Exception` announces and returns 3. `FarEditorSet` gains `GetRegionMapper()`,
`GetBaseGeneration()`, `m_baseGeneration = 0` and `++m_baseGeneration` as the last line of
`ReloadBase`, so the first real generation is 1. `CMakeLists.txt:24` gains `MdExport.cpp`. The
driver mirrors `highlight-driver.sh` (`LOOK_BUILD`, `--case`, `ok`/`FAILED`/`DONE`) and runs
`look_first_call`, `look_real_hrd` (ten exact pairs, `0x99EEEE/0` … `0xFFFFFF/0`), and
`look_cost` (1000 fetches, mean in microseconds, to `closeout.md`). T2 adds
`MdRegionLook{fore,style}`, `MdLookSource::Fetch`, `MdGetLookSource` and
`MdSetLookSourceForTest`, sharing one dlopen handle with the token source. T3 adds
`MD_ROLE_EM` (`0xd7d7af`, `F_LIGHTMAGENTA`) and `MD_ROLE_LISTMARK` (`0x8787af`, `F_CYAN`)
at the end of the role list, a `span_em` counter, and a two-run lead split. T4 inserts the
Colorer step between the ini and the fork table, with a generation-keyed look cache and a
`(background, generation)` 3:1 batch verdict; underline means bit `0x8000`. T5 re-applies the
bit with one GetText/PutText pair per row, because `Console::Text` drops it. T6 runs the F4/F6
parity session and the scheme-change session in 000d. T2 owns `MdColorer.*`; T3 and T4 share
`MdPalette.cpp`, in order, no overlap. No done check hides work in a pipe. The export ABI, the
bridge struct, the cache keys, the underline transport, the ten-name list, and the smoke cell
reads are all concrete."

## Flow

flow: tier=full docs=spec,plan design=no orchestrator=yes final=final-reviewer gate=stop
scan=yes verifier=none

## Spend

- Budget: M story. $15 spend, $20 hard stop.
- Last recorded (close-out): $13.88 of budget $15.00, hard stop $20.00: ok. The sum of the
  harness task costs is below. T1 0.64, T2 1.45, T3 1.71, T4 6.17, T5 0.80, T6 1.77, final
  review 1.34. The fix wave, this session's orchestration and the merge-gate check ride on the
  same store (the 10:32 `cost.jsonl` snapshot). The money at merge lands on this line at merge.

## Check

```sh
cd /Users/Viachaslau_Kudzinau/Projects/far2l/.worktrees/markdown-colorer-look
export BUILD=../far2l.markdown-colorer-look.build
cmake --build "$BUILD" -j8            # far2l and all plugins, once
sh testing/markdown/ascii-golden.sh
sh testing/markdown/ascii-driver.sh
HIGH_BUILD="$BUILD" sh testing/markdown/highlight-driver.sh
sh testing/markdown/look-driver.sh
(cd testing && pkill -9 -f -- '--test=/tmp/far2l'; sleep 1
 find tests -maxdepth 1 -type s -delete
 for s in tests/*; do [ -S "$s" ] && rm -f "$s"; done
 for t in $(ls tests); do pkill -9 -f -- '--test=/tmp/far2l'; sleep 0.5
  find tests -maxdepth 1 -type s -delete; for s in tests/*; do [ -S "$s" ] && rm -f "$s"; done
  o=$(./far2l-smoke-run.sh "$PWD/../far2l.markdown-colorer-look.build/install/far2l.app/Contents/MacOS/far2l" "$t" 2>&1) && echo "$o" | grep -q '\[DONE\]' && echo "PASS $t" || echo "FAIL $t"; done)
```

The full build (with the plugins) is part of the check: this story draws its colors from the
Colorer plugin. `0004-vt-shell` and `000b-find-file` fail today, for a reason we do not know.
Any other failure is new.

## Risks

- contract: `ColorerRegionLookV1` is a new export of the Colorer plugin. An old plugin binary
  without it gives the fork table, and the fence tokens keep working.
- contract: `MdRunColor` is the viewer-to-palette seam. It gains the underline bit in the word
  it returns; the signature does not change.
- The underline is visible only in a terminal that draws it (see `manual-checks.md`).

## Plan

| Task | Scope | Depends | Done check |
|---|---|---|---|
| T1 | The plugin export, the `FarEditorSet` accessors, the look driver, and the three measurements | - | `cmake --build "$BUILD" --target colorer -j8 && sh testing/markdown/look-driver.sh --case look_first_call && sh testing/markdown/look-driver.sh --case look_real_hrd && sh testing/markdown/look-driver.sh --case look_cost` exits 0 |
| T2 | The core bridge in `MdColorer`, with the shared module handle and a fake source | T1 | `cmake --build "$BUILD" --target far2l -j8 && sh testing/markdown/look-driver.sh --case look_bridge && sh testing/markdown/look-driver.sh --case look_codes` exits 0 |
| T3 | The `em` and `listmark` roles on the fork path, with the 0007 updates | - | `cmake --build "$BUILD" --target far2l -j8 && sh testing/markdown/look-driver.sh --case look_new_roles` exits 0 |
| T4 | The Colorer step in the resolver, the batch contrast verdict, and the cache | T2, T3 | `cmake --build "$BUILD" --target far2l -j8 && sh testing/markdown/look-driver.sh --case look_roles && sh testing/markdown/look-driver.sh --case look_contrast && sh testing/markdown/look-driver.sh --case look_ini && sh testing/markdown/look-driver.sh --case look_generation` exits 0 |
| T5 | The underline on screen in `MarkdownPrinter`, and the new 000d smoke test | T4 | `000d-view-md-look` prints `[DONE]` with no `FAILED` |
| T6 | The 000d smoke sessions: F4/F6 parity on one file, and a scheme change moving F3 | T5 | `000d-view-md-look` prints `[DONE]` with no `FAILED` |

## At merge

### What was built

Same as "What it builds", plus: in a 16-color terminal the F3 look is the console.hrd rows, the
scheme's 4-bit path. The table draws with box characters, and the scheme underline rides every
run the scheme underlines. F4 and F6 on the same file show the same scheme. A scheme change (the
editor's parse, then the reopen) moves the F3 look: the cache and generation contract work.

### Decisions changed during the build

None of the decision table's rows. The 000d expectations are the console.hrd rows, because the
harness runs a 16-color terminal (ruling R9). The truecolor far2l.hrd rows are asserted by the
driver's `look_real_hrd`.

### What the reviews caught

- T2: the code-2 gate now includes `drawSyntax` (accessor `GetSyntaxDraw`) - a scheme with syntax
  drawing off must fall back to the fork table.
- T4: same accessor on the gate (codes 1/2/3 answer no generation).
- T5: probes removed before the commit; the underline pass is one GetText/PutText pair per row.

### Run it

Run the Check section above; expect all PASS except the two known Mac failures (`0004-vt-shell`,
`000b-find-file`). In the built app, F3 a `.md` file. In a truecolor terminal you see the
far2l.hrd colors (`#8888ff` headings underlined, `#40a000` strong, `#ff20a0` emphasis). In a
16-color terminal you see the console.hrd rows, all underlined. F4 on the same file matches.

### What changed

- `0a0eef809` colorer: export the look of a region list for the F3 Markdown view. The new
  `ColorerRegionLookV1` dispatch entry, the far attribute build, and the generation counter in
  `FarEditorSet`.
- `99e1f2a4e` markdown: bridge the Colorer look export into the core palette. `MdColorer`
  fetches the ten-name list once per view. The cache rides the palette, and the codes drive the
  fallback.
- `4b21a5f35` markdown: resolve the Colorer look between the user file and the fork table. The
  resolver order, the underline bit on the run, and the 3:1 contrast verdict.
- `9e9ddd13f` markdown: print the Colorer look in the F3 viewer, underline included. The
  printer paints the foreground and the underline for a run, the 4-bit path included.
- `19f4464a9` markdown: check the Colorer look in the 000d smoke session. The real-plugin cell
  reads for the console.hrd rows.
- `3e708935b` markdown: record the Colorer look build at the merge gate. This gate page, the
  story ledger, and the close-out draft.
- `84a79a5c8` docs: record the Colorer look in the durable views: ADR-009, the viewer spec, the
  data view, and the invariants.

### How to verify

In the built app, F3 a `.md` file: headings and links show `#8888ff`, underlined, in a truecolor
terminal (far2l.hrd). Press F4 on the same file: the same scheme. Change the scheme (F4, Ctrl-F7,
pick a scheme, reopen F3): the look moves. The smoke `000d` asserts the console.hrd rows, because
the harness runs 16 colors; the truecolor rows are asserted by the driver's `look_real_hrd`.

### Spend

Budget $15, hard stop $20. The fix wave and the merge-gate check ride on this line; the harness
task costs are recorded at close-out. The build used the pre-baked cmake config, so the rebuild
after each task is incremental.
