# Close-out draft: markdown-preview-reading

## Owner items (decide before merge)

1. **Long lines are cut at the screen edge.** Markdown rows never wrap, and Right or Ctrl-Right do nothing in the default wrap mode (viewer.cpp KEY_RIGHT needs !VM.Wrap). In situations.md 49 of 180 lines are longer than 80 columns. This predates the branch and no spec row covers it, but it works against "comfortable to read". Options: (a) cheap: let Right and Ctrl-Right move LeftPos in Markdown view whatever the Wrap flag says (two guards in viewer.cpp); (b) proper: soft-wrap paragraph, list and quote text in Builder (follow-up story). Recommendation: (b), with (a) as a stop-gap if wanted now.
2. **Spec row "Down stops at the last line" is built as "stops at the last full page"** (review ruling: no blank screen). Update the spec row or say if the literal reading is wanted.
3. **Black-and-white palette (BlackColorsIndex16):** heading differs, but code and art entries equal the body foreground there. The default theme is fully distinct. Accept, or pick entries that stay distinct in both.
4. **Art (light gray) and code (white)** are the closest pair in the default theme. Swap COL_PANELHIGHLIGHTTEXT if it reads too alike in a real terminal (one-line change in ViewerPrinter.cpp MarkdownRunColor).
5. **Commit trailer:** all commits use "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>" as instructed, although implementer and reviewer subagents ran on other models.
6. The spec.md Colors rule for tables (owner ruling) was built: table header text = heading entry, borders = art entry, body cells = body.
7. The untracked `.worktrees/` directory in the repo is not from this build.

Commits (base 6e03557b3, docs 7539e1bf7, spec update dd107b10f):
- T1 Scrolling: a96249a86, fix e4d5bdc58
- T2 Spacing: 2edf0130d, fix f538ad1d0
- T3 Palette: ca8661dca, fix 4b2e943e3

Check: `cd /Users/Viachaslau_Kudzinau/Projects/far2l && cmake --build ../far2l.build --target far2l -j8 && cd testing && ... ./far2l-smoke -t 30 ... tests/0007-view-file-markdown` (full command in story.md Notes; wrapper .build/check.sh) exit 0 (last run after 4b2e943e3). Final review: Approved.

## Palette table (spec Colors "Choice")

| Role | Elements | COL_ constant | Default-palette value | Source |
|---|---|---|---|---|
| body | body, lists, emphasis, quote bar, HR, table body cells, image alt; also every cell background | COL_VIEWERTEXT | light cyan on blue | ViewerPrinter.cpp MarkdownRunColor (def16index.h:62) |
| heading | H1-H6 text and rules, links, strong, table header row | COL_HELPTOPIC | yellow (foreground only) | def16index.h:72 |
| code | code spans, fence text | COL_HELPHIGHLIGHTTEXT | white (foreground only) | def16index.h:71 |
| art | mermaid art, table borders | COL_PANELHIGHLIGHTTEXT | light gray (foreground only) | def16index.h:17 |

Colors are resolved at draw time (FarColorToReal), so a theme change applies on the next redraw (verified by reading; no test).

## Rulings

1. Owner: table header/border colors (T3), via spec commit dd107b10f.
2. T1 fix 1: hex+markdown predicate MarkdownView() = VM.Markdown && !VM.Hex; line offset = start of the source line holding the first text; Down stops at MarkdownMaxTop; tests for Up, toggle and hex.
3. T2 fix 1: blocks inside list items spaced like top-level blocks, items tight, nested list gets no blank, outermost list keeps before/after blank; explicit spacer flag; second fixture spacing.md.
4. T2 and T3 escalated to task-reviewer-critical after Important findings; final review ran as final-reviewer.
5. T3 fix 1: art vs code assert, table header/body asserts, header comments.
6. Final: findings 1-4 and 6 owner items or parked (below).

## Parked findings

- T1: dead MarkdownDoc API (NextLines, Cursor, SetCursor) left in place; offsets are UTF-8 offsets of the decoded text, off for UTF-16 and non-UTF-8 code pages (predates).
- T2: HR and HTML blocks get blank lines the spec does not list; a block after a nested list with a loose last item still gets a blank.
- T3: links inside body table cells use the heading entry (follows "links use heading"); theme change untested.
- Final: a file changed on disk keeps the old render (set m_md_ready=false in KEY_IDLE, one line, predates); wheel and resize verified by reading, no test; tabs and wide characters break layout (ViewerPrinter.cpp:193, MarkdownRender.cpp:509); plain toggle on a fence's top row skips the opening fence line.

## Lessons

- A subagent hitting the API rate limit leaves a zero-token <synthetic> row that made spend.mjs exit 2; the main session fixed spend.mjs. Resuming the same implementer after the reset kept its uncommitted work.
- The Agent tool offered no foreground flag here: subagents ran in the background and the orchestrator waited with Monitor on a commit or review file.
- Haiku implementer needed one fix round on T1 (Critical: hex), T2 (Important: list items) and T3 (Important: missing assert); consider implementer on a higher tier for viewer-state tasks.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| task-reviewer-critical | opus | 3 | 1 | 1 | 5 |
| re-reviewer | haiku | 1 | 0 | 0 | 0 |
| task-reviewer | haiku | 2 | 0 | 3 | 9 |
| final-reviewer | opus | 1 | 0 | 1 | 5 |
