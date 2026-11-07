# Plan: File tree and local links in the browser page

Spec: `spec.md` (governs exact values). Each implementer reads `spec.md` before its task. It holds the state words, the encoding, the escapes, the slug rule, the note texts, the key fields, the fallbacks and the scroll rules. No design doc: the plan has four tasks and no architecture change, so the design is here.

Read before editing: `docs/architecture/features/markdown/markdown.invariants.md` (section "Browser view"), `docs/architecture/decisions/adr-005-browser-view-script.md`, `docs/architecture/features/markdown/markdown-browser-view.spec.md`, `far2l/bootstrap/md-open.sh`, `testing/markdown/browser-page.sh`.

## Global Constraints

- POSIX `sh` in the style of the current `md-open.sh`: `LC_ALL=C`, no bashisms, no `eval`, no `sh -c`, no unquoted expansion of a user value. The scan and the build use `sh`, `sed`, `awk`, `grep`, `find`, `sort` and `cksum`. They also use the tools that `md-open.sh` uses at BASE (`basename`, `cat`, `chmod`, `cut`, `dirname`, `ls`, `mkdir`, `mv`, `rm`, `uname`). `find` has POSIX options only: no `-maxdepth`, `-printf` or `-print0`. Do not add `realpath`, `readlink`, `stat`, `wc`, `xargs`, `node` or another tool. If the work needs one, stop and report BLOCKED. This is the owner's hard stop.
- Files this story may touch: `far2l/bootstrap/md-open.sh`, `testing/markdown/browser-page.sh`, `testing/markdown/browser-page/` (fixtures), `testing/markdown/browser-links-dom.sh` (new), `testing/markdown/browser-links/` (new, fixtures), and `docs/stories/markdown-browser-tree-panel/measure.md`. Nothing else. No C++ change. No new vendored file.
- New files under `testing/` and `far2l/bootstrap/` need `git add -f`. The root `.gitignore` pattern `Testing` matches them on this case-insensitive disk.
- Untrusted input: the Markdown text, link targets, file names and folder names. Only these values reach the page from the script: an encoded path (`A-Za-z0-9._~/%-`), a decimal number, and an accepted word. The script never escapes a raw name for the page, except `<title>` and `<base href>`, which keep the escapes of today.
- `find` runs from the root (`cd "$root" && find . ...`). A path reaches a command only as `./<tree path>` or as an absolute path that starts with the root. A file is read only through `<` redirection, or as an argument after `--`, or with the `./` prefix.
- The page policy stays `default-src 'none'; script-src 'unsafe-inline' https://cdn.jsdelivr.net; style-src 'unsafe-inline'; img-src data: file:`. The mermaid URL and its SRI hash stay. The case `page_policy_unchanged` stays green.
- In one-file mode the page has no panel CSS and no panel markup. The script writes the tree CSS only in tree mode. The popover notice "File tree: off ..." comes from the page script, with `data-tree-why`.
- The page script builds DOM from Markdown only through `marked.parse` into a `template` element. Every other value (a name, a number, a note, the title) goes in through `textContent`, `setAttribute` or `document.title`. No `innerHTML` with a value.
- `localStorage` stays inside `try/catch`. The page key and the tree state are read from the head of the page only (`sed -n '1,/^<\/head>$/p'`), as today.
- Test scripts: `browser-page.sh` keeps its form. Each new case is a function `case_<name>` in a subshell, with `fresh`, added to the `for c in ...` list at the end. It prints `ok <case>` or `FAIL <case>`. `browser-links-dom.sh` has the same form. It finds Chrome from `$CHROME`, else `/Applications/Google Chrome.app/Contents/MacOS/Google Chrome`, else `google-chrome` or `chromium` in `PATH`. With no Chrome it prints `skip all` and exits 0. It runs `--headless=new --disable-gpu --no-first-run --user-data-dir=<temp> --virtual-time-budget=3000 --dump-dom <url>`, with a fresh temporary profile for each run. `--headless=new`, `--dump-dom` and `--virtual-time-budget` come from the spec facts. `--disable-gpu`, `--no-first-run` and the budget of 3000 are implementer choices. The implementer may change them if Chrome needs other values. The task report records the change. Test scripts make their temporary folder with `mktemp -d`.
- A done check guards every condition with `&&` or `|| exit 1`. Run test scripts through `sh`; the orchestrator shell is zsh. A done check is a shell command. It keeps the output of the test script. For each case that the task adds, it runs `grep -qx "ok <case>"`. It also runs `! grep -q '^skip'` on the output. This guard is for this Mac, where Chrome is present and no case skips. On Linux, limit it to the cases of the task (`grep -q "^skip \($cs_regex\)$"`), because an old case skips there. Each task shows the form. An exit code 0 is not enough. The test script exits 0 for a case that is not in its `for c in` list, and for a case that skips.
- Build folder of this worktree: `../far2l.markdown-browser-tree-panel.build` (a sibling, inside `.worktrees/`). It does not exist yet. Make it once with the `cmake -S . -B` command of `AGENTS.md`, with that folder. Before each smoke test, clean the stale sockets as `AGENTS.md` says.
- Commit message: a short imperative summary line, a blank line, then the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. One commit per task. There is no `main` ref; diff from BASE `7f6523ad5`.
- Check: the `The check` command of `AGENTS.md`, with this build folder and with `sh testing/markdown/browser-page.sh &&` added before the final `echo "check ok"`:

  ```sh
  BUILD=${BUILD:-../far2l.markdown-browser-tree-panel.build}; APP=$(cd "$BUILD" && pwd)/install/far2l.app/Contents/MacOS/far2l; TESTS=${TESTS:-0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off}
  cmake --build "$BUILD" --target far2l -j8 && sh testing/markdown/ascii-golden.sh && sh testing/markdown/ascii-driver.sh && (cd testing && for t in $(echo $TESTS); do o=$(./far2l-smoke-run.sh "$APP" "$t" 2>&1) && echo "$o" | grep -q '\[DONE\]' && ! echo "$o" | grep -q FAILED || { echo "FAILED: $t"; exit 1; }; done) && sh testing/markdown/browser-page.sh && echo "check ok"
  ```

- Owner-checked paths: `AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`, `testing/markdown/golden/*.txt`, `far2l/src/markdown/md4c/`, `far2l/third_party/quickjs/`, `.github/workflows/`. No task edits them. The close-out updates `markdown-browser-view.spec.md` (it adds the flags `--tree` and `--list-tree`), `markdown.data.md` and ADR-005.
- Rulings (they settle choices; the spec governs exact values):
  - The scan needs no root check in the script: `find` starts at the root and does not follow symlinks. The `find` line of the scan has the marker comment `scan-rule` on the line before it. Step 8 of the page script is one function `inRoot(parts)` with the marker comment `root-rule` on its first line. A regression copy changes the marked code with `sed`.
  - `root=repo` walk: a function `find_repo_root START` that checks `[ -e "$d/.git" ]` from `START` upward with `cd -P ..` and `pwd -P`. It compares with the real path of `$HOME`.
  - The depth prune pattern is built in a loop: `./` and then `tree_depth`+1 times `*`, joined with `/`.
  - The build reads all `page` files in one `awk` pass. It writes each `script` element with its escapes. It does not call `sed` once per file.
  - Tree order: an `awk` program prints `<depth> <tree path>` for each line of `find`. `LC_ALL=C sort -k1,1n -k2` sorts them. The cap cuts the sorted list.
  - `big` comes from a second `find` with `-size +1048576c` and the same rules. `cut` comes from the byte count in the build pass, and `--list-tree` runs the same count.
  - The panel width is a CSS variable `--md-panel: 260px`. The toggle is `button#md-tree-toggle`. The panel is `nav#md-tree` with `aria-label="Files"`.
  - The `root=repo` walk with no `.git` gives the `root=dir` folder. A non-number or `0` in a number key gives the default.
  - A folder that the reader opens by hand stays open. `show` only opens the folders of the shown file, and never closes one.
  - Mermaid errors and an offline CDN keep the behavior of today: the source shows, with the error line when there is one.
  - A test script removes its temporary folder with `trap` on exit, as `browser-page.sh` does now.
  - A case that can loop runs `md-open.sh` under a watchdog in the test script: a `sleep` in the background, then `kill` of the run. A kill fails the case. The 20 s of `sleep 20` is an implementer choice. It is far above the 0.5 s of a DOM dump plus the build of a 600-file tree.
  - The implementer chooses the fixture text, the names of generated files, the grep strings of each case, and the panel colors, inside the spec. The task reviewer checks them.

## T1: Settings, root and scan

**Depends on:** none
**Done check:** this command exits 0.

```sh
cs="tree_ini_keys tree_ini_ceiling tree_ini_hostile tree_flag tree_off_no_scan root_dir root_repo root_repo_none root_home_off scan_skip_rules scan_symlinks scan_depth scan_cap scan_big scan_order scan_names_hostile list_tree_flag"; m=docs/stories/markdown-browser-tree-panel/measure.md
o=$(sh testing/markdown/browser-page.sh) && [ -z "$(for c in $cs; do echo "$o" | grep -qx "ok $c" || echo "$c"; done)" ] && ! echo "$o" | grep -q '^skip' && grep -q '^scan time:' "$m" && grep -q '^awk pass:' "$m"
```

Every older case still prints `ok` (the script exits 0). Then make a copy of `md-open.sh` whose marked `find` has `-L` after `find`. Run `MD_OPEN=<copy> sh testing/markdown/browser-page.sh`. It must print `FAIL scan_symlinks`. Record the line in the task report.
**Role:** implementer-risky — review task-reviewer-critical

- Extend `read_ini` with `tree`, `root`, `tree_depth` and `tree_max_files`. A number must match `^[0-9]+$` and be 1 or more. Clamp to the ceiling. Parse `--tree` and `--list-tree` in the option loop.
- Functions: `find_repo_root`, `scan_tree` (sets the sorted list of `<state> <tree path>` lines, the count and the cap flag), `enc_path` (an `awk` program that prints the encoded path).
- `--list-tree` prints the result of the spec form and exits before any page work.
- `tree_off_no_scan` proves the spec rule "In one-file mode the script runs no `find` under the root" (spec.md, section "Scan", and the row for `tree=off` in "Validation"). It puts a `find` stub in `PATH` that logs each call, and checks that the log has no call under the root. It also checks that `--list-tree` prints line 1 and `off`.
- The `find` line uses only the POSIX primaries `-name`, `-path`, `-prune`, `-type`, `-size`, `-newer` and `-print`. T1 must show that the depth limit and the skip rules work with them. The fixture cases run on this Mac. If Linux cannot be run, say so in the task report. `gate.md` lists "Linux scan not run" for the owner at merge.
- Measure (the story asks for it in the first task). Generate a root with 500 files of 8000 bytes in 20 folders. Use `time` for the real times. Record two lines in `measure.md`. The line `scan time:` is `--list-tree` on that root. The line `awk pass:` is one `awk` pass that reads the 500 files and writes one `script` element for each, with the escapes of the spec. A stub is enough, because T2 builds the real pass. If the scan time or the awk pass is over 2 s, stop and report it to the main session.
- The test makes the fixtures at run time in the temporary folder:
  - a tree with `.git`, `node_modules`, `.hid` and a hidden file;
  - a symlink file and a symlink folder that point outside;
  - a chain of 14 folders, 600 small files, and one file of 1048577 bytes;
  - the hostile names of the spec.
- `root_home_off` sets `HOME` to the temporary folder and opens a file in it. `--list-tree` prints `off`.

## T2: Page in tree mode, and reuse

**Depends on:** T1
**Done check:** this command exits 0.

```sh
cs="tree_page_markup tree_page_index tree_page_attrs tree_page_cut tree_page_names_hostile tree_off_same_page tree_key_line cache_tree_reuse cache_tree_changed_file cache_tree_added_file cache_tree_deleted_file cache_tree_head_only cache_tree_no_cache"
o=$(sh testing/markdown/browser-page.sh) && [ -z "$(for c in $cs; do echo "$o" | grep -qx "ok $c" || echo "$c"; done)" ] && ! echo "$o" | grep -q '^skip'
```

**Role:** implementer-risky — review task-reviewer-critical

- In tree mode, write the page parts of the spec table "The page in tree mode". These are the `<base href>` of the root, the opened file first, the `page` files, the tree index, and the `html` attributes. One-file mode writes the page of today plus `data-tree="off"` and `data-tree-why`.
- Add the new fields to the page key. Write the tree state comment after the `far2l-md-ini` comment. Extend `page_fresh` with the tree rules of the spec.
- `tree_off_same_page` compares a `--tree off` page with a page of the script at BASE (`git show 7f6523ad5:far2l/bootstrap/md-open.sh`). The only lines that may differ are the `html` line, the key line, and the page script. In one-file mode the page has no panel CSS and no panel markup, so the head has no new CSS line. The tree CSS exists in tree mode only.
- `cache_tree_no_cache` runs a second call with `--no-cache` in tree mode on a page that a normal call would reuse. It checks that the page is built again, with the method of the `--no-cache` case of story 1. `cache_tree_reuse` uses the method of `cache_reuse` (the folder at mode 500, the page bytes the same). `cache_tree_head_only` puts a fake tree state line in the Markdown text and checks that it has no effect.

## T3: Page script: route, links, images and anchors

**Depends on:** T2
**Done check:** this command exits 0.

```sh
cs="dom_start_file dom_route_file dom_route_unknown dom_link_in_tree dom_link_not_in_tree dom_link_out_of_root dom_link_out_of_root_query dom_link_out_of_root_backslash dom_link_absolute dom_link_bad_escape dom_link_file_scheme dom_link_percent dom_link_big dom_other_link dom_image_base dom_heading_ids dom_anchor_same_file dom_anchor_other_file dom_one_file_anchor dom_names_hostile dom_raw_html_text dom_script_scheme"
o=$(sh testing/markdown/browser-links-dom.sh) && [ -z "$(for c in $cs; do echo "$o" | grep -qx "ok $c" || echo "$c"; done)" ] && ! echo "$o" | grep -q '^skip' && sh testing/markdown/browser-page.sh >/dev/null
```

Then make a copy of `md-open.sh` with the body of `inRoot` replaced by `return true`. Run `MD_OPEN=<copy> sh testing/markdown/browser-links-dom.sh`. It must print `FAIL dom_link_out_of_root`. Record the line in the task report. Rules 4, 5 and 7 of the link rules do not go through `inRoot`. Make a second copy that removes the rule 5 and rule 7 checks (the stops for `?`, `\`, a control byte and a decode error). It must print `FAIL dom_link_out_of_root_query`, `FAIL dom_link_out_of_root_backslash` and `FAIL dom_link_bad_escape`. Make a third copy that removes rule 4 (the stop for a target that starts with `/`). It must print `FAIL dom_link_absolute`. Record those lines too.
**Role:** implementer-risky — review task-reviewer-critical

- Page script functions:
  - `encPath(path)`: `encodeURIComponent` of each part, then `!'()*` to `%XX`.
  - `target(href, dir)`: text rules 1 to 9. Returns `{kind, path, frag}`. `kind` is `anchor`, `candidate`, `out`, `other` or `drop`.
  - `slug(text)`, `route()` (reads `location.hash`), `show(enc, frag)`.
- `dom_link_out_of_root_query` uses the target `../outside.md?x`. `dom_link_out_of_root_backslash` uses `..%5Coutside.md`. `dom_link_absolute` uses the target `/outside.md`. `dom_link_bad_escape` uses two targets: `bad%01.md` (a control byte after decode) and `bad%zz.md` (a bad `%` escape, a decode error). Each case checks that the link has no `href` and has the note ` (not in this tree)`. Each stop of the text rules gives plain text with the note, except rule 1 (anchor) and rule 2 (drop). Only `http:`, `https:` and `mailto:` keep the `href`.
- Read the files from the `script[type="text/markdown"]` elements, and the tree from `#md-tree`, into maps keyed by `ENC`. Ignore an `ENC` that does not match the spec pattern.
- Order inside `show`:
  1. `marked.parse` into a `template`.
  2. Remove script schemes.
  3. Give heading ids.
  4. Fix the links and images of the spec.
  5. Move the nodes into `#content`.
  6. Run `highlight.js`.
  7. Set `items` to the mermaid blocks of this file, and draw them. Load the mermaid script once, at the first file with a diagram.
  8. Set `document.title`, then scroll.
- On `hashchange`, call `show` again. If the file is the shown one, only scroll.
- In one-file mode, only heading ids and same-file anchors are new.
- `dom_route_file` also checks that `document.title` is the decoded base name of the shown file, as text.
- `dom_raw_html_text` puts raw HTML (`<b>x</b>` and a `script` tag) in the opened file and in a second file of the tree. It checks that each shows as text in `#content` in tree mode, with no element made from it.
- `browser-links-dom.sh` builds each page with `md-open.sh --no-open --out`, then dumps the DOM of `file://<page>` and of `file://<page>#/<ENC>`. The page URL uses the `base_url` encoding of `md-open.sh`. One case puts the page in a folder whose name has a space.

## T4: Panel, toggle and measure

**Depends on:** T3
**Done check:** this command exits 0.

```sh
cs="dom_panel_tree dom_panel_current dom_panel_folders_open dom_panel_root_name dom_panel_cap_note dom_panel_big_style dom_panel_toggle_markup dom_panel_off_notice dom_panel_width_rule dom_panel_gear_fixed dom_panel_names_hostile"; ps="page_tree_key_guard page_tree_storage_try page_tree_print_hidden page_tree_narrow_rule"; m=docs/stories/markdown-browser-tree-panel/measure.md
o=$(sh testing/markdown/browser-links-dom.sh) && [ -z "$(for c in $cs; do echo "$o" | grep -qx "ok $c" || echo "$c"; done)" ] && ! echo "$o" | grep -q '^skip' && p=$(sh testing/markdown/browser-page.sh) && [ -z "$(for c in $ps; do echo "$p" | grep -qx "ok $c" || echo "$c"; done)" ] && ! echo "$p" | grep -q '^skip' && for l in 'page bytes:' 'build time:' 'reuse time:' 'dump time:' 'date:'; do grep -q "^$l" "$m" || exit 1; done
```

**Role:** implementer-risky — review task-reviewer-critical

- Build `nav#md-tree` with DOM calls from the tree map: the root name, then nested `details` per folder and a link per file, in tree order. Mark the shown file and open its folders on each `show`.
- `dom_panel_cap_note` greps the spec form `Showing the first N files.` (N = `data-files`).
- `dom_panel_gear_fixed` opens a page with the panel open and with it closed. It checks that `#md-gear` has the same fixed position at the top right in both, and that the toggle does not cover it.
- `dom_panel_names_hostile` uses the hostile names of T1 (`"><script>.md` and the others). It checks three things. The panel has no element or attribute that comes from a name. Each name shows decoded, as text. `"><script>.md` adds no element.
- `button#md-tree-toggle`, key `b`, `data-panel` on `html`, the saved state `far2l-md-tree`. The CSS rules of the spec table "Panel".
- The popover lines of the spec: `b tree` in the keys line, and the one-file notice from `data-tree-why`. The page script writes the notice, so the one-file page markup does not change. The script writes the panel CSS only in tree mode.
- `dom_panel_off_notice` checks the notice text for each value of `data-tree-why`. It also checks that the keys line of the popover has `b tree` in tree mode and has no `b tree` in one-file mode.
- Measure: make the same root again (500 files of 8000 bytes in 20 folders). The T1 test removes its temporary folder. T1 recorded the scan time and the awk pass. Add five lines in `measure.md`, each with its label: `page bytes:`, `build time:`, `reuse time:`, `dump time:` and `date:` (ISO). The dump time is one DOM dump in headless Chrome, and it includes the page load. Use `time` for the real times.
