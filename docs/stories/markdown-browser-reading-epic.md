---
description: Epic for reading Markdown in the browser from Alt-F3 - GitHub look, highlighted code, width and theme settings, a choice of browser, and links between local files.
status: planned
---

# Epic: Read Markdown in the browser from Alt-F3

## Goal

The owner reads Markdown files with Alt-F3 in a browser page. The page should look like GitHub, highlight code and draw mermaid diagrams. The text should fill 60, 70 or 80 percent of the width. The page has a light and a dark theme. The owner sets this once and keeps it. Linked local files should open in the same page.

Owner words, 2026-11-07: "use github markdown renderer with mermaid and code highlighting for the alt-f3 mode, the content should occupy 60/70/80% of width and use light/dark schemas these settings should be easiliy configurable somewhere".

## Stories, in build order

| # | Story | Size | Risk | Depends on | Outcome |
|---|---|---|---|---|---|
| 1 | [markdown-browser-github-look](markdown-browser-github-look/story.md) | M | none | markdown-browser-view | GitHub style, highlighted code and a mermaid theme that follows light and dark. A small gear control with a saved choice. `md-view.ini`, a browser setting and a page cache. |
| 2 | [markdown-browser-linked-files](markdown-browser-linked-files/story.md) | M | security | story 1 | A link to a local `.md` file opens that file, rendered, in the same page. Bundle, limits, and a manifest cache. |

The stories run in series, not in parallel. Both edit `far2l/bootstrap/md-open.sh` and the page template, and story 2 reuses the page name and the cache rule of story 1.

## Rules that hold for both stories

- No C++ change. The work is in the script, the page and the vendored files.
- The Markdown file is untrusted. Raw HTML shows as text, and the Content-Security-Policy is not wider than ADR-005 gives now.
- Settings are in `~/.config/far2l/settings/md-view.ini` (section `[View]`: `width=`, `theme=`, `browser=`). The order of use is: saved page choice, then the file, then the built-in defaults (70, `auto`, `auto`).
- Mermaid stays on the pinned CDN. `highlight.js` and the GitHub CSS are vendored in the repo.
- The control on the page is one small gear in the gutter. It never covers the text column and takes no layout space.

## Decisions

| Date | Decision | Reason |
|---|---|---|
| 2026-11-07 | Settings are on the page and in a defaults file. | The owner wants an easy control and a saved choice. |
| 2026-11-07 | The control is small and outside the text. | "the bar should not obscure text and occupe visual area" |
| 2026-11-07 | A separate `md-view.ini`, not `config.ini`. | far2l rewrites `config.ini` on exit and drops unknown keys. |
| 2026-11-07 | `highlight.js` is vendored. | It works offline and adds no network trust. |
| 2026-11-07 | Mermaid stays on the CDN and follows the theme. | Offline mermaid reverses ADR-005 and is a separate decision. |
| 2026-11-07 | The browser is a setting: `auto`, a name, or a path. | The default is Firefox, but it may be Chrome. |
| 2026-11-07 | Linked files are a pre-rendered bundle, in a second story. | A `file://` page cannot fetch other files, and a local server needs a new ADR. |
| 2026-11-07 | The page is cached by a hash of the file path. | Stable storage, several files in several tabs, and no crawl for an unchanged bundle. |

## Assumed

- Default width 70, default theme `auto`, default browser `auto`.
- Below 700 px the column is the full width.
- The look is `github-markdown-css`, and `marked` stays the parser.
- The macOS and Linux openers only. Windows is not covered.

## Open

- Does browser storage survive between runs for a `file://` page in Firefox and in Chrome? The first task of story 1 tests it. If not, the paste-into-`md-view.ini` line is the main way to keep a choice.
- Can POSIX tools find links reliably, including reference-style links and links in code fences? Story 2 settles this with a fixture.

## Out of the epic

- Offline mermaid (the 5 MB bundle) and a local server. Each needs a new ADR.
- The terminal Markdown view (F3) and its colors. These are the stories `markdown-colorer-look`, `markdown-colorer-fences` and `markdown-code-tint`.
- A far2l Options dialog for these settings.

## Close-out of the epic

- `markdown-browser-view.spec.md` and `markdown.data.md` describe the new page, the settings file, the cache and the link rules.
- ADR-005 has a note about the vendored files, the cache and the bundle.
- The licenses of the vendored files are in the repo.
