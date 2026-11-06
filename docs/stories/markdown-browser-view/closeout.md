# Close-out draft: markdown-browser-view

Commits: 92a76a055 (script, vendored marked, check), 74b543adf (final-review fixes); e08c5e117 (CDN decision in story.md, owner-checked path, shown to the owner). Check: `sh stories/markdown-browser-view/check.sh` exit 0 (54 pass).

## Rulings

1. Mermaid loads from jsdelivr, pinned 12.3.0, with SRI. No local `mermaid.min.js`.
2. The Alt-F3 command is `sh <abs path to md-open.sh> !.!` with `!.!` unquoted, because far2l escapes it. The script path is quoted if it has an unsafe character.
3. The page shows the Markdown text first. Mermaid is added by a script the page creates afterward. A slow or blocked CDN never blanks the page.
4. Embedded text escaping is reversible: `\` first, then `</` and `<!--`.
5. Install: macOS puts the script, `marked.min.js` and the license in `Contents/Resources/` (EXCLUDE_FROM_ALL); Linux puts them in `share/far2l`. `md-open.sh` is out of the `MacOS/` copy glob.
6. A CSP meta allows inline script and style, the one CDN origin, and `img-src data: file:`. Remote images do not load (stated in the gate).
7. `story.md` does not promise a shipped association; the script prints the entry and the owner adds it once (main session edited it).

## Parked findings

- First review #9: in-page `#anchor` links do not work (accepted limit).
- #10: `LC_ALL=C` is exported to `open` and `xdg-open`.
- #11: the fallback temp name `/tmp/far2l-md-<pid>.html` is predictable when `TMPDIR` is unset.
- #12: some test gaps remain (mutation coverage of the link filter and the mermaid label escape).
- #13: `check.sh` pins the base commit.
- T3 re-review: names with `<`, `>` or a leading `#` still break the printed command; a `--out` name starting with `-` reaches `open`; the cmake `file(GLOB)` runs at configure time only.
- Second final review: `--out` equal to the input file overwrites it (not reachable from Alt-F3); the stalled-CDN check can pass if `nc` fails to bind.
- Linux install layout was verified only through a wrapper project, not on Linux.

## Lessons

None for the plugin. The lite tier was promoted after the first final review found six Important issues; a security-sensitive script (shell quoting, HTML embedding) fits standard from the start.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| final-reviewer-light | sonnet | 2 | 0 | 6 | 11 |
| task-reviewer | haiku | 1 | 0 | 0 | 3 |
