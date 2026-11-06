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

A run holds an `MdRole` (`MD_ROLE_BODY`, `MD_ROLE_HEADING`, `MD_ROLE_CODE`, `MD_ROLE_ART`), not a color. `MarkdownRunColor` in `far2l/src/ViewerPrinter.cpp` maps each role to a far palette entry at draw time. The background of every cell is the viewer text background.

| Role | Used for | Palette entry |
|---|---|---|
| body | text, lists, emphasis, quote bars, table body | `COL_VIEWERTEXT` |
| heading | headings, links, strong text, table header | `COL_HELPTOPIC` |
| code | code spans and fences | `COL_HELPHIGHLIGHTTEXT` |
| art | diagram art, table borders | `COL_PANELHIGHLIGHTTEXT` |

## Consequences

- A theme change shows on the next redraw, because roles are resolved when the screen is drawn.
- Layout also marks structure (the `│ ` quote prefix, indented code), so a theme with equal entries still reads.
- The reading story close-out records two weak spots. In the black and white theme (`BlackColorsIndex16`), code and art equal the body foreground. In the default theme, art and code are the closest pair. Both are open owner items, and this ADR did not recheck them.
- New elements take an existing role. A new role needs a new palette entry and an update of this ADR.

## Rejected alternatives

### Fixed ANSI colors per element

**Why rejected:** they ignore the theme and gave black and red blocks. The owner asked for theme colors only.

### One color for all runs (the first owner ruling)

**Why rejected:** it left headings, code and diagrams hard to tell apart. It was replaced by the four roles.
