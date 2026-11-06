---
description: Rules that always hold in the whole far2l fork, each with where it was learned. Feature rules are in the feature folders.
---

# Invariants

Rules that always hold for the whole app. Format: `- **[area]**: rule (Learned from: source, date)`. To retire one, delete it and say why in the commit or an ADR. A feature keeps its own rules in `docs/architecture/features/{slug}/{slug}.invariants.md`. For the Markdown feature see [markdown.invariants.md](features/markdown/markdown.invariants.md).

## Project-wide

- **[text]**: `wchar_t` is 4 bytes on Linux. In printf-like calls `%s` is always multibyte and `%ls` is always wide. See [HACKING.md](../../HACKING.md). (Learned from: HACKING.md, upstream)
- **[console]**: `CHAR_INFO` attributes are 64 bit and hold 24 bit RGB colors. Use the `GET_RGB_*` and `SET_RGB_*` macros. See [HACKING.md](../../HACKING.md). (Learned from: HACKING.md, upstream)
- **[style]**: New code follows [CODESTYLE.md](../../CODESTYLE.md): tabs for indentation, `CamelCase` for types and functions, `snake_case` for variables. The files in `far2l/src/markdown/` use 4 spaces and do not follow this yet. (Learned from: CODESTYLE.md, upstream; deviation found by the docs close-out, 2026-11-06)
- **[repo]**: The root `.gitignore` matches `far2l/bootstrap` and `Testing` (which also matches `testing/` on a case-insensitive file system). Add a new file there with `git add -f`. (Learned from: story markdown-preview-fixes, ruling 6, 2026-11-05; story markdown-ascii-diagrams, plan, 2026-11-06)

## Tests

- **[smoke]**: `testing/src/main.go` passes `FAR2L_STD` and `FAR2L_TESTCTL` to far2l. Without them every smoke run times out. (Learned from: story markdown-preview-fixes, close-out ruling 5, 2026-11-05)
- **[smoke]**: A smoke test reads a copy of `initdir` in `workdir`. Refresh `workdir` after you change `initdir`. `testing/far2l-smoke-run.sh` does it. (Learned from: story markdown-preview-fixes, close-out, 2026-11-05)
- **[tests]**: Run a test that must catch a regression against that regression (mutation) before you trust it. (Learned from: story markdown-default-on, close-out, 2026-11-06)
- **[specs]**: A spec row about the behavior of a third-party package is checked against that package when the plan is written. (Learned from: story markdown-ascii-diagrams, close-out lessons, 2026-11-06)
