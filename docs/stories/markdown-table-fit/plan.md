# Plan: markdown-table-fit

Spec: `spec.md` governs exact behavior. The implementation keeps table data as rows, cells, and styled runs until the width-aware layout pass.

## Global Constraints

- Touch `far2l/src/markdown/MarkdownRender.cpp`, the existing Markdown smoke fixture, and the three Markdown contract documents. Do not change md4c, vendored code, browser-view HTML, or unrelated wrapping.
- Column widths use natural content width when the table fits the viewer, and only then shrink and wrap. Widths include cell padding and vertical borders, and every emitted line stays within the viewer width.
- Prefer whitespace breaks. Split a word when no break fits. Repeat borders and padding on continuation lines.
- Preserve `MdRole`, source offsets, blank-row rules, separator-row placement, and rendered-line scrolling. Do not emit trailing spaces.
- Commit message: a short imperative summary line. Do not commit directly to `main` or a `next/*` branch.
- Check: use the repository Markdown check from `AGENTS.md` and include `000f-view-md-wrap` in `TESTS`, in addition to `0007`, `0008` and `0009`. Clean stale sockets before each smoke test. Owner-checked paths are `AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`, `testing/markdown/golden/*.txt`, vendored code, and `.github/workflows/`.

## T1: Implement width-aware table layout

**Depends on:** none
**Done check:** `cmake --build ../far2l.build --target far2l -j8` exits 0
**Role:** implementer — review task-reviewer-critical

Update `MarkdownRender.cpp` so the table model computes deterministic widths for the available viewer area, wraps each cell into styled visual lines, repeats borders and padding, and preserves source offsets and existing separator behavior. Keep the data shape as `rows × cells × MdRun`; make the layout pass own allocation and wrapping.

## T2: Add behavioral smoke coverage

**Depends on:** T1
**Done check:** `node --check testing/tests/0007-view-file-markdown/test.js` exits 0
**Role:** implementer — review final

Extend the existing table fixture and smoke controller with a table wider than the terminal, a long word, multiple body rows, empty or missing cells, and inline code and link runs. Assert visible continuation text, bounded line contents, borders, separator placement, and no additional table content after Right or Left.

## T3: Update durable contracts

**Depends on:** T1
**Done check:** `node -e "for (const f of ['markdown-viewer.spec.md','markdown.runtime.md','markdown.invariants.md']) { if (!require('fs').readFileSync('docs/architecture/features/markdown/'+f,'utf8').includes('wrap')) process.exit(1) }"` exits 0
**Role:** implementer — review final

Replace the horizontal table-scroll contract with vertical cell wrapping in the viewer spec, runtime flow, and invariants. Record the fixed narrow-viewer policy, repeated borders, role preservation, and resize behavior.
