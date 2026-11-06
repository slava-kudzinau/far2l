# Plan: Markdown palette

Spec: `spec.md` (governs exact values). Design: none — the plan carries the design.

## Global Constraints

- Language C++, far2l coding rules in `CODESTYLE.md` (CamelCase types, snake_case variables, `_` prefix for private fields, tabs for indent, 110-char soft limit). New code goes in `far2l/src/markdown/`. The change in `far2l/src/ViewerPrinter.cpp` stays small (a call into the new module plus a small dispatch).
- Files this story may touch: `far2l/src/markdown/MarkdownRender.hpp`, `MarkdownRender.cpp`, `ViewerPrinter.cpp`, `far2l/CMakeLists.txt` (new source into the `far2l_md` target only), `testing/far2l-smoke-run.sh`, and `testing/tests/0007-view-file-markdown/` (test.js, fixtures). Also new files `far2l/src/markdown/MdPalette.hpp` and `MdPalette.cpp` (these exact names). Nothing else without a ruling.
- Owner-checked paths (never edit): `AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`, `testing/markdown/golden/*.txt`, `far2l/src/markdown/md4c/`, `far2l/third_party/quickjs/`, `.github/workflows/`.
- Existing facts (from the survey, trust these):
  - `enum MdRole : uint64_t { MD_ROLE_BODY = 0, MD_ROLE_HEADING, MD_ROLE_CODE, MD_ROLE_ART };` at `MarkdownRender.hpp:17-22`. `MdRun { std::wstring text; uint64_t role; }` at `:25-28`.
  - Role assignment sites: `Builder::text_role()` (`MarkdownRender.cpp:230-238`). Heading open `:374-377` (`b.heading_level` holds the level). Horizontal rule `:412`, today body. Table borders in `row_rule` `:559-562`, today art. Table cells `:551`. Quote lead runs `:220-222`, today body. Art rows `:734`. Hint row `:751`.
  - `MarkdownRunColor(uint64_t role)` at `ViewerPrinter.cpp:116-127` masks the background with `kBackBits` (`:110`). `MarkdownPrinter::Print` sets the color per run at `:170` and fills trailing cells with `FarColorToReal(COL_VIEWERTEXT)` at `:206-207`.
  - `ExprToFarColor(const char *exsp, size_t len)` is public (`console/farcolorexp.h:35`) and returns 0 both for a bad expression and for a valid black. The palette module must validate the value syntax itself before calling it, and treat a value that fails its own validation as unset.
  - Palette names map positionally in `ColorsInit[]` (`farcolors.cpp:43-52`). Relevant entries: `Viewer.Text` (COL_VIEWERTEXT), `Help.Topic` (COL_HELPTOPIC), `Help.Text.Highlight` (COL_HELPHIGHLIGHTTEXT), `Panel.Text.Highlight` (COL_PANELHIGHLIGHTTEXT). `ColorsInit` is static to `farcolors.cpp`; the fork holds its own name-to-COL_ list in `far2l/src/markdown/`.
  - `double wcagComputeContrast(const RGB& fg, const RGB& bg)` (`utils/include/Colorspace.h:40`), RGB fields 0..1, ratio at least 1.0. Convert a real color with `extractColor(uint64_t color, RGB& fg, RGB& bg)` (`farcolors.hpp:108`).
  - `GetConsoleColorPalette` returns 4, 8 or 24 (`WinPortDecl.h:51`). The smoke harness does not set `COLORTERM`, so the probe path usually returns 4 and the RGB path does not run. The wrapper `testing/far2l-smoke-run.sh` must export `COLORTERM=truecolor` so tests reach the RGB path; `--norgb` cases stay on the fallback.
  - `ReadCell` in the smoke test exposes `Fore` (4-bit), `ForeTC` (24-bit) and `IsForeTC` (`testing/README.md:86-95`). Tests treat `Fore*` as the foreground.
- Exact defaults: the table in `spec.md` (Roles) governs, byte for byte. Do not invent other values.
- Precedence for a run: code, link, strong, heading level, quote, body (spec, Precedence).
- Resolve order and contrast rule: spec, Resolution order and Contrast rule. The contrast threshold is 3.0, all or none.
- User file: `InMyConfig("settings/markdown-colors.ini")`, section `[markdown]`, keys `body`, `heading1`, `heading2`, `heading3`, `strong`, `link`, `code`, `quote`, `rule`, `art`. Read-only; far2l never writes it. Accepted names: at least `@Viewer.Text`, `@Help.Topic`, `@Help.Text.Highlight`, `@Panel.Text.Highlight`.
- The rendered text and row count do not change. Only colors change.
- The F5 `AnsiEsc::Printer` path must not break; it does not show role colors.
- Commit message: a short imperative summary line, a blank line, then the trailer `Co-Authored-By: Claude <noreply@anthropic.com>`. One commit per task.
- Check: `sh docs/stories/markdown-palette/check.sh` from the worktree root (`BUILD` points at the worktree build folder; the script also clears stale sockets). The build folder for this worktree is `../../../far2l.palette.build` (create it with the cmake line of `AGENTS.md` on first use).

## T1: Probe and test hooks

**Depends on:** none
**Done check:** `sh testing/markdown/ascii-driver.sh` exits 0 in the worktree build. A scratch smoke run of `0007` with `COLORTERM=truecolor` shows `IsForeTC` true on a colored cell; report the observed cells.
**Role:** implementer — review final

- Create the build folder `../../../far2l.palette.build` with the cmake line of `AGENTS.md` (`-DTESTING=Yes -DUSEWX=No -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0`).
- Add `export COLORTERM=truecolor` to `testing/far2l-smoke-run.sh` (before the far2l invocation), so `GetConsoleColorPalette` reports 24 in smoke tests. Do not touch the `--norgb` mechanism: a test that passes `--norgb` still gets the fallback path.
- Confirm by a scratch run that a heading cell in `0007` reports `IsForeTC == true` and a plausible `ForeTC`. Record the observed values in your report; T4 turns them into assertions.
- Confirm `ExprToFarColor("foreground:#87AFFF", 18)` returns a nonzero real color, and record what it returns for `garbage!!` (expected 0). Record both in the report. No code change in this bullet.

## T2: Roles in the renderer

**Depends on:** none
**Done check:** the check builds, and `sh testing/markdown/ascii-driver.sh` exits 0. The golden and driver tests prove no layout change. The color assertions of T4 do not pass yet.
**Role:** implementer — review task-reviewer-critical (public contract: the `MdRole` enum changes)

- In `MarkdownRender.hpp`: replace the enum values with `MD_ROLE_BODY = 0, MD_ROLE_H1, MD_ROLE_H2, MD_ROLE_H3, MD_ROLE_STRONG, MD_ROLE_LINK, MD_ROLE_CODE, MD_ROLE_QUOTE, MD_ROLE_RULE, MD_ROLE_ART`. Keep `MD_ROLE_BODY = 0`. `MdRun.role` stays `uint64_t`.
- In `Builder::text_role()` (`MarkdownRender.cpp:230-238`): keep `span_code > 0` first, then `span_link > 0` → `MD_ROLE_LINK`, then `span_strong > 0` → `MD_ROLE_STRONG`, then heading level: 1 → `MD_ROLE_H1`, 2 → `MD_ROLE_H2`, 3 to 6 → `MD_ROLE_H3`, else body. This implements the precedence of the spec (code, link, strong, heading, quote, body) for inline runs.
- Heading text runs take the role of the current heading level (they flow through `text_role()`).
- Quote text: inside `quote_depth > 0`, a run whose role would be body gets `MD_ROLE_QUOTE`. Runs with code, link, strong or heading role keep it (spec precedence).
- Quote bars: in `row_lead()` (`:196-204`) and the blank-spacer lead (`:306`), when `quote_depth > 0` the bar runs get `MD_ROLE_QUOTE`; outside a quote they stay body.
- Horizontal rule: `b.append(hr, MD_ROLE_RULE)` at `:412`.
- Table borders (`row_rule`, `:559-562`): `MD_ROLE_RULE`. Table cell padding runs (`:548`, `:552`): body, as today. Header cells keep the cell run's own role; a header cell with no other span gets `MD_ROLE_STRONG` (the header role replaces the old `MD_ROLE_HEADING`).
- Mermaid art stays `MD_ROLE_ART`; the hint row stays body.
- Do not change layout, text or row count anywhere.

## T3: The palette module and draw-time resolution

**Depends on:** T2
**Done check:** the check builds. A scratch smoke run of `0007` shows, in the report, at least three roles with different `ForeTC` values, different from the old values. T4 makes the full assertions.
**Role:** implementer — review task-reviewer-critical (public contract: ADR-004 changes)

- New files `far2l/src/markdown/MdPalette.hpp` / `.cpp` (or a name in that style), added to the `far2l_md` target in `far2l/CMakeLists.txt`.
- API (minimum): `uint64_t MdRunColor(uint64_t role)` — the entry point `MarkdownRunColor` calls. It resolves the role and returns a real color whose background bits are the viewer background (mask with the `kBackBits` pattern of `ViewerPrinter.cpp:110`).
- Internals, in resolve order (spec):
  1. User overrides: read from `InMyConfig("settings/markdown-colors.ini")`, section `[markdown]`, with `KeyFileHelper`. A "draw-time batch" is one call of `MdRunColor`. The module re-reads the file when its mtime changed since the last read, else uses the cache. A stat error on the ini counts as "no file": that role uses its default. Value forms: an `ExprToFarColor` expression, or `@Name` with the fork's name list (`Viewer.Text` → `COL_VIEWERTEXT`, `Help.Topic` → `COL_HELPTOPIC`, `Help.Text.Highlight` → `COL_HELPHIGHLIGHTTEXT`, `Panel.Text.Highlight` → `COL_PANELHIGHLIGHTTEXT`). The accepted grammar is exactly: `foreground:<value>` or `<value>`. A `<value>` is `#rrggbb`, `#rgb`, an `F_*` color name, or `@Name` with the four names above. No quoted values, no other forms. Validate the syntax before calling `ExprToFarColor`. Anything else, an unknown name, an empty file or a missing file leaves that role at its default. An override replaces the whole resolve chain for that role, with no contrast check.
  2. Today's look. The entries are:
     - `COL_HELPTOPIC` for h1, h2, h3 and strong
     - `COL_HELPHIGHLIGHTTEXT` for code
     - `COL_VIEWERTEXT` for quote
     - `COL_PANELHIGHLIGHTTEXT` for art and rule (table borders and horizontal rules both)
  3. RGB defaults: when `GetConsoleColorPalette` reports at least 8, build the real color from the spec table. The RGB value carries its 16-color index, so a terminal that ignores RGB shows the index. Follow the truecolor real-color layout that `MarkdownRunColor` uses today. Run the contrast rule first. Convert the viewer background and each RGB default with `extractColor`. Then check `wcagComputeContrast` at 3.0 or more, all or none. On failure use step 2. `body` and `art` never take an RGB default.
  4. Otherwise the 16-color fallback of the spec table (`F_YELLOW`, `F_LIGHTRED`, `F_LIGHTGRAY`, `F_WHITE`, `F_LIGHTGREEN`, `F_LIGHTMAGENTA`, `F_CYAN`, `F_CYAN`).
- The `@Name` values resolve through `FarColorToReal` at call time, so a theme change shows on the next redraw. Do not cache resolved far entries across a theme change.
- In `ViewerPrinter.cpp`: `MarkdownRunColor` becomes a small dispatch to `MdRunColor` plus the background mask it already has. The 4-bit `MD_ROLE_BODY` path returns `FarColorToReal(COL_VIEWERTEXT)` as today.
- The trailing-cells fill at `:206-207` stays `COL_VIEWERTEXT`.

## T4: Smoke test coverage

**Depends on:** T3
**Done check:** `sh docs/stories/markdown-palette/check.sh` exits 0 and prints `check ok`.
**Role:** implementer — review task-reviewer (parser-adjacent test script, not critical)

- Fixtures: extend `testing/tests/0007-view-file-markdown/initdir/left/sample.md` with an H2 heading, an H3 heading, an autolink or labeled link, and a thematic break. Put the `---` on its own line, not under a paragraph. Add a quote that contains a bold span and a code span. Keep the existing content that other checks use.
- Assertions in `test.js`, using `ReadCell` and the existing helper style:
  - Each role cell pair differs. Pairs: h1 vs h2, h2 vs h3, h1 vs strong, strong vs link, link vs code, code vs quote, quote vs rule. Assert on `IsForeTC ? ForeTC : Fore`, so both paths work.
  - H3, H4, H5, H6 cells share one color.
  - A bold span in a heading shows the strong or link color, not the heading color. A code span in a table header shows the code color. Table header text with no other span shows the strong color.
  - Table borders and the horizontal rule share the rule color. Mermaid art keeps its own color (differs from rule).
  - A `---` line under a paragraph renders as an H2 with no rule cell (text assertion; the layout story covers the rule).
  - Background of every checked cell equals the viewer background (`CheckBack` style, as today).
  - A `--norgb` case: run the same file with the flag. Assert each role shows a readable 4-bit `Fore` (the fallback index) and `IsForeTC` is false.
  - Overrides: write `settings/markdown-colors.ini` into the test config dir with `heading1=foreground:#00FF00` before the run. Assert the h1 cell shows that value. A second case sets `link=@Help.Topic`. Attempt both sub-cases. Park only with a recorded reason that the harness cannot observe a theme change.
  - A hostile ini case: unknown key, bad value, very long line, empty file. The viewer opens with defaults for the bad entries. Assert the h1 fore equals the h1 default.
- Update the four assertions the story names as changed by design. They are not regressions: "Heading Four same as heading One" becomes H4 shares the h3 color. The table header check becomes the strong color. The table border vs flowchart art check becomes rule vs art, and they differ. Block quote vs body becomes they differ.

## T5: Manual-check notes and handover

**Depends on:** T4
**Done check:** `docs/stories/markdown-palette/manual-checks.md` exists in the story folder and lists the two items below with what to look at. The full check passes.
**Role:** implementer — review final

- Write `docs/stories/markdown-palette/manual-checks.md` (working tree, committed on the story branch) with two sections, one line of "what to look at" each:
  - macOS Terminal (a terminal without 256 colors): open a sample `.md`. Check that each role shows its 16-color fallback and stays readable. Record what `38;2` sequences do there.
  - Ghostty, one dark theme and one light theme: check the RGB defaults read well. Check that H2 orange, the link green and the code magenta suit a full fence. The owner decides on the look here.
- No code change in this task.

## Task roles summary

| Task | Role | Review |
|---|---|---|
| T1 | implementer | final |
| T2 | implementer | task-reviewer-critical |
| T3 | implementer | task-reviewer-critical |
| T4 | implementer | task-reviewer |
| T5 | implementer | final |
