---
description: The Alt-F3 browser page looks like GitHub, highlights code, and has a width (60, 70 or 80 percent) and a theme (light, dark or auto) that the reader sets in a small page control or in a defaults file.
status: in-review
size: M
risk: none
depends-on: markdown-browser-view
---

# Story: GitHub look for the Alt-F3 browser page

## Goal

The owner reads Markdown files in the browser with Alt-F3. Today the page has a small custom stylesheet and no code highlighting. The owner wants the GitHub look, highlighted code, mermaid diagrams, a content width of 60, 70 or 80 percent, and light or dark colors. The owner changes the layout rarely, so the control is small, sits outside the text, and the choice is kept.

## Context

- [ADR-005](../../architecture/decisions/adr-005-browser-view-script.md): the page is made by `far2l/bootstrap/md-open.sh`, with vendored `marked.min.js` and mermaid 12.3.0 from a pinned CDN.
- [markdown-browser-view.spec.md](../../architecture/features/markdown/markdown-browser-view.spec.md) and [markdown.data.md](../../architecture/features/markdown/markdown.data.md).
- Install rules: `far2l/bootstrap/CMakeLists.txt` (line 121) installs `marked.min.js` and its license next to the script.

## Scope

**In:**

| Part | Rule |
|---|---|
| GitHub style | Vendor `github-markdown.css` (MIT, light and dark in one file). The page uses it for the rendered body. |
| Code highlight | Vendor `highlight.min.js` (BSD-3, common languages) and one light and one dark theme CSS. Code fences get highlighted after `marked` runs. A fence with `mermaid` is not highlighted. |
| Mermaid theme | Mermaid stays on the pinned CDN (ADR-005 does not change). It starts with the `default` theme in light and `dark` in dark. In `auto`, a change of the system theme redraws the diagrams. |
| Width | The content column is 60, 70 or 80 percent of the window width. Default 70. |
| Theme | `light`, `dark` or `auto` (system setting). Default `auto`. |
| Browser | The key `browser=` in `md-view.ini` takes `auto` (default), `firefox`, `chrome`, `safari` or the full path of an executable. `auto` uses `open` or `xdg-open` and honors `$BROWSER`. A named browser maps to `open -a` on macOS and to its command on Linux. The script runs `exec "$browser" "$url"` with no shell evaluation. A browser that is not installed falls back to `auto` with one line on stderr. `--browser` sets it for one run. |
| Override flags | `md-open.sh --width N --theme T` sets the values for one run. The flags win over the file. |
| Defaults file | `md-open.sh` reads `${XDG_CONFIG_HOME:-$HOME/.config}/far2l/settings/md-view.ini`, an ini file with a `[View]` section and the keys `width=` (60, 70 or 80) and `theme=` (light, dark or auto). It writes them into the page as the start values. A missing file or a wrong value gives the defaults and no error. `--print-association` also tells where the file is and which values it accepts. `--init-config` writes a commented sample file if none exists, and never overwrites one. The sample lists the browsers it found. |
| Install | `far2l/bootstrap/CMakeLists.txt` installs the new vendored files with the script, and adds their license files. |
| Narrow window | Below 700 px the column is the full width. |
| Page control | One small gear, low contrast, fixed in the top-right gutter outside the text column. It opens a popover with width (60, 70, 80), theme (light, dark, auto) and a help line. The popover closes on a click elsewhere or on Esc. The keys `t` (theme) and `w` (width) do the same, and `?` opens the help. The control takes no layout space, never overlaps the text column, and is hidden when printing. |
| Saved choice | The choice is kept in browser storage (wrapped in try/catch). Order of use: saved choice, then the defaults file, then the built-in defaults (70 and `auto`). |
| Stable page path | The page goes to `${XDG_CACHE_HOME:-$HOME/.cache}/far2l/md/<name>.html`. `<name>` is a hash of the real path of the Markdown file. Each file has its own page, and the name is the same on every run. Browser storage then stays the same between runs. Two files can be open in two tabs. The folder is mode 700. `--out` still overrides it. |
| Reuse | The script opens the existing page and builds nothing if all of these are true. The page is newer than the Markdown file, the script, the vendored files and `md-view.ini`. The flags are the same. Else it builds the page again. `--no-cache` forces a build. |
| Storage fallback | If storage is blocked or forgets the choice, the popover shows a line to paste into the `[View]` section of `md-view.ini` (`width=80`, `theme=dark`). |

**Out:**

- Offline mermaid (the 5 MB bundle). It needs a new ADR; plan a separate story if wanted.
- A far2l Options dialog and `config.ini` keys. There is no C++ change here.
- The terminal Markdown view (F3).
- Live reload, a local web server, Linux and Windows openers.

## Size

M. One script, one page template, about five vendored files, one install change, one test. No contract in C++. Hard stop: if the page needs a second script or a C++ change, stop and ask the owner.

## Unknowns

- The exact `highlight.js` language set that keeps the file near 120 KB. The first task measures it and takes the common set.
- Whether browser storage survives between runs for a `file://` page in the owner's browser. Chrome shares it across local files. Firefox and Safari can give each file its own storage. Unmeasured. The first task is a spike in Firefox (the owner's default) and in Chrome. If it fails, the paste-into-conf line is the main path.
- Whether the Content-Security-Policy needs any change for the page script. The rule is: no wider policy than ADR-005 gives now.
- Linux layout of the new files was not tested in ADR-005 either; the wrapper-project check covers it.

## Acceptance criteria

- A page made from a sample file shows GitHub styling: headings, tables, task lists, quotes, inline and fenced code.
- A fenced `js`, `python` and `sh` block is highlighted in light and in dark. A fence with no language and a `mermaid` fence are not highlighted as code.
- Width 60, 70 and 80 give a column of that share of the window.
- A script test checks the width rule in the generated page. One manual check in a browser confirms it.
- Theme `light`, `dark` and `auto` work. `auto` follows the system. A mermaid diagram changes its theme with the page theme.
- `md-view.ini` with `[View]`, `width=80` and `theme=dark` gives those start values. A bad or missing file gives 70 and `auto`, and the script exits 0.
- `browser=firefox`, `browser=chrome`, an executable path and `auto` each open the page in that browser. A value with spaces, quotes or `;` is not run as a shell command. A missing browser falls back to `auto`.
- `--width` and `--theme` override the file for one run. A wrong flag value prints usage and exits 2.
- `--init-config` writes a sample file with the defaults, and does not overwrite an existing file.
- The gear sits in the gutter and the popover shows width, theme and the help line. At 60, 70 and 80 percent the control never overlaps the text column, and opening it does not move the text.
- The keys `t`, `w` and `?` work, and are ignored when a modifier key is held or a form field has focus.
- A saved choice is used on the next Alt-F3 run of another file. If storage throws, the page still renders and the popover shows the paste-into-conf line.
- The control is hidden when printing.
- Two different files get two different page names. A second Alt-F3 on an unchanged file builds nothing and opens the same page. Editing the file, the config or a flag builds a new page.
- Hostile input: the config values cannot inject into the page (a value like `"><script>` gives the default). Raw HTML in the Markdown still shows as text, and the policy is not wider than before.
- The page works with no network: text and highlighting show, mermaid shows its source.
- A new smoke or script test covers the generated page. It was run against a build with a broken width rule and failed.
- The check in `AGENTS.md` passes. `--print-association` output is still right.
- Close-out: update `markdown-browser-view.spec.md` with the style, the settings and the config file.
- Close-out: update `markdown.data.md` with the new vendored files and the config file.
- Close-out: add a note to ADR-005, or a new ADR if the policy changes. Add the licenses.

## Notes

- Owner decisions, 2026-11-07: settings in a defaults file with run flags, and a small page control with a saved choice. Owner words: "the bar should not obscure text and occupe visual area" "the reading layout is not something we change often", and "CLI is hard to find, how to have a minimal control on the page, may be a hint or help too, the settings should be persistent". `highlight.js` vendored in the repo; mermaid stays on the CDN and follows the theme. Owner words: "use github markdown renderer with mermaid and code highlighting for the alt-f3 mode, the content should occupy 60/70/80% of width and use light/dark schemas these settings should be easily configurable somewhere".
- Branch `feat/markdown-browser-github-look`, base `next/2.9.1-ovr.1`. The brief is not committed until this branch exists.
- Budget/hard stop: M $15/$20.
- Out of this story: following links to other local `.md` files. Story `markdown-browser-linked-files` takes it and runs after this one, because both edit the script and the page.
- Assumed (owner can overturn): the gear, popover and keys as written above.
- Assumed: the owner has not named the browser yet. The storage spike tests Firefox first, then Chrome. The owner's default is Firefox, but it may be Chrome.

- Owner words, 2026-11-07: "we can keep default UI settings in ini file". Decision: a separate `md-view.ini`, because far2l rewrites `config.ini` on exit and would drop keys it does not know.
- Assumed: default width 70 and default theme `auto`. `github-markdown-css` is the GitHub style source. `marked` stays the parser, with GitHub-flavored mode on.
- started: 2026-11-07T11:19:21Z
- flow tier=standard docs=spec,plan design=no orchestrator=yes final=final-reviewer-light gate=read-later scan=no verifier=none
- Gate page: read later in the PR.
- Check: the command in `AGENTS.md`, plus the new script test of this story. Owner-checked paths: as in `AGENTS.md`.
- G4-merge: owner, 2026-11-07: "you can merg PRs by your own". The agent may merge this PR into `next/2.9.1-ovr.1` after the check is green. G3-spend still stops the build.
- Network: the owner chose vendored files, so the first task downloads pinned releases of `github-markdown-css` and `highlight.js` once. It records the version and the sha384 of each in the license notes.
