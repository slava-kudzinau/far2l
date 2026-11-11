# Spec: md-quick-view-wide

## Unit: `far2l/src/panels/qview.cpp` (layout apply and restore)

| Call | Result |
|---|---|
| `QuickView::ShowFile(name, ...)` where `name` ends in `.md` or `.markdown` (case-insensitive), ini `Markdown_QuickViewWide=1`, layout not applied | Apply runs before the `Viewer` is created (before `new Viewer(true)`, `qview.cpp:451`): save sibling `GetViewMode()`; `sibling->SetViewMode(2)` (VIEW_2, the two-name-column mode); `ViewSettings.FullScreen = 1` on the quick-view panel; set `Opt.WidthDecrement` per the formula below; `CtrlObject->Cp()->SetScreenPosition()`. New viewer rect = new panel rect. Applied flag set. |
| `QuickView::ShowFile(name, ...)` with layout applied and name not matching | Restore: sibling `SetViewMode(saved mode)`; `ViewSettings.FullScreen = 0`; `Opt.WidthDecrement = saved decrement`; `SetScreenPosition()`. Flag cleared. Viewer then created at the restored half-screen rect. |
| Quick view closes (Ctrl-Q path, `~QuickView`) with layout applied | Same restore as above, then `SetScreenPosition()`. far2l.ini is not written by the feature. |
| Tab focus switch while applied | Layout unchanged; state stays on the `QuickView` object. |
| Sibling is `TreeList` or `InfoList` | `SetViewMode(2)` is safe (base `Panel::SetViewMode` stores only); `WidthDecrement` and `FullScreen` apply as for a file panel. |
| `Opt.PanelsDisposition == 1` (horizontal panels) with layout applied | `ViewSettings.FullScreen = 1` on the quick-view panel and sibling `SetViewMode(2)` apply and restore; `Opt.WidthDecrement` untouched (it adjusts height in horizontal panels). |

## Unit: split formula (vertical panels, strip width `W = 20`)

| Sibling panel side | `Opt.WidthDecrement` | Resulting strip (from `SetPanelPositions`, `filepanels.cpp:277-289`) |
|---|---|---|
| left (`QuickView` is right, `FullScreen=1`) | `ScrX / 2 + 1 - W` | left spans `0 .. ScrX/2 - WidthDecrement` = width `W` |
| right (`QuickView` is left, `FullScreen=1`) | `W - ScrX / 2` | right spans `ScrX/2 + 1 - WidthDecrement .. ScrX` = width `W` |

The existing clamp `|WidthDecrement| <= ScrX/2 - 10` in `SetPanelPositions` bounds narrow screens.

## Unit: ini key

| Key | Section | Field | Default |
|---|---|---|---|
| `Markdown_QuickViewWide` | `NSecViewer` (same section as `Markdown_Auto`, `ConfigOpt.cpp:289-292`) | `Opt.ViOpt.ViewerMdQuickViewWide` (`config.hpp`, next to `ViewerMarkdownAuto`) | 1 |

Help text: `Widens Quick view to the full screen for .md and .markdown files; the other panel keeps a narrow single-column strip`.

## Unit: gate

Extension check mirrors `IsMarkdownFileName` (`viewer.cpp:86-106`): trailing `.md` or `.markdown`, case-insensitive, without the `Markdown_Auto` condition. A file whose saved Shift+F4 choice shows it plain still applies the wide layout.

## Validation

| Call | Expected |
|---|---|
| `ShowFile("notes.MD")` with feature on | Wide layout applied |
| `ShowFile("notes.markdown")` | Wide layout applied |
| `ShowFile("readme.txt")` right after an apply | Layout restored once; flag cleared; a second restore call is a no-op |
| Apply twice without restore (same file re-shown) | State saved once; second apply is a no-op (flag checked first) |
| `ShowFile(".md")` (a name that is only the extension) | Wide layout applied (the extension check matches) |
| `ShowFile("")` | Treated as non-Markdown: restore runs (no-op when nothing applied); no crash |
| `ScrX = 30` | `WidthDecrement` clamped to 5 (`ScrX/2 - 10`); strip 11 columns; no crash |

## Done

`BUILD=$PWD/.build; APP=$BUILD/install/far2l.app/Contents/MacOS/far2l; cmake --build "$BUILD" --target far2l -j8 && sh testing/markdown/ascii-golden.sh && sh testing/markdown/ascii-driver.sh && (cd testing && for t in 0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off 000a-view-md-wide; do o=$(./far2l-smoke-run.sh "$APP" "$t" 2>&1) && echo "$o" | grep -q '\[DONE\]' && ! echo "$o" | grep -q FAILED || { echo "FAILED: $t"; exit 1; }; done) && echo "check ok"` exits 0 (run from the worktree root; configure `.build` once with `cmake -S . -B .build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTESTING=Yes -DUSEWX=No -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0`). Every row above is covered by the smoke test `testing/tests/000a-view-md-wide/test.js` (geometry rows) and by inspection of the pure branches (disposition, tree sibling, clamp).

## Out of scope

F3 (full-screen viewer) layout, panel disposition changes, view-mode editor edits, plugin panel columns, mermaid behavior, the `.md` rule list in settings.
