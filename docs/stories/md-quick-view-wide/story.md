---
description: Quick view (Ctrl-Q) on a Markdown file widens itself by collapsing the sibling panel to a narrow single-column strip, and restores the layout on exit.
status: in-progress
size: M
refs:
  - docs/architecture/features/markdown/markdown.runtime.md
  - docs/architecture/features/markdown/markdown.data.md
  - docs/architecture/decisions/adr-007-markdown-default-on.md
---

# Story: md-quick-view-wide

## Goal

Markdown text reads well in wide lines: tables fit, long prose wraps less. Quick view is
the F3 `Viewer` with full Markdown rendering, but it owns only half the screen. The owner
wants quick view to take most of the screen while it shows a Markdown file, with the file
list shrinking to a narrow single-column strip, and the original layout back when quick
view shows a non-Markdown file or closes.

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
  `FilePanels::ChangePanelViewMode` (`filepanels.cpp:846`). The built-in table has no true
  1-column mode; pick the fewest-columns mode.
- Markdown width re-layout on resize exists: `Invalidate(Width)` + `SyncMarkdownTop`
  (`viewer.cpp:1164-1196`).

## Scope

**In:** `far2l/src/panels/qview.cpp/.hpp` (apply/restore + state), small wiring in
`far2l/src/filepanels.cpp`, one ini key `Markdown_QuickViewWide` (default 1) in
`far2l/src/cfg/config.hpp` and `ConfigOpt.cpp`, one smoke test, spec rows in
`docs/architecture/features/markdown/` at close-out.

**Out:** the F3 (full-screen viewer) layout, panel disposition changes, column-mode editor
changes, plugin-panel specifics beyond "do not crash", mermaid behavior.

## Behavior (owner's rulings, 2026-11-11)

1. Trigger is per file: apply when quick view shows a `.md`/`.markdown` file, restore when
   it shows any other file, restore when quick view closes. State survives Tab focus switches.
2. Layout: the quick-view panel takes the full screen span; the sibling panel keeps a
   strip of about 20 columns at the far side.
3. The sibling file list switches to the fewest-columns view mode while applied; tree and
   info siblings get the split change only. Horizontal panel disposition applies the mode
   only, no split change.
4. Restore is temporary: saved `ViewMode` + saved `Opt.WidthDecrement` + quick-view
   `FullScreen` flag; far2l.ini is not rewritten by the feature itself.
5. Gate is the file extension only (`.md`, `.markdown`), like `IsMarkdownFileName`
   (`viewer.cpp:86`) without the `Markdown_Auto` condition. Markdown rendering mode on or
   off does not matter.
6. New ini key `Markdown_QuickViewWide`, default on.

## Mechanism (builder calls)

- Hook `QuickView::ShowFile` at entry, before `new Viewer(true)` at `qview.cpp:451`, so the
  viewer is created with the final rect. Apply sets: sibling `SetViewMode(fewest-column
  mode)` (save the old one), this panel's `ViewSettings.FullScreen = 1`, `Opt.WidthDecrement`
  = `ScrX/2 - list_width + 1` (sibling on left) or `list_width - ScrX/2` (sibling on right),
  then `CtrlObject->Cp()->SetScreenPosition()`.
- State fields on `QuickView` (applied flag, saved mode, saved decrement). Restore in
  `ShowFile` for non-MD files and in `~QuickView` (Ctrl-Q close path in
  `FilePanels::ProcessKey` `filepanels.cpp:559-604`), followed by `SetScreenPosition()`.
- The `QuickView` object lives across Tab switches (panel object kept by `ChangePanel`),
  dies on Ctrl-Q close, so the saved state is exactly the quick-view lifetime.
- `list_width` default 20; clamp through the existing `±(ScrX/2 - 10)` clamp.

## Size

M: two panels files + one ini key + one smoke test; no contract change to Markdown
rendering; parallel to the `MarkdownRender.cpp` series (touches `qview.cpp`,
`filepanels.cpp` only).

## Unknowns

None structural. Builder verifies: which built-in `VIEW_*` index has fewest columns in the
default table (`flmodes.cpp:56-127`), and that `SetScreenPosition` + `Show` repaint both
panels cleanly mid-file-list (the `Ctrl-PgUp` pattern at `filelist.cpp:2005-2018` is the
reference).

## Acceptance criteria

1. Ctrl-Q on a `.md` file: quick view spans the full screen width; the file list shows a
   single narrow strip (~20 columns) at the far side; Markdown renders at the wider width.
2. Arrow to a non-`.md` file: split and column mode return to the pre-quick-view values.
3. Arrow back: wide layout again. Tab to the file list: layout keeps; quick view stays wide
   while focused files are `.md`.
4. Ctrl-Q close on an MD file: original mode, split, and ini values are unchanged.
5. `Markdown_QuickViewWide=0` in ini: quick view keeps the plain half-screen split for MD.
6. Quitting far2l while the wide layout is applied persists the applied split and mode in
   ini (documented edge).
7. The check command passes, with the new smoke test added to `TESTS`.

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
