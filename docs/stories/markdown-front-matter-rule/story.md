---
description: The F3 Markdown view shows the closing front matter line as a rule row, so the front matter reads as a region delimited top and bottom.
status: done
size: S
refs: markdown-viewer.spec.md
---

# Story: markdown-front-matter-rule

## Goal

A Markdown file with YAML front matter, such as a skill's `SKILL.md`, opens in F3 with a rule under the keys. Before, the opening `---` showed as a rule and the closing `---` showed nothing, so the block looked cut open at the bottom.

## Context

The viewer parses with the vendored md4c (ADR-002), a pre-0.5 version with no front matter extension. The runtime flow is in markdown.runtime.md. The rows contract is in markdown-viewer.spec.md.

## Scope

**In:** `MarkdownRender.cpp`: a setext heading draws its consumed underline line as a rule row. The smoke test `000c-view-md-frontmatter`. The pagination updates in `0007-view-file-markdown` that follow from one extra row. The Heading and Horizontal rule rows of markdown-viewer.spec.md.

**Out:** a front matter key/value table like the browser view, front matter folding, the front matter of TOML (`+++`) files, upgrading the vendored md4c.

## Size

S. One render rule, one fixture, two test files. No new role, no setting, no cache change.

## Unknowns

None. The vendored md4c fires no callback for a consumed underline, so the renderer recovers the line from the source; a 0.5 upgrade with `MD_FLAG_FRONT_MATTER` can replace the scan later, as one change in `MarkdownRender.cpp`.

## Acceptance criteria

- F3 on a file whose first line is `---` shows a rule row directly under the last front matter key line.
- A setext heading of level 1 and of level 2 shows a rule row for its underline, and keeps its source blank rows.
- An ATX heading keeps no rule row; a thematic break under it keeps its own rule row.
- The Markdown smoke check passes: golden, driver, tests 0007, 0008, 0009, 000c.

## Notes

Root cause, from runtime evidence: md4c consumes the closing `---` as the underline of a setext H2 and fires no text event for it, so the renderer drew the heading text and dropped the marker. The fix reads the line after the heading's last text from the source, checks it against the level's marker, and draws `rule_row()` for it. The HR case shares the same helper.
