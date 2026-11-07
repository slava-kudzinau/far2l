---
description: The full Markdown view with diagrams is a shell script that Alt-F3 runs through File associations, and it shows the page in the system browser.
status: accepted
date: 2026-11-05
---

# ADR-005: Browser view as an external script

## Context

- The terminal Markdown view cannot draw every diagram well. The owner decided to keep it for text and to open a real web page for the full document.
- A browser draws every mermaid diagram type, so it is the fallback for what the terminal cannot draw.
- The Markdown file can come from anywhere.

## Decision

`far2l/bootstrap/md-open.sh <file.md>` writes one `.html` page and opens it in a browser. Alt-F3 runs the script through a File associations entry. The script prints that entry with `--print-association`, and the owner adds it once. No association is shipped.

The page is `${XDG_CACHE_HOME:-$HOME/.cache}/far2l/md/<base>-<cksum>.html`. The name comes from the real path of the file, so it is the same for each run. The folder has mode 700. The script writes a temporary file and moves it over the page. It opens the old page with no build while no input is newer and the key of the page matches. If the cache folder cannot be used, the script writes `${TMPDIR:-/tmp}/far2l-md-<pid>.html`. `--out <page>` writes that path directly.

The start width (60, 70, 80) and theme (light, dark, auto) and the browser come from `${XDG_CONFIG_HOME:-$HOME/.config}/far2l/settings/md-view.ini`, section `[View]`. The flags `--width`, `--theme`, `--browser`, `--no-cache`, `--out`, `--no-open` and `--init-config` act for one run, and a flag beats the file. The file is separate from `config.ini`, because far2l rewrites `config.ini`. A width or a theme reaches the page only as an accepted word.

The script starts the browser directly with `exec`, from a checked name or an absolute path, and never through `eval` or `sh -c`. `browser=` is `auto`, `firefox`, `chrome`, `safari` or an absolute path.

The page holds the Markdown text, the vendored `marked.min.js` and `highlight.min.js`. It also holds four style files (GitHub light and dark, highlight light and dark) and a short stylesheet. It has the GitHub look, a small gear with a popover for the width and the theme, and the keys `t`, `w` and `?`. The page saves the choice of the reader in `localStorage`, inside `try`. It loads mermaid 12.3.0 from the jsdelivr CDN with an `integrity` hash, and starts it with `securityLevel:'strict'`. A `Content-Security-Policy` meta allows inline scripts, scripts from all of `https://cdn.jsdelivr.net`, and local images only. The policy did not change when the vendored files were added, because the script inlines them. Only the page script pins the mermaid URL and its hash. Mermaid is still on the CDN. It follows the page theme and draws again when the theme changes. The text shows first, and the page adds the mermaid script after that, so a slow or blocked CDN never blanks the page. Raw HTML in the Markdown shows as text.

## Consequences

- The terminal view and far2l C++ code are not changed by the script.
- Diagrams need internet. Without it, each diagram shows its source.
- The policy trusts the whole jsdelivr host for scripts. The pin of the URL and the hash is in the page script only.
- Remote images do not load. This stops the file from sending the reader IP address to a remote host.
- The script path in the Alt-F3 command is quoted only when needed, and `!.!` stays unquoted because far2l escapes it.
- A page is opened again with no build while no input changed. The reuse rule uses the modification time of the Markdown file only.
- The cache folder keeps one page for each Markdown file that the user opened. The script does not delete them.
- The vendored files must stay in the folder of the script. A missing file gives exit 3.
- Install rules put the script and the vendored files with their licenses in one folder: `Contents/Resources` on macOS, `share/far2l` on Linux. The Linux layout was tested only through a wrapper project.
- The terminal hint `(diagram not drawn here - Alt-F3 opens it in the browser)` points at this key. It does nothing until the owner adds the association.

## Rejected alternatives

### A new C++ viewer key

**Why rejected:** Alt-F3 exists, and a script is easy to change and test alone.

### Markdown to HTML in C++ with md4c

**Why rejected:** the script stays small and POSIX with `marked`, and no native tool is added.

### A local web server, or the 5 MB `mermaid.min.js` in the repository

**Why rejected:** nothing to start or stop, and no large file to ship. The owner chose the CDN on 2026-11-06. Revisit if offline diagrams become a need.

### The script edits `config.ini` to add the association

**Why rejected:** far2l rewrites `config.ini` on exit and could undo the edit.

### Loose mermaid security, or remote images allowed

**Why rejected:** the file may come from anywhere.
