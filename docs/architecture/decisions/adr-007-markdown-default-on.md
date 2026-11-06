---
description: F3 on a md or markdown file opens Markdown mode when the Markdown_Auto setting is on, and the position cache stores only a deviation from that rule.
status: accepted
date: 2026-11-06
---

# ADR-007: Markdown mode on by default, with a per-file choice

## Context

- Before this decision, a user had to press Shift-F4 after F3 on every `.md` file. One global flag held the mode, and a later viewer inherited the mode of the last file.
- The owner decided on 2026-11-06 that F3 on a Markdown file shows the rendered view at once ("it should be on").
- Shift-F4 must still keep a file in plain text. The position cache keeps a value per file in `Param[3]`.
- The first cache scheme saved the default as a choice. The check box then did nothing for any file that the user had read. The final review found this.

## Decision

- The viewer setting `Markdown_Auto` (section `Viewer`, default 1, check box "Open Markdown files in Markdown mode") turns the rule on and off.
- The rule: a file name that ends in `.md` or `.markdown` opens in Markdown mode when the setting is on. The test ignores letter case. A file named exactly `.md` does not match.
- Every other file opens plain, even after a Markdown file.
- Start order: a saved choice for the file wins, then the rule, then plain.
- `Param[3]` stores only a deviation from the rule (`MarkdownCacheParam`, `MarkdownStartMode` in `far2l/src/viewer.cpp`):
  - 0 follows the rule.
  - 1 is Markdown against a plain rule.
  - 2 is plain against a Markdown rule.
- The rule is read once when a file opens and kept in `Viewer::m_md_rule`. Both save sites use that value.

## Consequences

- Turning the setting off changes every file that the user did not change by hand.
- A file that was saved as plain (0) before this change now follows the rule. A `.md` file opens in Markdown, and Shift-F4 once keeps plain.
- An old far2l build reads a saved 2 as Markdown. Switching builds on one profile can show Markdown for a file chosen as plain.
- `Opt.ViOpt.ViewerMarkdownMode` ("Markdown" key) is written by Shift-F4 and not read at start.
- `far2l/src/hist/poscache.hpp` still documents viewer `Param[3] = 0`. The comment is stale.
- A Quick View viewer that is open while the setting changes cannot freeze a default as a choice, because of `m_md_rule`.

## Rejected alternatives

### Always force Markdown on `.md` files

**Why rejected:** Shift-F4 could never keep a file in plain text.

### Keep inheriting the mode of the last file

**Why rejected:** a text file opened in Markdown mode after a `.md` file.

### Sniff the file content, or reset the cache

**Why rejected:** the suffix is simple and predictable. A cache reset would also reset the saved positions.

### Save the mode itself in the cache (the first scheme)

**Why rejected:** it froze defaults as choices, so the check box had no effect on files that the user had read. Revisit only with a new cache format.
