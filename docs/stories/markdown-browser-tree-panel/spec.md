# Spec: File tree and local links in the browser page

Two units are under test. They are the script `far2l/bootstrap/md-open.sh` and the page that it writes. The rules of story markdown-browser-github-look stay true, except where this spec changes them.

Facts measured before the design, on 2026-11-07:

- `marked` v18.1.3 writes each link target URL-encoded. `<my file.md>` becomes `my%20file.md`, `é.md` becomes `%C3%A9.md`, and `%2e%2e/up.md` stays as it is. A link in a code span or in a fence is not a link.
- On macOS, `find . \( -type d \( -name '.?*' -o -name node_modules \) -prune \) -o -path './*/*/*/*' -prune -o -type f ! -name '.*' \( -name '*.[mM][dD]' -o ... \) -print` lists the Markdown files to depth 3. It skips `.git`, `node_modules`, hidden folders and hidden files, a symlink file, a symlink folder and a name with a newline. It keeps names with spaces, a leading `-` and `"><script>`.
- Headless Chrome (`--headless=new --dump-dom --virtual-time-budget=3000`) runs the page from a `file://` URL with a `#/x` hash. It prints the DOM after the page script in about 0.5 s.
- The page policy is `default-src 'none'; script-src 'unsafe-inline' https://cdn.jsdelivr.net; style-src 'unsafe-inline'; img-src data: file:`. A `<script src="file:...">` is blocked by it. This story keeps the policy as it is, so every file goes into the page inline.

## Terms

| Term | Meaning |
|---|---|
| Opened file | The file that `md-open.sh` gets on its command line. |
| Root | The folder that the tree shows. See "Root". |
| Tree path | The path of a file relative to the root, with `/` between the parts, for example `sub/c.md`. Its depth is its number of parts: `top.md` has depth 1. |
| Encoded path (`ENC`) | The tree path with each byte outside `A-Za-z0-9._~-` written as `%XX` (two upper-case hex digits). `/` stays. Only these characters reach the page: `A-Za-z0-9._~/%-`. |
| Tree mode | `tree` is `on` and the root is not `$HOME` or `/`. Else one-file mode, which is the page of story 1. |

## Settings

New keys in `md-view.ini`, section `[View]`, with the parse rules of today. A flag beats the file. A value that is not accepted gives the default, with no message.

| Key | Accepted | Default | Ceiling |
|---|---|---|---|
| `tree=` | `on`, `off` | `on` | none |
| `root=` | `dir`, `repo` | `dir` | none |
| `tree_depth=` | a decimal number from 1 | 6 | 12. A larger number gives 12. |
| `tree_max_files=` | a decimal number from 1 | 500 | 2000. A larger number gives 2000. |

| Flag | Result |
|---|---|
| `--tree on\|off` | Replaces `tree=` for this run. Another value: exit 2, usage. |
| `--list-tree <file.md>` | Prints the root as an encoded path on line 1, then one line `<state> <ENC>` per tree file in tree order. `state` is `page`, `big` or `cut`. A last line `cap <N>` shows when the file cap stopped the scan. Writes no page and opens nothing. Exit 0. In one-file mode it prints only line 1 and `off`. |

The script has two fixed size limits, which the file cannot change. One file can have 1048576 bytes (1 MiB). The page can have 8388608 bytes (8 MiB) of Markdown text.

## Root

| Case | Root |
|---|---|
| `root=dir` | `cd -P` to the folder of the opened file, then `pwd -P`. |
| `root=repo` | Start at the `root=dir` folder. Check each folder upward for `.git` (a folder or a file). The first match is the root. Stop after the check of `$HOME` (its real path), or before `/`. No match gives the `root=dir` folder. |
| The root is `$HOME` or `/` | One-file mode for this run. The popover says `File tree: off, the root is the home folder` or `... the root is /`. |

## Scan

The script does `cd` to the root, then runs `find .` from the root. Thus a root path with `*`, `?` or `[` cannot change a pattern. In one-file mode (`tree=off`, `--tree off`, or a root of `$HOME` or `/`) the script runs no `find` under the root. In that mode `--list-tree` prints line 1 and `off`.

| Rule | Detail |
|---|---|
| Skip | A folder named `.git`, `node_modules`, or a name that starts with `.`. A file whose name starts with `.`. A name with a newline or another byte below 0x20. |
| Type | Plain files only (`-type f`). `find` does not follow a symlink, so a symlink file and a symlink folder are not in the tree. |
| Name | Ends in `.md` or `.markdown`, in any case. |
| Depth | A path with more than `tree_depth` parts is pruned (`-path` with `tree_depth`+1 parts of `*`). |
| Order | By depth, then by the byte order of the tree path (`LC_ALL=C sort`). |
| File cap | The first `tree_max_files` files in that order. If more exist, the page has the cap note. |
| Opened file | Always in the page and in the tree, at any depth and size. A symlink opened file shows under its own name. If it is past the cap, the script adds it after the cap. Then it is not counted in `data-files`, and the cap note counts only the other files. |
| Size | A file larger than 1 MiB (`find -size +1048576c`) is `big`: in the tree, not in the page. A file that would take the Markdown text in the page over 8 MiB is `cut`: in the tree, not in the page. The opened file counts first. |

## The page in tree mode

| Part | Rule |
|---|---|
| Base | `<base href>` is the `file://` URL of the root, with the escapes of today. |
| Files | `<script type="text/markdown" id="md" data-path="ENC">` for the opened file, then `<script type="text/markdown" class="md-file" data-path="ENC">` for each other `page` file. The opened file is written once. The text of each file has the escapes of today (`\`, then `</`, then `<!--`; BOM and CR removed). |
| Tree index | `<script type="text/plain" id="md-tree">` with one line `<state> <ENC>` per tree file, in tree order. |
| Raw HTML | Raw HTML in any file shows as text in tree mode, as in story 1. |
| Attributes on `html` | `data-tree="on"`, `data-root="ENC of the root folder name"`, `data-files="N"` (files in the tree, up to the cap; the opened file is counted only if it is within the cap), `data-cap="0\|1"`. The page uses a number only if it matches `^[0-9]+$`, and an `ENC` only if it matches `^[A-Za-z0-9._~/%-]*$`. |
| One-file mode | The page of story 1, with one file. `html` has `data-tree="off"` and `data-tree-why="ini\|flag\|home\|slash"`. No tree index. The page has no panel CSS and no panel markup. The script writes the tree CSS only in tree mode. The popover notice comes from the page script, with `data-tree-why`. |

The page script decodes an `ENC` for display only, with `decodeURIComponent` in `try`. A decode error shows the `ENC`. Every name goes in through `textContent`.

## Panel

| Part | Rule |
|---|---|
| Place | Fixed on the left, full height, 260 px wide, its own scroll. The root name at the top. |
| Gear | `#md-gear` stays fixed at the top right, above the panel. The panel and the toggle do not move it. |
| Names | The panel has no element and no attribute name that comes from a file name or a folder name. Each name shows decoded, as text (`textContent`). A name such as `"><script>.md` adds no element. |
| Tree | Folders are `details` elements with the folder name in `summary`. Files are links with `href` = the page URL without its hash, then `#/ENC`. The folders on the path of the shown file are open. The others start closed. |
| Current file | Its link has `aria-current="page"` and a marked style. The panel scrolls it into view. |
| `big` and `cut` files | Shown in the tree with a low-contrast style. A click shows a note in the reading pane: `This file is too large for the page.` or `This file is over the page limit.` |
| Cap | If `data-cap="1"`, the last line of the tree is `Showing the first N files.`, with N = `data-files`. It counts only the files up to the cap. |
| Toggle | One button at the top-left, at the right edge of the panel when open, at the left edge of the window when closed. `aria-expanded` and `aria-controls`. The key `b` does the same. `b` is ignored with Ctrl, Alt or Meta held, and in a form field, as `t` and `w` are. |
| Saved state | `localStorage` key `far2l-md-tree` with `open` or `closed`, inside `try/catch`. No saved value gives `open`. The file has no key for the start state. `tree=off` removes the panel, and a saved state cannot bring it back. |
| Width | `html` has `data-panel="open\|closed"`. When open, `body` gets `padding-left` of 260 px plus the gutter, and `#content` takes 60, 70 or 80 percent of the space that is left. |
| Narrow window | Below 700 px the panel and its toggle are hidden, and `b` does nothing. |
| Focus | After a click in the tree or on a link, the focus goes to `#content` (`tabindex="-1"`, `focus({preventScroll: true})`). |
| Print | The panel and its toggle are hidden. |
| Popover | The keys line adds `b tree` in tree mode. In one-file mode the popover has the line `File tree: off (tree=off in md-view.ini)`, `(--tree off)`, `(the root is the home folder)` or `(the root is /)`. |

## Route and links

| Part | Rule |
|---|---|
| Route | The hash is `#/ENC` or `#/ENC#FRAG`. An empty hash, or an `ENC` that is not a tree file, shows the opened file. The page reads the route at load and on each `hashchange`. Each route is a history entry, so Back and Forward work. |
| Show a file | `marked.parse` into a `template`. Remove script schemes (today's rule). Give heading ids. Fix links and images (rows below). Move the result into `#content`. Run `highlight.js` and the mermaid steps of today on this file only. Set `document.title` to the decoded base name, as text. Scroll. |
| Candidate | A link target that passes the text rules below. |
| Link in the root | A candidate whose tree path names a `page` file gets `href` = the page URL without its hash, then `#/ENC`, then `#FRAG` if it has one. A candidate to a `big` or `cut` file does the same, and shows the note of that file. |
| Link not in the tree | This is a candidate inside the root that is not a tree file. The link becomes plain text. The note ` (not in this tree)` follows it, in a `span` with the class `md-note`. |
| Link out of the root | Rules 3, 4, 5, 7 and 8 stop these targets. The link becomes plain text with the same note. It loses its `href`. |
| Other link | An `http:`, `https:` or `mailto:` link keeps its `href`. A relative link to a file that is not Markdown keeps working: it gets the absolute URL of the file in its folder. |
| Image | A relative `img[src]` gets `new URL(value, fileBase)`. `fileBase` is the URL of the folder of the shown file under the root. |

Text rules, in this order. The page script runs them on the `href` that `marked` wrote.

| Step | Rule |
|---|---|
| 1 | Stop if the target is empty or starts with `#`. A `#` target is an anchor in the same file. An empty target is not changed. |
| 2 | Stop at `javascript:`, `vbscript:` and `data:`: the link loses its `href` (today's rule). |
| 3 | Stop if the target has another scheme (`^[A-Za-z][A-Za-z0-9+.-]*:`). `http:`, `https:` and `mailto:` keep the `href`. Every other scheme (`file:`, `ftp:` and so on) gives plain text with the note ` (not in this tree)`. |
| 4 | Stop if the target starts with `/`. The result is plain text with the note. |
| 5 | Stop if the target has `\`, `?`, a byte below 0x20, or the byte 0x7F. The result is plain text with the note. |
| 6 | Split at the first `#`: the path and `FRAG`. |
| 7 | Decode the path once with `decodeURIComponent`. A decode error stops. Stop if the result has `\`, a byte below 0x20, or the byte 0x7F. Each stop gives plain text with the note. |
| 8 | Join the path to the folder of the shown file. Split at `/`. Drop each empty part and each `.`. A `..` removes the part before it. A `..` with no part before it stops: the target is out of the root, and the result is plain text with the note. |
| 9 | The result is a candidate if the last part ends in `.md` or `.markdown`, in any case. Else it is an other link. |

## Anchors

| Part | Rule |
|---|---|
| Heading ids | `id="user-content-SLUG"`. `SLUG` is the heading text in lower case. Each character that is not a letter, a digit, a space, `-` or `_` is removed. Each space becomes `-`. A repeated slug gets `-1`, `-2` and so on. |
| Same-file anchor | `href` becomes the page URL without its hash, then `#/ENC#FRAG` in tree mode, or `#FRAG` in one-file mode. |
| Scroll | Decode `FRAG` once (an error keeps the raw text). Scroll to `user-content-FRAG` with `scrollIntoView()`, else to its lower-case form, else to the top. |

The prefix keeps a heading from taking the id of a page element or a global name (`marked`, `hljs`, `mermaid`).

## Cache and reuse

| Part | Rule |
|---|---|
| Page key | `<meta name="far2l-md-key" content="w=..;t=..;f=..;tr=on\|off;r=dir\|repo;d=N;n=N">`, from accepted values only. |
| Tree state | `<!-- far2l-md-tree: CKSUM COUNT -->` in the head. `CKSUM` is the first field of `cksum` of the `--list-tree` output. |
| Reuse | Three things must hold. Today's rules hold. The head has the same tree state as a new scan. A `find` with the same rules and `-newer "$page"` prints nothing. Else the script builds again. |
| `--no-cache` | Builds again in tree mode too. |
| One-file mode | Today's rules only. No scan. |

In tree mode, the scan runs on each call, also for a reuse. Only the build is skipped.

## Validation

| Call | Result |
|---|---|
| `--tree maybe F.md` | Exit 2. Usage. |
| `tree_depth=99`, `tree_max_files=99999` | Depth 12, cap 2000. |
| `tree_depth=-1`, `tree_depth=<script>` | Default 6. The page has no text from the value. |
| A tree of 600 files | `data-files="500"`, `data-cap="1"`. |
| A tree of 600 files, and the opened file is file 550 | The opened file is in the page and in the tree. `data-files="500"`. The cap note says 500. |
| A file of 1048577 bytes | `big`. Not in the page. |
| A file named `"><script>.md` and names with a space, `%`, `'`, `<`, `>`, `&`, a leading `-` | Each `data-path` and tree index line matches `^[A-Za-z0-9._~/%-]*$` after the state word. No element comes from a name. |
| `tree=off`, `--tree off`, or a root of `$HOME` or `/` | No `find` runs under the root. `--list-tree` prints line 1 and `off`. |
| A symlink to a file outside the root, a symlink folder, `.git/x.md`, `node_modules/x.md`, `.hid/x.md` | Not in the tree. |

## Done

`BUILD=../far2l.markdown-browser-tree-panel.build` and the check in `plan.md` exits 0 with `check ok`. Each table row above has a case in `testing/markdown/browser-page.sh` (script, scan and page text) or in `testing/markdown/browser-links-dom.sh` (page in headless Chrome). Some items get one manual check in Firefox and Chrome, listed in `gate.md`. They are Back, Forward, the toggle, the saved state, the focus, the narrow window and the scroll to a heading. The new cases were run against copies with the root rule removed, and they failed.

## Out of scope

A local server and live reload. A file that changes while the page is open. Non-Markdown files in the tree. Per-file script chunks (they need a new page policy source). A panel that the reader can resize. Any C++ change. The terminal Markdown view. Cleaning old pages from the cache folder.
