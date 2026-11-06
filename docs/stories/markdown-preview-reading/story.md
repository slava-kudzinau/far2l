---
status: in-review
size: M
risk: none
depends-on: markdown-preview-fixes
---

# Story A: F3 Markdown preview — reading experience

Owner feedback (2026-11-05), after trying `/Users/Viachaslau_Kudzinau/Projects/fde-kb/handbook/situations.md` in Markdown mode:
1. Scrolling with the keys does not work (Down, PgDn; also check Up, PgUp, mouse wheel).
2. No colors for headings, code and diagrams: hard to read. Owner also said earlier: use the usual far theme colors, no rainbow colors. Together: a few entries from the far theme palette, no fixed ANSI colors.
3. No blank line after headings and between paragraphs.

Acceptance: situations.md is comfortable to read and scroll in Markdown mode.

## Notes

- Check: `cd /Users/Viachaslau_Kudzinau/Projects/far2l && cmake --build ../far2l.build --target far2l -j8 && cd testing && rm -rf tests/0007-view-file-markdown/workdir && mkdir tests/0007-view-file-markdown/workdir && cp -r tests/0007-view-file-markdown/initdir/* tests/0007-view-file-markdown/workdir/ && ./far2l-smoke -t 30 ../../far2l.build/install/far2l.app/Contents/MacOS/far2l tests/0007-view-file-markdown` (retry on `operation not permitted`)
- Branch `markdown-preview/reading`, stacked on `markdown-preview/fixes` (unmerged). No worktree.
- Owner-checked paths: docs/stories/*/story.md.
- Budget/hard stop: M $15/$20.
- flow tier=standard docs=spec,plan design=no orchestrator=yes final=final-reviewer-light gate=read-later scan=no verifier=none
- Story B (mermaid quality and speed) follows after this one; it is out of scope here.
- started: 2026-11-05T20:47:44Z
- Known cause from the previous story (closeout.md owner item 1): SHOW_UP and SHOW_DOWN never rebuild the markdown screen, and `Builder::pos` counts md4c text bytes, not file offsets, so PgDn blanks the screen. End and Home work.
