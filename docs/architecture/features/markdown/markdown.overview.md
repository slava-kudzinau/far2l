---
description: Entry point of the Markdown feature - scope, solution strategy with ADR links, the files of this folder, and the risks and open questions.
refs:
  - docs/architecture/blueprint.md
  - docs/architecture/features/markdown/markdown.invariants.md
  - docs/architecture/features/markdown/markdown-viewer.spec.md
  - docs/architecture/features/markdown/markdown-browser-view.spec.md
  - docs/architecture/features/markdown/markdown.runtime.md
  - docs/architecture/features/markdown/markdown.data.md
---

# Markdown feature (overview)

This fork (`origin` is `slava-kudzinau/far2l`, `upstream` is `elfmz/far2l`) adds Markdown in two places. Everything else is upstream code. For the whole app see the [blueprint](../../blueprint.md).

- **F3 viewer:** Markdown mode with headings, lists, tables, code and links. Six mermaid diagram types show as text art. → [viewer spec](markdown-viewer.spec.md)
- **Browser view:** Alt-F3 renders the file as an HTML page in the system browser, with all mermaid types. → [browser spec](markdown-browser-view.spec.md)

## Files in this folder

| File | What it holds |
|---|---|
| [markdown.overview.md](markdown.overview.md) | This file. Scope, strategy, risks. |
| [markdown.invariants.md](markdown.invariants.md) | Rules that always hold for the feature. |
| [markdown-viewer.spec.md](markdown-viewer.spec.md) | What Markdown mode in F3 must do. |
| [markdown-browser-view.spec.md](markdown-browser-view.spec.md) | What the browser view must do. |
| [markdown.runtime.md](markdown.runtime.md) | Open in F3, render and scroll, mermaid fence to ASCII art, open in browser. |
| [markdown.data.md](markdown.data.md) | Screen lines, color runs, position cache, settings keys, temp files, bundled JS. |

The decisions are in [docs/architecture/decisions/](../../decisions), because a decision can cross features.

## Solution strategy

- **Parse in the viewer with md4c:** a vendored C parser, one layout pass for the whole file ([ADR-002](../../decisions/adr-002-md4c-parser.md)).
- **Scroll by rendered line index:** the file offset of a line serves only the position cache ([ADR-003](../../decisions/adr-003-line-index-scrolling.md)).
- **Runs hold palette roles, not colors:** the viewer resolves them at draw time ([ADR-004](../../decisions/adr-004-palette-roles.md)).
- **Diagrams as ASCII art in the terminal:** a committed JS bundle runs in vendored quickjs-ng, under limits. Other types go to the browser ([ADR-006](../../decisions/adr-006-ascii-diagrams.md)).
- **The browser view is a shell script,** not C++ ([ADR-005](../../decisions/adr-005-browser-view-script.md)).
- **Markdown is on by default** for `.md` and `.markdown` files. The position cache stores only a deviation ([ADR-007](../../decisions/adr-007-markdown-default-on.md)).

## Parts

| Part | Owns | Detail |
|---|---|---|
| `far2l/src/markdown/` | Parse, layout, ASCII diagrams (`MarkdownDoc`, `AsciiMermaid`) | [Runtime](markdown.runtime.md) |
| `far2l/src/viewer.cpp`, `ViewerPrinter.cpp` | Mode, scrolling, cache, drawing | [Runtime](markdown.runtime.md) |
| `far2l/bootstrap/` files | `mermaid-ascii.js`, `md-open.sh`, `marked.min.js` | [Data](markdown.data.md) |
| Vendored code | md4c, quickjs-ng | [Data](markdown.data.md) |
| `far2l/far2sdk/farplug-wide.h` | `VSMT_MARKDOWN`, `ViewerMode.Markdown` | [Viewer spec](markdown-viewer.spec.md) |

## Cross-cutting

- **Untrusted input:** diagram text runs in quickjs with no `std`, `os` or DOM. The browser page escapes the text, uses a `Content-Security-Policy` and a pinned CDN hash for the mermaid script. → [invariants](markdown.invariants.md)
- **Failure never stops the viewer:** a fence that cannot be drawn keeps its source.
- **Bounds:** a 2 s deadline for each fence, a 1 MB JS stack, and a 128 MB limit on the one shared runtime. → [viewer spec](markdown-viewer.spec.md)
- **Offline:** the terminal view uses no network. Only the browser view calls a CDN, for diagrams.
- **Tests:** smoke tests, Markdown golden tests and a driver. → [how-we-build.md](../../../how-we-build.md)

## Risks and open questions

- **No durable test of the browser contract.** The only check is a story `check.sh`. One row, about the story base, fails after later stories by design. → [browser spec](markdown-browser-view.spec.md), [how-we-build.md](../../../how-we-build.md)
- **CI does not run the Markdown golden tests.** See the CI risk in the [blueprint](../../blueprint.md).
- **Plugin ABI change.** `ViewerMode` grew by one `int` and `FARMANAGERVERSION` stayed. An old plugin binary reads `ViewerInfo` at wrong offsets. → [viewer spec](markdown-viewer.spec.md)
- **No file-size limit.** F3 on a large `.md` file reads it all on the UI thread, now by default. The document has no diagram budget either. → [viewer spec](markdown-viewer.spec.md)
- **The Alt-F3 hint can mislead.** The viewer shows it. The key works only after the owner adds a File associations entry by hand. → [browser spec](markdown-browser-view.spec.md)
- **Diagram limits.** A large diagram can hit the stack limit or use the full 2 s. The 128 MB limit is shared by the bundle and all fences until a timeout frees the runtime. → [viewer spec](markdown-viewer.spec.md)
- **CDN scope.** The page policy allows scripts from all of `cdn.jsdelivr.net`. Only the page script pins the URL and the hash. → [browser spec](markdown-browser-view.spec.md)
- **Style:** the files in `far2l/src/markdown/` do not follow `CODESTYLE.md`. → [architecture invariants](../../invariants.md)
