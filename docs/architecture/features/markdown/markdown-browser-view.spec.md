---
description: Durable contract for the external-browser view - what starts md-open.sh, how it makes the page, how the page treats untrusted content, and what the host must supply.
status: ready
refs:
  - docs/architecture/features/markdown/markdown.overview.md
  - docs/architecture/features/markdown/markdown.invariants.md
  - docs/architecture/features/markdown/markdown-viewer.spec.md
  - docs/architecture/features/markdown/markdown.runtime.md
  - docs/architecture/features/markdown/markdown.data.md
  - docs/architecture/decisions/adr-005-browser-view-script.md
---

# Spec: Markdown in the external browser

## Intent

**Problem:** the terminal cannot draw every mermaid type, images or real typography.
**Approach:** a shell script writes one self-contained page and gives it to the system opener. The far2l C++ code has no part in this path.

The code is `far2l/bootstrap/md-open.sh`. The Markdown parser is the vendored `far2l/bootstrap/marked.min.js` (marked v18.1.3). The page has the GitHub look from two vendored style files, and `highlight.js` marks the code. The script does not use md4c or `mermaid-ascii.js`. The versions of all vendored files are in [the data view](markdown.data.md).

## Contract

### What starts it

| Step | Rule |
|---|---|
| Trigger | Alt-F3 on a file panel. `FileList::ProcessKey` calls `ProcessLocalFileTypes` with `FILETYPE_ALTVIEW`. |
| Association | The user adds the entry one time. far2l ships no entry, and no script edits `config.ini`. |
| Entry text | `sh md-open.sh --print-association` prints the mask `*.md`, the command `sh <path>/md-open.sh !.!` and the steps. |
| Quoting | `!.!` stays unquoted because far2l escapes the name (`EscapeSpace` in `far2l/src/fnparce.cpp`). The script path is quoted only if it needs quotes. |
| No entry | Alt-F3 opens the external viewer if one is set and `UseExternalViewer` is off, else the internal viewer (`EnableExternal` in `FileList::ProcessKey`, `far2l/src/panels/filelist.cpp`). |

The script can also run alone. Inside the F3 viewer, Alt-F3 does nothing.

### Script interface

```
md-open.sh [--out <page.html>] [--no-open] [--width 60|70|80]
           [--theme light|dark|auto] [--browser B] [--no-cache] <file.md>
md-open.sh --init-config
md-open.sh --print-association
```

| Input | Meaning |
|---|---|
| `--out <page.html>` | The page path. The folder must exist. An existing file is replaced. The script writes it directly. The cache page is not used. `--out` alone does not stop the open step. |
| `--no-open` | Writes the page and does not start a browser. |
| `--width 60\|70\|80` | The start width of the text column, in percent. A flag also makes the page ignore the saved page choice. |
| `--theme light\|dark\|auto` | The start theme. Same rule for the saved page choice. |
| `--browser B` | Replaces the `browser=` key for this run. `B` is `auto`, `firefox`, `chrome`, `safari`, or an absolute path to a program. A value that is not accepted gives one line on stderr, and the script uses `auto`. |
| `--tree on\|off` | Tree mode for one run. `on` is the default. `off` gives the page of one file, with no scan. The same as the `tree=` key. |
| `--list-tree <file.md>` | Prints the encoded root on line 1, then one line `<state> <ENC>` per tree file in tree order. `state` is `page`, `big` or `cut`. A last line `cap N` shows when the cap stopped the scan. Writes no page and opens nothing. Exit 0. In one-file mode it prints line 1 and `off`. |
| `--no-cache` | Builds the page again, also if a current page exists. |
| `--init-config` | Writes a commented sample `md-view.ini` if there is no file, and prints its path. An existing file, link or other entry is not changed. Exit 0. |
| `--` | The next argument is the file name, also if it starts with `-`. |
| `TMPDIR` | The folder of the fallback page. Default is `/tmp`. |
| `XDG_CACHE_HOME` | The base of the cache folder. It must be an absolute path. Else `$HOME/.cache`. |
| `XDG_CONFIG_HOME` | The base of the folder of `md-view.ini`. Else `$HOME/.config`. |
| `BROWSER` | Used by `auto` only, and only if it is one command name. A value that starts with `-` or has other characters is ignored with one line on stderr. |
| `MARKED_JS` | Another path for `marked.min.js`. |

| Exit | When |
|---|---|
| 0 | The page is written, and it is opened or its path is printed. With `--no-open`, nothing is printed. |
| 1 | The page cannot be written, or the opener fails. |
| 2 | No file, an unreadable file, an unknown option, a missing `--width` or `--theme` value, or a value that is not accepted. Also a page folder that does not exist. No page is written. |
| 3 | `marked.min.js` or a vendored style or `highlight.min.js` file is not found. No page is written. |

A `--browser` value or a `browser=` key that is not accepted is not an error. It gives one line on stderr and the script uses `auto`.

### Defaults file

The file is `${XDG_CONFIG_HOME:-$HOME/.config}/far2l/settings/md-view.ini`. It is a separate file, because far2l rewrites `config.ini` on exit and drops keys that it does not know. far2l never reads or writes it.

- Only the section `[View]` is read. A line that starts with `#` or `;` is a comment. Spaces around `=` and a CR at the line end are ignored. The last valid line of a key wins.
- `width=` is `60`, `70` or `80`. `theme=` is `light`, `dark` or `auto`. `browser=` is `auto`, `firefox`, `chrome`, `safari`, or an absolute path to a program (on macOS also a `.app` folder).
- `tree=` is `on` (default) or `off`. `root=` is `dir` (default) or `repo`.
- `root=dir` is the folder of the opened file. `root=repo` is the nearest folder upward with `.git`, else the folder of the file. The walk stops at `$HOME` or `/`.
- `tree_depth=` is a number from 1 to 12, default 6. `tree_max_files=` is a number from 1 to 2000, default 500. A number over the ceiling gives the ceiling. A value that is not accepted is ignored.
- A value that is not accepted is ignored, and the next source applies. A `browser=` value that is not accepted also gives one line on stderr.
- The order of use is the saved page choice, then the file, then the built-in defaults: width 70, theme `auto`, browser `auto`. A flag replaces the file value for one run. The saved page choice does not apply to a page that a flag started (see "The page").
- The script never runs a value from the file. A width or a theme reaches the page only if it equals an accepted word.

### How the browser starts

| Value | Action |
|---|---|
| `auto` | The first that exists: `$BROWSER` (one command name), `open`, `xdg-open`. If none exists, the script prints the page path and exits 0. |
| `firefox`, `chrome`, `safari` on macOS | `open -a Firefox`, `open -a "Google Chrome"`, `open -a Safari`, then the page path. |
| `firefox`, `chrome` on other systems | `firefox`, or `google-chrome` then `chromium`. A command that is not found gives `auto`. `safari` gives `auto`. |
| An absolute path | The script starts that program with the page path as its one argument. |

The script starts each command with `exec` in a subshell. The page path is one argument. No config value, flag value or file name goes through `eval`, `sh -c` or an unquoted expansion. If the opener fails, the script prints one line and exits 1.

### How the page is made

1. The script writes the head. It holds a `Content-Security-Policy` meta, the page key, the state of `md-view.ini` (see "Page file and reuse") and the styles. `<title>` is the base name of the file. `<base href>` is the `file://` URL of its folder. The styles are four inlined vendored files (GitHub light and dark, highlight light and dark). A short stylesheet of the page follows them.
2. `sed` removes a UTF-8 BOM and each CR at a line end. It doubles each `\`, then writes `</` as `<\/` and `<!--` as `<\!--`.
3. The text goes into `<script type="text/markdown" id="md">`.
4. The script adds `marked.min.js` inline, then `highlight.min.js`, then the page script.
5. In the browser, the page script reverses the escapes and calls `marked.parse`. The result goes into `<main id="content">`, so the text shows before any network use. Then `highlight.js` marks each `pre > code` block that has a known language. A fence with no language, an unknown language or `mermaid` stays plain, and a highlight error leaves the plain block.

The start values are the attributes `data-start-width` and `data-start-theme` of `html`. They can hold only an accepted word. A flag that sets the width or the theme adds `data-forced="1"`.

### The tree page

Tree mode is on by default. It shows the opened file and the other Markdown files under the root, in one page, with a file tree on the left.

| Part | Rule |
|---|---|
| Scan | `find` runs from the root. It skips `.git`, `node_modules`, hidden folders and hidden files, does not follow symlinks, and stops at `tree_depth` and `tree_max_files`. Names with a control byte are skipped. Only `*.md` and `*.markdown` match, in any letter case. The opened file joins the scan even if it is a symlink. A scan failure exits 2 with one line. A root of `$HOME` or `/` gives the one-file page. The scan uses POSIX `sh`, `sed`, `awk`, `grep`, `find`, `ls`, `sort` and `cksum` only. |
| Files in the page | Each tree file with state `page` goes in the page, in tree order. The opened file is first and counts last. A file over 1 MiB is `big`. A file that would take the Markdown text over 8 MiB is `cut`. A `big` or `cut` file stays in the tree only. |
| Page contents | `<script type="text/markdown" id="md">` for the opened file. One `<script type="text/markdown" class="md-file" data-path="ENC">` per other file. A `<script type="text/plain" id="md-tree">` holds one `<state> <ENC>` line per tree file. |
| Attributes on `html` | `data-tree="on"`, `data-root`, `data-files`, `data-cap`, `data-panel="open\|closed"`. The page uses a number only if it matches `^[0-9]+$`, and an `ENC` only if it matches `^[A-Za-z0-9._~/%-]*$`. |
| Route | The page URL without a hash is the page. `#/ENC` shows that file. `#/ENC#FRAG` also scrolls to the anchor. Back and Forward move in the browser history. |
| Links | The page script reads each link of the rendered text. A relative target is resolved against the file of the link. `%XX` is decoded once before the check. A target is in the root if no `..` segment leaves the root and it stays under the root as a real path. A target outside the root, an absolute path or a `file:` target shows as text with the note `not in this tree`. A link to a page file gets `href` = page URL `#/ENC` and the note is not added. A link to a `big` or `cut` file goes there, and that file shows its limit note. |
| Raw HTML | Raw HTML shows as text in tree mode, as in one-file mode. |
| Panel | Fixed on the left, full height, 260 px wide, own scroll. The root name at the top. Folders are `details` with the name in `summary`. Files are links. The folders on the path of the shown file are open. The shown file has `aria-current="page"` and the panel scrolls it into view. A `big` or `cut` file has a low-contrast style. At the cap the last line is `Showing the first N files.`. |
| Toggle | One button at the top-left, at the right edge of the panel when open, at the left edge when closed. `aria-expanded` and `aria-controls`. The key `b` does the same, and follows the rule of the other keys. |
| Saved state | `localStorage` key `far2l-md-tree` with `open` or `closed`, inside `try/catch`. No saved value gives `open`. `tree=off` removes the panel, and a saved value cannot bring it back. |
| Width | `html` has `data-panel`. When the panel is open, `body` gets `padding-left` of 260 px and the gutter. The text column takes the width setting of the space that is left. |
| Narrow window | Below 700 px the panel and its toggle are hidden, and `b` does nothing. |
| Focus | After a click in the tree or on a link, the focus goes to `#content`. |
| Print | The panel and its toggle are hidden. |
| One-file mode | `tree=off` or `--tree off`, or a root of `$HOME` or `/`, gives the page of one file: no scan, no panel markup and no panel CSS. `html` has `data-tree="off"` and `data-tree-why="ini\|flag\|home\|slash"`. The popover shows why. |

### The page: look, gear and keys

| Part | Rule |
|---|---|
| Look | The body has the class `markdown-body`. The page holds one light and one dark GitHub style, and one light and one dark highlight theme. The page script enables one of each pair with the `media` attribute. |
| Width | `html` has `data-width` with 60, 70 or 80. The column is centered and has that width in percent. With the tree panel open, the percent applies to the space on its right. |
| Narrow window | Below 700 px the column has the full width, and the gear and the tree panel are hidden. The keys still work, so `?` still opens the popover. |
| Theme | `html` has `data-theme` with `light`, `dark` or `auto`. `auto` follows `prefers-color-scheme` and changes with it. |
| Gear | One low-contrast button, fixed in the top-right corner. It takes no layout space. It opens a popover. |
| Popover | Width buttons 60, 70, 80. Theme buttons light, dark, auto. The keys. A line to paste into `md-view.ini`. A Reset button. A click outside or Esc closes it. |
| Keys | `t` changes the theme in the order light, dark, auto. `w` changes the width in the order 60, 70, 80. `b` opens or closes the tree panel. `?` opens the popover. The page ignores a key with Ctrl, Alt or Meta held, and when a form field has the focus. |
| Saved choice | The page saves the choice in `localStorage`, keys `far2l-md-width`, `far2l-md-theme` and `far2l-md-tree`. Order of use: the saved choice (unless `data-forced`), then the start value. Reset removes the saved choice and applies the start values with no reload. |
| Storage blocked | If a read or write throws, the page still works. The popover then shows a notice and the lines to paste: `[View]`, `width=N`, `theme=T`, with the current values. |
| Print | The gear, the popover, the tree panel and its toggle are hidden. |
| Offline | Text, style and highlight show with no network. A mermaid block shows its source. |

### How diagrams render

A fence is a diagram when the first word of its language is `mermaid`. This match is case-sensitive, but the F3 match is not. Other fences stay plain code.

1. The page adds one script element for mermaid 12.3.0 from `cdn.jsdelivr.net`, with a `sha384` integrity hash and `crossorigin="anonymous"`. It does this only if the page has a diagram.
2. On load, `mermaid.initialize` runs with `startOnLoad: false` and `securityLevel:'strict'`. The theme is `dark` or `default`, as the used page theme says. When the used theme changes, the page draws each diagram again from its saved source text. A key, a button, or the system in `auto` can change it. One batch runs at a time, with a time limit of 10 s. A batch that waits is not added twice, and a redraw is skipped if the drawn theme is the same.
3. `mermaid.parse` checks each diagram alone. A diagram that fails shows its source and its error. The others still draw.
4. `mermaid.run` draws the good diagrams.

### Content cases

| Case | Fixture | Result |
|---|---|---|
| Normal text, table, list, code | `plain.md` | Rendered in the GitHub look. Empty files give an empty page. |
| Code fence with a known language | none | Highlighted. A fence with no or an unknown language stays plain. |
| `width=<script>` in `md-view.ini` | none | The value is ignored. Width is 70. The page has no script from the value. |
| A diagram | `diagram.md` | An `<svg>` in the block. |
| `</script>` in the text | `hostile.md` | It cannot end the script element. The rest of the file renders. |
| Raw HTML, an `onerror` attribute | `hostile.md` | The `html` renderer shows it as text. No element is made. |
| Link with a `javascript:`, `vbscript:` or `data:` target | none | The page removes `href` after the parse, so entity-encoded schemes are caught too. |
| Remote image | `remote.md` | `img-src data: file:` blocks it. The text after it shows. |
| Local image, relative link | none | These resolve against `<base href>`. |
| Broken diagram | `broken-diagram.md` | The source and a `.mermaid-error` line show. The next diagram draws. |
| Literal `<\/` and `<\!--` | `escapes.md` | They come back as written. |
| CDN blocked or integrity failure | none | Each diagram shows as a code block, with no error line. |
| CDN stalled | none | The text and the diagram source show while the script waits. |
| Tree with a hostile name | a file named `\"><script>.md` | The name shows as text in the tree. No element is made. |
| Link out of the root | `../outside.md`, `/etc/x.md`, `file:` URL, `%2e%2e/x.md` | Text, with the note `not in this tree`. |
| Link to a page file | two files in one root | The other file shows in the same page, and Back returns. |
| Link to a `big` or `cut` file | a file over 1 MiB | The file shows its note, `This file is too large for the page.` or `This file is over the page limit.` |
| Tree over the cap | `tree_max_files=5` | Five files and the line `Showing the first 5 files.` |
| `tree=off`, `--tree off`, root `$HOME` or `/` | none | The page of one file, and the popover says why. |

The fixtures are in the folder of story markdown-browser-view.

The policy is `default-src 'none'; script-src 'unsafe-inline' https://cdn.jsdelivr.net; style-src 'unsafe-inline'; img-src data: file:`. The GitHub look did not change it, because the script inlines every vendored file.

### Page file and reuse

| Rule | Detail |
|---|---|
| Name | `${XDG_CACHE_HOME:-$HOME/.cache}/far2l/md/<base>-<cksum>.html`. `<base>` is the file name with each byte outside `A-Za-z0-9._-` changed to `_`, cut to 40 bytes. `<cksum>` is the first field of `cksum` of the real path of the file. |
| Folder | The folder has mode 700. The page file has mode 600. A link or a non-plain file at the page path is not used. |
| Fallback | If the folder cannot be made or used, the script prints one line on stderr and writes `${TMPDIR:-/tmp}/far2l-md-<pid>.html`. |
| Write | The script writes a temporary file next to the page, then moves it over the page. A reader never sees a half page. The temporary file goes on an error or a signal. `--out` keeps the direct write. |
| Reuse | The script opens the old page and writes nothing if three things hold. The page exists. No input is newer than the page. The head of the page has the key of this run and the state of `md-view.ini`. |
| Inputs | The Markdown file (by modification time only), `md-open.sh`, `marked.min.js`, the five other vendored files, and `md-view.ini`. In tree mode also the scan: the page holds a line with the tree of this scan, and a newer tree file rebuilds the page. |
| Key | `<meta name="far2l-md-key" content="w=<width>;t=<theme>;f=<0 or 1>;tr=<on or off>;r=<root mode>;d=<depth>;n=<max files>">`, and a `far2l-md-tree` line with the state of this scan in tree mode. A comment `far2l-md-ini` holds the size and the time of `md-view.ini`, or `none`. The script reads both from the head of the page only, so the Markdown text cannot fake them. |
| No reuse | `--no-cache` and `--out` always build. The fallback page is never reused. |
| Cleanup | The script never deletes a page. The operating system cleans the temporary folder. The cache folder grows with the files that the user opens. |

### Host dependencies

| Need | Detail |
|---|---|
| Shell tools | POSIX `sh`, `awk`, `sed`, `cat`, `basename`, `dirname`, `cksum`, `cut`, `find`, `grep`, `ls`, `mv`, `mkdir`, `chmod`, `sort` |
| Opener | `open`, else `xdg-open`. If both are missing, the script prints the page path and exits 0. |
| Browser | Any browser that the opener starts. Chrome is tested. Firefox is a manual check. |
| Network | `cdn.jsdelivr.net` for a page with a diagram. No other network use. |
| Install, macOS | `Contents/Resources/` of the app: `md-open.sh`, `marked.min.js`, `github-markdown-light.css`, `github-markdown-dark.css`, `highlight.min.js`, `hljs-github.min.css`, `hljs-github-dark.min.css` and the three license files. Component `app`, `EXCLUDE_FROM_ALL`. |
| Install, other | `share/far2l/`, same files, component `base`. |

`md-open.sh` reads `marked.min.js` and the other vendored files from its own folder, so the files must stay together. The copy step of the build leaves the script out of `MacOS/`.

### Boundaries

**Always:**

- Treat the file as untrusted.
- Show the text before any network use.
- Keep the mermaid URL pinned, with its hash.
- Let a width or a theme reach the page only as an accepted word.
- Run the browser with no shell evaluation.

**Ask first:** add another network origin, ship a default association, or add keys to `config.ini`.

**Never:**

- Change the Markdown file through the Alt-F3 path.
- Run raw HTML from the file.
- Load a remote image.
- Edit `config.ini` or read `mermaid.min.js`.
- Run a value from `md-view.ini` or a flag as a command, except an absolute path that names a program for `browser=` or `--browser`.
- Change the terminal view.

## Tests

Two checks hold this contract.

- `sh testing/markdown/browser-page.sh` needs only `sh`. It drives `md-open.sh` with `--no-open`. It checks the flags, the exit codes, `md-view.ini`, the page name, the reuse rule, the temporary file, the key and the page text. It runs in the AGENTS.md check. The gear, the popover, the keys and the saved choice are checked as page text only. A person checks them in Firefox and Chrome.
- The `check.sh` script of story markdown-browser-view has fixtures. It runs each content row with `--no-open` and renders pages in headless Chrome. It reads the install rules through a wrapper project. It needs Chrome and the network.

The rows of `check.sh` are:

| Part | Check rows |
|---|---|
| Page shape, CSP, pinned CDN script | `plain:` rows |
| Paths with spaces, empty file, BOM and CRLF | `paths with spaces`, `empty file`, `BOM and CRLF` |
| Diagrams, stalled and refused CDN | `diagram:`, `stalled CDN:`, `refused CDN:` |
| Untrusted content | `hostile:`, `escapes:`, `remote image:`, `broken diagram:` |
| Script interface | `usage:`, `leading dash`, `open:` |
| Association text | `association:` rows |
| Install rules | `install` rows |

## Risks and open questions

Story markdown-browser-view has no owner items. The items below are parked findings and new findings, with the state checked against `far2l/bootstrap/md-open.sh`.

### Defects

| Defect | Evidence |
|---|---|
| The check script no longer runs in place. Its repo path is two folders up from the script, and the story folders moved one level down. 47 rows fail. A scratch copy in the old layout passed all rows except the git row, which needs a repo. | Run of the script, 2026-11-06 |
| No durable test holds this contract. The check lives in a story folder and is not part of the smoke tests. | `testing/` |
| The printed mask is `*.md` only. The F3 rule also takes `.markdown`. | `print_association` |
| The F3 hint promises Alt-F3 before the user adds the entry. See [the viewer spec](markdown-viewer.spec.md). | `MERMAID_HINT` |
| The note in `marked.LICENSE.md` names the old `stories/` path of the check. | `far2l/bootstrap/marked.LICENSE.md` |

### Parked findings that remain open

- The reuse rule compares the Markdown file by modification time only. It misses a package upgrade that keeps the old times.
- A read error inside the page write is not detected, so a bad page can be reused.
- A dark page can flash light before the page script runs.
- The test of the page script is text only. A syntax error in the script would pass.
- After a mermaid timeout, pressing the same theme does not retry. A run that ends after the timeout can still write its SVG.
- Esc closes the popover with no focus return when the gear is hidden under 700 px.
- The Firefox check of `localStorage` between two `file://` pages is manual (Chrome 156 shares the value).
- `--out` equal to the input file destroys the input. The redirection empties the file before `sed` reads it. Alt-F3 cannot reach this case.
- In-page `#anchor` links do not work. marked writes no heading ids, and `<base href>` sends the link to the folder.
- The fallback temp name is predictable when `TMPDIR` is not set. The cache page does not have this problem (folder mode 700, noclobber write, a link is refused).
- `LC_ALL=C` is exported to `open` and `xdg-open`.
- File names with `<`, `>` or a leading `#` break the printed command. An `--out` name that starts with `-` reaches `open`.
- `file(GLOB)` in `far2l/bootstrap/CMakeLists.txt` runs at configure time only.
- The Linux install layout was tested only through the wrapper project (unverified on Linux).
- The check script pins a base commit, and its stalled-CDN row can pass if `nc` cannot bind.

### Other risks

- The policy has `'unsafe-inline'`, so it does not stop an inline handler. The `html` renderer and the escapes are the only guards against script in the file.
- The policy allows scripts from all of `https://cdn.jsdelivr.net`, not from one URL. Only the page script pins the mermaid URL (`MERMAID_URL`) and its hash (`MERMAID_SRI`). A script from another jsdelivr path would pass the policy.
- A page with a diagram sends a request to the CDN. The CDN sees the IP address and the time. The file text stays local.
- Files in another encoding go into a page that says UTF-8, so non-ASCII text shows wrong.
- A plain build has no `md-open.sh` in the app. Only `cmake --install` with component `app` puts it there.
- A new vendored `marked` or `highlight.js` needs a new check for `</script` and `</style`. Each current file has one `<!--`, and the HTML parser accepts it in a script element.
