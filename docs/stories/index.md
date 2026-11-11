---
description: Index of the story records with their file status, the real merge state, and the open items that the stories left.
---

# Stories

"Status" is the value in each `story.md`. "Merged" is the real state in git on branch `next/2.9.1-ovr.1`. All stories in the first table are done. Each merged pull request is in the history of that branch. Each `story.md` shows `status: done`. The durable docs of the feature are in [docs/architecture/features/markdown/](../architecture/features/markdown/markdown.overview.md). Open items that are still open also appear in its specs.

| Story | Status | Merged | Outcome | Depends on | Branch | Merge commit |
|---|---|---|---|---|---|---|
| [markdown-preview-fixes](markdown-preview-fixes/story.md) | done | yes, inside PR #1 | The Markdown mode of the F3 viewer (md4c, mermaid through SVG) got a render that works and viewer-text colors. | none | `markdown-preview/fixes` | `dc887fa78` (PR #1; no PR of its own) |
| [markdown-preview-reading](markdown-preview-reading/story.md) | done | yes, PR #1 | Scrolling by line index, spacing between blocks, and four palette roles. | markdown-preview-fixes | `markdown-preview/reading` | `dc887fa78` |
| [markdown-ascii-diagrams](markdown-ascii-diagrams/story.md) | done | yes, PR #2 | Six mermaid types as box art from a JS bundle in quickjs. The SVG path is removed. | markdown-preview-reading | `markdown-preview/ascii-diagrams` | `3b533938b` |
| [markdown-default-on](markdown-default-on/story.md) | done | yes, PR #3 | F3 on `.md` and `.markdown` files opens Markdown mode. The `Markdown_Auto` setting and a deviation-only cache keep the choice. | markdown-preview-reading | `markdown-preview/default-on` | `141a329d3` |
| [markdown-browser-view](markdown-browser-view/story.md) | done | yes, PR #4 | `md-open.sh` shows a Markdown file as a web page, with diagrams from a pinned CDN. Alt-F3 runs it. | markdown-preview-reading | `markdown-preview/browser-view` | `a2f720209` |
| [markdown-compact-layout](markdown-compact-layout/story.md) | done | yes, PR #8 | Fewer blank rows, no heading rule rows, lighter table borders. | none | `feat/markdown-compact-layout` | see PR #8 |
| [markdown-front-matter-rule](markdown-front-matter-rule/story.md) | done | not merged, PR pending | The closing front matter line shows as a rule row: a setext heading draws its consumed underline. | none | `fix/markdown-front-matter-rule` | pending |

`markdown-preview-fixes` has no pull request. Its commits are in the history of `markdown-preview/reading`, so PR #1 merged them.

The proof of concept `mermaid-image-poc` (status `approved`) is not in this folder. Its records are only on branch `markdown-preview/image-poc` and are not merged. See the research note on the image PoC.

## Planned

Epic [markdown-browser-reading-epic](markdown-browser-reading-epic.md) groups `markdown-browser-github-look` and `markdown-browser-tree-panel`. They run in series.

Build these stories in this order. Each one changes `MarkdownRender.cpp` and the test `0007-view-file-markdown`, so they do not run in parallel.

| Story | Status | Size | Outcome | Depends on | Branch |
|---|---|---|---|---|---|
| [markdown-palette](markdown-palette/story.md) | done | M | A color for each Markdown element, from a fork table that the user can override by value or by palette entry name. | markdown-compact-layout | `feat/markdown-palette` |
| [markdown-code-highlight](markdown-code-highlight/story.md) | done | M | Syntax colors in fenced code, with tokens from the Colorer plugin through one small export. Merged in PR #11, `c7b612286` (2026-11-07). | markdown-palette | `feat/markdown-code-highlight` |
| [markdown-colorer-look](markdown-colorer-look/story.md) | done | M | Prose colors and underline from the active Colorer scheme, so F3 matches F4. Merged in PR #12, `f2611362e` (2026-11-07). | markdown-code-highlight | `feat/markdown-colorer-look` |
| [markdown-colorer-fences](markdown-colorer-fences/story.md) | done | M | Fenced code with the exact F4 token colors, through the V2 export and the generation-keyed cache. Merged in PR #14, `52ad4d440` (2026-11-07). | markdown-colorer-look | `feat/markdown-colorer-fences` |
| [markdown-code-tint](markdown-code-tint/story.md) | done | S | A full-width background tint on code blocks, gated on true color and 3:1. Merged in PR #15, `2a032e3f8` (2026-11-07). | markdown-colorer-fences | `feat/markdown-code-tint` |
| [markdown-browser-github-look](markdown-browser-github-look/story.md) | in-review | M | The Alt-F3 browser page gets the GitHub look, code highlight, a 60/70/80 width and a light/dark theme, set in a defaults file or by flags. It edits only the script and the page, so it can run in parallel with the F3 stories above. | markdown-browser-view | `feat/markdown-browser-github-look` |
| [markdown-browser-tree-panel](markdown-browser-tree-panel/story.md) | done | M | A file-tree panel on the left of the Alt-F3 page. A click on a tree file or on a local link shows that file in the same page, and Back returns. Merged in PR #16, `019c8ffda` (2026-11-07). | markdown-browser-github-look | `feat/markdown-browser-tree-panel` |
| [markdown-blank-lines](markdown-blank-lines/story.md) | done | M | One blank row for each empty source line in the F3 Markdown view. The renderer makes no blank rows of its own. Reverses the spacing rules of markdown-compact-layout. | markdown-code-tint | `feat/markdown-blank-lines` |
| [markdown-horizontal-scroll](markdown-horizontal-scroll/story.md) | in-progress | M | F2 soft wrap for prose, quote, list and table cells in the F3 Markdown view; Right, Left, Home and End scroll a long row in both modes. | markdown-blank-lines | `feat/markdown-horizontal-scroll` |
| [markdown-table-fit](markdown-table-fit/story.md) | planned | M | Width-aware F3 tables wrap cell content within the viewer width instead of scrolling horizontally. | markdown-horizontal-scroll | `feat/markdown-table-fit` |
| [md-quick-view-wide](md-quick-view-wide/story.md) | in-review | M | Quick view on a `.md` file takes the full screen width; the sibling panel collapses to a ~20-column strip; layout restores on exit. In PR #32. | - | `feat/md-quick-view-wide` |

Story [markdown-theme-colors](markdown-theme-colors/story.md) is superseded. The owner replaced it with markdown-palette and markdown-code-tint on 2026-11-06.

## The move to `docs/stories/`

The folders moved from `stories/` to `docs/stories/`. The `check.sh` files and the paths in the story text are fixed.

## Open owner items

Items from the close-outs that no later story closed. Items 1, 4, 6, 7, 8, 10 and 12 were checked in the code on 2026-11-06. Items 5 and 9 were not rechecked.

1. Long lines are cut at the screen edge, and Markdown rows do not wrap. The reading close-out proposed soft wrap as a follow-up story.
2. The reading spec row "Down stops at the last line" is still written that way. The build stops at the last full page.
3. Weak theme contrast. In the black and white theme, code and art equal the body foreground. In the default theme, art and code are close (reading items 3 and 4).
4. No text selection in Markdown mode (`bSelection` is false in `ReadStringMarkdown`; fixes item 3).
5. The manual check in macOS Terminal and ghostty was never recorded (fixes item 4).
6. The changelog and the man page have no note about the Markdown mode (fixes next step 4).
7. A fence that starts with a `%%` line goes to the hint. Large flowcharts can time out and block the UI for 2 s (ascii close-out).
8. The hint `(diagram not drawn here - Alt-F3 opens it in the browser)` names a key that works only after the owner adds the File associations entry by hand. No entry is shipped.
9. The grep filter copy keeps the `.md` name, so it opens in Markdown mode (default-on parked finding).
10. `far2l/src/hist/poscache.hpp` documents viewer `Param[3] = 0`, which is stale after the default-on story.
11. The smoke harness leaves a stale unix socket in `testing/tests/` (ascii close-out).
12. The test numbers `0007`, `0008` and `0009` each exist twice in `testing/tests/`. A prefix like `0007` selects both folders.
13. The ignored file `far2l/bootstrap/mermaid.min.js` (5.5 MB) is still on disk and unused. It can be deleted (see [how-we-build.md](../how-we-build.md)). A copy in the build folder's Resources is a leftover of an old build.
