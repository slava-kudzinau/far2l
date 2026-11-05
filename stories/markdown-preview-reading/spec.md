# Spec: Markdown preview — reading experience

## Scrolling (`far2l/src/viewer.cpp`, `far2l/src/markdown/MarkdownRender.cpp`)

| Key in Markdown mode | Result |
|---|---|
| Down | top line moves down by 1 rendered line; stops at the last line |
| Up | top line moves up by 1; stops at the first line |
| PgDn | top line moves down by one screen height minus 1; never shows a blank screen; stops so the last page is full |
| PgUp | moves up by one screen height minus 1 |
| Home / End | first / last page (already works; must keep working) |
| Mouse wheel | same as Up / Down |
| Percent in the status line | follows the top line (0% at top, 100% on the last page) |
| Resize of the window | keeps the same top line |

## Spacing

| Markdown | Rendered |
|---|---|
| Heading | one blank line after it (and one before it, unless it is the first line) |
| Paragraph | one blank line between paragraphs |
| List | one blank line before and after the whole list; none between items |
| Code fence, table, blockquote, mermaid art | one blank line before and after |
| Two blank lines in the source | still one blank line |

## Colors (theme palette)

| Element | Rule |
|---|---|
| Body text, list text | `COL_VIEWERTEXT` |
| Headings | one far theme palette entry that differs from the body (not a fixed ANSI color); H1 and H2 may differ from H3 to H6 |
| Code span, code fence text | a second palette entry, different from heading and body |
| Mermaid art and table borders | a third palette entry (the art entry) |
| Table header row text | the heading entry; other table cell text uses the body color |
| Link, strong, emphasis | strong and links use the heading entry; emphasis uses the body color |
| Background of every cell | the viewer background; no black and no red |
| Theme change | colors follow it after the next redraw |
| Choice | the implementer picks entries from the existing far theme palette (`COL_*` in the far2l color table) and lists them in a table in `closeout.md` |

## Done

The check (see `story.md` Notes) exits 0. Test 0007 asserts: after PgDn the screen is not blank and shows later text; after PgUp the earlier text returns; a blank row follows the heading row; heading, code and body cells have three different foreground colors with the same background.

## Out of scope

Mermaid look and speed (story B). Text selection in Markdown mode. Italic or bold text. Hex and text modes.
