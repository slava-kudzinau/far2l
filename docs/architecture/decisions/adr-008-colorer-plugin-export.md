---
description: The core reaches the Colorer plugin through one C export and dlopen, not a link.
status: accepted
date: 2026-11-07
---

# ADR-008: The Colorer plugin export, not a link

## Context

- Fenced code blocks showed one flat color. The owner asked for keyword, string, comment and number colors, from the Colorer plugin syntax schemes.
- The Colorer plugin owns the HRC schemes and the catalog. The core does not link the Colorer library today.

## Decision

The core reaches the Colorer plugin through one C export and `dlopen`/`dlsym`.

- The plugin exports `ColorerTokenizeV1` (`plugins/colorer/src/pcolorer2/ColorerExport.cpp`): a language name, wide-character lines, and a deadline in microseconds. It emits cells: `start` and `end` (1-based, inclusive) and a `class` number. The return is 0, -1 (no plugin), -2 (unknown language), -3 (fatal parse).
- The core loads it in `MdColorer` (`far2l/src/markdown/MdColorer.cpp`): `dlopen` on the first fence, `dlsym` one time. No new link library.

The four token classes map to the roles keyword, string, comment and number (`MdRoleForTokenClass` in `far2l/src/markdown/MdPalette.cpp`). Every other class keeps the plain code role.

## Consequences

- No new link to the Colorer library and to libxml2: the core keeps its load-time shape.
- One catalog copy: the plugin ships the schemes, and the viewer reads them through the plugin.
- No hidden editor: the viewer does not build an editor for the fence.
- Fallback: with no plugin, every fence shows the plain code role.
- The export is a data contract (see [markdown data](../features/markdown/markdown.data.md)). A signature change needs a new version name.

## Rejected alternatives

### Link the Colorer library to the core

**Why rejected:** the core gains a second heavy link (the Colorer library and libxml2), and every package must carry both.

### A hidden editor

**Why rejected:** `FarEditor::init` builds a full editor, with menu and file read, for every file view.

### A helper process

**Why rejected:** one process per view, a startup cost, and a wide-character IPC protocol.

### HRC data with a new tokenizer

**Why rejected:** the core would parse the catalog and the scheme XML, and the result differs from the plugin colors.
