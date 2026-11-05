---
status: in-progress
size: S
risk: none
depends-on: markdown-preview-reading
---

# Story B: Alt-F3 opens a Markdown file as a web page

## Why
The terminal Markdown view (Shift-F4 in F3) is good for quick reading, but it cannot draw diagrams well. The owner decided: keep the terminal view for text, and open a real web page for the full document with diagrams.

## What
- `far2l/bootstrap/md-open.sh <file.md>` writes one temp `.html` file and opens it with `open` (macOS). The page holds the markdown text, an inline markdown renderer (`marked`, MIT), a `<script>` tag that loads mermaid from the jsdelivr CDN (pinned version, integrity hash, securityLevel strict), and a small stylesheet. Owner decision 2026-11-06: no local `mermaid.min.js`; the CDN is used. Without internet the diagrams show as source. Light and dark follow the system setting.
- Alt-F3 on a `.md` file runs the script, through far2l's file associations. A default association `*.md` is shipped (the user can change it in far2l).
- The terminal Markdown view is not changed by this story.

## Not in this story
- Removing the terminal mermaid art and the quickjs engine (story C, stacked after this one).
- Live reload, a local web server, Linux and Windows openers.

## Notes
- Branch `markdown-preview/browser-view`, worktree `.worktrees/markdown-browser-view`, stacked on `markdown-preview/reading` (unmerged).
- Budget/hard stop: S $8/$10.
- Owner rules: no PR, no push until the owner says so. Commit trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- Owner-checked paths: stories/*/story.md.
- started: 2026-11-05T22:55:00Z
