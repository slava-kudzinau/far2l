---
description: Give each Markdown element in the F3 view its own color, from a fork-owned table of defaults that the user can override by value or by palette entry name.
status: in-review
size: M
risk: contract
depends-on: markdown-compact-layout
---

# Story: Markdown palette

## Goal

A reader of a Markdown file in F3 can tell headings, bold text, links, code, quotes and rules apart by color. Today headings, links, bold text and table headers all have one yellow color.

## Context

- [ADR-004](../../architecture/decisions/adr-004-palette-roles.md): this story changes it.
- [markdown.invariants.md](../../architecture/features/markdown/markdown.invariants.md): the `[colors]` rules change.
- [markdown-viewer.spec.md](../../architecture/features/markdown/markdown-viewer.spec.md): "Palette roles", and the open items on weak contrast.
- `AGENTS.md`: the rule "Use palette roles for colors. Do not use fixed ANSI colors." changes.
- Role code: `MdRole`, `MdRun` in `far2l/src/markdown/MarkdownRender.hpp`. `Builder::text_role()`, the horizontal rule, the table rows and `row_lead()` in `far2l/src/markdown/MarkdownRender.cpp`.
- Color code: `MarkdownRunColor` and `MarkdownPrinter::Print` in `far2l/src/ViewerPrinter.cpp`.
- Patterns to copy: `extractColor` in `far2l/src/console/farcolors.cpp` and `CorrectHighlightColorContrast` in `far2l/src/hilight.cpp` (real background), `wcagComputeContrast` in `utils/include/Colorspace.h`, `ExprToFarColor` in `far2l/src/console/farcolorexp.h`, `KeyFileHelper` and `InMyConfig` in `utils/include/`.

## Scope

**In:**

1. Roles. `MD_ROLE_HEADING` goes away. The roles are body, h1, h2, h3, strong, link, code, quote, rule and art. The h3 role covers H3 to H6.
2. A new file in `far2l/src/markdown/` holds this table of defaults:

| Role | Used for | 256 index | RGB | 16-color fallback | Today's look (light theme) |
|---|---|---|---|---|---|
| h1 | `#` text | 227 | `#FFFF5F` | `F_YELLOW` | `COL_HELPTOPIC` |
| h2 | `##` text | 215 | `#FFAF5F` | `F_LIGHTRED` | `COL_HELPTOPIC` |
| h3 | `###` to `######` text | 111 | `#87AFFF` | `F_LIGHTGRAY` | `COL_HELPTOPIC` |
| strong | bold text, table header text | 231 | `#FFFFFF` | `F_WHITE` | `COL_HELPTOPIC` |
| link | link text, autolinks | 120 | `#87FF87` | `F_LIGHTGREEN` | `COL_HELPTOPIC` |
| code | code spans, code fences, HTML blocks | 213 | `#FF87FF` | `F_LIGHTMAGENTA` | `COL_HELPHIGHLIGHTTEXT` |
| quote | quote text, quote bars | 146 | `#AFAFD7` | `F_CYAN` | `COL_VIEWERTEXT` |
| rule | horizontal rules, table borders | 103 | `#8787AF` | `F_CYAN` | `COL_PANELHIGHLIGHTTEXT` for borders, `COL_VIEWERTEXT` for rules |
| art | Mermaid art | none | none | none | `COL_PANELHIGHLIGHTTEXT` |
| body | all other text | none | none | none | `COL_VIEWERTEXT` |

3. Precedence for a run: code, link, strong, heading level, quote, body. Table header text with no other span is strong. Code and links in a table header keep their role. A heading in a quote keeps the heading role. Text in a table cell in a quote is quote text.
4. The full lead of a row in a quote has the quote role. Outside a quote the lead stays body.
5. Resolve at draw time, in this order. The background is always the `COL_VIEWERTEXT` background.
   1. If the user file sets the role, use that value.
   2. If the viewer background fails the contrast rule, use today's look.
   3. If `GetConsoleColorPalette` reports at least 8 bits, use the RGB default with its 16-color index.
   4. Otherwise use the 16-color fallback.
6. Contrast rule: all or none. Each RGB default must have 3:1 or more against the real viewer background. If one fails, all defaults go to today's look. An override from the user file has no check.
7. User file: `InMyConfig("settings/markdown-colors.ini")`, section `[markdown]`. Each role has one key: `body`, `heading1`, `heading2`, `heading3`, `strong`, `link`, `code`, `quote`, `rule`, `art`.
   far2l reads this file and does not write it.
8. Values: an `ExprToFarColor` expression (`foreground:#87AFFF`, `F_LIGHTBLUE`), or a name with `@` such as `@Help.Topic`. The name is a far palette entry from `ColorsInit`. It is resolved at draw time, so it follows the theme. The fork holds a small name list in `far2l/src/markdown/`, because `ColorsInit` is private to `farcolors.cpp`. The list holds at least `@Viewer.Text`, `@Help.Topic`, `@Help.Text.Highlight` and `@Panel.Text.Highlight`.
9. An unknown key, an unknown name, a bad value or a missing file changes nothing: that role uses its default. Only the foreground of a value is used.

**Out:**

- New `COL_*` entries and Colors menu items. The owner chose the fork table.
- A code block background tint (story markdown-code-tint).
- Syntax colors in fences (story markdown-code-highlight). The code role stays the single look of a fence here.
- Underline for links, and a look for emphasis. Emphasis stays body.
- The F5 view: `AnsiEsc::Printer` draws those rows, so the roles do not show. It must not break.
- The F4 editor, which gets its colors from the Colorer plugin. The browser view.
- Six separate colors for H3 to H6.

## Size

M. Three source files change (`MarkdownRender.cpp` and `.hpp`, `ViewerPrinter.cpp`) and one or two are new. One upstream file changes, by a few lines. One contract changes (ADR-004 and the rule in `AGENTS.md`). Test `0007` changes.

## Unknowns

- Does the smoke run (`--tty --nodetect`) report 8 bits or more from `GetConsoleColorPalette`? If not, the RGB path does not run in tests. The first task finds this out and gives the tests a way to reach that path.
- What `ExprToFarColor` returns for a bad value. The first task tests it, and the code must tell a bad value from a valid black.
- The default blue. Research found `#000080` in the palette code, and `#0000AA` in saved values. The real color also depends on the terminal, so it can remap the 16 colors. The contrast figures below show both. The owner checks the result in Ghostty.
- What a terminal without 256 colors does when it gets `38;5;n`. Step 3 of the resolve order avoids that case if `GetConsoleColorPalette` is correct. macOS Terminal needs a manual check.
- Whether the magenta look suits a full code fence. The owner decides at the manual check. Story markdown-code-highlight changes fences later.

## Acceptance criteria

- With the default palette at 8 bits or more, each role cell has the RGB of the table.
- The background of each cell is the viewer background.
- Bold text differs from H1, and H1 differs from H2. A bold or link span in a heading shows the strong or link color.
- H3 to H6 cells have one color.
- A code span in bold text has the code color. A code span in a table header has the code color.
- Table header text has the strong color. Table borders and horizontal rules have the rule color. Mermaid art keeps its palette entry and changes with the theme.
- A `---` line under a paragraph is an H2, and it has no rule cell.
- With `--norgb`, each role shows the 16-color fallback of the table, and no RGB bits take effect.
- On a light viewer background, every role shows today's look, with no RGB.
- On the black palette (`BlackColorsIndex16`), the RGB defaults show.
- An override with a hex value changes that role. An override with `@Help.Topic` follows a theme change at the next redraw.
- A hostile `markdown-colors.ini` (bad values, unknown keys and names, a very long line, an empty file) opens the viewer with defaults for the bad entries.
- Search highlight, selection in plain mode, Shift-F4, scrolling and F5 work as before. The rendered text and row count do not change.
- Test `0007` reads the cells and checks the items above. `sample.md` gets H2, H3, a link, a rule and a quote with a span. Its old color checks change as named in the Notes.
- The check command in `AGENTS.md` prints `check ok`.
- Close-out updates these docs:
  - ADR-004
  - the `[colors]` rules in `markdown.invariants.md`
  - the roles and open items in `markdown-viewer.spec.md`
  - the settings list in `markdown.data.md`
  - the color rule in `AGENTS.md`
- The new rule: a run holds a role. A role resolves from the user file, the fork table, or a far palette entry. Code outside the table holds no color value.

## Notes

- Owner's words, 2026-11-06: "we need different color for # and ## as yellow is for bold text", and "we can use other colors too, magent etc we are on 256 color terminals at least ghostty", and "is it posssible to use theme colors or create markdown entries and use ref color names?"
- Owner's choices, 2026-11-06:
  - Use the fork table plus names.
  - This story replaces markdown-theme-colors.
  - The tint goes to a later story.
  - H2 is orange, and body stays the theme color.
- Owner's words, 2026-11-06: "don't forget we will color code blocks with colorer". So this story must not block token colors from another source in fences. See story markdown-code-highlight.
- Research on options: existing entries give about 5 colors. The default table has no green, magenta or light blue text, so H2, links and code cannot each have a color. New `COL_MD*` entries touch about 8 upstream files. far2l has no reference to a color by name.
- `MarkdownRunColor` already keeps `FOREGROUND_TRUECOLOR` and the RGB bits, because it masks only the background.
- All RGB defaults are on the xterm color cube, so `TTYOutput` sends `38;5;n`. `--norgb` and `TERM=screen*` remove the true color flags, so the 16-color index shows.
- Bold does not show on a terminal: `TTYOutput` sends no bold attribute. Thus strong needs a color.
- Contrast by calculation. Nobody has seen these colors on a terminal.

| Role | On `#000080` | On `#0000AA` |
|---|---|---|
| h1 | 15.0 | 12.5 |
| h2 | 8.8 | 7.3 |
| h3 | 7.3 | 6.1 |
| strong | 16.0 | 13.3 |
| link | 12.7 | 10.6 |
| code | 7.7 | 6.4 |
| quote | 7.6 | 6.3 |
| rule | 4.7 | 3.9 |

- This story must change these checks in `0007`. They are not regressions:
  - "Heading Four same as heading One"
  - the table header same as heading
  - the table border same as flowchart art
  - block quote same as body
- The smoke framework can read colors: `ReadCell` gives `ForeTC` and `IsForeTC`. See `testing/README.md`.
- Tier from `flow-for.mjs`: `flow tier=full docs=spec,plan design=no orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=none`. The owner reads the gate before the build.
- Branch `feat/markdown-palette`. Base `next/2.9.1-ovr.1`, after `markdown-compact-layout` merges. Budget and hard stop M $15/$20.
- Check: "The check" in `AGENTS.md`, with `BUILD` set to the build folder of the worktree.
- started: 2026-11-06T17:29:02Z
- Worktree: `.worktrees/markdown-palette` under the compact-layout worktree, stacked on `feat/markdown-compact-layout` (a7b61f26f, done, unmerged). BASE recorded in the ledger.
- Budget and hard stop confirmed: M $15/$20.
- Flow: `flow tier=full docs=spec,plan design=no orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=none`
- Gate page: stop for the owner (G2-design), per the flow line.
- G2-design waived by owner, 2026-11-06: "go in auto mode, nothing is build yet". The flow line names no verifier, so the build starts.
