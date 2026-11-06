---
description: Durable contract for Markdown mode in the F3 viewer - how a file enters the mode, keys, layout, palette roles, diagrams, failure behavior and limits.
status: ready
refs:
  - docs/architecture/features/markdown/markdown.overview.md
  - docs/architecture/features/markdown/markdown.invariants.md
  - docs/architecture/features/markdown/markdown-browser-view.spec.md
  - docs/architecture/features/markdown/markdown.runtime.md
  - docs/architecture/features/markdown/markdown.data.md
  - docs/architecture/decisions/adr-002-md4c-parser.md
  - docs/architecture/decisions/adr-003-line-index-scrolling.md
  - docs/architecture/decisions/adr-004-palette-roles.md
  - docs/architecture/decisions/adr-006-ascii-diagrams.md
  - docs/architecture/decisions/adr-007-markdown-default-on.md
---

# Spec: Markdown mode in the F3 viewer

## Intent

**Problem:** a Markdown file in the plain viewer shows its markup, and a mermaid fence shows as source text.
**Approach:** the viewer parses the whole file with md4c and draws styled rows. It draws mermaid fences as box art. Any fence that it cannot draw stays as code, and the browser view is the fallback for that fence.

The code is `far2l/src/markdown/`, `far2l/src/viewer.cpp` and `MarkdownPrinter` in `far2l/src/ViewerPrinter.cpp`.

## Contract

### Enter the mode

`Viewer::OpenFile` sets the start mode each time it opens a file. Thus the rule also applies to the next file (gray plus and minus), to Quick View and to the grep filter.

| Step | Rule | Symbol |
|---|---|---|
| 1. File name | The name ends in `.md` or `.markdown`, in any ASCII letter case. A name that is only the suffix does not match. | `IsMarkdownFileName` |
| 2. Setting | Key `Markdown_Auto` in section `Viewer`, default 1. The check box is "Open Markdown files in Markdown mode" in the viewer settings dialog. | `ViewerMarkdownAuto` |
| 3. Rule | The rule is true when the setting is on and the name matches. The viewer keeps this value for the file. | `MarkdownRuleMode`, `m_md_rule` |
| 4. Saved choice | `Param[3]` of the position cache wins: 1 is Markdown, 2 is plain. Any other value applies the rule. | `MarkdownStartMode` |

The cache stores only a deviation from the rule that the file opened with:

| Mode at save | Rule true | Rule false |
|---|---|---|
| Markdown | 0 | 1 |
| Plain | 2 | 0 |

`MarkdownCacheParam` computes this value. Two sites save it: `Viewer::SavePosCache` and the `KEY_ADD`, `KEY_SUBTRACT` case in `Viewer::ProcessKey`. If `SavePos` is off, no choice is read or saved. The key `Markdown` in section `Viewer` is written by Shift-F4 and never read at start.

Other ways to change the mode:

| Way | Effect |
|---|---|
| Shift-F4 | `ProcessMarkdownMode` toggles the mode and forces a new render. |
| View menu, "Toggle markdown preview" | Sends `KEY_SHIFTF4`. |
| Plugin API | `VSMT_MARKDOWN` sets the mode. `ViewerMode.Markdown` reports it. |
| Macro | Bit `0x00000040` of the viewer state shows the mode. |

Hex has priority. If `VM.Markdown` and `VM.Hex` are both on, the viewer shows hex and uses the hex keys. `MarkdownView()` is the test for this rule.

### Keys and scrolling

The viewer scrolls by rendered line. The top row is `m_md_top`, an index into `MarkdownDoc::Lines()`. `FilePos` follows the top row, so the status line and the position cache keep working.

| Key | Result |
|---|---|
| Down, Up | The top row moves by 1. Down stops at the last full page. Up stops at the first row. |
| Mouse wheel | Repeats Up or Down `MsWheelDeltaView` times. With Alt, it repeats one time. |
| PgDn, PgUp, Ctrl-Down, Ctrl-Up | The top row moves by the screen height minus 1. PgDn stops at the last full page. Alt-PgUp and Alt-PgDn add the boost of plain mode. |
| Home, Ctrl-Home, Ctrl-PgUp | First row. |
| End, Ctrl-End, Ctrl-PgDn | Last full page. |
| Scrollbar drag | The thumb maps to the rows from the first row to the last full page. |
| Right, Ctrl-Right | Moves `LeftPos` only when Wrap is off (F2), as in plain mode. |
| Left, Ctrl-Left, Ctrl-Shift-Left | Move `LeftPos` back. |
| Status percent | 0% at the first row and 100% on the last page. A file that fits on one screen shows 100%. |
| Change of width | The doc lays out again. The top row keeps its source offset and its place among rows with that offset. |
| Shift-F4 off, then on | The plain view starts at the source line of the top row. The way back finds the first row with that source offset. |

| Mode | F4 label | Shift-F4 label |
|---|---|---|
| Plain | Hex | Markdown |
| Hex, Markdown off | Text | Markdown |
| Markdown on, also with hex | Markdown | Text |

Other viewer functions act on the source file:

| Function | Result |
|---|---|
| Search (F7), Alt-F8, Alt-Backspace, bookmarks | These set `FilePos`. `SyncMarkdownTop` moves the top row to the rows of that offset. |
| F2, Shift-F2 | The layout does not change. Rows never wrap. |
| F5 | Runs `view.sh` and opens its output file (`VM.Processed` branch of `Viewer::OpenFile`). The rule `m_md_rule` still comes from the `.md` name, so the rows are the Markdown render of that output (path, file info, glow text). `AnsiEsc::Printer` draws them, so the role colors do not show. If `view.sh` fails, the rows stay. |
| Clickable URLs | The scan reads the text of the rows. Only a URL that shows as text is clickable. |

### Rendering

`MarkdownDoc::Render` parses the whole file with `MD_FLAG_TABLES`, `MD_FLAG_TASKLISTS`, `MD_FLAG_STRIKETHROUGH`, `MD_FLAG_PERMISSIVEAUTOLINKS` and `MD_FLAG_UNDERLINE`.

| Source | Rows |
|---|---|
| Heading | The text. No rule row follows it. |
| Paragraph | One row for each source line. A soft break or a hard break starts a new row. |
| Bullet item | A lead of 2 cells for each list level, then `• `. |
| Ordered item | The same lead, then `<n>. `. The first number comes from the source. |
| Task item | `[ ] ` or `[x] ` after the bullet. |
| Block quote | `│ ` for each quote level, after the list lead. |
| Fenced or indented code | 4 cells of indent after the lead. Blank lines of the code stay. |
| HTML block | Verbatim, as code. Nothing runs. |
| Horizontal rule | A row of `─`. This is the only full-width line. A `---` line under a paragraph makes a heading, not a rule. |
| Emphasis, strikethrough, underline | The markers go and the text stays. No italic, bold or underline attribute shows. |
| Link | The label shows. The target is dropped. |
| Image | The alt text shows. The target is dropped. |

### Spacing

- A block that ends queues one blank row. The next block emits it before its first row.
- Thus repeated blanks collapse into one, and the document never starts with a blank row.
- Source blank lines give no rows, except inside code.
- A heading has one blank row before it, except at the top of the document.
- An H1 has one blank row after it. An H2 to H6 has no blank row after it. If another heading follows, the rule above gives one blank row.
- A code block or an HTML block has one blank row before it and after it. It has no blank row before it directly after an H2 to H6.
- An outermost list has no blank row before it or after it. A heading, a code block or an HTML block next to a list keeps its own blank row. An H1 before a list keeps its blank row.
- Items stay tight, and a nested list has no blank row.
- A paragraph, a quote, a table and a thematic break have one blank row between them.
- A block inside a list item is spaced like a top-level block. A code block in an item keeps its blank row when another item follows.
- A heading, a code block or an HTML block as the first block of an item has no blank row before it. It stays on the row of the bullet.
- A blank row inside a list or a quote keeps its lead.

### Palette roles

A run holds an `MdRole`. `MarkdownRunColor` reads the palette when it draws, so a new theme shows on the next redraw. The background of each cell is always the `COL_VIEWERTEXT` background.

| Role | Elements | Foreground entry |
|---|---|---|
| body | Body text, lists, emphasis, quote bars, horizontal rules, body table cells, the hint row | `COL_VIEWERTEXT` |
| heading | Heading text, links, strong text, table header text | `COL_HELPTOPIC` |
| code | Code spans, code fences, HTML blocks | `COL_HELPHIGHLIGHTTEXT` |
| art | Mermaid art, table borders | `COL_PANELHIGHLIGHTTEXT` |

### Tables

- The width of a column is the largest cell in that column, counted in `wchar_t` units.
- Text is left-aligned in all columns. The alignment markers in the source have no effect.
- The first row uses the heading role. Other cells keep their inline roles, so a link in a body cell uses the heading role.
- A separator row follows the header. Body rows have no separators, and the table has no top or bottom border.
- A table never shrinks to the screen. A row that is too wide is cut at the right edge.

### Diagrams

A code block is a mermaid fence when its info word is `mermaid`, in any letter case.

1. `Render` keeps the fence body as code rows. These rows are the placeholder.
2. `AsciiMermaid::Render` sends the source to `renderAscii` in `far2l/bootstrap/mermaid-ascii.js`. One quickjs-ng runtime serves the process.
3. If the call succeeds, art rows replace the code rows. Each art row has the source offset of the fence.
4. If the call fails, the code rows stay and the hint row follows them.

| Rule | Value |
|---|---|
| First word of a drawn diagram | `graph`, `flowchart`, `stateDiagram`, `stateDiagram-v2`, `sequenceDiagram`, `classDiagram`, `erDiagram`, `xychart-beta`. The match is case-sensitive. |
| Unspaced edges | For `graph` and `flowchart`, `entry.js` puts spaces around each edge operator before the parse. |
| Art width | The bundle takes no width. The art is not scaled. A wide row is cut at the right edge. |
| Art lead | Art rows have no list lead and no quote bar. The hint row keeps its lead. |
| Hint row | `(diagram not drawn here - Alt-F3 opens it in the browser)` |
| Cache | `m_mermaid_cache` uses the source hash and the width as its key. It lives for one render pass, so fences with the same source draw once. |

### Failure behavior

Each failure writes one stderr line, `AsciiMermaid: render failed: <err>`, and shows the code rows plus the hint row. The viewer stays open.

| Cause | `err` |
|---|---|
| Type outside the list | `unsupported: <word>` |
| Parse error | The one-line message of the parser |
| Deadline passed | `timeout` |
| No bundle found | `bundle missing` |
| No rows | `empty output` |
| Other JS failure | `js: <message>`. A plain `Error` from the package has no prefix. |

`locate_bundle` uses `FAR2L_MERMAID_ASCII_JS` when it is set. Else it takes the first readable file: `../Resources/`, `../share/far2l/` and the folder of the binary, then `/usr/local/share/far2l/`, `/usr/share/far2l/`, `/usr/local/lib/far2l/`, `/usr/lib/far2l/`, and the working folder.

### Limits

| Limit | Value |
|---|---|
| Deadline for each fence | 2 s, or `FAR2L_MD_DEADLINE_MS`. The render runs on the UI thread. |
| Document budget | None. The deadlines of the fences add up, and each new layout runs all fences again. |
| JS stack, JS memory | 1 MB, and 128 MB for the one shared runtime. `build_engine` sets the limit one time. The bundle and all fences use it until a timeout frees the runtime. |
| After a timeout | The runtime is freed. The next call builds it again from disk. |
| File size | None. `ShowMarkdown` reads the whole file on the UI thread. It casts the count to `int`. |

### Boundaries

**Always:**

- Keep source offsets non-decreasing down the doc.
- Resolve colors at draw time.
- Keep the fence source on screen if the art fails.

**Never:**

- Change the file, or use the network.
- Run HTML or script from the file.
- Wrap a row.
- Send `VE_REDRAW` to plugins in this mode, so plugin colors do not apply.
- Read the `Markdown` key at start.

## Tests

| Part | Test |
|---|---|
| Default-on rule, saved choice, setting off | `testing/tests/0008-view-md-default`, `testing/tests/0009-view-md-setting-off` |
| Scrolling, percent, toggle, hex priority | `testing/tests/0007-view-file-markdown` with `long.md` |
| Spacing | `testing/tests/0007-view-file-markdown` with `spacing.md` |
| Palette roles, tables | `testing/tests/0007-view-file-markdown` with `sample.md` |
| Art and hint row in the viewer | `testing/tests/0007-view-file-markdown` with `sample.md` |
| Bundle output, size limit (300 KB), no `elkjs` | `sh testing/markdown/ascii-golden.sh` |
| `AsciiMermaid` failures, deadline, speed | `sh testing/markdown/ascii-driver.sh` |

No test covers these parts:

- Mouse wheel, scrollbar and resize.
- Theme change.
- The `KEY_ADD` save site and Quick View.
- Search and F5.
- A fence in a list or a quote.
- The failure paths through the viewer, except the hint row for an unsupported type.

See [testing/README.md](../../../../testing/README.md) for the smoke tool and [how-we-build.md](../../../how-we-build.md) for the commands.

## Risks and open questions

### Open owner items

States are checked against the code of branch `next/2.9.1-ovr.1`.

| Item | From story | State |
|---|---|---|
| Long lines are cut at the right edge, and in the default Wrap mode Right does nothing. F2 is a workaround. The owner must choose a soft wrap or a change to the key guard. | markdown-preview-reading | Open |
| No text selection. `ReadStringMarkdown` sets `bSelection` to false, so search shows no highlight. `MarkdownPrinter` has the selection color, but no row uses it. | markdown-preview-fixes | Open |
| In `BlackColorsIndex16`, code and art have the same foreground as body. Only heading differs. | markdown-preview-reading | Open |
| In the default palette, art (light gray) and code (white) are the closest pair. A real terminal must show if this is good enough. | markdown-preview-reading | Open |
| The manual check in macOS Terminal and ghostty has no record. | markdown-preview-fixes | Open (unverified) |
| A fence that starts with `%%` goes to the hint (`unsupported: %%`), although the package can draw it. | markdown-ascii-diagrams | Open |
| The pre-pass can split circle, cross and edge-id endpoints. `A--B` becomes `A -- B`. | markdown-ascii-diagrams | Open |
| Large diagrams: a 26-node `graph TD` failed at once (stack overflow), and a 26-node `graph LR` chain can use the full 2 s. The limits in the code are the same, but the run is not repeated. | markdown-ascii-diagrams | Open |

Closed items: broken scrolling and the ignored `mermaid.min.js` asset (story markdown-preview-fixes). No code reads that asset now. The spec row "Down stops at the last line" (story markdown-preview-reading) is settled here as the last full page.

### Defects

"Smoke probe" means a scratch smoke test on the `far2l.build` binary, run on 2026-11-06. It is not in the repo.

| Defect | Evidence |
|---|---|
| A file with a UTF-8 BOM does not parse its first line as a heading. `# Title` shows with its marker. The browser view strips the BOM. | Smoke probe. md4c gets the BOM as text. |
| HTML entities show as source: `&amp;` stays `&amp;`. | Smoke probe. `cb_text` does not decode `MD_TEXT_ENTITY`. |
| F8 and Shift-F8 change the code page in the status line, but the rows stay the same. `m_md_ready` stays true. | Smoke probe |
| A file that changes on disk keeps the old render. `KEY_IDLE` does not clear `m_md_ready`. | Code |
| In Markdown mode the F4 label says Markdown, but F4 goes to hex. | Smoke probe, `ChangeViewKeyBar` |
| After Shift-F4 in Quick View, the panel key bar does not update. `QuickView::ProcessKey` lists F2, F4, F8 and Shift-F2 only. | Code |
| Source offsets are UTF-8 offsets of the decoded text. On other code pages the top row after a toggle is wrong. | Code |
| The hint says Alt-F3 opens the browser. Only a panel handles Alt-F3, and only after the user adds the association. The viewer ignores the key. | `far2l/src/panels/filelist.cpp` |
| The help files have no text about Markdown mode, Shift-F4 or the new check box. | `far2l/bootstrap/scripts/*.hlf.m4` |
| `ViewerMode` grew by one `int`, but `Reserved[3]` stayed and `FARMANAGERVERSION` did not change. A plugin binary from before this change reads `ViewerInfo` at wrong offsets. | `far2l/far2sdk/farplug-wide.h` |
| Tabs do not expand, and wide characters count as one unit. Table columns and rules go out of line. | Code |
| The comment in `far2l/src/hist/poscache.hpp` still says viewer `Param[3] = 0`. | Code |

### Other risks

- With default-on, F3 on any large `.md` file reads it into memory at 4 bytes for each character, with no progress and no stop key. A size limit for the mode is an open question.
- `Ctrl-F7` (grep filter) opens a temp copy with the `.md` name, so the filtered view opens in Markdown (recorded by story markdown-default-on).
- A plain toggle on the top row of a fence skips the opening fence line (recorded by story markdown-preview-reading).
- An old far2l build reads a saved 2 as true, so a plain choice shows Markdown there.
- Dead code remains: `MarkdownDoc::NextLines`, `Cursor`, `SetCursor`, `FirstBlockFilePos`, the undefined `MdLine::Width`, and the unbuilt `md4c-html.c`. The `MENU_VIEW_VIEW_*` enum lacks the new row, but nothing reads the later values.
- Build leftovers of the old SVG path remain. `far2l/CMakeLists.txt` calls the static `far2l_md` a shared library and links it to `z`. Local checkouts keep untracked `mermaid.min.js` copies.
