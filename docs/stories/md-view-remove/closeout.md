---
description: Close-out of md-view-remove, with the rulings, smoke results, parked findings, lessons and review yield.
---

# Close-out

Commits: b51146802..0b24ac36f. T1 48179eac9 (drop the hint row), T2 85c327508 (delete the browser view files and CMake rules), T3 bf95b3f6e (docs), fix 0b24ac36f (final review doc fixes). Check: `sh .build/check.sh` exit 0 (`check ok`). The full build with all plugins exits 0.

## Rulings

1. T3 may edit only the doc files that plan T3 lists, owner-checked paths included. The owner approved them through the story scope.
2. The commit trailer is `Co-Authored-By: Claude Opus 5.5` (plan). The T1 implementer used another name. The orchestrator amended the local commit.
3. The final review diff starts at BASE b51146802 (stacked story, no main ref).
4. The UTF-8 BOM defect row stays in markdown-viewer.spec.md without the sentence about the browser view. Plan line 199 told T3 to delete the whole row. That was wrong.
5. Acceptance 1 uses reading (a): the grep excludes ADR-005 as a whole.

## Smoke results (verify hook)

All smoke tests ran on the story binary.

- PASS: 0001, 0002, 0003, 0005, 0006, 0007 (both), 0008 (both), 0009 (both), 000a (both), 000c (both), 000d, 000e, 0010-view-md-table-cols, 0011-view-md-wide-edge.
- Known FAIL: `0004-vt-shell`, `000b-find-file`.
- FAIL, not caused by this story: `000f-fenceprobe` and `000f-view-md-wrap`. Each test failed two times out of two on the story binary and on the base binary (b51146802), each run alone with a socket clean.
  - `000f-fenceprobe`: far2l stops with SIGILL at exit. The last test key arrives while the app exits, in `TestController::ClientDispatchSendKey` (`WinPort/src/Backend/TestController.cpp:248`). The crash handler then recurses in `SafeMMap.cpp` until the stack guard stops it.
  - `000f-view-md-wrap`: the assertion "the table header row is split by wrap" (test.js line 70) fails. The row under the table header is a body row, not the rule row.
- Acceptance 5 names only two known failures. These two tests are new failures on the base too. They need a fix story of their own.

## Parked findings

- T1 Minor 1: a comment in look_driver.cpp has a short broken line.
- T1 Minor 2: the look-driver tint size check had no RED run. The old code gave three rows.
- T1 Minor 3: test 0007 finds the hint by its old wording only.
- Final Minor: markdown.runtime.md line 12 says "UI thread" two times.
- The manual F3 check of a `swimlane-beta` fence in a live far2l did not run. Test 0007 and the look driver case `tint` check the same rows.

## Lessons

- `far2l/bootstrap` is in the root `.gitignore`. Edits of tracked files commit, but git ignores a new file there with no message. Check with `git check-ignore`.
- A plan step that deletes a table row must say which part of the row is dead. Plan line 199 deleted a live defect.
- An implementer writes its own model name in the trailer when the dispatch does not state it. Keep the trailer in the dispatch header.
- Run each smoke test alone when you find a failure. Two runs at the same time give false failures.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| task-reviewer-critical | opus | 2 | 0 | 0 | 3 |
| final-reviewer | opus | 1 | 0 | 1 | 4 |
| re-reviewer | haiku | 1 | 0 | 0 | 0 |
