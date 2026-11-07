# Close-out draft: markdown-colorer-look

Commits: T1 `0a0eef809`, T2+T3 bridge `99e1f2a4e`, T3 roles + T4 `4b21a5f35`, T5 `9e9ddd13f`, T6 `19f4464a9` (on `feat/markdown-colorer-look`, base `6bf36be25`). Check: full build + ascii-golden + ascii-driver + highlight-driver + `look-driver.sh --rgb` + smoke suite exit 0 (14 pass, 2 known Mac failures `0004-vt-shell`, `000b-find-file`).

## Rulings

- R1: T1 and T3 run in parallel; the first to land creates the look driver scaffold, the other
  adds its case on top. Path-scoped commits, no conflict.
- R2: the ten-name list of Global Constraints governs (it ends with `markdown:Text`); the spec's
  Region map table covers the nine scheme rows. `body` takes `markdown:Text`, then `def:Text`.
- R3: spend bootstrap uses `--allow-none` before the first dispatch.
- R4: the export's catch logs `ex.what()` (the tree's `Exception` has no `announce()`).
- R5: the look driver's canonical run is `--rgb` (the spec's region table is the rgb scheme
  far2l.hrd); the headless default palette takes the 16-color console.hrd path.
- R6: agent spawning is disabled in this harness; the orchestrator plays implementer and reviewer
  inline, one reviewed commit per task.
- R7/R8: the T2 canonical check is one full-suite `--rgb` run in case order; a lone
  `look_bridge_real` on a fresh process needs the priming of the earlier cases.
- R9: the 000d app runs a 16-color terminal, so the app look is the console.hrd rows plus the
  scheme underline on every run; the truecolor far2l.hrd rows are asserted by `look_real_hrd`.
- R10: with the plugin the table draws with box characters (000d asserts them); pipes are the
  far2l-only look (0007).
- R11: 000d session 1 colors are the fork table (first render precedes the plugin's first viewer
  event, generation 0); session 2's reopen shows the console scheme after the editor's parse
  advances the generation - the cache and generation contract working.
- R12: the colorer scheme popup (F6 in the editor) has no on-screen text the harness can assert;
  the scheme-change movement is proven by session 2's moved row plus `look_bridge_real`.

## Measurements (T1)

- `look_first_call`: first call (no FarEditorSet) returns code 1 in 0 us. After the first viewer
  event: code 0 in 68-84 ms (the fetch warms the region-define chain), generation 1.
- `look_real_hrd`: all ten far2l.hrd rows exact (def:Text `0x99EEEE/0`, HeadText `0x8888FF/4`,
  LinkText `0x8888FF/4`, StrongText `0x40A000/0`, EmText `0xFF20A0/0`, Code `0xFFE977/0`,
  QuoteText `0x6699FF/0`, ListMark `0xFFFFFF/0`, Rule `0xFFFFFF/0`, Text non-zero), generation 1.
- `look_cost`: ten-name fetch mean 1 us over 1000 warm calls. The T4 cache is cheap insurance.

## Parked findings

- Codes 1/2/3 answer no generation (the cache reads generation only on code 0) - accepted T2.
- Driver probes removed before the T5 commit; the app-side probe chain (dlopen NOLOAD, generation
  at print time) was confirmed by hand, not by an assertion - accepted T5.

## Lessons

- The harness `far2l-smoke-run.sh` resolves the test directory from its own binary's location, not
  the caller's cwd: a new test directory must be mirrored into the main repo's `testing/tests/` to
  run from a worktree. Worth a line in the project rules.
- `spend.mjs --root` mirroring (`.build/<story>/spend.sh`) is needed under the omp transcript
  layout; the transcripts flush at session close, so mid-build spend reads the last snapshot.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---|---|---|---|
| implementer-risky | opus medium | 2 | 0 | 0 | 1 |
| task-reviewer-critical | opus medium | 2 | 0 | 1 | 0 |
| implementer | haiku xhigh | 4 | 0 | 0 | 2 |
| task-reviewer | haiku xhigh | 4 | 0 | 1 | 1 |
