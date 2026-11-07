#pragma once

#include <cstdint>
#include "MarkdownRender.hpp"

// Resolves a markdown run's palette role (MdRole, see MarkdownRender.hpp) to a real color at draw
// time.  Resolve order: a user override from settings/markdown-colors.ini, then today's look, then
// the RGB defaults under the contrast rule, then the 4-bit fallback.  The returned color always
// carries the viewer text background.
uint64_t MdRunColor(uint64_t role);

// The code block's background (story markdown-code-tint): one eighth of the code role's
// resolved foreground mixed into the viewer background (the BlendRgb pattern of
// far2l/src/filediff.cpp, alpha 32/256).  `viewer_bg` is a far attribute: its truecolor
// bits, else the palette entry of its index nibble, are the base.  Returns the packed
// 0xBBGGRR, or -1 for none: true color gates the tint, and the pair (code foreground,
// tint) must reach 3:1 (wcagComputeContrast).  One verdict, cached on the pair (viewer
// background, code color): a theme change and a code-color change each recompute it.
int64_t MdCodeTint(uint64_t viewer_bg);

// The palette role of a Colorer token class (MdToken::cls, spec.md: 1 keyword,
// 2 string, 3 comment, 4 number).  Any other class keeps the plain code role.
// The 256-color look of a role: the RGB of the spec's 256 index, as the far attribute holds
// it (0xBBGGRR in the truecolor bits).  The `print_norgb` driver case checks the token roles
// against it; the 000c smoke run paints it under --norgb (R52).
uint64_t MdRoleColor256(uint64_t role);
inline MdRole MdRoleForTokenClass(uint8_t cls)
{
	switch (cls) {
		case 1: return MD_ROLE_TOKEN_KEYWORD;
		case 2: return MD_ROLE_TOKEN_STRING;
		case 3: return MD_ROLE_TOKEN_COMMENT;
		case 4: return MD_ROLE_TOKEN_NUMBER;
		default: return MD_ROLE_CODE;
	}
}
