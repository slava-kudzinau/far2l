# Close-out draft: md-quick-view-wide

Commits: 7424e0e39 (T1 ini key plumbing), 87e81c415 (T2 apply/restore in QuickView), 3dddc185e (T3 smoke test 000a-view-md-wide, amended from a8b8bd011 to trim test.js under 40 lines). Check: `BUILD=$PWD/.build; APP=$BUILD/install/far2l.app/Contents/MacOS/far2l; cmake --build "$BUILD" --target far2l -j8 && sh testing/markdown/ascii-golden.sh && sh testing/markdown/ascii-driver.sh && (cd testing && for t in 0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off 000a-view-md-wide; do ...; done) && echo "check ok"` exit 0 (4 smoke tests [DONE], golden + driver ok; the check's driver leg needs `ASCII_BUILD=$PWD/.build` on this machine).

## Rulings

1. T2 reads the setting as `Opt.ViOpt.ViewerMdQuickViewWide` — `Opt.ViOpt` is a value member (`config.hpp`); the plan's "dereference the ViOpt pointer" has no pointer in `QuickView`.
2. The side test compares the sibling with `CtrlObject->Cp()->LeftPanel` (the plan's `Opt.LeftPanel` shorthand resolved to the GetPanelPositions idiom).
3. The gate accepts a name that is exactly `.md`/`.markdown` (spec Validation row `ShowFile(".md")`); the mirrored `IsMarkdownFileName` skip test relaxes from `name_len <= suffix_len` to `name_len < suffix_len`.
4. The sibling handle is `CtrlObject->Cp()->GetAnotherPanel(this)` (the plan block omits `Cp()`).
5. T1's ConfigOpt row copies the `Markdown_Auto` row shape (`OST_COMMON`, `NSecViewer`, group `L"ViewerSettings"`), placed right after it; help text exactly from spec.
6. The story check uses `ASCII_BUILD=$PWD/.build` for `ascii-driver.sh` (libqjs.a lives in the story build) and full all-target builds; the smoke harness in `testing/src` needed `make -B` once (the committed binary was built in an older worktree).
7. T3's narrow-phase proof is `Plain`@(41,1) (split restored; quick view shows plain.txt) then `TypeDown` re-applies wide (`Wide`@(1,1), `TAILWORD`@(64,2)): the quick-view box top is y=0 (content row 1) in full-screen, and the restored panel is the right panel, content x 41. The plan's literal narrow pair (Wide/TAILWORD in a half-screen box) has no matching screen state; the plan's own "fix any off-by-one in test.js only" is the authority. The semantic pair (wide TAILWORD x 64 > 40; restored content in the right half) is preserved.
8. Sibling strip paint: the full-screen quick-view box draws over the sibling's 20-column strip during the ShowFile draw cycle; the strip returns on the next FilePanels redraw. Spec row 7 governs the applied state; T3's TAILWORD@x 64 is the machine proof of the span past the half-screen border.
9. `spend.mjs` matched 0 transcripts late in the run (omp writes session transcripts to `~/.omp/logs`; spend scans `~/.claude/projects` — the store rotated). The budget verdict is cached (`ok`).

## Parked findings

- T2 Minor (kept, plan-mandated): member names `m_md_wide_applied`, `m_md_saved_view_mode`, `m_md_saved_width_decrement` follow the plan; CODESTYLE's `_field` convention applies to new classes, and qview.cpp already mixes `LastWrapMode`-style statics.
- T2 Minor (kept): the gate's ASCII case fold mirrors `IsMarkdownFileName` (viewer.cpp); non-ASCII extensions behave as before.

## Lessons

- macOS `find tests -maxdepth 1 -type s -delete` is a no-op here; the AGENTS.md socket cleanup should use the `[ -S "$s" ] && rm -f "$s"` loop.
- The plan's check command should carry `ASCII_BUILD=$PWD/.build` for the driver leg on machines without the shared qjs build.
- `handoff.sh final` takes the output file as its first argument (the interface changed mid-run from the ws-dir form).

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| task-reviewer | haiku | 1 | 0 | 0 | 0 |
| final-reviewer | opus | 1 | 0 | 0 | 0 |

## Follow-up fixes after the first close-out

These fixes came from owner tests. They replace the rulings above where they differ (the full screen flag, view mode 2, the destructor restore, the 20 column constant). `spec.md` holds the shipped contract.

| Fix | Cause | Change |
|---|---|---|
| The restore ran after `ChangePanel` copied the quick view position | The copy used the wide position | `FilePanels::ChangePanel` calls `RestoreMdWideLayout` first |
| The file list strip was hidden | The quick view had the full screen flag | The quick view takes the screen minus the strip, with no `FullScreen` flag |
| Crash at exit with a wide quick view | `~QuickView` used a sibling panel that was gone | The destructor does not restore the layout |
| A hidden quick view left an empty area beside a narrow list | `FileList::UpdateViewPanel` does not call `ShowFile` while the quick view is hidden | `QuickView::Hide` restores. `QuickView::Show` applies again |
| The strip showed two NAME columns | The built-in table has no one column mode, and the list copies its settings from the table at each redraw | View mode 10, an 11th table entry with one NAME column |
| The strip layout reached the saved setup | `ConfigOptSave` read `Opt.WidthDecrement` and the list view mode while the layout was on | `ConfigOptSave` restores before the save and applies again after |
| The strip width had no setting | The width was a constant | Ini key `Markdown_QuickViewStripWidth` (default 20) |

Open items:

- Ctrl+Left, Ctrl+Right and Ctrl+Clear work while the layout is on, with either panel focused. The next restore overwrites the change. The ini key is the way to keep a width.
- Stacked panels get the one column view mode in a full width list. A strip is not there. The owner has not asked for a change.
- Ctrl+U (swap panels) while the layout is on flips `Opt.WidthDecrement`. By the code, the restore then writes back the saved value with the old sign. No test checked this.
- `SavePanelModes` writes `[Panel/ViewModes/Mode10]` in `panel.ini` on the first save.
- On macOS, Mission Control can take Ctrl+Left and Ctrl+Right ("Move left a space" and "Move right a space" in System Settings, Keyboard, Keyboard Shortcuts, Mission Control). The tests send the keys straight to far2l, so they cannot show this. It is a likely cause when the keys do nothing in a terminal. It is not proven.
