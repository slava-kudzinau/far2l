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

`far2l/bootstrap/md-open.sh <file.md>` writes one temp `.html` page and opens it with `open` (or `xdg-open`). Alt-F3 runs the script through a File associations entry. The script prints that entry with `--print-association`, and the owner adds it once. No association is shipped.

The page holds the Markdown text, the vendored `marked.min.js`, and a stylesheet. It loads mermaid 12.3.0 from the jsdelivr CDN with an `integrity` hash, and starts it with `securityLevel:'strict'`. A `Content-Security-Policy` meta allows inline scripts, scripts from all of `https://cdn.jsdelivr.net`, and local images only. Only the page script pins the mermaid URL and its hash. The text shows first, and the page adds the mermaid script after that, so a slow or blocked CDN never blanks the page. Raw HTML in the Markdown shows as text.

## Consequences

- The terminal view and far2l C++ code are not changed by the script.
- Diagrams need internet. Without it, each diagram shows its source.
- The policy trusts the whole jsdelivr host for scripts. The pin of the URL and the hash is in the page script only.
- Remote images do not load. This stops the file from sending the reader IP address to a remote host.
- The script path in the Alt-F3 command is quoted only when needed, and `!.!` stays unquoted because far2l escapes it.
- Install rules put the script, `marked.min.js` and the license in one folder: `Contents/Resources` on macOS, `share/far2l` on Linux. The Linux layout was tested only through a wrapper project.
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
