# far2l fork: rules for people and agents

far2l is a Linux, macOS and BSD port of the FAR Manager file manager. This fork (`origin`, `slava-kudzinau/far2l`) adds Markdown support to the F3 internal viewer. It also adds a view that renders a Markdown file to HTML and opens it in an external browser. `upstream` is `elfmz/far2l`. This file is the index for all work on the project.

## Build and test

Details and the reasons are in [docs/how-we-build.md](docs/how-we-build.md). Run the commands below from the repo root.

**Build.** Always build out of tree. Each worktree uses its own build folder.

```sh
# new build folder (one time)
cmake -S . -B ../far2l.build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTESTING=Yes -DUSEWX=No -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
# far2l only
cmake --build ../far2l.build --target far2l -j8
# far2l and all plugins
cmake --build ../far2l.build -j8
```

The macOS binary is `../far2l.build/install/far2l.app/Contents/MacOS/far2l`. The Linux binary is `install/far2l` in the build folder, so change `APP` in the check below on Linux. Smoke tests need `-DTESTING=Yes`.

**The check.** This is the one command that proves that a story is done. It builds far2l, then runs the Markdown golden test, the C++ driver test and the three Markdown smoke tests. It takes about 12 s after a build. It prints `check ok` and exits 0, or it names the failed test and exits 1. In a worktree, set `BUILD` to the build folder of that worktree.

```sh
BUILD=${BUILD:-../far2l.build}; APP=$(cd "$BUILD" && pwd)/install/far2l.app/Contents/MacOS/far2l; TESTS=${TESTS:-0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off}
cmake --build "$BUILD" --target far2l -j8 && sh testing/markdown/ascii-golden.sh && sh testing/markdown/ascii-driver.sh && (cd testing && for t in $(echo $TESTS); do o=$(./far2l-smoke-run.sh "$APP" "$t" 2>&1) && echo "$o" | grep -q '\[DONE\]' && ! echo "$o" | grep -q FAILED || { echo "FAILED: $t"; exit 1; }; done) && echo "check ok"
```

**All smoke tests.** Use this when a change touches more than Markdown. It prints PASS or FAIL for each test and takes about 32 s. Read the output, because the loop has no useful exit code.

```sh
APP=$(cd ../far2l.build && pwd)/install/far2l.app/Contents/MacOS/far2l
(cd testing && for t in $(ls tests); do o=$(./far2l-smoke-run.sh "$APP" "$t" 2>&1) && echo "$o" | grep -q '\[DONE\]' && echo "PASS $t" || echo "FAIL $t"; done)
```

On this Mac, `0004-vt-shell` and `000b-find-file` fail today. The cause is not known. Any other failure is new, so stop and find the cause.

**Worktrees.** A worktree is in `.worktrees/{story}`. Its build folder is next to the main checkout, at `../../../far2l.{story}.build` from the worktree root. Set `BUILD` to that folder.

The path of a worktree is long. Thus the unix socket of a smoke test can land in `testing/tests/` as a stale file, for example `0007-view-fi`. The next run of `far2l-smoke-run.sh` then stops with `mkdir: tests/0007-view-fi: Not a directory`, and the check prints `FAILED` for a test with no code fault. Before each smoke test in a worktree, delete only the sockets at that level:

```sh
(cd testing && find tests -maxdepth 1 -type s -delete)
```

Do not trust the exit code of `far2l-smoke-run.sh`. It returns 0 when a test only logs `[FAILED]`. There is no `make test`, and `ctest` at the build root finds nothing. CI does not run on the branches of this fork, so run the check on your own machine.

## Where things are

| Need | Look in |
|---|---|
| Whole-app design, top risks | [docs/architecture/blueprint.md](docs/architecture/blueprint.md) |
| Rules that always hold, whole app | [docs/architecture/invariants.md](docs/architecture/invariants.md) |
| A feature: overview, rules, flows, data, contracts | `docs/architecture/features/{slug}/`, see the table below |
| Build, test, CI, branches, delivery | [docs/how-we-build.md](docs/how-we-build.md) |
| Story records and open owner items | [docs/stories/index.md](docs/stories/index.md) |
| Upstream developer guide, code style | [HACKING.md](HACKING.md), [CODESTYLE.md](CODESTYLE.md) |
| Smoke test framework | [testing/README.md](testing/README.md) |
| Doc templates | `docs/.templates/`: adr, view, blueprint, story. The other story templates are in the `fde-delivery` plugin. |

Whole-app views, in `docs/architecture/views/`. Runtime and data views of the whole app do not exist yet.

| View | Shows |
|---|---|
| [context.md](docs/architecture/views/context.md) | far2l, its users, hosts, the operating system and remote systems |
| [containers.md](docs/architecture/views/containers.md) | Top-level parts of the repo, plugins by kind, how far2l starts |

The Markdown feature, in `docs/architecture/features/markdown/`:

| File | Shows |
|---|---|
| [markdown.overview.md](docs/architecture/features/markdown/markdown.overview.md) | Start here: scope, strategy, risks and open questions |
| [markdown.invariants.md](docs/architecture/features/markdown/markdown.invariants.md) | Rules that always hold for the feature |
| [markdown-viewer.spec.md](docs/architecture/features/markdown/markdown-viewer.spec.md) | What Markdown mode in F3 must do |
| [markdown-browser-view.spec.md](docs/architecture/features/markdown/markdown-browser-view.spec.md) | What the browser view must do |
| [markdown.runtime.md](docs/architecture/features/markdown/markdown.runtime.md) | Open in F3, render and scroll, mermaid fence to ASCII art, open in browser |
| [markdown.data.md](docs/architecture/features/markdown/markdown.data.md) | Screen lines, color runs, position cache, settings keys, temp files, bundled JS |

Decisions, in `docs/architecture/decisions/`:

| ADR | Decision |
|---|---|
| [001](docs/architecture/decisions/adr-001-docs-structure.md) | Docs structure, flat stories, delivery through `fde-delivery` |
| [002](docs/architecture/decisions/adr-002-md4c-parser.md) | Parse Markdown with the vendored md4c event API |
| [003](docs/architecture/decisions/adr-003-line-index-scrolling.md) | Scroll by rendered line index, not by file offset |
| [004](docs/architecture/decisions/adr-004-palette-roles.md) | Runs carry a palette role, not a color |
| [005](docs/architecture/decisions/adr-005-browser-view-script.md) | The browser view is a shell script run through File associations |
| [006](docs/architecture/decisions/adr-006-ascii-diagrams.md) | ASCII diagrams from a committed JS bundle in quickjs-ng |
| [007](docs/architecture/decisions/adr-007-markdown-default-on.md) | Markdown mode on by default, and the cache stores only a deviation |

Research: [mermaid-image-poc.md](docs/research/mermaid-image-poc.md) records the proof of concept that drew a mermaid fence as a PNG. It did not ship.

Before you open files, get a list of all docs: `grep -r '^description:' docs --include='*.md'`.

## Global policies

**Code**
- Follow [CODESTYLE.md](CODESTYLE.md) and the rules in [invariants.md](docs/architecture/invariants.md). Before you change the viewer or the Markdown code, read [markdown.invariants.md](docs/architecture/features/markdown/markdown.invariants.md).
- If you add a new top-level file or folder, check it with `git check-ignore`. The root `.gitignore` has wide patterns.
- Keep the fork's changes to upstream files small. The fork must merge from `upstream` with few conflicts. Put new code in `far2l/src/markdown/` and not in the upstream files, where you can.
- Treat Markdown files and mermaid text as untrusted input. A render has a time limit and a stack limit, and a failed render must not stop the viewer.
- Use palette roles for colors. Do not use fixed ANSI colors.

**Tests**
- Do not regenerate `testing/markdown/golden/*.txt` to make a test pass. The golden files come from the `beautiful-mermaid` package, not from our bundle.
- If a test must catch a regression, run it against that regression before you trust it.
- If a change alters a contract, update the matching `*.spec.md` file at close-out, in the same branch.

**Docs**
- Every doc has frontmatter: `description`, and `status` where it applies. See [ADR-001](docs/architecture/decisions/adr-001-docs-structure.md).
- The top level of `docs/architecture/` and `views/` are for the whole app. A feature has one folder, `docs/architecture/features/{slug}/`.
- Name each file in a feature folder `{slug}.{type}.md`, or `{component-slug}.{type}.md` for one component. The type is `overview`, `spec`, `runtime`, `data`, `invariants` or `component`. The name keeps its context when you share one file.
- A frontmatter value does not contain `: ` or ` #`, does not end in `:`, and does not start with a backtick or a quote.
- Durable docs (`architecture/`, `specs/`, `research/`, `how-we-build.md`) do not link into `docs/stories/`. Name the story in plain text.
- At close-out, do three checks. If a contract changed, update the `*.spec.md` file. If a choice outlives the story, write an ADR. If you learned a rule, add it to the matching invariants file.
- Write in ASD-STE100 Simplified Technical English. Run `check_ste.py` from the `fde-delivery` plugin (`kb/scripts/`) on each page. The rules do not apply to code, commands and quotes.

**Git and delivery**
- Do story work with the `fde-delivery:deliver-story` skill. Story folders go in `docs/stories/{story}/`.
- Name each new branch `{type}/{slug}`. The type is `feat`, `fix`, `refactor`, `test`, `doc` or `chore`. A story branch is `feat/{story}` or `fix/{story}`. The old `markdown-preview/*` branches keep their names.
- When stories run in parallel, each story has a worktree in `.worktrees/{story}`. Pull requests go into `next/2.9.1-ovr.1`.
- Always pass `--repo slava-kudzinau/far2l` to `gh`. Without it, `gh` uses `elfmz/far2l` in this checkout.
- Do not commit, push or merge if the owner did not ask. Do not push to `master`. Do not send a pull request to `upstream` if the owner did not ask.
- Do not install from the network if the owner did not ask.
- Do not put secrets, client data or personal paths in the repo.

**Owner-checked paths.** A builder does not edit these paths during a build. The main session or the owner edits them at close-out or with the owner's approval:
- `AGENTS.md`, `docs/architecture/`, `docs/how-we-build.md`
- `testing/markdown/golden/*.txt`
- Vendored code: `far2l/src/markdown/md4c/`, `far2l/third_party/quickjs/`
- `.github/workflows/`

## Main session

The main session talks to the owner and manages the work. It keeps decisions and summaries, and it delegates raw material.

- **Keep:** the talk with the owner, gates and rulings, briefs, commits, pushes and pull requests, and the decision on what to do next.
- **Delegate:** reading more than two files to answer a question, drafting a long doc, debugging, and checking fixes against findings. Use subagents for parallel work, for a fresh context and for independent review.
- **Pick the model and effort by the work.** In a story, use the roles from the `fde-delivery` plugin (`node scripts/model-for.mjs --all`). Outside a story, use this guide:
  - To locate things, use haiku with low effort.
  - To write and to judge, use sonnet with medium or high effort.
  - For contracts, security and independent review, use opus with high effort.
- **Read results, not material.** Read the subagent's reply and the headings of its review. Open the file only to settle a ruling.
- A subagent can propose a ruling. Only the owner or the main session makes it.
- Give each parallel subagent its own paths, so that two subagents do not write the same file.
