# Spec: GitHub look for the Alt-F3 browser page

Three units are under test. They are the script `far2l/bootstrap/md-open.sh`, the defaults file `md-view.ini`, and the page that the script writes. The vendored files sit next to the script in `far2l/bootstrap/`.

Current facts, measured before the design. `marked` is v18.1.3 with `gfm: true`. It writes no heading ids. The page has one policy: `default-src 'none'; script-src 'unsafe-inline' https://cdn.jsdelivr.net; style-src 'unsafe-inline'; img-src data: file:`. Inline scripts and styles are allowed. The mermaid script is added by the page after the text shows. This story keeps that policy as it is. Every new file is inlined into the page, so no new source is needed.

## Command line of `md-open.sh`

| Call | Result |
|---|---|
| `md-open.sh F.md` | The page goes to `${XDG_CACHE_HOME:-$HOME/.cache}/far2l/md/<name>.html`. The script opens it. Exit 0. |
| `md-open.sh --no-open F.md` | The page is made and not opened. The script prints no page path. Exit 0. |
| `md-open.sh --out P F.md` | The page goes to `P`. The cache is not used. Old behavior stays. |
| `md-open.sh --width 60\|70\|80 F.md` | The page starts at that width. The saved browser choice is ignored for this page load. |
| `md-open.sh --theme light\|dark\|auto F.md` | The page starts with that theme. Same rule for the saved choice. |
| `md-open.sh --browser B F.md` | `B` replaces the `browser=` key for this run. Values: see the `browser=` row below. |
| `md-open.sh --no-cache F.md` | The page is built again, even if a current page exists. |
| `md-open.sh --init-config` | If `md-view.ini` is absent, the script writes a commented sample and prints its path. Exit 0. If it exists, the script changes nothing and prints a line. Exit 0. |
| `md-open.sh --print-association` | The old text, plus the path of `md-view.ini` and the accepted values. The association command line is not changed. |

A flag beats the file. The file beats the built-in defaults, which are width 70 and theme `auto`.

## Defaults file `md-view.ini`

The path is `${XDG_CONFIG_HOME:-$HOME/.config}/far2l/settings/md-view.ini`. Only the `[View]` section is read. Keys are lower case. Spaces around `=` and CR at line end are ignored. A line that starts with `#` or `;` is a comment. The last valid line of a key wins.

| Key | Accepted values | Other value |
|---|---|---|
| `width=` | `60`, `70`, `80` | Built-in default 70. No message. |
| `theme=` | `light`, `dark`, `auto` | Built-in default `auto`. No message. |
| `browser=` | `auto`, `firefox`, `chrome`, `safari`, or an absolute path to an executable file | `auto`, with one line on stderr. |

A missing, unreadable or empty file gives the defaults. The script still exits 0. The script does not run the file and does not pass any value to a shell. A value reaches the page only after an exact match with the list above.

## Opening the browser

| Value | Action |
|---|---|
| `auto` | The first that exists: a single-word `$BROWSER`, `open`, `xdg-open`. The script runs it with `exec`, with the page path as one argument. If none exists, the script prints the page path. |
| `firefox`, `chrome`, `safari` on macOS | `open -a Firefox`, `open -a "Google Chrome"`, `open -a Safari`, then the page path. |
| `firefox`, `chrome`, `safari` on Linux | The first of `firefox`; `google-chrome`, `chromium`; `safari` is not on Linux. A command that is not found falls back to `auto`. |
| An absolute path | `exec "$path" "$page"`. The path must be an executable file. |

No value goes through `eval`, `sh -c` or an unquoted expansion. A value with a space, a quote, `;`, `$` or a backtick is a plain string. It names no executable, so it falls back to `auto` with one line on stderr. A `$BROWSER` with a space is ignored with one line on stderr. If the opener fails, the script prints one line and exits 1, as it does today.

## The page

| Part | Rule |
|---|---|
| Style | Two inline style blocks from `github-markdown-css`, one light and one dark. The page enables one at a time. The body has the class `markdown-body`. |
| Code | `highlight.js` runs after `marked`. It marks `pre > code` blocks that have a known language. A fence with no language, with an unknown language, or with `mermaid` stays plain. A highlight error leaves the plain block. Two inline highlight theme blocks, light and dark, follow the page theme. |
| Width | The element `html` has `data-width` with 60, 70 or 80. The CSS rule `html[data-width="N"] #content` sets `width: N%`. The column is centered. The rule `max-width` of the GitHub style is `none`. |
| Narrow window | Below 700 px the column is `width: 100%`, and the gear is hidden. The keys still work. |
| Theme | The element `html` has `data-theme` with `light`, `dark` or `auto`, and the page computes the used theme. `auto` follows `prefers-color-scheme` and changes with it. |
| Start values | The page holds the start width and theme as the attributes `data-start-width` and `data-start-theme` on `html`. Only the six accepted words can be there. A flag adds `data-forced="1"`. |
| Mermaid | Mermaid is loaded as today, from the pinned URL with the same SRI hash. It starts with theme `default` in light and `dark` in dark. A theme change draws each diagram again from its saved source text. |
| Gear | One button, fixed in the top-right gutter, low contrast. It takes no layout space and never overlaps the column at 60, 70 or 80. It opens a popover. |
| Popover | Width buttons 60, 70, 80. Theme buttons light, dark, auto. A help line with the keys. The line to paste into `md-view.ini`. A Reset button that clears the saved choice. A click outside or Esc closes it. Opening it does not move the text. |
| Keys | `t` changes the theme in the order light, dark, auto. `w` changes the width in the order 60, 70, 80. `?` opens the popover. Keys are ignored with Ctrl, Alt or Meta held, and when a form field has the focus. |
| Saved choice | Keys `far2l-md-width` and `far2l-md-theme` in `localStorage`. Every read and write is in `try/catch`. Order of use: saved choice (unless `data-forced`), then start value. |
| Storage blocked | If a read or write throws, the page works as before. The popover shows a notice and the line to paste: `[View]`, `width=N`, `theme=T`, with the current values. |
| Print | The gear and the popover are `display: none` when printing. |
| Offline | Text, style and highlight show with no network. A mermaid block shows its source. |
| Input safety | Raw HTML in the Markdown shows as text, as today. A file name, a config value and a path reach the page only through today's escapes or through the list of accepted words. |

## Page name and reuse

| Call | Result |
|---|---|
| `<name>` of the page | `<base>-<crc>.html`. `<base>` is the file name with each character outside `A-Za-z0-9._-` changed to `_`, cut to 40 characters. `<crc>` is the first field of `cksum` of the real folder path, `/`, and the file name. |
| Folder | `${XDG_CACHE_HOME:-$HOME/.cache}/far2l/md`, created with mode 700. The page file has mode 600. |
| Folder cannot be made | The script prints one line on stderr and uses `${TMPDIR:-/tmp}/far2l-md-<pid>.html`, as today. |
| Reuse | The script opens the existing page and writes nothing if three things hold. The page exists. No input is newer than the page. The inputs are the Markdown file, `md-open.sh`, every vendored file and `md-view.ini`. The page key equals the key of this run. |
| Page key | A `<meta name="far2l-md-key">` in the page holds the used width, theme, and whether a flag forced them. It is built from accepted words only. |
| Build | The script writes to a temporary file in the same folder, then moves it over the page. A reader never sees a half page. |
| `--no-cache` or `--out` | The script always builds. |

## Validation

| Call | Exit and message |
|---|---|
| `md-open.sh --width 75 F.md` | Exit 2. Usage on stderr. No page. |
| `md-open.sh --theme blue F.md` | Exit 2. Usage on stderr. No page. |
| `md-open.sh --width` | Exit 2. Usage on stderr. |
| `md-open.sh --browser 'a b;c' F.md` | Exit 0. One line on stderr, then `auto` is used. Nothing runs. |
| `md-open.sh` with no file | Exit 2. Usage on stderr. |
| A vendored file is missing | Exit 3. `md-open: cannot find <file>` on stderr. |
| Config with `width=<script>` | Exit 0. Width 70. The page has no `<script>` from the value. |

## Done

`BUILD=../far2l.markdown-browser-github-look.build` and the check in `plan.md` exits 0 with `check ok`. Every table row above is covered by a case in `testing/markdown/browser-page.sh`, except the rows of the browser, gear, popover and keys. Those rows get one manual check in Firefox and Chrome, listed in `gate.md`. The test script was run against a page with a broken width rule and it failed.

## Out of scope

The 5 MB offline mermaid bundle. A far2l Options dialog and `config.ini` keys. Any C++ change. The terminal Markdown view. Live reload and a local server. Links between local Markdown files (story `markdown-browser-tree-panel`). Cleaning old pages from the cache folder.
