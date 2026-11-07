#pragma once

#include <cstdint>
#include "MarkdownRender.hpp"

// Resolves a markdown run's palette role (MdRole, see MarkdownRender.hpp) to a real color at draw
// time.  Resolve order: a user override from settings/markdown-colors.ini, then today's look, then
// the RGB defaults under the contrast rule, then the 4-bit fallback.  The returned color always
// carries the viewer text background.
uint64_t MdRunColor(uint64_t role);

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

// The far attribute of a cached fence range: the scheme's 24-bit foreground
// under a truecolor-capable backend, the scheme's 4-bit foreground on the
// 16-color path, over the viewer text background.  The underline bit is the
// printer's to apply.
uint64_t MdRangeColor(uint32_t fg, uint32_t fg4);
