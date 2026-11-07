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

// The alias row of a Colorer token class (the V1 export's four classes): the
// fork table's 24-bit default and its 4-bit foreground - the pair the bridge
// puts into a range when the plugin has no ColorerTokenizeV2 (story
// markdown-colorer-fences).  An unknown class takes the plain code row.
void MdTokenClassLook(uint8_t cls, uint32_t *fg, uint32_t *fg4);

// The far attribute of a cached fence range (story markdown-colorer-fences):
// the scheme's 24-bit foreground under the look's batch verdict - the one
// view-wide verdict, keyed (viewer background, scheme generation), that sends
// the prose to the fork table when it fails (spec.md, Contrast); a failing
// verdict sends the range to the plain code role, the same verdict the prose
// path consults.  A 4-bit scheme answers the 4-bit path: the scheme's 4-bit
// row, no 3:1 check.  `underline` (the style word's bit 4) sets
// COMMON_LVB_UNDERSCORE; the printer's span pass re-applies it.  The returned
// color carries the viewer text background.
uint64_t MdRangeColor(uint32_t fg, uint32_t fg4, bool underline);
