---
description: The viewer parses Markdown with the vendored md4c event API and lays out the whole file in one pass.
status: accepted
date: 2026-11-05
---

# ADR-002: Parse Markdown with md4c

## Context

- The F3 viewer needs a Markdown mode (Shift-F4). It must show headings, lists, tables, code fences, links and quotes.
- far2l is C and C++ and had no Markdown code.
- The parser must run in the viewer process, with no new runtime tool, and must work on all far2l platforms.
- The first plan file (`local://viewer-markdown-preview-plan.md`) is not in the repository, so the study behind this choice is not recorded.

## Decision

Vendor md4c 0.5.2 (MIT) in `far2l/src/markdown/md4c/` and use its event API (`md_parse` with `MD_PARSER` callbacks). `MarkdownDoc::Render` in `far2l/src/markdown/MarkdownRender.cpp` makes the screen lines for the whole file in one pass. Only `md4c.c` is built.

## Consequences

- md4c 0.5.2 gives no byte offsets in block details. The `text` callback points into the input, so `Builder::offset_of` derives the source offset from that pointer. A line takes the offset of the source line that holds its first text.
- The parse flags are fixed in `MarkdownDoc::Render`: tables, task lists, strikethrough, permissive auto links and underline.
- A new md4c version needs a check of that pointer rule.

## Rejected alternatives

### Other Markdown parsers

**Why rejected:** no other parser is named in the record. Revisit if md4c cannot parse a construct that readers need.
