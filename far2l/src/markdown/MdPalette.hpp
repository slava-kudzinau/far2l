#pragma once

#include <cstdint>

// Resolves a markdown run's palette role (MdRole, see MarkdownRender.hpp) to a real color at draw
// time.  Resolve order: a user override from settings/markdown-colors.ini, then today's look, then
// the RGB defaults under the contrast rule, then the 4-bit fallback.  The returned color always
// carries the viewer text background.
uint64_t MdRunColor(uint64_t role);
