---
description: The terminal Markdown view draws six mermaid types as box art with a committed JS bundle in the vendored quickjs-ng, and shows other types as code.
status: accepted
date: 2026-11-06
---

# ADR-006: Draw mermaid fences with the mermaid-ascii bundle

## Context

- The first design ran `mermaid.min.js` (a 5 MB library) in quickjs-ng. It took the SVG and drew cells with an in-house `SvgCanvas` (1,400 lines).
- It was slow (seconds), and the art looked poor. The first plan chose it because mermaid.js covered every diagram type, including swimlane.
- quickjs-ng 0.14 was vendored in `far2l/third_party/quickjs` for the first design. The record names no other JS engine.

## Decision

Bundle the ASCII part of the npm package `beautiful-mermaid` 1.1.3 (MIT) into `far2l/bootstrap/mermaid-ascii.js`, and run it in the vendored quickjs-ng through `AsciiMermaid::Render` (`far2l/src/markdown/AsciiMermaid.cpp`).

- A fence is drawn when its first word is `graph`, `flowchart`, `stateDiagram`, `stateDiagram-v2`, `sequenceDiagram`, `classDiagram`, `erDiagram` or `xychart-beta`. The art uses the art role of [ADR-004](adr-004-palette-roles.md).
- Any other type, or a failed render, shows the fence as a code block.
- The bundle is built with `far2l/bootstrap/mermaid-ascii/build.sh` (esbuild) and committed. The far2l build and run need no Node.
- Golden files in `testing/markdown/golden/` come from the package in Node, not from the bundle.

## Consequences

- `MermaidEngine`, `SvgCanvas` and the use of `mermaid.min.js` are gone from `far2l/src`. The 5.5 MB `far2l/bootstrap/mermaid.min.js` is still on disk but is ignored by git and not installed.
- A render has a 2 s deadline (`FAR2L_MD_DEADLINE_MS` overrides it), a 1 MB JS stack and a memory limit. A timeout frees the runtime, and the next render builds it again. The render blocks the UI thread for up to 2 s.
- Known limits (from the story close-out):
  - A 26-node `graph TD` fails at once.
  - A 26-node `graph LR` chain can time out.
  - A fence that starts with `%%` shows as a code block.
- The bundle has a pre-pass that spaces edge operators (`A-->B`), because package 1.1.3 draws that as one wrong box.
- One art color per diagram. Art wider than the screen is cut at the screen edge.

## Rejected alternatives

### Run `mermaid.min.js` and rasterize its SVG

**Why rejected:** slow, large, and poor art, as in Context. Commit `eb4b3c287` removed it. Revisit only if the terminal must draw swimlane and other types that the bundle does not draw.

### Port `beautiful-mermaid` to C++ now

**Why rejected:** running its JS was faster to ship. The golden files let a later port prove it is the same.

### Build the bundle during the far2l build

**Why rejected:** the far2l build must not need Node.

### Render diagrams to PNG with headless Chrome

**Why rejected:** a proof of concept drew a picture in a terminal that supports images, but the owner dropped the approach. The reason is not recorded. See [the image PoC note](../../research/mermaid-image-poc.md).
