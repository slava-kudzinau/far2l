---
description: Docs follow architecture, research and stories, with one folder under architecture for each feature, flat stories, AGENTS.md as the index, and delivery through the fde-delivery plugin.
status: accepted
date: 2026-11-06
---

# ADR-001: Docs structure and delivery flow

## Context

- This fork of far2l added the Markdown viewer in five stories. The story folders sat in a top-level `stories/` directory, with no home for design, contracts or lasting decisions.
- The upstream repository has `HACKING.md`, `CODESTYLE.md` and `README.md`. The new docs must add to them and not repeat them.
- The fork has one feature line (the Markdown view), so an epic level has nothing to group.
- A reader may share one doc alone. The doc must keep its context in its name.

## Decision

`docs/` follows architecture, research and stories. Stories are flat in `docs/stories/`, with no epic level.

```
docs/
  how-we-build.md   build, test, CI and delivery guide
  architecture/
    blueprint.md, invariants.md, views/   the whole app
    features/{slug}/                      one folder for each feature
    decisions/                            ADRs
  research/         experiments behind ADRs
  stories/          one folder per story, plus index.md
  .templates/       adr, view, blueprint, story
```

- The top level of `docs/architecture/` and `views/` are for the whole app. The views are context and containers. The runtime and data views of the whole app do not exist yet.
- A feature has one folder, `docs/architecture/features/{slug}/`. It holds the overview (the entry point), the invariants, the specs and the feature views.
- Every file in a feature folder repeats the slug in its name: `{slug}.{type}.md`, or `{component-slug}.{type}.md` for a doc about one component. Examples: `markdown.overview.md`, `markdown-viewer.spec.md`. The reason is that a name keeps its context when someone shares the file alone.
- ADRs stay in `docs/architecture/decisions/`, because a decision can cross features.
- `docs/specs/` holds nothing. The folder does not exist until needed. A contract for the whole app would go there.

- `AGENTS.md` is the index and holds the global policies.
- Delivery follows the `deliver-story` skill of the `fde-delivery` plugin. A story goes from `in-progress` to `in-review` to `done`.
- A story status lives in the `story.md` frontmatter. `docs/stories/index.md` lists the stories and shows the merge state.
- Durable docs (`architecture/`, `research/`, `how-we-build.md`) never link into `docs/stories/`. They name a story in plain text.
- Before a story is `done`, close-out runs three checks. A contract changed: update the spec in `docs/architecture/features/{slug}/`. A choice outlives the story: write an ADR. A rule was learned: add it to `docs/architecture/invariants.md` for the whole app, or to the feature `{slug}.invariants.md`.
- Write an ADR or a rule only if the next story, with only the code and tests, could repeat the mistake or redo the study.

## Consequences

- A reader finds the design in `architecture/` without reading story history. The story folders stay as a record and are not authoritative.
- Story docs name repository paths. A move of the story folders breaks the paths in them, so move them with a path check.
- A second feature gets its own folder in `docs/architecture/features/`. An epic level needs a new ADR.

## Rejected alternatives

### Stories in a top-level `stories/` directory

**Why rejected:** it keeps story records apart from the other docs, and nothing links design to stories. Revisit if the stories get a tool that needs a fixed top-level path.

### An epic level above the stories

**Why rejected:** the fork has one feature line, so an epic folder would hold all five stories. Revisit when a second feature line starts.
