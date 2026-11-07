---
description: Measured times of the tree scan and the page build for story markdown-browser-tree-panel
status: active
---

# Measure

Root: 500 files of 8000 bytes (100 lines of 79 characters) in 20 folders, `root=repo`, opened file `d01/f01.md`. Machine: Apple M5 Max, macOS, `LC_ALL=C`. Real time from `/usr/bin/time -p`, best of three runs.

scan time: 0.11 s (`md-open.sh --list-tree`, 501 output lines)
awk pass: 0.21 s (one `awk` pass, stub of T2: reads the 500 files with `getline` and writes one `script` element for each, with the BOM, CR, `\`, `</` and `<!--` escapes; 4039500 bytes out)

Panel, T4. Same root, same run settings. The page has the panel and the inline text of the 500 files.

page bytes: 4305574
build time: 0.29 s (`md-open.sh --out`, no tree cache; best of three)
reuse time: 0.29 s (the same command with the tree cache; best of three)
dump time: 0.70 s (one `--dump-dom` in headless Chrome, page load included; best of three)
date: 2026-11-07
