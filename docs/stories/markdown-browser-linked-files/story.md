---
description: A link to another local Markdown file in the Alt-F3 browser page opens that file, rendered, inside the same page.
status: planned
size: M
risk: security
depends-on: markdown-browser-github-look
---

# Story: Follow local Markdown links in the browser page

## Goal

The owner keeps notes in several Markdown files that link to each other. Today a relative link in the Alt-F3 page opens the other file as raw text. A page opened from `file://` cannot fetch or render another local file. The owner wants to click the link and read the next file in the same look, and to go back.

## Context

- [ADR-005](../../architecture/decisions/adr-005-browser-view-script.md): the page is made by `far2l/bootstrap/md-open.sh`. The page loads no local files. Raw HTML in the Markdown shows as text.
- Story `markdown-browser-github-look` changes the page template and the script first. This story edits the same files, so it runs after it.
- Owner decision, 2026-11-07: pre-render a bundle. A local server was considered and left out (it needs a new ADR, a process to run, and an L size).

## Scope

**In:**

| Part | Rule |
|---|---|
| Crawl | `md-open.sh` reads the start file and follows relative links to `.md` and `.markdown` files, depth first, with each file read once. A link with a `#anchor` follows to the file and the anchor. |
| Root | Only files under the folder of the start file are followed. A link that leaves it (`..` out of the root, an absolute path, a `file:` URL) is not followed. |
| Symlinks | A file that is a symlink to a target outside the root is not followed. |
| Limits | At most 50 files, 2 MB in total, and depth 5. Over the limit, the extra links stay plain links and the page tells the reader once, in the help line, how many were not followed. |
| One page | All files go into one page. The route is `#/relative/path.md`, so Back and Forward work. A link to a file in the bundle changes the route and shows the file at once. |
| Base for each file | Images and other relative links in a file resolve against that file's folder, not the start folder. |
| Not followed | A link to a file that is not in the bundle (not found, not Markdown, over the limit) works as before: the browser opens it. |
| Anchors | A link to `#heading` in the same file scrolls to the heading. |
| Untrusted input | Link targets are data. A name with spaces, `%` escapes, quotes, `<`, `>` or `&` cannot break out of the page. Raw HTML still shows as text. |
| Cache | The page keeps a manifest of the bundle. It lists each file with its size and modification time, and each link target that did not exist. An unchanged bundle opens from the cached page with no crawl. A changed, added or removed file, or a link target that now exists, builds the page again. The cache is checked in the same way as the page of the look story, and `--no-cache` forces a build. |
| Help | The help line of the page control lists the number of files in the bundle, and how many were not followed. |

**Out:**

- A local server, live reload, and files that change while the page is open (run Alt-F3 again).
- Links to other file types, which keep their current behavior.
- Any C++ change, and the terminal Markdown view.

## Size

M. One script, one page change, one test set with fixtures. Risk `security`: the script reads files named by untrusted text. Hard stop: if the crawl needs a tool other than POSIX `sh`, `sed`, `awk` and `grep`, stop and ask the owner.

## Unknowns

- Whether link extraction in POSIX tools is reliable for reference-style links (`[text][ref]`) and for links inside code fences. The first task builds a fixture and decides which forms are supported. Unsupported forms stay plain links, and the spec lists them.
- The size of a 50-file bundle in the browser. Unmeasured; the first task measures it.

## Acceptance criteria

- A fixture of three files that link to each other (including a cycle) opens as one page. Clicking each link shows the right file, and Back returns to the one before.
- A file in a subfolder with `![](img.png)` shows its own image.
- A link to `../outside.md`, an absolute path, a `file:` URL, and a symlink to a file outside the root are not followed.
- A bundle over 50 files, 2 MB or depth 5 stops at the limit and the page opens. The help line says how many links were not followed. A generated tree is the hostile fixture.
- File names with spaces, `%`, quotes, `<`, `>`, `&`, and a name that starts with `-` cannot inject into the page. A file name like `"><script>.md` shows as text.
- An unchanged bundle opens from the cache with no crawl. Each of these builds again: a changed file, a deleted file, and a new file that a link pointed to before.
- A single file with no links makes the same page as before (no route change).
- A cyclic link set ends. The check in `AGENTS.md` passes, and a new script test was run against a build with the root rule removed and failed.
- Close-out: update `markdown-browser-view.spec.md` with the link rules and the limits. Update `markdown.data.md` with the bundle. Add a note to ADR-005, since the page now holds more than one file.

## Notes

- Owner words, 2026-11-07: "what if the MD has a link to another (local) file is it possible to follow?" and "we need to have several files". Decision: bundle, as a separate story.
- Branch `feat/markdown-browser-linked-files`, base `next/2.9.1-ovr.1`, started after `markdown-browser-github-look` is merged. The brief is not committed until this branch exists.
- Budget/hard stop: M $15/$20.
