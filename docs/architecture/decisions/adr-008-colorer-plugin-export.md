---
description: The viewer gets fence token classes from the Colorer plugin through a small C export, located by dlopen. No new link into the Colorer library.
status: accepted
date: 2026-11-07
---

# ADR-008: Colorer plugin export for fence tokens

## Context

- Fenced code blocks show in one plain code color. The owner asked for syntax colors: keywords, strings, comments and numbers get their own colors, and the user can set them in `settings/markdown-colors.ini`.
- The Colorer plugin already parses code with the HRC scheme catalog, and it is loaded by default. The viewer needs the token classes of a fence line, not the colors.
- The Colorer library sits behind libxml2. Linking the core against it adds a second heavy dependency to `far2l` itself.

## Decision

The core reaches Colorer through one C export and `dlfcn`, not a link. The core never links the Colorer library or libxml2.

- The plugin exports `ColorerTokenizeV1(lang, lines, count, emit, ctx, deadline_ms)`. It parses the lines with the shared `FarEditorSet` and emits one event per region: `(line, start, end, token_class)`. A token class is `Keyword`, `String`, `Comment` or `Number`, mapped from the region by the region parents. The export reads no colors.
- `MdColorer` in `far2l/src/markdown/` finds the loaded module by the exact path far2l passed to its own `dlopen` (the `PluginManager` answers with `GetModuleName`), then reopens it with `RTLD_NOLOAD` and resolves the export. The module stays the one the process already loaded: one catalog copy, no second parser.
- `MdTokenSource` (in `MdColorer.hpp`) is the interface the render uses: `Tokenize(lang, lines, budget_ms, out)` and `Ready()`. A test source replaces it through `MdSetTokenSourceForTest`, and `MdGetTokenSource` prefers it when one is set; the plugin source is the default.
- When no plugin is loaded, `Tokenize` returns false and the fence keeps the plain code role. The viewer never blocks on the plugin.

## Consequences

- No new third-party link in the core. `plugins/colorer/CMakeLists.txt` adds one source file.
- The first parse of a language pays the scheme file load (tens of ms). `Ready()` warms the spec's languages before the render budget starts.
- The four new roles (`keyword`, `string`, `comment`, `number`) have ini keys and RGB defaults in `MdPalette.cpp`. Cells past the last token of a group keep the code role.

## Rejected alternatives

### Link Colorer into the core

**Why rejected:** it adds the Colorer library and libxml2 to every far2l binary, even when no Markdown file is open.

### Open a hidden editor per fence

**Why rejected:** one editor per fence is heavy, and the editor's own draw path is not what the viewer needs.

### A helper process

**Why rejected:** one more process to start, feed and keep alive, for data the already-loaded plugin can answer in one call.

### HRC data with a new tokenizer

**Why rejected:** a second tokenizer over the catalog duplicates Colorer's parser for a smaller result.
