# Plan: Alt-F3 opens a Markdown file as a web page

Tier: lite (`flow tier=lite`). No `spec.md`: the spec is the section below.

## Spec

| Call | Result |
|---|---|
| `sh far2l/bootstrap/md-open.sh <file.md> --out <page.html> --no-open` | exits 0; `page.html` exists; it holds the markdown text, the `marked` library inline, one script for mermaid from the CDN, added by the page after the text shows (pinned version, with an `integrity` hash and `crossorigin="anonymous"`), the init call `mermaid.initialize` with `securityLevel:'strict'`, and a `Content-Security-Policy` meta that allows that script and local images only; that one script is the only network use |
| Same, file with a mermaid fence | the page, opened in Chrome headless (`--dump-dom --virtual-time-budget=5000`), has an `<svg>` inside the diagram block and the text of the heading |
| Same, file that contains the text `</script>` | the page still renders the rest of the file (the text is not able to close the script tag) |
| Same, file that does not exist, or no argument | exits non-zero, prints a usage line, writes no page |
| `sh far2l/bootstrap/md-open.sh <file.md>` (no `--no-open`) | writes the page to a temp file and runs `open <page>` on macOS; exits 0 |
| `sh far2l/bootstrap/md-open.sh --print-association` | prints the File associations entry for `*.md` (mask, and the Alt-F3 view command that runs this script with `!.!`, unquoted) and the steps to add it in far2l (F9, Commands, File associations) |
| The terminal Markdown view (F3, Shift-F4) | not changed |

### Done

The check exits 0 and the page test shows an `<svg>` and the heading. The owner then adds the association once and presses Alt-F3 on a `.md` file.

## Global Constraints

- macOS first (`open`). The script is POSIX `sh`. Mermaid comes from the CDN, not from a local file: `https://cdn.jsdelivr.net/npm/mermaid@12.3.0/dist/mermaid.min.js`, with an `integrity="sha384-..."` hash that the implementer computes from the downloaded file (`openssl dgst -sha384 -binary | openssl base64 -A`). The local `mermaid.min.js` is not used. If the CDN is not reachable the page shows the Markdown text with each diagram as a code block (the script catches the load error). `marked` stays vendored and inline as one file `far2l/bootstrap/marked.min.js` (MIT, version 15.x or newer; keep its license text in `far2l/bootstrap/marked.LICENSE.md`). Use `git add -f` for files under `far2l/bootstrap` and `testing/` (the root `.gitignore` matches both).
- The markdown text goes into the page inside `<script type="text/markdown" id="md">`, with `</` replaced by `<\/` so it cannot end the tag. The page script renders it with `marked.parse`, turns each fenced `mermaid` block into `<pre class="mermaid">`, then calls mermaid when it loaded (`securityLevel:'strict'`, theme from `prefers-color-scheme`). HTML in the markdown is escaped by `marked`'s renderer or sanitized (no raw script from the file may run).
- Temp page path: `${TMPDIR:-/tmp}/far2l-md-<pid>.html`; the script leaves it in place (the browser needs it) and never deletes the user's file.
- Files this story may touch: `far2l/bootstrap/md-open.sh` (new), `far2l/bootstrap/marked.min.js`, `far2l/bootstrap/marked.LICENSE.md`, `far2l/bootstrap/CMakeLists.txt` (install the script and `marked.min.js` and its license), `docs/stories/markdown-browser-view/fixtures/`. Do not touch `far2l/src/`.
- Commit message: a short imperative summary line, a blank line, then the trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. One commit per task.
- Rulings (settle the choices the plan left open):
  - Lookup: `marked.min.js` is read beside the script; env `MARKED_JS` overrides. If it is missing: print `md-open: cannot find marked.min.js` to stderr, exit 3, write no page. The script never reads `mermaid.min.js`.
  - Usage error (no argument, missing file, unknown option): print `usage: md-open.sh <file.md> [--out <page.html>] [--no-open]` and `md-open.sh --print-association` to stderr, exit 2, write no page.
  - `--out <path>` sets the page path and overwrites it; the directory must exist. `--out` does not change opening: without `--no-open` the script still opens it.
  - No `open` command (not macOS): try `xdg-open`; if both are missing, print the page path and exit 0.
  - Escape `</` as `<\/` and `<!--` as `<\!--` in the markdown text, with `sed`. The page script reverses both before `marked.parse`.
  - Raw HTML in the markdown: `marked` renderer override for `html` returns the text escaped. No sanitizer library.
  - Mermaid fence: a renderer override on `code` for language `mermaid` (first word of the info string) emits `<pre class="mermaid">` with the diagram text HTML-escaped. Other fences stay plain `<pre><code>`, with no highlighting.
  - Relative links and images are not rewritten (accepted limit; the page has a `<base href>` set to the markdown file's folder as a `file://` URL so they resolve).
  - Input edge cases: empty file renders an empty page; CRLF and a UTF-8 BOM are accepted (strip the BOM); paths with spaces work (quote everything); the page is `<meta charset="utf-8">`.
  - `<title>` is the file's base name. CSS is a short inline block: system font, max width 50em, centered, `prefers-color-scheme` for light and dark. Mermaid theme: `default` for light, `dark` for dark; chosen once at load.
  - Old temp pages are not cleaned up (accepted; the temp folder is the OS's to clean). A reused pid overwrites.
  - Vendor the newest `marked` 15.x or later release (from the npm package `marked`, `lib/marked.umd.js` minified file) and record its version in `marked.LICENSE.md`.
  - `fixtures/diagram.md` is a copy of the flowchart; paste its text in (the copy is a fixture, it is not kept in sync).
  - `check.sh` needs network for the CDN script (if `curl -sI` to the CDN fails, print `no network` and exit 1). It matches in the dumped DOM: `<svg` inside the text between `class="mermaid"` and the next `</pre>`, the heading text by `grep -F`; the hostile fixture passes when the dumped DOM holds the text after `</script>` and no element with `id="x"`. If Chrome is missing, `check.sh` prints `chrome not found` and exits 1 (fail, not skip).
  - Install: `install(PROGRAMS md-open.sh ...)` for the script (executable), `install(FILES ...)` for `marked.min.js` and its license, into the same destination as the other bootstrap files. The association command runs the script by its installed absolute path, with `sh` in front; `--print-association` prints that path from the script's own folder.
  - Association entry format: print the lines for far2l's dialog: Mask `*.md`; Command for Alt-F3 view: `sh <abs path>/md-open.sh !.!` (ruling 1: `!.!` unquoted, the script path quoted only when it needs it), with numbered steps to add them.
  - Final-review rulings (fix round; the reviewer's findings are in `.build/final-review.md`):
    1. The Alt-F3 command prints and documents `sh <abs path to md-open.sh> !.!` with `!.!` unquoted: far2l already escapes it (`fnparce.cpp:102-111`). Quote only the script path if it has spaces. The spec row "(`\"!.!\"`)" is amended to this. Test with a file name that has a space and a quote.
    2. The page renders the Markdown text first; mermaid is added by a script element created after the text is shown (load or error), so a slow or blocked CDN never blanks the page.
    3. Escaping must not lose text: in the embedded text first replace `\` with `\\`, then `</` with `<\/` and `<!--` with `<\!--`; the page reverses in the opposite order. Add a fixture with the literal sequences `<\/` and `<\!--` and check them.
    4. Install: one layout. macOS: the script, `marked.min.js` and the license all go to `Contents/Resources/` (COMPONENT app); exclude `md-open.sh` from the `cp *.sh` glob that copies scripts into `MacOS/`. Linux: install all three to `share/far2l`. Add `EXCLUDE_FROM_ALL` where the existing sibling install rules have it, so a plain `cmake --install` does not create a partial app.
    5. Content security: add a `<meta http-equiv="Content-Security-Policy">` that allows only inline script and style, the one CDN script origin, `img-src data: file:` (local images stay possible, remote images are blocked), and no other network. Re-run the diagram test: the svg must still draw. State this limit in the gate: remote images in a Markdown file do not load.
    6. `story.md` no longer promises a shipped default association; it says the script prints the entry and the owner adds it once. (Main session edits this.)
    7. Minor findings (reviewer #7 to #15): fix #7 (show the mermaid error text in the block), #8 (a name starting with `-` after `--` works) and #14 (the stale gate text); park the rest in `closeout.md`.
  - Commit trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>` (put in the dispatch; verify).
- Check: `sh docs/stories/markdown-browser-view/check.sh` (written in T1; builds the fixtures, runs the Spec rows with `--no-open`, and runs Chrome headless on the page). Owner-checked paths: `docs/stories/*/story.md`.

## T1: The script, the vendored library, the page test

**Depends on:** none
**Done check:** `sh docs/stories/markdown-browser-view/check.sh` exits 0
**Role:** implementer — review final
Write `md-open.sh`, vendor `marked`, add `fixtures/plain.md` (a heading, a table, a list, a code fence), `fixtures/diagram.md` (a heading and the flowchart from `/Users/Viachaslau_Kudzinau/Projects/fde-kb/handbook/situations.md` block 4), `fixtures/hostile.md` (contains `</script><b id="x">`), and `check.sh`, which runs every Spec row (Chrome path `/Applications/Google Chrome.app/Contents/MacOS/Google Chrome`, override with env `CHROME`). Install the script, `marked.min.js` and its license in `far2l/bootstrap/CMakeLists.txt`.

## T2: Install and association text

**Depends on:** T1
**Done check:** `sh far2l/bootstrap/md-open.sh --print-association` exits 0 and prints the mask `*.md`; `cmake` configure of the far2l tree exits 0 and the generated install rule copies the script, `marked.min.js` and its license
**Role:** implementer — review final
Implement `--print-association`. Verify the install rule (configure into a temp build dir; do not run the full build).

## T3: Final-review fixes

**Depends on:** T1
**Done check:** `sh docs/stories/markdown-browser-view/check.sh` exits 0
**Role:** implementer — review task-reviewer
Fix the six Important findings (#1 to #6 of `.build/final-review.md`, except #6, which the main session already did in `story.md`) and Minor #7, #8, #14 and #15, exactly as the "Final-review rulings" 1 to 7 above say. Add checks to `check.sh` and fixtures for each ruling (name with a space and a quote; stalled CDN shows text; `<\/` and `<\!--` literal text survives; CSP present and the svg still draws; mermaid syntax error shows its text; leading-dash name). Update the gate text for ruling 5 (remote images do not load) and the stale `mermaid.min.js` line (#15). For #14 add a one-line note in `marked.LICENSE.md` or a grep in `check.sh`. Do not edit `story.md`.
