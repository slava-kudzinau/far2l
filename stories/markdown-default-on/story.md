---
status: in-review
size: S
risk: none
depends-on: markdown-preview-reading
---

# Story D: F3 on a Markdown file opens the Markdown view by default

## Why
Owner decision 2026-11-06: pressing F3 on a `.md` file should show the rendered view at once. Today the user must also press Shift-F4.

## What
- When the viewer opens a file whose name ends in `.md` or `.markdown` (case-insensitive), it starts in Markdown mode. Shift-F4 still switches to plain text and back.
- A viewer setting "Open Markdown files in Markdown mode" turns this off. It is on by default. It is saved with the other viewer settings.
- Moving to another file inside the viewer applies the rule to the new file only: a `.md` file gets Markdown mode, any other file keeps the plain view. A manual Shift-F4 choice holds until the file changes.

## Not in this story
- Other file types; the browser view (story B); diagram drawing (story C).

## Notes
- Branch `markdown-preview/default-on`, worktree `.worktrees/markdown-default-on`, based on `markdown-preview/reading`. It edits `viewer.cpp` and the viewer settings code, so it may conflict with nothing in B or C.
- Budget/hard stop: S $8/$10. Lite tier. The owner said to run it now, in parallel with B and C ("it is isolated").
- Owner rules: no PR, no push until the owner says so. Commit trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- Owner-checked paths: stories/*/story.md.
- The owner chose "on by default" in these words: "it should be on".
- started: 2026-11-06T00:30:00Z
