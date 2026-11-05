# Spec: Alt-F3 opens a Markdown file as a web page

This is the plan's `## Spec` table, kept as the story's spec for the standard tier. `plan.md` Rulings amend it.


| Call | Result |
|---|---|
| `sh far2l/bootstrap/md-open.sh <file.md> --out <page.html> --no-open` | exits 0; `page.html` exists; it holds the markdown text, the `marked` library inline, one `<script src>` for mermaid from the CDN (pinned version, with an `integrity` hash and `crossorigin="anonymous"`), and the init call `mermaid.initialize` with `securityLevel:'strict'`; that one script is the only network use |
| Same, file with a mermaid fence | the page, opened in Chrome headless (`--dump-dom --virtual-time-budget=5000`), has an `<svg>` inside the diagram block and the text of the heading |
| Same, file that contains the text `</script>` | the page still renders the rest of the file (the text is not able to close the script tag) |
| Same, file that does not exist, or no argument | exits non-zero, prints a usage line, writes no page |
| `sh far2l/bootstrap/md-open.sh <file.md>` (no `--no-open`) | writes the page to a temp file and runs `open <page>` on macOS; exits 0 |
| `sh far2l/bootstrap/md-open.sh --print-association` | prints the File associations entry for `*.md` (mask, and the Alt-F3 view command that runs this script with `!.!`, unquoted) and the steps to add it in far2l (F9, Commands, File associations) |
| The terminal Markdown view (F3, Shift-F4) | not changed |

## Done

The check exits 0 and the page test shows an `<svg>` and the heading. The owner then adds the association once and presses Alt-F3 on a `.md` file.
