# Manual checks: markdown palette

These two checks need a real terminal. The smoke test cannot judge how a color looks.
Run the build from the worktree root.

## 1. macOS Terminal (no 256-color support)

macOS Terminal reports fewer colors than 256. The RGB path must not run there.

1. Start far2l in macOS Terminal.
2. Press F3 on a sample `.md` file with every role in it.
3. Look at the 16-color fallback of each role:
   - H1 yellow, H2 light red, H3 light gray.
   - Strong white, link light green, code light magenta.
   - Quote and rule cyan.
4. Check that each role stays readable on the viewer background.
   Look for a role that is too dim or washed out.
5. Note what the terminal does with the `38;2` sequences.
   They can be ignored, mapped to a wrong color, or printed as garbage.
6. Record the color you saw for each role and any unreadable role.
7. Record the `38;2` behavior in one sentence.

Result:

## 2. Ghostty, one dark theme and one light theme

The owner decides the look here. The automated checks only prove the values.

1. Open the same sample `.md` in Ghostty with a dark theme.
2. Check that the RGB defaults read well on the dark background.
3. Check H2 orange (`#FFAF5F`), link green (`#87FF87`) and code magenta (`#FF87FF`).
   Look at a full code fence, not only one token.
4. Check that no role fights the theme or the cursor line.
5. Switch to a light theme and open the file again.
   The contrast rule should send every role to today's look, with no RGB.
6. Record the dark-theme look, the fence look, and the light-theme result.
7. State whether the owner accepts the look or wants a value changed.

Result:
