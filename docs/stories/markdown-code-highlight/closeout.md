---
description: Close-out record for markdown-code-highlight: what shipped, the lasting rulings, and the final review round.
---
# Close-out: markdown-code-highlight

Branch `feat/markdown-code-highlight` (base `origin/next/2.9.1-ovr.1` at `54bd9cc79`),
35 commits, `8e452d972..3b89a328f`, merged in PR #11 (`c7b612286`). Check: golden + ascii
fence driver + four smoke tests, exit 0, green on the merged tip. Full tier, tier kept
(no promotions, no demotions).

## What the story built

A fenced code block in the F3 Markdown mode now shows keyword, string, comment and number
colors. The core asks the Colorer plugin through one new C export
(`ColorerTokenizeV1`, `plugins/colorer/src/pcolorer2/ColorerExport.cpp`) and `dlopen`
(`far2l/src/markdown/MdColorer.cpp`). Tokens are cached per fence by language plus a 64-bit
hash, so a resize does not re-tokenize. With no plugin, a fence shows the plain code role.

## Lasting rulings

- **R47** The token group is in cells of the code run (not source columns), so a horizontal
  scroll colors the right cells. Cost if wrong: none.
- **R38** The cache key is the fence language and a 64-bit FNV-1a hash of the fence text;
  the cache lives in the viewer window's `MarkdownDoc`, across re-renders and resizes.
- **R37** Tokenizing runs under four limits: 1000 characters per source line, 400 lines per
  fence, 65536 bytes per fence, 40 ms for all fences of one render. A fence that hits a limit
  shows the plain code role.
- **R49** The fence driver exits with `_exit`: the plugin's static teardown throws on the
  headless console; the exit code is the verdict.
- **R52** The "256 index" of the spec table is a color. The palette table carries it
  (`def256`): a terminal with no RGB paints that color, and the 16-color index stays in the
  low attribute bits. The code role shows the split: 256 index 213 (`#D7D7AF`) differs from
  its RGB default (`#FF87FF`).
- The ini keys are the dashed names (`token-keyword`, ...); a literal override carries no
  16-color index.
- T6's review is the final review (the plan's Role line).

## Findings that changed code

- T2 review: split rc -2 (unknown language) from rc -1 (no plugin).
- T4: a tokenized row pads with the code role, not the body color (420f65912).
- T5: fence-per-export-call and headless `_exit` (487b98ff7); the 256-color look on the
  norgb path, the dashed ini keys, and the smoke-script int-result rule (ab4c425c2).

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|

No separate per-task reviewer tiers kept needing fix rounds: one fix round on T4, three on
T5 (the last two on test expectations, not code). No model change is needed.

## Owner gates

- `gate.md` is approved; the fresh-reader paraphrase is pasted in the story folder.
- Manual look check in macOS Terminal and ghostty: the owner reviews the fence colors in
  both terminals (truecolor and 256-color), then sets the story status.
- Docs: ADR-008, viewer spec, data and invariants pages are updated by T6 (8f4d4faf0).

## Final review round

Reviewer (opus, full-branch diff): correct, no blockers; two P2 findings and nits, all
fixed in `8ec336614` and `75e5f4b7f`:

- Four bridge alias rows sent `ignore`, `conf`, `hbs`, `bf` to the export; the catalog
  resolves `gitignore`, `apache`, `twig`, `lisp`. `gitignore`, `handlebars` and
  `brainfuck` fences showed the plain code role. The bridge rows now send the export
  targets, and the new driver case `alias_probe` pins all four end to end.
- Test gaps closed: `000c` asserts the horizontal-scroll row (F2 unwrap, Right, token
  colors of the visible slice); `slug_probe` now carries the eleven no-scheme slugs
  (`matlab` resolves by type name, the rest keep the code role).
- Nits: dead global and dead null-check in the export, stale comments, `print_len` by
  reference (tail pad now fills only the remaining cells), duplicate driver rows,
  `MdRoleColor256` now used by the `print_norgb` case, doc drift in spec and story
  (38-row tables, `editorSet` created on demand, `ClassForName`, `--norgb` attribute).
- `.build/` scaffolding and the `testing/tests/*/workdir` run state left the tracked set;
  `.gitignore` covers both.
