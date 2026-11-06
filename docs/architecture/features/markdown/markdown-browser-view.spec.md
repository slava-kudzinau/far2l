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

The code is `far2l/bootstrap/md-open.sh`. The Markdown parser is the vendored `far2l/bootstrap/marked.min.js` (marked v18.1.3). The script does not use md4c or `mermaid-ascii.js`.

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
md-open.sh <file.md> [--out <page.html>] [--no-open]
md-open.sh --print-association
```

| Input | Meaning |
|---|---|
| `--out <page.html>` | The page path. The folder must exist. An existing file is replaced. `--out` alone does not stop the open step. |
| `--no-open` | Writes the page and does not start a browser. |
| `--` | The next argument is the file name, also if it starts with `-`. |
| `TMPDIR` | The folder of the default page. Default is `/tmp`. |
| `MARKED_JS` | Another path for `marked.min.js`. |

| Exit | When |
|---|---|
| 0 | The page is written, and it is opened or its path is printed. With `--no-open`, nothing is printed. |
| 1 | The page cannot be written, or the opener fails. |
| 2 | No file, an unreadable file, an unknown option, or a page folder that does not exist. No page is written. |
| 3 | `marked.min.js` is not found. No page is written. |

### How the page is made

1. The script writes the head with a `Content-Security-Policy` meta and a short stylesheet. `<title>` is the base name of the file. `<base href>` is the `file://` URL of its folder.
2. `sed` removes a UTF-8 BOM and each CR at a line end. It doubles each `\`, then writes `</` as `<\/` and `<!--` as `<\!--`.
3. The text goes into `<script type="text/markdown" id="md">`.
4. The script adds `marked.min.js` inline, then the page script.
5. In the browser, the page script reverses the escapes and calls `marked.parse`. The result goes into `<main id="content">`, so the text shows before any network use.

The stylesheet follows `prefers-color-scheme`. The body width is 50em at most.

### How diagrams render

A fence is a diagram when the first word of its language is `mermaid`. This match is case-sensitive, but the F3 match is not. Other fences stay plain code.

1. The page adds one script element for mermaid 12.3.0 from `cdn.jsdelivr.net`, with a `sha384` integrity hash and `crossorigin="anonymous"`. It does this only if the page has a diagram.
2. On load, `mermaid.initialize` runs with `startOnLoad: false` and `securityLevel:'strict'`. The theme is `dark` or `default`, chosen one time at load.
3. `mermaid.parse` checks each diagram alone. A diagram that fails shows its source and its error. The others still draw.
4. `mermaid.run` draws the good diagrams.

### Content cases

| Case | Fixture | Result |
|---|---|---|
| Normal text, table, list, code | `plain.md` | Rendered. Empty files give an empty page. |
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

The fixtures are in the folder of story markdown-browser-view.

The policy is `default-src 'none'; script-src 'unsafe-inline' https://cdn.jsdelivr.net; style-src 'unsafe-inline'; img-src data: file:`.

### Temp files

- The default page is `${TMPDIR:-/tmp}/far2l-md-<pid>.html`.
- Each run writes one file. The script never deletes it. The operating system cleans the folder.
- A reused pid replaces the old page.

### Host dependencies

| Need | Detail |
|---|---|
| Shell tools | POSIX `sh`, `sed`, `cat`, `basename`, `dirname` |
| Opener | `open`, else `xdg-open`. If both are missing, the script prints the page path and exits 0. |
| Browser | Any browser that the opener starts. Only Chrome headless is tested. |
| Network | `cdn.jsdelivr.net` for a page with a diagram. No other network use. |
| Install, macOS | `Contents/Resources/` of the app: `md-open.sh`, `marked.min.js`, `marked.LICENSE.md`. Component `app`, `EXCLUDE_FROM_ALL`. |
| Install, other | `share/far2l/`, same files, component `base`. |

`md-open.sh` reads `marked.min.js` from its own folder, so the files must stay together. The copy step of the build leaves the script out of `MacOS/`.

### Boundaries

**Always:**

- Treat the file as untrusted.
- Show the text before any network use.
- Keep the mermaid URL pinned, with its hash.

**Ask first:** add another network origin, or ship a default association.

**Never:**

- Change the Markdown file through the Alt-F3 path.
- Run raw HTML from the file.
- Load a remote image.
- Edit `config.ini` or read `mermaid.min.js`.
- Change the terminal view.

## Tests

The only check is the `check.sh` script of story markdown-browser-view, with its fixtures. It runs each row above with `--no-open`, renders pages in headless Chrome and reads the install rules through a wrapper project. It needs Chrome and the network.

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

- `--out` equal to the input file destroys the input. The redirection empties the file before `sed` reads it. Alt-F3 cannot reach this case.
- In-page `#anchor` links do not work. marked writes no heading ids, and `<base href>` sends the link to the folder.
- The default temp name is predictable when `TMPDIR` is not set. The write follows a link that another user put at that path.
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
- A new vendored `marked` needs a new check for `</script`. The current file has one `<!--`, in a regex, and the page accepts it.
