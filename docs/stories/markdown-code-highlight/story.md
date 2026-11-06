---
description: Add syntax colors to fenced code blocks in the F3 Markdown view, with a small in-tree tokenizer for twelve languages.
status: planned
size: M
risk: security
depends-on: markdown-theme-colors
---

# Story: Syntax colors in fenced code blocks

## Goal

A reader of a Markdown file in F3 can read a code block as easily as in an editor. A fence with a known language shows keywords, strings, comments and numbers in their own colors.

## Context

- [ADR-004](../../architecture/decisions/adr-004-palette-roles.md), as changed by story markdown-theme-colors
- [markdown.invariants.md](../../architecture/features/markdown/markdown.invariants.md): Markdown is untrusted input
- Code: `code_lang` and the `MD_BLOCK_CODE` case of `cb_leave_block` in `far2l/src/markdown/MarkdownRender.cpp`
- Color reference only: the `def:*` region names in `plugins/colorer/configs/base/hrc/` and `hrd/rgb/far2l.hrd`

## Scope

**In:**
- A table-driven tokenizer in a new file in `far2l/src/markdown/`. One scanner, and one data table for each language: keywords, comment marks, string marks, number rules.
- Token classes: keyword, string, comment, number. A type class is optional.
- Twelve languages. Fence names match in any case:

| Language | Fence names |
|---|---|
| JavaScript | `js`, `javascript`, `jsx` |
| TypeScript | `ts`, `typescript`, `tsx` |
| Python | `py`, `python` |
| Shell | `sh`, `bash`, `shell`, `zsh` |
| JSON | `json` |
| YAML | `yaml`, `yml` |
| C | `c` |
| C++ | `cpp`, `c++`, `cc` |
| Go | `go` |
| Rust | `rust`, `rs` |
| Java | `java` |
| SQL | `sql` |

- Multi-line comments and strings, with state from one line of the fence to the next.
- Token colors derived from the theme with the method of story markdown-theme-colors, each with a 16-color index.
- ADR-008: an in-tree tokenizer, not Colorer, with the trigger to look at it again.

**Out:**
- A link to the Colorer library, any new dependency, or a call into the Colorer plugin.
- Semantic analysis, and languages other than these twelve.
- Indented code blocks and HTML blocks, because they have no language name.
- Mermaid fences.

## Size

M. The scanner is one file and each language is data. If the gate finds that a language needs its own scanner, re-size or drop that language.

## Unknowns

- How to store token runs. `MdRun` holds one `std::wstring` for each run, so a run for each token costs much memory on long files. The gate must choose compact spans on a line or tokens made only for drawn rows. It must show the memory on a large fixture.
- How exact the result is on hard cases: template strings in `js`, heredocs in `sh`, raw strings in `rust`, nested comments. The rule: a wrong color is accepted, a crash or a slow render is not.

## Acceptance criteria

- Each of the twelve languages has a fixture that shows a keyword, a string, a comment and a number in the expected class.
- A fence with an unknown or empty language shows the plain code role, as today.
- A multi-line comment keeps its color on all rows, and the state ends with the fence.
- The scan is linear in the size of the fence. A hostile fixture (one 1 MB line, an unclosed string, deep nesting) renders in the limit that the gate sets, and the viewer stays responsive.
- Rendered text, row count and row offsets do not change: only colors do.
- A mermaid fence, drawn or not drawn, does not change.
- With `--norgb`, tokens show their 16-color indexes.
- ADR-008 and `markdown-viewer.spec.md` are updated at close-out.

## Notes

- Decided by the owner on 2026-11-06: a small in-tree tokenizer. The Colorer library runs with no editor, but a link adds libxml2 to the core. It also adds 2-3 MB of code and up to 14 MB of data. The first fence costs about 80-150 ms on a debug build. No test showed that its parser can obey a time limit.
- Branch `feat/markdown-code-highlight`. Base `next/2.9.1-ovr.1`, after `markdown-theme-colors` merges. Budget and hard stop M $15/$20.
- The owner reads the gate before the build, because of the storage choice and the input limits.
