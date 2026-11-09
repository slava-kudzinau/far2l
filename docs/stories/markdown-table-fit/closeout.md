# Close-out draft: markdown-table-fit

Commits: none. Check: all-target build, `sh testing/markdown/ascii-golden.sh`, `sh testing/markdown/ascii-driver.sh`, and smoke tests `0007-view-file-markdown`, `0008-view-md-default`, `0009-view-md-setting-off`, and `000f-view-md-wrap`. Exit 0. All checks passed after compact-table sizing was added.

## Rulings

1. Keep tables as rows, cells, and styled runs until layout. Measure split limits and padding in terminal cells, including full-width glyphs.
2. Keep original columns. Use natural widths when they fit. Otherwise wrap at whitespace and split long words when required. Repeat vertical borders and padding.
3. Keep the story in-progress as requested. The final reviewer approved the display-cell implementation. A follow-up reviewer approved natural-width sizing for compact tables on 2026-11-09.

## Parked findings

None.

## Lessons

The smoke fixture must retain independent coverage for whitespace breaks and unbroken long words. Character count does not match terminal-cell width for wide glyphs.

## Review yield

| Role | Model | Reviews | Critical | Important | Minor |
|---|---|---:|---:|---:|---:|
| reviewer | configured final reviewer | 2 | 0 | 4 | 0 |
| reviewer | configured follow-up reviewer | 1 | 0 | 0 | 0 |
