# Plan: GitHub look for the Alt-F3 browser page

Spec: `spec.md` (governs exact values). No design doc: the design is here, in the tasks and their interfaces.

Read before editing: `docs/architecture/features/markdown/markdown.invariants.md`, `docs/architecture/decisions/adr-005-browser-view-script.md`, `far2l/bootstrap/md-open.sh`.

## Global Constraints

- POSIX `sh` only, in the style of the current `md-open.sh` (`LC_ALL=C`, no bashisms, no `eval`, no `sh -c`, no unquoted expansion of a user value). Needed tools: `sed`, `cksum`, `find`, `mkdir`, `chmod`, `mv`, `cat`. No C++ change.
- Files this story may touch: `far2l/bootstrap/md-open.sh`, `far2l/bootstrap/CMakeLists.txt`, the new vendored files in `far2l/bootstrap/`, `testing/markdown/browser-page.sh` (new), `testing/markdown/browser-page/` (new, fixtures), and `docs/stories/markdown-browser-github-look/` (notes of T1). Nothing else.
- Vendored file names, exact: `github-markdown-light.css`, `github-markdown-dark.css`, `github-markdown.LICENSE.md`, `highlight.min.js`, `hljs-github.min.css`, `hljs-github-dark.min.css`, `highlight.LICENSE.md`. The license files name the version and the sha384 of each file.
- The page policy is not wider than ADR-005: `default-src 'none'; script-src 'unsafe-inline' https://cdn.jsdelivr.net; style-src 'unsafe-inline'; img-src data: file:`. Every vendored file is inlined in the page. The mermaid URL and its SRI hash stay as they are.
- Markdown text, file names, config values and paths are untrusted. A value from the config or a flag reaches the page only if it equals one accepted word. Names that reach HTML use the escape that `title` uses today.
- Test script `testing/markdown/browser-page.sh` needs only `sh`. It does not need the far2l binary. It reads the script under test from `$MD_OPEN` (default `far2l/bootstrap/md-open.sh`). It uses a fresh temporary `HOME`, `XDG_CONFIG_HOME` and `XDG_CACHE_HOME`, and never touches the real ones. It prints `ok <case>` or `FAIL <case>` and exits 1 on any failure.
- Build folder of this worktree: `../far2l.markdown-browser-github-look.build` (a sibling, inside `.worktrees/`). Set `BUILD` to it. Before each smoke test, clean the stale sockets as `AGENTS.md` says.
- Commit message: a short imperative summary line, a blank line, then the trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. One commit per task.
- Check: the single `The check` command of `AGENTS.md`, with `sh testing/markdown/browser-page.sh &&` added before the final `echo "check ok"`. It is:

  ```sh
  BUILD=${BUILD:-../far2l.build}; APP=$(cd "$BUILD" && pwd)/install/far2l.app/Contents/MacOS/far2l; TESTS=${TESTS:-0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off}
  cmake --build "$BUILD" --target far2l -j8 && sh testing/markdown/ascii-golden.sh && sh testing/markdown/ascii-driver.sh && (cd testing && for t in $(echo $TESTS); do o=$(./far2l-smoke-run.sh "$APP" "$t" 2>&1) && echo "$o" | grep -q '\[DONE\]' && ! echo "$o" | grep -q FAILED || { echo "FAILED: $t"; exit 1; }; done) && sh testing/markdown/browser-page.sh && echo "check ok"
  ```

  A task whose done check is `sh testing/markdown/browser-page.sh` does not need the build.
- Owner-checked paths: `AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`, `testing/markdown/golden/*.txt`, `far2l/src/markdown/md4c/`, `far2l/third_party/quickjs/`, `.github/workflows/`. No task edits them. The close-out updates `markdown-browser-view.spec.md`, `markdown.data.md` and ADR-005.
- Rulings from the fresh read (they settle open choices; the spec still governs exact values):
  - `md-view.ini`: a line that starts with `#` or `;` is a comment. The last copy of a repeated key wins. Keys outside `[View]` are ignored. Key names are lowercase and exact.
  - A `browser=` value that is a directory ending in `.app` runs as `open -a <value>` on macOS. Else the value must be an executable file.
  - `--no-cache` builds the page and writes it to the stable cache path, over the old page.
  - The reuse rule records the state of `md-view.ini` in the page as a comment: its size and modification time, or `none`. A deleted, added or edited ini makes the page stale.
  - The script removes its temporary file on error and on INT, TERM and HUP (`trap`).
  - Keys: `t` cycles `auto`, `light`, `dark`. `w` cycles 60, 70, 80, then 60 again.
  - Reset removes the saved choice and applies the `md-view.ini` values in place, with no reload.
  - The "Storage is blocked" notice shows in the popover only, after the first failed read or save.
  - The paste block is three lines: `[View]`, `width=80`, `theme=dark` (the current values).
  - Mermaid themes are `default` for light and `dark` for dark. The highlight themes style the `hljs` class that `highlight.js` adds to each `code` element.
  - The script puts the vendored files inside `style` and `script` elements of the page. T2 or T3 adds a test that no vendored text holds a closing `</script` or `</style` tag.
  - Button labels, popover text, the `sample.md` content and the shape of the redraw list are the implementer's choice inside the spec. The task reviewer checks them.

## T1: Spike, and the vendored files

**Depends on:** none
**Done check:** `sh -c 'cd far2l/bootstrap && for f in github-markdown-light.css github-markdown-dark.css highlight.min.js hljs-github.min.css hljs-github-dark.min.css github-markdown.LICENSE.md highlight.LICENSE.md; do test -s $f || exit 1; done; ! grep -l "</script" highlight.min.js; ! grep -il "</style" *.css; grep -q sha384 highlight.LICENSE.md github-markdown.LICENSE.md'` exits 0, and `cmake --build ../far2l.markdown-browser-github-look.build --target bootstrap` exits 0.
**Role:** implementer-risky — review task-reviewer-critical

This task may use the network once, for the downloads below. It installs nothing.

1. Download these pinned releases. The versions are the plan's best knowledge. Verify that each exists on npm. If not, take the next lower one. First: `github-markdown-css@5.8.1`. Files: `github-markdown-light.css`, `github-markdown-dark.css`, `LICENSE`. Second: `@highlightjs/cdn-assets@11.11.1`. Files: `highlight.min.js`, `styles/github.min.css`, `styles/github-dark.min.css`, `LICENSE`. Use the "common" build of `highlight.min.js`. Download each into a new empty folder and pass paths as arguments.
2. Copy the files to `far2l/bootstrap/` under the names in the Global Constraints. Write the two `*.LICENSE.md` files. Each holds the license text, the package name and version, and the source URL. Each also holds the sha384 of every copied file (`openssl dgst -sha384`). Record the size of `highlight.min.js` and its language list.
3. Check that `highlight.min.js` holds at least `javascript`, `python`, `bash`, `json`, `c`, `cpp`, `xml`, `css`, `yaml`, `markdown`, `shell`. If a language is missing, report it. Do not build a custom set unless the file passes 180 KB.
4. In `far2l/bootstrap/CMakeLists.txt`, add the seven files to the `install(FILES ...)` list that holds `marked.min.js`. Use the same `DESTINATION`, `COMPONENT` and options. Change nothing else.
5. Spike for the storage unknown. Write a 20-line page in the scratch folder. It writes a value to `localStorage` and reads it back. Open two copies with different names, in Firefox and in Chrome, with `open -a`. Record in `docs/stories/markdown-browser-github-look/spike.md` whether the second copy sees the value of the first. Four lines: browser, same value seen yes or no, the version, the date. If a browser cannot be driven, say so. Do not guess.

## T2: Defaults file, flags, browser launch, and the test script

**Depends on:** T1
**Done check:** `sh testing/markdown/browser-page.sh` exits 0, with the cases `flags_ok`, `flag_width_bad`, `flag_theme_bad`, `flag_width_missing`, `ini_valid`, `ini_hostile`, `ini_missing`, `ini_browser_bad`, `init_config_writes`, `init_config_keeps`, `print_association_ok`, `browser_no_shell`, `file_name_hostile`.
**Role:** implementer-risky — review task-reviewer-critical

All work is in `md-open.sh` and the new test files. The page body does not change in this task, except for the start values below.

- Parse `--width N`, `--theme T`, `--browser B`, `--init-config`, `--no-cache` in the existing option loop. `--width` takes 60, 70 or 80. `--theme` takes light, dark or auto. Any other value, or a missing value, calls `usage` (exit 2). `--no-cache` is stored and used in T5.
- Add a function `read_ini` that reads `md-view.ini` with `while IFS= read -r line`. It removes CR and spaces around the key and value, skips comments, and tracks the `[View]` section. It sets `cfg_width`, `cfg_theme`, `cfg_browser` only for exact matches (the browser may also be an absolute path to an executable file). No value is ever used as code.
- Resolve once: `width` (flag, else file, else 70), `theme` (flag, else file, else `auto`), `forced` (1 if a flag set a width or theme). Write them on the `<html>` tag as `data-start-width`, `data-start-theme`, and `data-forced`.
- `--init-config` makes the folder, writes the sample if the file is absent, prints the path, and exits 0. It never overwrites. The sample is commented, has the `[View]` section with `width=70`, `theme=auto`, `browser=auto`, and a comment line that lists the browsers found with `command -v` and `/Applications/*.app`.
- `--print-association` adds the path of `md-view.ini` and the accepted values. The old text stays.
- Replace the open step with a function `open_page` as in the spec table "Opening the browser". It uses `exec` for the final call (in a subshell, so the script can report the exit status). A value that is not an accepted word or an executable path prints one stderr line and uses `auto`.
- Create `testing/markdown/browser-page.sh` and the fixtures in `testing/markdown/browser-page/`. The file `sample.md` has a table and a task list. It also has a `js`, a `python` and an `sh` fence, a fence with no language, and a `mermaid` fence. The test makes hostile names at run time in its temporary folder. One name has a quote and `<script>`. One name has `$(touch PWNED)`. `browser_no_shell` sets `browser=` to a value with `;touch PWNED` and checks that no `PWNED` file exists. All cases run the script with `--no-open --out`.

## T3: The page template

**Depends on:** T1, T2
**Done check:** `sh testing/markdown/browser-page.sh` exits 0, with the new cases `page_has_github_css`, `page_width_rules`, `page_start_values`, `page_policy_unchanged`, `page_narrow_rule`, `page_hljs_after_marked`, `page_mermaid_theme_hook`. Run it also with `MD_OPEN` set to a copy where the `width: 80%` rule reads `width: 8%`. Then `page_width_rules` prints `FAIL`.
**Role:** implementer — review task-reviewer

- Replace the old `<style>` block. Inline these blocks in this order. First the light GitHub style, then the dark one. Then the light highlight theme and the dark one. Last, a small block of the page's own rules. The inline styles use `id="gh-light"`, `id="gh-dark"`, `id="hl-light"`, `id="hl-dark"`. The page script enables one pair by setting the `media` attribute to `all` or `not all`. Read each file with `cat` from the script folder. If a file is missing, exit 3 with `md-open: cannot find <file>`.
- Own rules: `#content` gets the class `markdown-body`. `html[data-width="60"] #content { width: 60% }`, and the same for 70 and 80. `#content { max-width: none; margin: 0 auto; box-sizing: border-box }`. `@media (max-width: 699px) { #content { width: 100% } }` placed after the width rules. The `body` background and text color come from the active GitHub style. Keep the mermaid and error rules that exist now, restyled to read well on both themes.
- Order of scripts: the Markdown text, `marked.min.js`, `highlight.min.js`, then the page script. `marked` keeps `gfm: true` (default) and the same `html` and `code` renderer overrides. Do not add a `highlight` option to `marked`.
- In the page script, after `content.innerHTML = marked.parse(src)`: for each `pre > code` whose class is `language-X`, where `hljs.getLanguage(X)` is true, call `hljs.highlightElement` inside `try/catch`. A `mermaid` block is already a `pre.mermaid`, so it is skipped. Add the class `hljs` handling that the highlight themes need.
- Function `usedTheme()` returns `dark` or `light`. It uses `data-theme`, and `matchMedia('(prefers-color-scheme: dark)')` for `auto`. Function `applyTheme()` sets the four `media` attributes and `color-scheme`, and redraws mermaid. Start values come from `data-start-width` and `data-start-theme`. T4 adds the saved choice.
- Mermaid: keep `src` in a `data-src`-like JS list. A function `drawDiagrams` calls `mermaid.initialize` with `theme` from `usedTheme()`. A function `redraw` puts the source text back in each block, removes `data-processed`, and runs `mermaid.run`. A `change` listener of the `matchMedia` query calls `applyTheme()` when the theme is `auto`. Keep the SRI hash and URL as they are.
- The page also holds `<meta name="far2l-md-key" content="w=70;t=auto;f=0">` built from the accepted words. T5 uses it.
- Test cases read the page text with `grep -F`. `page_hljs_after_marked` checks that the `highlight.min.js` banner comes after the `marked` banner in the file.

## T4: Gear, popover, keys, and the saved choice

**Depends on:** T3
**Done check:** `sh testing/markdown/browser-page.sh` exits 0, with the new cases `page_gear_markup`, `page_print_hidden`, `page_storage_try_catch`, `page_keys_guard`, `page_paste_line`.
**Role:** implementer — review task-reviewer

All the new code is in the page script and the page styles in `md-open.sh`. Build the gear and popover with DOM calls and `textContent`. Do not use `innerHTML` for any value.

- Gear: a `button#md-gear` with `aria-label="Page settings"`, fixed at top 8 px and right 8 px, 28 px square, low contrast (`opacity: .45`, full on hover or focus). It is hidden under 700 px wide. It never takes layout space (`position: fixed`).
- Popover `#md-pop`, fixed under the gear. Rows: width (three buttons, `aria-pressed`), theme (three buttons), a help line "Keys: t theme, w width, ? help", the paste block with the current `[View]`, `width=N`, `theme=T`, a Reset button. A click outside, or Esc, closes it. Opening it does not change the position of `#content`.
- Keys `t`, `w`, `?` on `keydown`. Ignore when `ctrlKey`, `altKey` or `metaKey` is true, and when `event.target` is `input`, `textarea`, `select` or content-editable.
- Functions `load()` and `save()`. Both wrap `localStorage` in `try/catch`. Keys `far2l-md-width` and `far2l-md-theme`. A saved value is used only if it equals an accepted word. With `data-forced="1"` the page ignores the saved values on load. When a read or write throws, set a flag. The popover then shows "Storage is blocked. Your choice is not kept." above the paste block.
- `@media print { #md-gear, #md-pop { display: none !important } }`.
- Do not make a visible bar. Do not move the text on any state.
- Cases check the markup and the source text of the script (`localStorage` only inside `try`), not the browser behavior. The browser behavior has one manual check, in `gate.md`.

## T5: Stable page path and reuse

**Depends on:** T4
**Done check:** `sh testing/markdown/browser-page.sh` exits 0, with the new cases `cache_path_stable`, `cache_two_files`, `cache_reuse`, `cache_rebuild_on_edit`, `cache_rebuild_on_flag`, `cache_rebuild_on_ini`, `cache_no_cache_flag`, `cache_dir_mode`, `cache_dir_fallback`, `cache_name_safe`.
**Role:** implementer-risky — review task-reviewer-critical

- Real path: `dir=$(cd "$(dirname -- "$file")" && pwd -P)`, then `real=$dir/$(basename -- "$file")`. Name: `<base>-<crc>.html`, as in the spec. `crc=$(printf '%s' "$real" | cksum | sed 's/ .*//')`. `base` uses `sed 's/[^A-Za-z0-9._-]/_/g'` and `cut -c1-40`.
- Folder `${XDG_CACHE_HOME:-$HOME/.cache}/far2l/md`: `(umask 077; mkdir -p "$d")`, then `chmod 700 "$d"`. Pages are made under `umask 077`. If it fails, print one stderr line and use the old temporary path.
- `--out` and `--no-cache` skip the reuse test. Else reuse if the page exists, `find <inputs> -newer "$page"` prints nothing, and `grep -F` finds the key `content="w=..;t=..;f=.."` of this run in the page. The inputs are the Markdown file, the script, every vendored file and `md-view.ini` (if it exists).
- Build to `$page.tmp.$$` in the same folder, then `mv -f`. On any error, remove the temporary file.
- With reuse, the script goes straight to `open_page`.
- The cases use a fresh `XDG_CACHE_HOME`. Make a file newer with `touch -t`. Check the mode with `ls -ld`.
