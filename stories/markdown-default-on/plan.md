# Plan: F3 on a Markdown file opens the Markdown view by default

Tier: lite. No `spec.md`: the spec is the section below.

## Spec

Today the viewer has one global Markdown flag. The first viewer reads it from the saved option `Opt.ViOpt.ViewerMarkdownMode` (`viewer.cpp:180`); later viewers inherit the last value (`InitMarkdown`, `viewer.cpp:299`). The saved per-file position cache can also set it (`viewer.cpp:410`).

| Case | Result |
|---|---|
| F3 on `a.md` or `a.markdown` (any letter case), setting on, no cache entry for the file | opens in Markdown mode |
| F3 on `a.txt`, no cache entry | opens in plain mode, even if the last viewed file was Markdown |
| Shift-F4 on `a.md`, close, F3 on `a.md` again | plain mode (the position cache holds the choice) |
| F3 on `a.md`, setting off, no cache entry | plain mode |
| The viewer setting | a check box "Open Markdown files in Markdown mode" in the viewer settings dialog; ini key `Markdown_Auto` in section `Viewer`, default 1 |
| Move to the next file in the viewer (the file list keys) | the rule above applies to the new file |
| Shift-F4 behavior, Markdown scrolling, the other modes | unchanged |

### Done

The check exits 0. It builds far2l in `../far2l.default-on.build` and runs smoke test `0008-view-md-default` (new). The test opens `sample.md` with F3 and no Shift-F4 and sees rendered text (no `# ` heading marker, a ruled heading), then opens a `.txt` file and sees plain text.

## Global Constraints

- C++ only in `far2l/src/viewer.cpp` (the open path), `far2l/src/cfg/config.hpp`, `far2l/src/cfg/ConfigOpt.cpp` (new field `Opt.ViOpt.ViewerMarkdownAuto`, int, default 1), the viewer settings dialog code, and the language files (one new string, in every language file that holds the viewer settings strings; copy the English text into the others). Test files under `testing/tests/0008-view-md-default/`. Do not change scrolling, palette or `MarkdownRender`.
- The extension test: case-insensitive suffix `.md` or `.markdown` of the file name.
- Keep the persisted option `ViewerMarkdownMode` working as before for its other uses; do not delete it. If an inherited value would make a non-Markdown file open in Markdown mode, ignore it (the rule table governs).
- Use `git add -f` for files under `testing/` (root `.gitignore`).
- Commit message: a short imperative summary line, a blank line, then the trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. One commit per task.
- Build dir: `/Users/Viachaslau_Kudzinau/Projects/far2l.default-on.build` (configure with the same flags as `../far2l.build`: read its `CMakeCache.txt`). Smoke tests: copy the test folder shape of `0007-view-file-markdown`; refresh `workdir` from `initdir` before a run; build `testing/src` with `make` for the `far2l-smoke` binary.
- Rulings (settle the choices the plan left open):
  - Decision order at open: (1) a position-cache entry for the file wins, whatever the setting; (2) else, if the setting is on and the name matches, Markdown; (3) else plain. The `InitMarkdown` inheritance and `ViewerMarkdownMode` seed do not set the mode for a file with no cache entry; they are not written back by this story. Change `viewer.cpp:180` and `:299` so they no longer apply to the starting mode, and keep `:410` as is (it already reads the cache).
  - Suffix test: name length greater than the suffix, so a file named exactly `.md` does not match; ASCII case folding; no trimming.
  - Moving to another file in the viewer (the keys that call the viewer's open-next path) reuses the same open path, so the rule applies there; add no new key.
  - Dialog: add the check box in the viewer settings dialog (find it by the strings of the other viewer options), after the last existing check box; it is saved on OK with the other options.
  - Language files: add the string to every language file that holds the existing viewer option strings, English text in all. A missing key is an error in the build or loader, so verify by running far2l.
  - Config read: add a row in the `ConfigOpt.cpp` table like the `Markdown` row, with default 1, so an old config without the key reads 1.
  - Test 0008: copy the layout of 0007. `sample.md` has an H1 heading and one paragraph; the pass test is that the screen holds the heading text without a leading `# ` and holds a heading rule line. The `.txt` file holds the line `# plain` and the screen must show `# plain` as is.
  - `check.sh`: configure if the build dir has no `CMakeCache.txt`, build target `far2l`, build `testing/src`, refresh `workdir` for 0007 and 0008, run both with `far2l-smoke`; any failure exits non-zero.
  - The "Done" text above is the target, not a report.
  - Commit trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>` (put in the dispatch; verify).
- Check: `sh stories/markdown-default-on/check.sh` (written in T1). Owner-checked paths: `stories/*/story.md`.

## T1: Option, open rule, dialog, test

**Depends on:** none
**Done check:** `sh stories/markdown-default-on/check.sh` exits 0
**Role:** implementer — review final
Implement the option, the open rule at the three places named above, the dialog check box and string, test 0008, and `check.sh` (configure and build far2l in the build dir, run test 0008 and the existing 0007).
