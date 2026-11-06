---
description: Solution design of the whole far2l app in this fork - scope, strategy, building blocks, crosscutting rules and whole-app risks, linking the views and ADRs.
refs:
  - docs/architecture/views/context.md
  - docs/architecture/views/containers.md
  - docs/architecture/invariants.md
  - docs/architecture/features/markdown/markdown.overview.md
---

# Blueprint: far2l (fork)

## Context and scope

far2l is a Linux, macOS and BSD fork of FAR Manager v2: a two-panel file manager. It has a viewer, an editor, a command line and an embedded shell. One process runs it. A terminal or a GUI window shows its screen. Plugins reach remote hosts. → [Context view](views/context.md), [README.md](../../README.md), [HACKING.md](../../HACKING.md)

This repository is a fork (`origin` is `slava-kudzinau/far2l`, `upstream` is `elfmz/far2l`). `master` is the upstream base. The fork adds features on top of it. This blueprint describes the whole app. A feature has its own folder.

## Features

- **Markdown:** Markdown mode in the F3 viewer and a browser view. → [overview](features/markdown/markdown.overview.md)

A feature folder is `docs/architecture/features/{slug}/`, with files named `{slug}.{type}.md`. See [ADR-001](decisions/adr-001-docs-structure.md).

## Solution strategy

- **Win32-style core on a portable layer:** the core calls WinPort, which emulates the Win32 API. Only the main executable links WinPort statically. → [Containers view](views/containers.md), [HACKING.md](../../HACKING.md)
- **The host is separate from the program:** a backend (TTY, wx or SDL) turns the console model into a screen and into input. The backend is picked at start.
- **Features beyond the core are plugins:** shared libraries that use the FAR plugin API and load at run time.
- **Build out of tree with CMake;** the build, test and delivery steps are in [how-we-build.md](../how-we-build.md).
- **Docs live in `docs/`** ([ADR-001](decisions/adr-001-docs-structure.md)).

## Building blocks

| Block | Owns | Detail |
|---|---|---|
| far2l core, `far2l/src` | Panels, viewer, editor, dialogs, command line, plugin host | [Containers view](views/containers.md) |
| WinPort | Win32 API emulation and the console model | [Containers view](views/containers.md) |
| Backends | Screen and input for TTY, wx and SDL hosts | [Containers view](views/containers.md) |
| Plugins, `plugins/` | Archives, remote panels, editor and panel tools | [Containers view](views/containers.md) |
| `utils`, `far2l/bootstrap`, `themes`, `testing` | Helpers, build-time generators and scripts, color schemes, smoke tests | [Containers view](views/containers.md) |
| Outside world | Users, hosts, operating system, remote systems, helper programs | [Context view](views/context.md) |

## Runtime and data

The runtime and data views of the whole app do not exist yet. The start sequence is in the [Containers view](views/containers.md). Feature flows are in the feature folders, for example [Markdown runtime](features/markdown/markdown.runtime.md) and [Markdown data](features/markdown/markdown.data.md).

## Crosscutting concepts

- **Text and console rules:** wide characters and 24 bit colors follow [HACKING.md](../../HACKING.md). → [invariants](invariants.md)
- **Code style:** [CODESTYLE.md](../../CODESTYLE.md).
- **Tests:** smoke tests drive far2l in a pseudo terminal. Features add their own scripts. → [how-we-build.md](../how-we-build.md), [testing/README.md](../../testing/README.md)

## Risks and open questions

- **CI does not run on the fork's branches.** A push to `next/*` or `markdown-preview/*` starts nothing. A PR into a story branch starts nothing. The fork has Actions enabled and 0 workflow runs (`gh api`, checked 2026-11-06). → [how-we-build.md](../how-we-build.md)
- **No `ctest` at the build root.** The root has no `enable_testing`, so `ctest` there finds no test. → [how-we-build.md](../how-we-build.md)
- **Two smoke tests fail on this Mac:** `0004-vt-shell` and `000b-find-file`. The cause and the state on `master` are unknown. → [how-we-build.md](../how-we-build.md)
- **Feature risks** are in the feature folders. → [Markdown overview](features/markdown/markdown.overview.md)
