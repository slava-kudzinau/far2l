---
description: Quick view (Ctrl+Q) on a Markdown file takes the screen minus a narrow one column file list strip, and gives the layout back on close, on hide and when the setup is saved.
status: ready
size: M
refs:
  - docs/architecture/features/markdown/markdown.runtime.md
  - docs/architecture/features/markdown/markdown.data.md
  - docs/architecture/decisions/adr-007-markdown-default-on.md
---

# Story: md-quick-view-wide

## Goal

Markdown text reads well in wide lines: tables fit, and long prose wraps less. Quick view is
the F3 `Viewer` with full Markdown rendering, but it owns only half the screen. The owner
wants quick view to take most of the screen while it shows a Markdown file. The file list
shrinks to a narrow single-column strip. The original layout comes back when quick view shows
another file, closes or hides.

## Context

- Quick view: `QuickView` panel wraps `Viewer(true)`; `QuickView::ShowFile`
  (`far2l/src/panels/qview.cpp:390`) creates the viewer with the panel rect, so it renders
  Markdown today at panel width. Cursor moves funnel through `FileList::UpdateViewPanel`
  (`far2l/src/panels/filelist.cpp:3420`) and `TreeList` (`treelist.cpp:2077`) into `ShowFile`.
- Geometry: `FilePanels::SetPanelPositions` (`far2l/src/filepanels.cpp:258`): vertical
  layout, left = `0 .. ScrX/2 - Opt.WidthDecrement`, right = `ScrX/2+1-Opt.WidthDecrement .. ScrX`;
  a panel with `ViewSettings.FullScreen` spans `0 .. ScrX` and the sibling keeps a strip.
  Quick-view text width = panel width - 2; the quick-view scrollbar sits on the border and
  costs no column (`viewer.cpp:540-548`).
- Modes: `VIEW_0..VIEW_9` with a per-mode `FullScreen` flag
  (`far2l/src/panels/flmodes.cpp:56-127`, `panel.hpp:39-53`); reusable entry
  `FilePanels::ChangePanelViewMode` (`filepanels.cpp:846`). The built-in table had no
  one column mode, so the story added mode 10 (not in the modes menu).
- Markdown width re-layout on resize exists: `Invalidate(Width)` + `SyncMarkdownTop`
  (`viewer.cpp:1164-1196`).

## Scope

**In:** `far2l/src/panels/qview.cpp` and `qview.hpp` (apply, restore, hide, show). `far2l/src/panels/flmodes.cpp` (view mode 10). `far2l/src/filepanels.cpp` (restore before `ChangePanel`). `far2l/src/cfg/config.hpp` and `ConfigOpt.cpp` (two ini keys, restore around the save). The smoke test `000a-view-md-wide`. The spec rows in `docs/architecture/features/markdown/`.

**Out:** the F3 (full-screen viewer) layout, panel disposition changes, column-mode editor changes, plugin-panel specifics beyond "do not crash", mermaid behavior, a settings dialog entry.

## Behavior (owner's rulings, 2026-11-11)

1. The trigger is per file. The layout applies when quick view shows a `.md` or `.markdown` file. It restores in four cases. Quick view shows another file. Quick view closes. Quick view hides (Ctrl+P, Ctrl+O). The setup is saved. Tab focus switches keep it.
2. The quick view panel takes the screen minus a strip. It is not full screen and has no `FullScreen` flag. The file list keeps a strip of `Markdown_QuickViewStripWidth` columns (default 20) on its side. At 80 columns the strip is 21 columns when the list is on the right.
3. The strip shows one NAME column (view mode 10). Tree and info siblings get the split change only. Stacked panels (disposition 1) have no strip. Nothing changes.
4. The restore is temporary. The saved view mode and the saved `Opt.WidthDecrement` live on the `QuickView` object. The feature never writes `far2l.ini`. `ConfigOptSave` takes the layout back before it writes the setup. Thus the saved split and the saved list mode are the user's own.
5. The gate is the file extension only (`.md`, `.markdown`). `Markdown_Auto` does not matter.
6. Two ini keys: `Markdown_QuickViewWide` (default 1) and `Markdown_QuickViewStripWidth` (default 20, at least 11 and at most `ScrX - 10`). Neither has a settings dialog entry.
7. Ctrl+Left, Ctrl+Right and Ctrl+Clear still move the split while the layout is on. The next restore overwrites the change. To keep a strip width, set the ini key.

## Mechanism (builder calls)

- `QuickView::ShowFile` calls `ApplyMdWideLayout` or `RestoreMdWideLayout` at entry, before `new Viewer(true)`, so the viewer is born with the final rect.
- `ApplyMdWideLayout` sets the sibling to view mode 10. It sets `Opt.WidthDecrement` (`ScrX / 2 + 1 - strip` for a sibling on the left, `strip - ScrX / 2` on the right). Then it calls `SetScreenPosition`.
- View mode 10 is an 11th entry in `ViewSettingsRef`. `FileList::ShowFileList` copies the view settings from the table at each redraw. A change to `ViewSettings` alone does not last.
- `FilePanels::ChangePanel` calls `RestoreMdWideLayout` before it reads the quick view position. `~QuickView` does not restore, because the sibling panel may be gone at exit.
- `QuickView::Hide` restores. `QuickView::Show` applies again for a gate file.
- `ConfigOptSave` restores before the save and applies again after.
- The existing clamp `|WidthDecrement| <= ScrX / 2 - 10` bounds the split.

## Size

M: the quick view, one view mode, the config save, two ini keys and one smoke test.
The story does not change Markdown rendering.

## Unknowns

None open.

## Acceptance criteria

1. Ctrl+Q on a `.md` file: the quick view takes the screen minus the strip. The file list shows one name column in the strip. Markdown renders at the wider width.
2. Arrow to a file that is not `.md`: the split and the list view mode return.
3. Arrow back: the wide layout again. Tab keeps the layout.
4. Ctrl+Q close on a `.md` file: the original view mode and split return. The borders meet in the middle.
5. `Markdown_QuickViewWide=0`: quick view keeps the half screen split for `.md`.
6. Ctrl+P hides the quick view and the list gets the half screen. Ctrl+P shows it and the wide layout returns. Ctrl+O twice does the same.
7. Exit with a wide quick view does not crash. With `AutoSaveSetup=1` the saved split and the saved list view mode are not the strip values.
8. `Markdown_QuickViewStripWidth=30` gives a strip of 30 columns.
9. The check command passes, with `000a-view-md-wide` in `TESTS`.

## Notes

- Story branch: `feat/md-quick-view-wide`, base newest `next/`.
- Known edge: `.md`/`.markdown` extension gate matches the viewer's own rule set (ADR-007
  default on), so the common case (Markdown rendering active) and the gate coincide.
- started: 2026-11-11T16:09:35Z
- budget: 15, hard stop: 20 (size M)
- flow tier=full docs=spec,plan design=no orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=plan-verifier-critical
- hooks: preflight none, verify = the check command (AGENTS.md), status none, cleanup none
- BASE: 0664be115 (origin/next/2.9.1-ovr.2), worktree .worktrees/md-quick-view-wide
- Check: `BUILD=$PWD/.build` in the worktree; TESTS add `000a-view-md-wide` once created
- Gate page: G2-design gate=stop, owner reads before build
- completed: 2026-11-11; check ok on 3dddc185e; budget verdict ok (budget 15, hard stop 20)
