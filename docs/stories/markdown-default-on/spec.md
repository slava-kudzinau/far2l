# Spec: F3 on a Markdown file opens the Markdown view by default

This is the plan's `## Spec` table, kept as the story's spec for the standard tier. `plan.md` Rulings amend it (the cache scheme in the final-review rulings governs).


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

## Done

The check exits 0. It builds far2l in `../far2l.default-on.build` and runs smoke test `0008-view-md-default` (new). The test opens `sample.md` with F3 and no Shift-F4 and sees rendered text (no `# ` heading marker, a ruled heading), then opens a `.txt` file and sees plain text.
