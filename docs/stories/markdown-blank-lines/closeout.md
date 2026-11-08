# Close-out: markdown-blank-lines

Commits:
- `368ba24a2` and `e3e400b69`: the story brief, gate page, spec and plan.
- `21eacfc7f`: the G2-design waiver and workspace ignore.
- `0436339c7`: T1, source-driven blank rows in `MarkdownRender.cpp` and removal of the gap queue.
- `862a10666`: T1 fix round 1, code fence blank-line tracking and thematic break scan.
- `9b0b5f547`: T2, `spacing.md` fixture and `0007-view-file-markdown/test.js` spacing assertions.

Check: the `AGENTS.md` check command run from `.worktrees/markdown-blank-lines` with `BUILD=../far2l.markdown-blank-lines.build` exits 0 and prints `check ok`. `ascii-golden.sh`, `ascii-driver.sh`, and smoke tests `0007-view-file-markdown`, `0008-view-md-default` and `0009-view-md-setting-off` pass.

## Rulings

1. The orchestrator model (`glm-5.3`) hit a 14-day monthly quota cap after pre-flight and two interrupted T1 dispatches. The main session ran the remaining dispatches directly with the same workspace record.
2. On the owner's request, the T1 fix round, T1 re-review, T2 implementation and final review ran on `google-antigravity/gemini-3.8-flash`.
3. Four row-offset assertions in `test.js` on `sample.md` and `long.md` checked old `markdown-compact-layout` blank-row offsets. T2 updated those four lines while keeping `sample.md`, `long.md` and the T4 palette block untouched.

## Parked findings

- `far2l/src/markdown/MarkdownRender.cpp:640`: a code fence with zero body lines has no `cb_text` callback. Blank lines before it appear after the empty code row.
- `testing/tests/0007-view-file-markdown/test.js:720`: trailing empty lines at the end of a file are covered by T1 unit tests, not by an end-to-end assertion in `0007-view-file-markdown`.

## Lessons

- In md4c, a code fence with an empty last line emits `MD_TEXT_CODE` with the line's trailing `\n` at offset `-1` (a static buffer in md4c). Keep `last_text_end` at `line_end`, and advance it across verbatim empty fence lines so trailing empty fence lines are not counted twice.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| task-reviewer-critical | opus | 1 | 1 | 1 | 4 |
| re-reviewer | google-antigravity/gemini-3.8-flash | 1 | 0 | 0 | 0 |
| final-reviewer | google-antigravity/gemini-3.8-flash | 1 | 0 | 0 | 2 |
