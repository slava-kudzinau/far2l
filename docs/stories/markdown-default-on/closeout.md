# Close-out draft: markdown-default-on

Commits: 3a3b8e52d (T1: option, open rule, dialog, tests 0007 edit and 0008), 41cdcb98b (T2: deviation-only cache scheme, tests 0008 and 0009), cce0a2dfa (fix round: rule captured at open). Check: `sh docs/stories/markdown-default-on/check.sh` exit 0 (tests 0007, 0008, 0009 pass; the final reviewer ran it once; the main session reruns it).

## Rulings

1. Open order: a cache entry that holds a choice wins; else the setting plus the suffix rule; else plain. The "inherit the last file's mode" start and the `ViewerMarkdownMode` seed no longer set the start mode.
2. Suffix test: case-insensitive `.md` or `.markdown`; a file named exactly `.md` does not match.
3. Cache scheme (replaces the first one): `Param[3]` is 0 when the mode equals the rule default, 1 for Markdown against a plain rule, 2 for plain against a Markdown rule. Read: 1 Markdown, 2 plain, else the rule. A legacy 0 on a `.md` file now follows the rule.
4. The rule is captured once when a file opens (`m_md_rule` in `viewer.hpp`) and used at both save sites, so a setting change while a Quick View viewer is open cannot freeze a default as a choice. `viewer.hpp` is outside the plan's file list; the ruling needs it.
5. `ViewerMarkdownMode` is write-only now (still saved by Shift-F4, nothing reads it). Its description and comments say so.
6. Test 0007 was edited by owner ruling (start plain with Shift-F4). Setting-off path is a new test 0009 (two starts on one profile).

## Parked findings

- Grep filter (#6): the temp copy keeps the `.md` base name, so the filtered view opens in Markdown. Same class as the old loss of the mode. Not in the spec.
- check.sh (#7): it copies `mermaid.min.js` from the main checkout, uses macOS `sysctl` and absolute paths. Acceptable for a story-local check.
- `far2l/src/hist/poscache.hpp:64` still documents viewer `Param[3] = 0`. The file is outside the allowed files.
- Gray +/- (next or previous file) has no automated test of the second cache save site.
- Old far2l builds read a saved `2` as truthy: a plain-by-choice file shows Markdown if the owner switches builds on one profile.
- `check.sh` header still says "(T1)". Cosmetic.
- Minor #3 (legacy plain entry) is settled by ruling 3.

## Lessons

- A lite story that changes a persisted format needs a standard review of the format choice; the first cache scheme froze defaults as choices and the final review caught it.
- A test that must catch a regression has to be run against that regression (mutation); the implementer did this for the inherit and seed cases.
- Name the allowed files with their headers (`viewer.hpp`) to avoid out-of-list edits.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| final-reviewer-light | sonnet | 1 | 0 | 2 | 5 |
| task-reviewer | haiku | 1 | 0 | 1 | 2 |
| task-reviewer-critical | opus | 1 | 0 | 0 | 0 |
| final-reviewer | opus | 1 | 0 | 0 | 6 |
