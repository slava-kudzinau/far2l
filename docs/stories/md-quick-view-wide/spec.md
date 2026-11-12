# Spec: md-quick-view-wide

This spec describes the shipped behavior. The durable contract is in `markdown-viewer.spec.md`, section "Quick view of a Markdown file".

## Unit: layout in `far2l/src/panels/qview.cpp`

| Call | Result |
|---|---|
| `QuickView::ShowFile(name, ...)` where `name` ends in `.md` or `.markdown` (any letter case) and is not a directory, `Markdown_QuickViewWide=1`, layout not applied | `ApplyMdWideLayout` runs before the `Viewer` is created. It sets its flag first. It saves the sibling `GetViewMode()` and `Opt.WidthDecrement`. With vertical panels it sets the sibling to view mode 10 (one NAME column) and sets `Opt.WidthDecrement` by the formula below. It calls `SetScreenPosition`. The quick view panel has no `FullScreen` flag. |
| `ShowFile` with the layout applied and a name that is not a gate file | `RestoreMdWideLayout` gives back the saved view mode and the saved `Opt.WidthDecrement`, then calls `SetScreenPosition`. A second restore does nothing. |
| `FilePanels::ChangePanel` replaces or removes the quick view | It calls `RestoreMdWideLayout` before it reads the quick view position. |
| `QuickView::Hide` (Ctrl+P, Ctrl+O) | It calls `RestoreMdWideLayout`, then `Panel::Hide`. |
| `QuickView::Show` on a gate file | It calls `ApplyMdWideLayout`, then `Panel::Show`. |
| `ConfigOptSave` (Shift+F9, `AutoSaveSetup` at exit) | It calls `RestoreMdWideLayout` before the save. It calls `ApplyMdWideLayout` after the save, if the restore did work. |
| `~QuickView` | It does not restore the layout. The sibling panel may be gone at exit. |
| Tab focus switch | The layout does not change. |
| Sibling is `TreeList` or `InfoList` | View mode 10 is safe (`Panel::SetViewMode` only stores it). The split applies as for a file list. |
| Panel disposition 1 (stacked panels) | No change. There is no strip. |
| Ctrl+Left, Ctrl+Right, Ctrl+Clear while the layout is applied | The keys change `Opt.WidthDecrement` as before. The next restore overwrites the change. |

## Unit: split formula (vertical panels)

`strip = max(10, min(Markdown_QuickViewStripWidth, ScrX - 10))`.

| Sibling side | `Opt.WidthDecrement` | Strip |
|---|---|---|
| left | `ScrX / 2 + 1 - strip` | `strip` columns |
| right | `strip - ScrX / 2` | `strip + 1` columns at 80 columns (`ScrX` is 79) |

The existing clamp `|WidthDecrement| <= ScrX / 2 - 10` in `SetPanelPositions` applies after the formula.

## Unit: ini keys

| Key | Section | Field | Default |
|---|---|---|---|
| `Markdown_QuickViewWide` | `NSecViewer` | `Opt.ViOpt.ViewerMdQuickViewWide` | 1 |
| `Markdown_QuickViewStripWidth` | `NSecViewer` | `Opt.ViOpt.ViewerMdQuickViewStripWidth` | 20 |

Neither key has an entry in a settings dialog. The user edits them in `config.ini`.

## Unit: view mode 10

`ViewSettingsRef` in `far2l/src/panels/flmodes.cpp` has an 11th entry. It has one NAME column. The panel modes menu lists modes 0 to 9 only, so mode 10 is not in the menu. `SavePanelModes` writes it as `[Panel/ViewModes/Mode10]`. `FileList::ShowFileList` copies the view settings from `ViewSettingsArray[ViewMode]` at each redraw, so the layout needs a table entry. A change to `ViewSettings` alone does not last.

## Unit: gate

The extension check mirrors `IsMarkdownFileName`: a trailing `.md` or `.markdown`, in any letter case. It does not test `Markdown_Auto`. A file with a saved Shift+F4 choice for plain text still gets the wide layout.

## Validation

| Call | Expected |
|---|---|
| Ctrl+Q on `wide.md` | Wide layout. The borders meet at column 19 of row 0. The viewer text goes past column 70. |
| The strip holds a 17 character name | The strip shows the whole name in one column. |
| Up to `plain.txt`, then Down to `wide.md` | The half screen split, then the wide layout again. |
| Ctrl+Q closes a wide quick view | The borders meet in the middle of row 0. |
| Exit with a wide quick view | No crash. |
| Ctrl+P on a wide quick view, then Ctrl+P | The list gets the half screen. Then the wide layout returns. |
| Ctrl+O twice with a wide quick view | The wide layout returns. |
| Exit with `AutoSaveSetup=1` and a wide quick view, then start | The split is in the middle. The list keeps its two column mode. |
| `Markdown_QuickViewStripWidth=30` | The borders meet at column 29. |
| `Markdown_QuickViewStripWidth` below 11 or above `ScrX - 10` | The code and the split clamp keep it between 11 and `ScrX - 10`. |

## Done

The check from `AGENTS.md` exits 0, with `000a-view-md-wide` in `TESTS`. Set `ASCII_BUILD` to the build folder for `ascii-driver.sh`.

## Out of scope

The F3 viewer layout, panel disposition changes, the panel modes editor, plugin panel columns, mermaid behavior, a settings dialog entry for the two keys.
