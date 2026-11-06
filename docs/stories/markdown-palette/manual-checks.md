# Manual checks: markdown palette

Two things the automated checks cannot see. Run both on a real build from the worktree root.

## macOS Terminal (no 256 colors)

Open a sample `.md` in the viewer (F3) under macOS Terminal, which reports fewer than 256 colors. What to look at: each role shows its 16-color fallback from the spec table (h1 yellow, h2 light red, h3 light gray, strong white, link light green, code light magenta, quote and rule cyan) and stays readable against the viewer background — no unreadable-on-dark or washed-out role. Record what the `38;2` RGB sequences do there: Terminal does not honor them, so note whether they are emitted at all and whether they degrade to garbage, a wrong color, or are simply ignored.

## Ghostty (dark and light)

Open the same sample `.md` in Ghostty twice, once with a dark theme and once with a light theme. What to look at: the RGB defaults read well on both backgrounds — with a light background the all-or-none contrast rule should send everything to today's look, and on dark every role keeps its RGB color; also judge whether the H2 orange (`#FFAF5F`), the link green (`#87FF87`) and the code magenta (`#FF87FF`) suit a full code fence — the owner decides on the look here.
