---
description: Index of the story records with their file status, the real merge state, and the open items that the stories left.
---

# Stories

"Status" is the value in each `story.md`. "Merged" is the real state in git on branch `next/2.9.1-ovr.1`. The story files are not edited, so Status is stale for all five stories. The durable docs of the feature are in [docs/architecture/features/markdown/](../architecture/features/markdown/markdown.overview.md). Open items that are still open also appear in its specs.

| Story | Status | Merged | Outcome | Depends on | Branch | Merge commit |
|---|---|---|---|---|---|---|
| [markdown-preview-fixes](markdown-preview-fixes/story.md) | in-review | yes, inside PR #1 | The Markdown mode of the F3 viewer (md4c, mermaid through SVG) got a render that works and viewer-text colors. | none | `markdown-preview/fixes` | `dc887fa78` (PR #1; no PR of its own) |
| [markdown-preview-reading](markdown-preview-reading/story.md) | in-review | yes, PR #1 | Scrolling by line index, spacing between blocks, and four palette roles. | markdown-preview-fixes | `markdown-preview/reading` | `dc887fa78` |
| [markdown-ascii-diagrams](markdown-ascii-diagrams/story.md) | in-review | yes, PR #2 | Six mermaid types as box art from a JS bundle in quickjs. The SVG path is removed. | markdown-preview-reading | `markdown-preview/ascii-diagrams` | `3b533938b` |
| [markdown-default-on](markdown-default-on/story.md) | in-review | yes, PR #3 | F3 on `.md` and `.markdown` files opens Markdown mode. The `Markdown_Auto` setting and a deviation-only cache keep the choice. | markdown-preview-reading | `markdown-preview/default-on` | `141a329d3` |
| [markdown-browser-view](markdown-browser-view/story.md) | in-review | yes, PR #4 | `md-open.sh` shows a Markdown file as a web page, with diagrams from a pinned CDN. Alt-F3 runs it. | markdown-preview-reading | `markdown-preview/browser-view` | `a2f720209` |

`markdown-preview-fixes` has no pull request. Its commits are in the history of `markdown-preview/reading`, so PR #1 merged them.

The proof of concept `mermaid-image-poc` (status `approved`) is not in this folder. Its records are only on branch `markdown-preview/image-poc` and are not merged. See the research note on the image PoC.

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
