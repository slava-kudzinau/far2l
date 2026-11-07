# Close-out draft: markdown-browser-github-look

Commits: 3fec17d70 (T1 vendor files and spike), d310c966b (T2 ini, flags, safe launch, test script), 045d44524 (T3 page template), f41419826 and 7270c063b (T4 gear, popover, keys, and its fix round), 8f47d6937 and b2357ce12 (T5 stable cache page, and its fix round). Check: the AGENTS.md check with `sh testing/markdown/browser-page.sh` added, exit 0 (`check ok`; 42 cases of browser-page.sh pass).

## Rulings

1. The Firefox localStorage spike is manual and waits for the owner (spike.md). Chrome 156: two file:// pages share the value. The page does not depend on this result.
2. highlight.min.js and marked.min.js each hold one `<!--`. The closing-tag test checks only `</script` and `</style`. This is safe in the HTML parser.
3. The narrow rule is `html[data-width] #content { width: 100% }` inside the `@media` block. The literal `#content` rule loses on specificity.
4. Under 700 px only the gear is hidden. The popover stays reachable with `?`, as the spec says.
5. `--out` keeps the old direct write. Only the cache page and the TMPDIR fallback use a temporary file and `mv`.
6. A `$BROWSER` value that starts with `-` is rejected.
7. Each mermaid redraw batch has a 10 s timeout. The queue skips a waiting batch, and a redraw is skipped when the drawn theme is unchanged.
8. Stale-page logic: the md-view.ini state (size and time) is a comment in the page head. The page key is read from the page head only, so Markdown text cannot fake it.

## Parked findings

- T4: focus is lost when Esc closes the popover under 700 px (the gear is hidden). A mermaid run that ends after its timeout can still write an SVG. After a timeout, pressing the same theme does not retry.
- T5: reuse compares the Markdown file by mtime only. `<base>` of the page name replaces bytes, not characters (change the spec word "character" to "byte").
- Final review, all Minor: the page script is tested as text only (a syntax error or swapped `all`/`not all` would pass; add a case that runs the script with node or a headless browser); reuse misses a package upgrade that keeps old mtimes; a read error inside the page write is not detected, so a bad page can be reused (end each `cat` with `|| exit`); the vendored file list is in three places (use one variable); a dark page can flash light before the script runs (write the start `media` from the shell); `cache_tmp_removed_on_error` waits with no time limit (add a watchdog).
- T3 Minors 1 to 8 and T2 Minors 1 to 7 are in task-3-review.md and task-2-review.md (polish).

## Close-out work for the main session

- `gate.md` must list the manual check in Firefox and Chrome (gear, popover, keys, saved choice, theme switch, mermaid redraw, print). The owner also runs the Firefox storage spike steps in spike.md.
- Update `markdown-browser-view.spec.md`, `markdown.data.md` and ADR-005 (page name in the cache folder, md-view.ini, flags, vendored files in `far2l/bootstrap/`, the page policy is unchanged). Add the invariants this story set (no value reaches the page except as an accepted word; `localStorage` only inside `try`).
- The check needs `git add -f` for new files under `far2l/bootstrap` and `testing`: the root `.gitignore` matches them. All files of this story are tracked.

## Lessons

- The plan's T1 done check used `;` and `!` in one `sh -c`, so only the last command set the exit status. A done check must guard every condition (`|| exit 1`). The T1 reviewer found this.
- The T4 re-review ran on haiku after an Important finding. The escalation rule says a later review runs as critical. The final-reviewer re-read the T4 code and found nothing.
- The orchestrator shell is zsh: `for t in $TESTS` does not split words. Run the check through `sh` (`.build/run-check.sh`).
- There is no `main` or `origin/main` ref in this repo, so `handoff.sh final` fails. The diff from BASE works.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| task-reviewer-critical | opus | 3 | 0 | 2 | 15 |
| task-reviewer | haiku | 2 | 0 | 1 | 15 |
| final-reviewer | opus | 1 | 0 | 0 | 6 |
