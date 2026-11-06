# Close-out: markdown-compact-layout

Commits: `c8cd9b1ba` (T1, the layout), `fa702b97b` (T2, review fixes). Check: `sh docs/stories/markdown-compact-layout/check.sh` exit 0 (`check ok`).

## Rulings

1. The owner sent T1 to a local model. The owner then stopped it two times because of the run time, and a haiku implementer finished it. These stops did not promote the story.
2. `open_block()` after `drop_soft_gap()` at list-item enter is accepted. C10 is the spec value, and the plan row was wrong.
3. The owner raised the budget to $12 and the hard stop to $14. This order replaced the promotion to standard for the fixes. T2 ran on the local model.
4. The owner checked the UI and gave the order to close and merge. Spend had passed the hard stop at that time.

## Parked findings

- `test.js:517` and `:524`: two comments say "first block of an item", but the block is the second one.
- `test.js:505-515`: check 5 allows the fence at +1 or +2, and it has no bullet cell check.
- The first review's Minor findings stay open. These are calls with no test, a repeated guard and stale comments. Others are the build folder name in `check.sh`, fixture texts that are substrings of other texts, and no direct `nFilePos` check for the new blank rows.

## Lessons

- The spend script counts all work in the worktree session since `started`. The planning of later stories and its research went into this figure. Plan other stories in a separate session or worktree.
- The local model was slow on T1: the owner stopped two runs at about 48 and 17 minutes. T2 took about 9 minutes.
- HTML blocks now have written spacing rules in the spec, so the second Minor finding of the second review is closed.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| final-reviewer-light | sonnet | 2 | 0 | 2 | 9 |
