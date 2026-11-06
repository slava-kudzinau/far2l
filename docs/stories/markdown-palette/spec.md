# Spec: Markdown palette

Spec for the `markdown-palette` story. The story brief (`story.md`) governs. Exact values live here and in `plan.md` Global Constraints.

## Unit under test: role resolution

`far2l/src/markdown/` holds the role table and the resolver. `MarkdownRunColor` in `far2l/src/ViewerPrinter.cpp` calls the resolver at draw time.

### Roles

`MD_ROLE_HEADING` goes away. The roles are, with their defaults:

| Role | Used for | 256 index | RGB | 16-color fallback |
|---|---|---|---|---|
| body | all other text | none | none | none (viewer text) |
| h1 | `#` text | 227 | `#FFFF5F` | `F_YELLOW` |
| h2 | `##` text | 215 | `#FFAF5F` | `F_LIGHTRED` |
| h3 | `###` to `######` text | 111 | `#87AFFF` | `F_LIGHTGRAY` |
| strong | bold text, table header text | 231 | `#FFFFFF` | `F_WHITE` |
| link | link text, autolinks | 120 | `#87FF87` | `F_LIGHTGREEN` |
| code | code spans, code fences, HTML blocks | 213 | `#FF87FF` | `F_LIGHTMAGENTA` |
| quote | quote text, quote bars | 146 | `#AFAFD7` | `F_CYAN` |
| rule | horizontal rules, table borders | 103 | `#8787AF` | `F_CYAN` |
| art | Mermaid art | none | none | none (today's look) |

### Role assignment

| Source element | Role |
|---|---|
| Paragraph, list item, emphasis, hint row | body |
| `#` heading text | h1 |
| `##` heading text | h2 |
| `###` to `######` heading text | h3 |
| `**bold**`, `__bold__`, table header text with no other span | strong |
| Link text, autolinks | link |
| Code spans, code fences, HTML blocks | code |
| Quote text, quote bars (the `│ ` lead) | quote |
| Horizontal rules, table borders | rule |
| Mermaid art rows | art |

### Precedence for a run

1. code
2. link
3. strong
4. heading level
5. quote
6. body

A heading in a quote keeps the heading role. Text in a table cell in a quote is quote text. Code and links in a table header keep their role. The full lead of a row in a quote has the quote role; outside a quote the lead stays body.

### Resolution order at draw time

The background is always the `COL_VIEWERTEXT` background. For the foreground:

1. If the user file sets the role, use that value. No contrast check applies to an override.
2. If the viewer background fails the contrast rule (below), use today's look. The entries are:
   - `COL_HELPTOPIC` for h1, h2, h3 and strong
   - `COL_HELPHIGHLIGHTTEXT` for code
   - `COL_VIEWERTEXT` for quote, body, horizontal rules and table borders (one role covers both)
   - `COL_PANELHIGHLIGHTTEXT` for art
3. If `GetConsoleColorPalette` reports at least 8 bits per channel, use the RGB default of the table with its 16-color index.
4. Otherwise use the 16-color fallback of the table.

`body` and `art` never take an RGB value: outside a user override, `body` stays `COL_VIEWERTEXT` and `art` stays `COL_PANELHIGHLIGHTTEXT` in every step.

### Contrast rule

All or none. Each RGB default must have contrast 3.0 or more against the real viewer background. The check uses the contrast functions of `utils/include/Colorspace.h`. If one RGB default fails, all defaults go to today's look (step 2). An override from the user file has no check.

### User file

Path: `InMyConfig("settings/markdown-colors.ini")`. Section `[markdown]`. One key per role: `body`, `heading1`, `heading2`, `heading3`, `strong`, `link`, `code`, `quote`, `rule`, `art`.

Value forms:

| Form | Example | Meaning |
|---|---|---|
| Expression | `foreground:#87AFFF` | `ExprToFarColor` expression |
| Name | `@Help.Topic` | A far palette entry from `ColorsInit`, resolved at draw time |

Accepted names include at least `@Viewer.Text`, `@Help.Topic`, `@Help.Text.Highlight`, `@Panel.Text.Highlight`. The fork holds the name list in `far2l/src/markdown/`, because `ColorsInit` is private to `farcolors.cpp`.

Rules:

- far2l reads this file and never writes it.
- The accepted grammar is exactly `foreground:<value>` or `<value>`, with `<value>` = `#rrggbb`, `#rgb`, an `F_*` color name, or `@Name` with the names above. No quoted values, no other forms.
- An unknown key, an unknown name, a bad value or a missing file changes nothing: that role uses its default. A bad value must be told from a valid black.
- Only the foreground of a value is used. The background of each cell stays the viewer background.

## Validation

| Call | Result |
|---|---|
| User file with a hex value for `heading1` | h1 cells show that value |
| User file with `@Help.Topic` for `link` | link cells follow a theme change at the next redraw |
| User file with an unknown key | That key is ignored; the role keeps its default |
| User file with an unknown name or bad value | That role uses its default; no crash, no wrong color |
| User file empty or missing | All defaults |
| Viewer background with contrast under 3.0 against one RGB default | All defaults go to today's look |
| `--norgb` | Every role shows its 16-color fallback; no RGB bits take effect |
| Light viewer background | Every role shows today's look, with no RGB |
| Black palette (`BlackColorsIndex16`) | The RGB defaults show |
| Theme change | Colors change at the next redraw, with no new render |

## Done

`sh docs/stories/markdown-palette/check.sh` exits 0 and prints `check ok`. The script is the check of `AGENTS.md`, with `BUILD` set for the worktree. Every table row above has a check in `testing/tests/0007-view-file-markdown/`.

## Out of scope

- New `COL_*` entries and Colors menu items.
- A code block background tint (story markdown-code-tint).
- Syntax colors in fences (story markdown-code-highlight). The code role stays the single look of a fence here.
- Underline for links, a look for emphasis (emphasis stays body).
- Six separate colors for H3 to H6 (h3 covers H3 to H6).
- The F5 view, the F4 editor, the browser view.
