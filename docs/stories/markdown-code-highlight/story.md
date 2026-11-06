---
description: Add syntax colors to fenced code blocks in the F3 Markdown view, with tokens from the loaded Colorer plugin through one small export.
status: in-progress
size: M
risk: security,contract
depends-on: markdown-palette
---

# Story: Syntax colors in fenced code blocks

## Goal

A reader of a Markdown file in F3 can read a code block as easily as in the F4 editor. A fence with a known language shows keywords, strings, comments and numbers in their own colors. The tokens come from Colorer, so the F3 view and the F4 editor agree on the language rules.

## Context

- [ADR-004](../../architecture/decisions/adr-004-palette-roles.md), as changed by story markdown-palette
- [markdown.invariants.md](../../architecture/features/markdown/markdown.invariants.md): Markdown is untrusted input
- Fence code: `code_lang` and the `MD_BLOCK_CODE` case in `far2l/src/markdown/MarkdownRender.cpp`. `MdRun` in `MarkdownRender.hpp`.
- Draw code: `MarkdownRunColor` and `MarkdownPrinter::Print` in `far2l/src/ViewerPrinter.cpp`
- Render call: `m_md_doc.Render` in `far2l/src/viewer.cpp`. It runs on the main thread.
- Plugin lookup: `PluginManager::FindPlugin` in `far2l/src/plug/plugins.cpp`, `Plugin::Load` and `GetModuleName` in `plclass.hpp`. The Colorer `SysID` is `0xD2F36B62`.
- Colorer: `plugins/colorer/src/pcolorer2/pcolorer.cpp`, `FarEditorSet` (holds the `ParserFactory`), and `TextParser`, `LineSource` and `RegionHandler` in `plugins/colorer/src/Colorer-library/src/colorer/`

## Scope

**In:**

1. One new export in the Colorer plugin, in a new file in `plugins/colorer/src/pcolorer2/`. The export uses the `ParserFactory` that the plugin already has. The first task sets the final signature. The shape:

   ```cpp
   int ColorerTokenizeV1(const wchar_t *lang, const wchar_t *const *lines, int count,
       void (*emit)(void *ctx, int line, int start, int end, int token_class),
       void *ctx, int deadline_ms);
   ```

2. The export gives token classes, not colors. It climbs `Region::getParent()` to the nearest `def:*` region and maps that region to a class:

| Class | `def:*` regions | Role | 256 index | RGB | 16-color fallback |
|---|---|---|---|---|---|
| keyword | `def:Keyword` | token-keyword | 111 | `#87AFFF` | `F_LIGHTGRAY` |
| string | `def:String`, `def:Character` | token-string | 120 | `#87FF87` | `F_LIGHTGREEN` |
| comment | `def:Comment` | token-comment | 103 | `#8787AF` | `F_CYAN` |
| number | `def:Number` | token-number | 215 | `#FFAF5F` | `F_LIGHTRED` |

3. All other regions stay the code role. The four new roles use the rules of story markdown-palette. These are the fork table, the contrast rule, the user file and the `@` names. The light theme look is the code role.
4. A new core file in `far2l/src/markdown/` finds the export one time per process. The steps are `FindPlugin`, `Load`, `dlopen` with `RTLD_NOLOAD` on `GetModuleName`, and `dlsym`. This file holds all knowledge of the plugin. Nothing in `far2l/src/plug/` changes. A pure function in this file or a small interface must let a test supply a fake token source.
5. Language selection. The export tries a fake file name (`x.<fence name>`) with `chooseFileType`. It tries a small alias table for names that the file name misses (`c++`, `zsh`). The result `default` means no colors. A fence name has at most 32 characters from `[A-Za-z0-9+#_-]`. Other names show the plain code role.
6. Input limits. Markdown is untrusted input:
   - At most 1000 characters of each source line go to the parser. The rest of the line stays the code role.
   - The total byte cap and the fence line cap are set at the gate.
   - The export calls `breakParse()` from `addRegion` when the deadline passes. Spans from before the deadline stay.
   - One time budget covers all fences in one render, not one fence. When the budget ends, the other fences show the plain code role.
   - The export catches each Colorer `Exception` and returns failure.
7. Tokens must not parse again on a viewer resize. The gate chooses how: tokens kept for each fence, or a store keyed by fence.
8. ADR-008: the core reaches Colorer through one optional plugin export, with the reasons and the rejected routes.

**Out:**

- A link from the core to the Colorer library or to libxml2.
- HRD colors. `far2l.hrd` holds fixed RGB for a navy background, so it does not follow the theme.
- The plugin's own F3 coloring (`ViewerColoring`). It stays off in Markdown mode.
- A built-in tokenizer as a fallback. With no plugin, a fence shows the plain code role.
- Indented code blocks and HTML blocks, because they have no language name.
- Mermaid fences.
- Code spans.
- A worker thread. `cregexp.h` says "Do not parse concurrently".

## Size

M. One new plugin file, about five lines in `pcolorer.cpp` and one line in the plugin `CMakeLists.txt`. One new core file. Run storage changes in `MarkdownRender`, and `ViewerPrinter.cpp` has a small change. One ADR. If the gate finds that storage needs a redesign of `MdRun`, re-size.

## Unknowns

- Does `dlopen` with `RTLD_NOLOAD` find the loaded plugin on macOS and Linux? The first task tests this in the running app. If not, use `CallPlugin` with a custom `OpenFrom` and a request struct. That route adds a case to `OpenPluginW`. `GetModulePFN` is private, so a third way is a one-line upstream change to `plclass.hpp`.
- Does `editorSet` exist before the first render? Reading `viewer.cpp` says yes, because `VE_READ` comes first. The first task confirms this. If `editorSet` is null, the export must return failure. It must not create it, because `ReloadBase` shows a "Reloading" message. A disabled plugin has the same result.
- How to store token runs. Parsing 20k lines of JS gave 320k regions, and `MdRun` holds one `std::wstring` for each run. The gate must choose compact spans on a line, or merged runs, and show the memory on a large fixture.
- How tokens survive line wrap in a fence. Spans are source positions, and rows can differ. The first task reads the code path.
- How long the worst single regex call takes. The cap is `parseStepLimit`, 1e6 steps. Nobody measured it. The time budget depends on it.
- How to run a smoke test with the plugin. The check builds `--target far2l`, which has no plugins, and no test loads a plugin today. The gate must name the build and the test.
- Whether each common language reaches the four `def:*` classes. A harness showed this for js, python, shell, rust, java and c only. The first task lists the result for all twelve names below.
- How the first-fence cost looks on a debug build. It is not measured.

## Acceptance criteria

- With Colorer loaded, a fixture for each fence name below shows a keyword, a string, a comment and a number in the token roles. JSON and YAML show the classes they have. The names are `js`, `ts`, `python`, `bash`, `json`, `yaml`, `c`, `cpp`, `go`, `rust`, `java` and `sql`.
- Other names work with no code change when Colorer knows the file type.
- `c++` and `zsh` show colors.
- A fence with an empty or unknown language shows the plain code role.
- Each fence shows the plain code role with no plugin, a disabled plugin, or a plugin without the export. The viewer opens with no message. The current smoke tests prove the no-plugin case.
- A multi-line comment keeps its color on all rows, and the state ends with the fence.
- One fence parses one time, also after a resize.
- Hostile fixtures render in the limit that the gate sets, and the viewer stays responsive:
  - python, one line of 1 MB of `'`
  - an unclosed string
  - deep nesting
  - many small fences
  - a very long fence name
- A failed or stopped parse shows the plain code role for the rest of that fence, and the viewer does not stop.
- Rendered text, row count and row offsets do not change: only colors do.
- A mermaid fence, drawn or not drawn, does not change.
- Opening a file in F4 while the same file shows in F3 does not change the editor colors.
- With `--norgb`, tokens show the 16-color fallback of the table. On a light viewer background, tokens show the code role.
- A test with a fake token source checks the role mapping with no plugin.
- ADR-008, `markdown-viewer.spec.md`, `markdown.data.md` and `markdown.invariants.md` change at close-out.

## Notes

- Owner's words, 2026-11-06: "don't forget we will color code blocks with colorer", and "colorer has tokenizer and used for F4 edit command, what is the most efficient way to re-use it?". These replace the earlier in-tree tokenizer decision.
- Owner's choice, 2026-11-06, on the plugin export route: "colorer confirmed".
- Research on 2026-11-06 used a harness built on `libcolorer.a` at `-O2`. Nobody ran these numbers in the app.
  - Catalog load: 14 ms, 6.4 MB RSS.
  - First load of a type: 3-43 ms.
  - Parse of a short snippet: under 0.1 ms. Parse of 20k JS lines: 73 ms.
  - Twelve languages loaded: about 29 MB peak RSS.
- The earlier figures changed. Code size is under 1 MB, not 2-3 MB. The 14 MB is data on disk, and only the types in use load.
- The plugin route pays no new catalog cost, because `ProcessViewerEventW` creates `FarEditorSet` when F3 opens a file. This assumes that the plugin is enabled.
- Time limit: nothing stops a parse by itself. The python case is quadratic: 256 KB took 681 ms, and 1 MB took 9.9 s. `breakParse()` in a callback stopped that case at 52 ms with a 50 ms limit. Other hostile inputs took under 73 ms.
- A per-fence deadline is not enough: a file with many fences adds the deadlines. Thus the budget is per render.
- Rejected routes:
  - Link to the core: adds libxml2 and a second copy of the catalog.
  - Hidden editor: `Editor` is bound to `CurEditor` and `EE_REDRAW`. Nobody tried it.
  - Helper process: this tree has no Colorer console tool.
  - HRC data with a new tokenizer: needs Colorer's regex dialect.
- `.tsx` maps to the `jsx` type. This is accepted.
- `plugins/colorer` had 14 commits since the move on 2026-08-26, and none since 2026-09-27. A new file there merges easily.
- Tier from `flow-for.mjs`: `flow tier=full docs=spec,plan design=no orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=none`. The owner reads the gate before the build, because of the storage choice and the input limits.
- Branch `feat/markdown-code-highlight`. Base `next/2.9.1-ovr.1`, after `markdown-palette` merges. Budget and hard stop M $15/$20.
- Budget and hard stop: M $15/$20 (recorded in the workspace ledger).
- Started: 2026-11-06T21:41:52Z (UTC).
- Flow: `flow tier=full docs=spec,plan design=no orchestrator=yes final=final-reviewer gate=stop scan=yes verifier=none`. Gate page: the owner reads it before the build (G2-design).
- Check: "The check" in `AGENTS.md` with `BUILD=../far2l.markdown-code-highlight.build` and `TESTS="0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off"`, plus the plugin smoke test named in the plan.
- Check: "The check" in `AGENTS.md`, plus the plugin test that the gate names.
