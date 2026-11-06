---
description: Proof of concept that drew a mermaid fence as a PNG from headless Chrome in the F3 Markdown view, what it measured, and why nothing shipped.
refs:
  - docs/architecture/decisions/adr-006-ascii-diagrams.md
---

# Mermaid as a picture in the Markdown view

Experiment, 2026-11-05. Branch `markdown-preview/image-poc`, worktree `.worktrees/mermaid-image-poc`. Not merged. The story record of the PoC is only in that branch (`stories/mermaid-image-poc/`).

## Question

Can a mermaid fence in Markdown mode show as a picture? The picture goes through the console image layer (`SetConsoleImage`), and headless Chrome makes the PNG. The viewer must fall back to the text path when images are not available. Scope: macOS, and a terminal that shows images (ghostty) or the far2l GUI.

## Method

- `far2l/bootstrap/mermaid-png.sh` writes a page that loads `mermaid.min.js`. The plan has headless Chrome run twice: once to read the SVG size, and once to take the screenshot. The PNG is cropped to the diagram.
- `MermaidImage` (`far2l/src/markdown/MermaidImage.cpp`) runs the script with a timeout and passes the PNG bytes to `SetConsoleImage` over blank placeholder rows. The feature was off unless `FAR2L_MD_IMAGES=1`.
- A final review drove the build in a Python pty that acts as a kitty-capable terminal. The review output is in the PoC workspace (`.build/final-review.md`, ignored by git). The author did not run ghostty.

## Results

| Measure | Value | Source |
|---|---|---|
| Headless Chrome, flowchart of the `situations.md` handbook file | about 1.8 s | PoC story text |
| `mermaid-png.sh` on `flow.mmd` | exit 0 in 1.56 s, 1292 x 296 px, cropped | PoC final review |
| Invalid diagram | exit 1, no PNG left | PoC final review |
| Picture sent to the console | 13,499 bytes PNG, 8 placeholder rows | PoC final review |

The review found no Critical issue and three Important issues:

- Up and Down keys deleted the pictures until a full redraw.
- Pictures stayed on the terminal during Ctrl+O and under dialogs.
- The picture was stretched to the full viewer width.

A fix round was started and stopped. The last commit is `WIP: image PoC fix round, stopped when the image approach was dropped`.

## Decision it informed

The terminal view draws box art with a bundle instead ([ADR-006](../architecture/decisions/adr-006-ascii-diagrams.md)). The record does not say why the owner dropped the picture approach. The PoC shows it works and needs Chrome, a terminal with image support, and a UI wait while Chrome runs.

## Not measured

- The wait seen by a user in ghostty. The PoC blocked the UI for the Chrome run time.
- Any run in a real image terminal.
- The cost of the PoC. The record holds no `cost.jsonl` for it.
