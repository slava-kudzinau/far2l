---
description: Markdown runs carry a palette role, and the viewer resolves the role to a far theme color when it draws.
status: accepted
date: 2026-11-05
---

# ADR-004: Palette roles, not fixed colors

## Context

- The first Markdown mode used fixed ANSI colors. The owner reported black blocks behind tables and box glyphs, red everywhere, and a rainbow of colors.
- One cause was a type error. `SetFarColor(uint16)` treats its value as a palette index. The viewer passed a 64-bit real color cut to 16 bits.
- The owner first ruled that every run uses `COL_VIEWERTEXT` and structure shows by layout only. Reading then needed more cues, and the owner asked for a few theme entries and no rainbow.

## Decision

A run holds an `MdRole` (`MD_ROLE_BODY`, `MD_ROLE_H1`, `MD_ROLE_H2`, `MD_ROLE_H3`, `MD_ROLE_STRONG`, `MD_ROLE_LINK`, `MD_ROLE_CODE`, `MD_ROLE_QUOTE`, `MD_ROLE_RULE`, `MD_ROLE_ART`), not a color. `MarkdownRunColor` in `far2l/src/ViewerPrinter.cpp` calls `MdRunColor` in `far2l/src/markdown/`, which resolves the role at draw time. The background of every cell is the viewer text background.

| Role | Used for |
|---|---|
| body | text, lists, emphasis, table body, the hint row |
| h1, h2, h3 | heading text, by level. h3 covers H3 to H6. |
| strong | bold text, table header text |
| link | link text, autolinks |
| code | code spans, code fences, HTML blocks |
| quote | quote text, quote bars |
| rule | horizontal rules, table borders |
| art | diagram art |

The resolver uses this order:

1. A value for the role in `settings/markdown-colors.ini`, section `[markdown]`. far2l reads this file and never writes it. A value is an expression (`foreground:#87AFFF`) or a palette entry name (`@Help.Topic`). A bad value, an unknown name or a missing file leaves the default.
2. Today's look, when the contrast rule fails. The entries are:
   - `COL_HELPTOPIC` for the headings and strong
   - `COL_HELPHIGHLIGHTTEXT` for code
   - `COL_VIEWERTEXT` for quote and for rule (horizontal rules and table borders)
   - `COL_PANELHIGHLIGHTTEXT` for art
3. The fork's RGB default, when `GetConsoleColorPalette` reports 8 bits or more.
4. The fork's 16-color fallback.

The fork holds the table of defaults in `far2l/src/markdown/MdPalette.cpp`. The contrast rule is all or none. Each RGB default needs 3:1 or more against the real viewer background, or all defaults use step 2. An override has no contrast check.

## Consequences

- A theme change shows on the next redraw, because roles are resolved when the screen is drawn.
- Layout also marks structure (the `│ ` quote prefix, indented code), so a theme with equal entries still reads.
- A new element takes an existing role. A new role needs a row in the fork table and an update of this ADR.
- The autolink case: `Viewer::UnderlineLinks` recolors autolink text after the printer, so `MD_ROLE_LINK` does not show on an autolink cell. A labeled link keeps the link role. This is open owner item 1 below.
- Open owner items:
  - Autolink color is the viewer's link highlight, not the link role.
  - In `BlackColorsIndex16` the 16-color fallbacks stay close to the body color.
  - The default blue behind the contrast check varies by terminal: `#000080` in the code, `#0000AA` in saved values.

## Rejected alternatives

### Fixed ANSI colors per element

**Why rejected:** they ignore the theme and gave black and red blocks. The owner asked for theme colors only.

### One color for all runs (the first owner ruling)

**Why rejected:** it left headings, code and diagrams hard to tell apart. It was replaced by the four roles.
