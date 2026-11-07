---
description: The Colorer scheme look is a resolver step for Markdown prose, fetched by region name with a generation counter.
status: accepted
date: 2026-11-07
---

# ADR-009: The Colorer look as a resolver step

## Context

- F3 Markdown prose used the fork table. F4 with Colorer showed the scheme: heading and link in `#8888ff` with underline, strong in `#40a000`, and so on. The owner asked for one look in both views: "let's be consistent with colorer for everything".
- ADR-008 fixed the shape: the core reaches Colorer through C exports and `dlopen`, not a link. ADR-004 fixed the run: a run holds a role, not a color.

## Decision

The Colorer scheme look becomes a resolver step between the user file and the fork table.

- The plugin exports `ColorerRegionLookV1(int abi, const wchar_t* const* names, int count, unsigned int* look, int capacity, unsigned int* generation)` (`plugins/colorer/src/pcolorer2/MdExport.cpp`). For each region name it writes a foreground `0xRRGGBB` and the Colorer style word, resolved through `HrcLibrary::getRegion` and the climbing `getRegionDefine(const Region*)`. With `count` 0 it writes only `*generation`: the cheap probe. It never creates `FarEditorSet`.
- The core asks for ten names (`MdRegionNames` in `far2l/src/markdown/MdColorer.cpp`): `def:Text`, `markdown:Text`, and the eight prose regions of `markdown.hrc`. A style of bit 4 becomes `COMMON_LVB_UNDERSCORE` on the run.
- `MdPalette.cpp` caches the ten rows on the scheme generation. `FarEditorSet` carries a generation counter, incremented in `ReloadBase`, so a scheme change in the Colorer settings dialog refetches the look.
- The 3:1 contrast rule decides for the whole batch, all or none, keyed on the pair (viewer background, scheme generation). An absent region is not a contrast failure: that role takes its fork default.

## Consequences

- F3 prose and F4 show the same colors and underline on the same scheme; a scheme change moves both.
- The fetch is one dlopen-time binding, a probe per draw, and one ten-name fetch per
  generation. The steady-state cost is about 1 microsecond, measured in story
  markdown-colorer-look.
- The underline needs a per-row re-apply after `Console::Text` (see the invariants file).
- Fenced code keeps the four token roles of ADR-008. A later story (markdown-colorer-fences) returns the real region per token, so the flat code color goes away.
- A scheme whose colors fall below 3:1 against the theme sends all prose roles to the fork table at once.
