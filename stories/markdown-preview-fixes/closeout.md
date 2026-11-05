# Close-out draft: markdown-preview-fixes

Commits (branch markdown-preview/fixes, BASE f67f2ac7a; 750bd7093 and a1a77c0f7 are owner spec commits):
- T1 mermaid renders SVG in the engine: 024be7098, fix e3793081d
- T2 SvgCanvas rasterizes the real SVG: aa431ae9a, fix 97937e219
- T3 viewer shows the box art, test 0007: 1e1947f93
- T4 viewer-theme colors: a75088c9d, fix 491cb484d
- Final-review fix (engine time and stack bounds, logging, cache key): c7911ff2c

Check: `cd /Users/Viachaslau_Kudzinau/Projects/far2l && cmake --build ../far2l.build --target far2l -j8 && cd testing && ./far2l-smoke -t 30 ../../far2l.build/install/far2l.app/Contents/MacOS/far2l tests/0007-view-file-markdown` exit 0 (at c7911ff2c; 1 test, passes). Driver cases `sh testing/markdown/run.sh flowchart|swimlane|invalid|cells|hostile|timeout` all exit 0.
Run the check through `testing/far2l-smoke-run.sh`, or first refresh the gitignored workdir (`rm -rf workdir; cp -r initdir workdir` in the test folder). The bare command uses a stale workdir copy of sample.md after any sample.md edit. In the Markdown viewer, press End to reach the mermaid art (see Owner items).

## Owner items (decide before merge)

1. **Scrolling is broken in Markdown mode.** Down and PgDn do not work: SHOW_UP and SHOW_DOWN never rebuild the markdown screen, and `Builder::pos` counts md4c text bytes, not file offsets, so PgDn blanks the screen. End and Home work. Test 0007 scrolls with End. The fix needs viewer.cpp changes (ShowPage, PgUp/PgDn, LastPage) and real byte offsets in MarkdownRender.cpp. This was outside the plan. It is the first thing you will hit when you check by hand. Suggest a follow-up story.
2. **`far2l/bootstrap/mermaid.min.js` is gitignored** (`.gitignore` line 14, pattern `far2l/bootstrap`). A clean checkout builds an app with no mermaid asset, and mermaid blocks fall back to raw fences. You must decide how to ship the asset.
3. **No text selection in Markdown mode.** Markdown rows never set `bSelection` (viewer.cpp, ReadStringMarkdown). I changed the printer's selection color to COL_VIEWERSELECTEDTEXT (viewer.cpp:661), but no selection can show until the rows set it.
4. **Manual check pending:** macOS Terminal and ghostty (spec Integration row).

## Rulings

1. Owner ruling (750bd7093): every Markdown run uses COL_VIEWERTEXT; structure by layout only. The cause of the black and red backgrounds: `SetFarColor(uint16)` treats the value as a palette index (FarColorToReal), and viewer.cpp passed a 64-bit real color truncated to 16 bits. Attributes are now uint64 and `SetColor` is used.
2. The sample.md swimlane block uses swimlane-beta; the bundled mermaid has no swimlane-alpha grammar. spec.md now says so (a1a77c0f7).
3. Mermaid shim contingencies applied: securityLevel loose, htmlLabels false, microtask setTimeout (delays ignored; clearTimeout works). Not applied: flowchart.useMaxWidth false.
4. Edit in viewer.cpp:661 (selection color) allowed because the cause is proved there. A heading rule line now covers H1 to H6 (H1 double, others single); code is indented 4 cells.
5. testing/src/main.go: T3 restored the Environment block (FAR2L_TESTCTL, FAR2L_STD) that the baseline commit deleted; without it every smoke run timed out. Net diff against master is empty.
6. Files under testing/ match the root `.gitignore` pattern "Testing" on this case-insensitive file system; they were committed with `git add -f`.
7. Engine bounds: 5 s wall-clock deadline per render, JS stack 1 MB, engine rebuilt (about 0.7 s) after a timeout. SvgCanvas bounds: nesting 64, 16 compounds per selector, 2M CSS match steps per parse.
8. The final diff was taken from BASE f67f2ac7a, not from a merge-base with main (handoff.sh final needs a `main` ref).
9. Trailers: three implementers used a Haiku trailer; the orchestrator amended to the exact Sonnet 5.5 trailer. No push was made.

## Parked findings

- Labels with `<` fail to render (mermaid sanitizer needs an HTML parser in the shim); the fence shows raw.
- A timing-out fence freezes the UI for about 5.8 s on every re-render (width change, Shift-F4 toggle); the cache clears each pass and there is no per-document budget. Hash collision of the cache key (hash of source plus width) is unchecked.
- Shim stubs still silently wrong: innerHTML (only style/script text kept), DOMParser, insertBefore with a foreign reference, getComputedStyle, setInterval.
- SvgCanvas: wide characters count as one cell; foreignObject skipped; arcs keep the end point only; arrow glyphs outside the BoxSymbols table; the subgraph title overlaps the top border; a fence inside a list or quote loses its prefix; the 2M CSS step budget is cumulative per parse.
- Text color is fixed at render time, blank cells read it at print time; after a theme change they differ until the mode is toggled.
- Dead or stale items: stale F_*/B_* comment and includes in MarkdownRender.hpp; unused link_href/link_label; first getBBox definition in MermaidEngine.cpp.
- Test gaps: mermaid art cell colors not asserted; no test for the engine-fails fallback through the viewer, two fences, or a fence in a list or quote.

## Lessons

- `handoff.sh final` needs a `main` ref; this repo uses `master` and a WIP baseline commit.
- Implementers on haiku used the session attribution (Haiku trailer) despite the brief; put the trailer in the dispatch header on every dispatch and verify with `git log`.
- Haiku task review found real Important issues in 3 of 4 tasks (stale callbacks, unbounded CSS matching, selection color); keep the reviewer tier.
- A baseline WIP commit hid two regressions (smoke harness env, test 0007 mistakes); check the harness before a build.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| task-reviewer | haiku | 3 | 0 | 4 | 21 |
| re-reviewer | haiku | 3 | 0 | 0 | 0 |
| final-reviewer-light | sonnet | 1 | 0 | 1 | 7 |
| task-reviewer-critical | opus | 1 | 0 | 0 | 0 |
