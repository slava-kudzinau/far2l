---
description: Markdown mode scrolls by an index into the rendered lines, and the file offset of each line is used only for the position cache.
status: accepted
date: 2026-11-05
---

# ADR-003: Scroll Markdown mode by rendered line index

## Context

- The viewer scrolls text by file offset. In Markdown mode the screen lines come from a render pass, not from the file bytes.
- The first Markdown mode counted md4c text bytes as `Builder::pos`. Down and PgDn did not rebuild the screen, and PgDn showed a blank screen. Only Home and End worked.
- The whole file is rendered at once, so every line is known.

## Decision

The top of the screen in Markdown mode is `Viewer::m_md_top`, an index into `MarkdownDoc::Lines()`. Each `MdLine` also carries `nFilePos`, the offset of the source line that holds its first text. The viewer uses `nFilePos` only to find a line for a file position (position cache, mode switch, resize) and to save the position.

## Consequences

- `MarkdownDoc::Lines()` offsets never decrease down the document. A row that replaces a fence takes the fence offset. Code that adds rows must keep this rule.
- Down stops at `MarkdownMaxTop`, the last full page. The literal "last line" shows a blank screen, so it was not built.
- The percent in the status line follows the line index. A resize keeps the same top line.
- `MarkdownView()` (Markdown on, hex off) guards the Markdown keys, so hex mode is not changed.
- Offsets are UTF-8 offsets of the decoded text. They are not exact for UTF-16 files and other code pages (unverified).

## Rejected alternatives

### Make `Builder::pos` a real file offset and keep byte scrolling

**Why rejected:** the whole file is rendered at once, so a line index is simpler. A line index cannot blank the screen. Revisit if the viewer must render only part of a file.
