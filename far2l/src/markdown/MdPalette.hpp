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
