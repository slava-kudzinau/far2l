# Spec: Markdown table fitting

## `MarkdownDoc::Render` table layout

| Input | Result |
|---|---|
| A table whose natural width fits within the viewer width | Use natural column widths and preserve borders, padding, separator row, roles, and source offsets. |
| A table whose natural width exceeds the viewer width | Allocate every column within the available table area. Render all cell content in bounded rows. |
| Cell text longer than its allocated width | Break at whitespace when possible. Split a word when no break fits. |
| A cell that occupies multiple visual rows | Repeat the complete vertical border structure and cell padding on every continuation row. |
| Header row | Render the complete header before the separator row. Use the strong role for header text and the rule role for borders. |
| Empty, missing, or short cells | Keep column boundaries aligned and emit no trailing spaces. |
| Inline code or link content in a wrapped cell | Preserve the code or link role on every resulting run. |
| Resize, F2, or Shift-F2 | Re-render with the new width and preserve rendered-line scrolling and non-decreasing source offsets. |

## Validation

| Input | Failure |
|---|---|
| A table with fewer cells than the widest row | No failure. Missing cells render as empty cells with aligned boundaries. |
| A single word longer than one column | No failure. The word splits across bounded rows. |
| A table with empty cells and multiple styled runs | No failure. Runs stay in their cells and columns do not shift. |

## Done

`BUILD=../far2l.build; APP=$(cd "$BUILD" && pwd)/install/far2l.app/Contents/MacOS/far2l; TESTS=${TESTS:-0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off}; cmake --build "$BUILD" --target far2l -j8 && sh testing/markdown/ascii-golden.sh && sh testing/markdown/ascii-driver.sh && (cd testing && for t in $(echo $TESTS); do pkill -9 -f -- '--test=/tmp/far2l' || true; find tests -maxdepth 1 -type s -delete; o=$(./far2l-smoke-run.sh "$APP" "$t" 2>&1) && echo "$o" | grep -q '\\[DONE\\]' && ! echo "$o" | grep -q FAILED || { echo "FAILED: $t"; exit 1; }; done) && echo 'check ok'` exits 0, with the table-fit fixture and existing Markdown tests passing.

## Out of scope

This story does not change prose, quote, list, fence, mermaid, or browser-view layout. It does not add table alignment, truncation, ellipsis, selection, parser changes, or horizontal table scrolling exceptions.
