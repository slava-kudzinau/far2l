# Plan: md-quick-view-wide

Status: history. The first build followed this plan. Owner tests then changed the design. There is no `FullScreen` flag. The strip has one NAME column (view mode 10). The layout restores before `ChangePanel` and on hide, not in the destructor. The key `Markdown_QuickViewStripWidth` replaces the constant `QVIEW_MD_STRIP`. `spec.md` and `closeout.md` hold the shipped behavior.

Spec: `spec.md` (governs exact values).

## Global Constraints

- C++ per `CODESTYLE.md`; keep fork edits in upstream files small. Files this story may touch: `far2l/src/panels/qview.cpp`, `far2l/src/panels/qview.hpp`, `far2l/src/cfg/config.hpp`, `far2l/src/cfg/ConfigOpt.cpp`, `testing/tests/000a-view-md-wide/` (new).
- Exact values: view mode index `2` (VIEW_2, two NAME columns); strip width `W = 20` (a named constant `QVIEW_MD_STRIP = 20` in `qview.cpp`); ini field `Opt.ViOpt.ViewerMdQuickViewWide`, section `NSecViewer`, key `Markdown_QuickViewWide`, default `1`, help text exactly as in `spec.md`.
- Formula (vertical panels): sibling is left panel ⇒ `Opt.WidthDecrement = ScrX / 2 + 1 - W`; sibling is right panel ⇒ `Opt.WidthDecrement = W - ScrX / 2`. Decide side with `CtrlObject->Cp()->GetAnotherPanel(this) == CtrlObject->Cp()->LeftPanel`.
- Extension gate: trailing `.md` / `.markdown`, case-insensitive, no `Markdown_Auto` condition (mirror `IsMarkdownFileName` in `viewer.cpp:86-106`, drop the `ViewerMarkdownAuto` test).
- Apply must run inside `QuickView::ShowFile` before `new Viewer(true)` (`qview.cpp:451`) so the viewer is born with the new rect; do not resize a live viewer.
- State lives in `QuickView` members: applied flag, saved sibling view mode, saved `Opt.WidthDecrement`. Apply with the flag set is a no-op; restore with the flag clear is a no-op.
- Horizontal disposition (`Opt.PanelsDisposition == 1`): apply and restore the quick-view panel's `ViewSettings.FullScreen = 1` and the sibling view mode only; `Opt.WidthDecrement` untouched (it adjusts height there).
- Markdown rows and mermaid treat input as untrusted: this story changes no render code (`markdown.invariants.md`).
- Commit message: Conventional Commits, one commit per task: T1 `feat(config): add Markdown_QuickViewWide setting`, T2 `feat(panels): widen quick view to full screen for Markdown files`, T3 `test(markdown): smoke test for wide Markdown quick view`.
- Check: `BUILD=$PWD/.build; APP=$BUILD/install/far2l.app/Contents/MacOS/far2l; cmake --build "$BUILD" --target far2l -j8 && sh testing/markdown/ascii-golden.sh && sh testing/markdown/ascii-driver.sh && (cd testing && for t in 0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off 000a-view-md-wide; do o=$(./far2l-smoke-run.sh "$APP" "$t" 2>&1) && echo "$o" | grep -q '\[DONE\]' && ! echo "$o" | grep -q FAILED || { echo "FAILED: $t"; exit 1; }; done) && echo "check ok"` (run from the worktree root; configure `.build` once; clean stale sockets per `AGENTS.md`).
- Owner-checked paths: none of the story files.

## T1: ini key plumbing

**Depends on:** none
**Done check:** `cmake -S . -B .build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTESTING=Yes -DUSEWX=No -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 > /dev/null && cmake --build .build --target far2l -j8` exits 0
**Role:** implementer — review final

Add `int ViewerMdQuickViewWide;` to the viewer options struct in `far2l/src/cfg/config.hpp` (next to `ViewerMarkdownAuto`, line ~260). Register it in `far2l/src/cfg/ConfigOpt.cpp` right after the `Markdown_Auto` row (`:291-292`): same `OST_COMMON`, `NSecViewer`, key `Markdown_QuickViewWide`, default 1, help text from `spec.md`. No other behavior.

## T2: apply and restore in QuickView

**Depends on:** T1
**Done check:** `cmake --build .build --target far2l -j8` exits 0 and `(cd testing && ./far2l-smoke-run.sh "$(cd .. && pwd)/.build/install/far2l.app/Contents/MacOS/far2l" 0008-view-md-default 2>&1 | grep -c '\[DONE\]')` prints 1
**Role:** implementer — review task-reviewer

In `far2l/src/panels/qview.hpp`: private members `int m_md_wide_applied{0}; int m_md_saved_view_mode{-1}; int m_md_saved_width_decrement{0};` and private methods `void ApplyMdWideLayout(); void RestoreMdWideLayout(); int MdWideGate(const wchar_t *name) const;` (names exact). In `far2l/src/panels/qview.cpp`: call `MdWideGate` at the top of `ShowFile` before `new Viewer(true)` (`qview.cpp:451`): gate pass -> `ApplyMdWideLayout()`, else -> `RestoreMdWideLayout()`. The destructor calls `RestoreMdWideLayout()` when applied. `MdWideGate` checks `Opt.ViOpt->ViewerMdQuickViewWide` (dereference the ViOpt pointer as the surrounding code does) and the extension rule from `spec.md` (trailing `.md`/`.markdown`, case-insensitive, no `Markdown_Auto` condition; empty name = gate fail).

Apply, vertical disposition, exact sequence (`sib = CtrlObject->GetAnotherPanel(this)`; the split knob is the QuickView panel's own `Opt.WidthDecrement` and its `ViewSettings.FullScreen`, both read by the QuickView branch of `FilePanels::SetScreenPosition` — verify against the side-test rows of `spec.md` and adjust only the sign of the formula, not the mechanism):

```
m_md_saved_view_mode = sib->GetViewMode();
m_md_saved_width_decrement = Opt.WidthDecrement;
sib->SetViewMode(2);                       // VIEW_2: two NAME columns
ViewSettings.FullScreen = 1;               // quick view spans the full width
Opt.WidthDecrement = (CtrlObject->GetFirstPanel() == this) ? (20 - ScrX/2) : (ScrX/2 + 1 - 20);
CtrlObject->Cp()->SetScreenPosition();
m_md_wide_applied = 1;
```

Horizontal disposition: same without the `WidthDecrement` line. Restore: write back `ViewSettings.FullScreen = 0`, `Opt.WidthDecrement` and `sib->SetViewMode(m_md_saved_view_mode)`, then `CtrlObject->Cp()->SetScreenPosition()`, clear the flag. Apply runs only when the flag is clear; restore only when set.

## T3: smoke test 000a-view-md-wide

**Depends on:** T2
**Done check:** the full check command from Global Constraints exits 0
**Role:** implementer — review final

Create `testing/tests/000a-view-md-wide/` with `test.js` and `initdir/left/` fixtures, modeled on `0008-view-md-default` (fresh profile, `StartAppWithSize([...], 80, 24)`, `Type()` on `far2l` title, two dismiss `TypeEscape`). Fixtures, exact content:

- `initdir/left/wide.md` — two lines: `# Wide`, then `alpha bravo charlie delta echo foxtrot golf hotel india juliet TAILWORD` (71 characters; `TAILWORD` occupies cells 64-71).
- `initdir/left/plain.txt` — one line: `Plain`.

Script (alphabetical list order: `plain.txt` is selected at start): `TypeDown` to `wide.md`; Ctrl-Q (quick view opens on the right panel; active panel stays left); assert `ExpectString("Wide", 1, 2, 16, 1)` and `ExpectString("TAILWORD", 64, 3, 16, 1)` — column 64 > 40 proves the viewer spans past the half-screen border into the sibling's area; `TypeUp` to `plain.txt` (quick view shows `plain.txt`; layout restores); assert `ExpectString("Wide", 1, 2, 16, 1)` and `ExpectString("TAILWORD", 26, 4, 16, 1)` — x 26 <= 40 on row 4 proves the paragraph wrapped inside the half-screen strip; Ctrl-Q closes; quit `F10` + Enter + `ExpectAppExit(0)`. Keep the test under 40 lines; comment each phase like `0008` does.

The coordinates above are computed (heading row y=2, paragraph y=3 wide; narrow wrap width 38 puts TAILWORD at x=26 row 4). Implementer runs the app once on the fixture before writing the final numbers and fixes any off-by-one in `test.js` only.
