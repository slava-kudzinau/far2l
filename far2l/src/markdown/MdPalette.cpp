#include "MdPalette.hpp"

#include "MarkdownRender.hpp"

#include "headers.hpp"   // WinPort: WINPORT(GetConsoleColorPalette)
#include "farcolors.hpp" // FarColorToReal, extractColor
#include "farcolorexp.h" // ExprToFarColor
#include "KeyFileHelper.h"
#include "utils.h"       // InMyConfig
#include "Colorspace.h"  // wcagComputeContrast
#include "MdColorer.hpp" // the Colorer look bridge (story markdown-colorer-look)

#include <array>
#include <ctime>
#include <mutex>
#include <string>

// ---------------------------------------------------------------------------
// Role -> color resolution at draw time.  Resolve order (spec): a user override from
// settings/markdown-colors.ini, then today's look, then the RGB defaults under the contrast
// rule, then the 4-bit fallback.  The returned color always carries the viewer text background.
// The @Name overrides resolve through FarColorToReal at call time, so a theme change shows on
// the next redraw.  Hostile ini content (unknown keys, bad values, huge files) never colors a
// run: anything that fails validation falls back to that role's default.
// ---------------------------------------------------------------------------

namespace {

// A truecolor foreground of a color written as 0xRRGGBB.  A far attribute holds its 24-bit color
// as 0xBBGGRR (COMPOSE_RGB in VT256ColorTable.h), so the red and blue bytes change place here.
constexpr uint64_t TrueForeColor(uint32_t rgb)
{
	const uint64_t bgr = ((rgb & 0xff) << 16) | (rgb & 0xff00) | ((rgb >> 16) & 0xff);
	return (bgr << 16) | FOREGROUND_TRUECOLOR;
}

// The user ini: read-only, one key per role in the [markdown] section.
const char MD_COLORS_INI[] = "settings/markdown-colors.ini";
const char MD_COLORS_SECTION[] = "markdown";

// The part of a palette entry that sets the background: its index nibble, the flag and the
// 24-bit color.  Kept in step with the kBackBits copy in ViewerPrinter.cpp: the module
// pre-masks the resolved entry and the dispatch re-masks against the live viewer background.
constexpr uint64_t kBackBits = 0xf0 | BACKGROUND_TRUECOLOR | (uint64_t(0xffffff) << 40);

// The RGB defaults and 16-color fallbacks of the spec table (Roles), in MdRole order from
// MD_ROLE_H1 up.  body and art never take an RGB value and have no row here.
struct RoleDefault
{
	uint32_t rgb;        // the spec value, written as 0xRRGGBB
	uint32_t def256;     // the RGB of the spec's 256 index: what a terminal without RGB shows (R52)
	uint64_t fallback16; // the 4-bit foreground: the color without RGB, and the index under RGB
};

constexpr std::array<RoleDefault, MD_ROLE_COUNT - 1> kRoleDefaults {{
	{0xffff5f, 0xffff5f, F_YELLOW},          // h1 (256 index 227)
	{0xffaf5f, 0xffaf5f, F_LIGHTRED},        // h2, token number (256 index 215)
	{0x87afff, 0x87afff, F_LIGHTGRAY},       // h3, token keyword (256 index 111)
	{0xffffff, 0xffffff, F_WHITE},           // strong (256 index 231)
	{0x87ff87, 0x87ff87, F_LIGHTGREEN},      // link, token string (256 index 120)
	{0xff87ff, 0xd7d7af, F_LIGHTMAGENTA},    // code (256 index 213)
	{0xafafd7, 0xafafd7, F_CYAN},            // quote (256 index 147)
	{0x8787af, 0x8787af, F_CYAN},            // rule, token comment (256 index 103)
	{0, 0, 0},                               // art: never reads this row (special-cased above)
	{0x87afff, 0x87afff, F_LIGHTGRAY},       // token keyword (256 index 111)
	{0x87ff87, 0x87ff87, F_LIGHTGREEN},      // token string (256 index 120)
	{0x8787af, 0x8787af, F_CYAN},            // token comment (256 index 103)
	{0xffaf5f, 0xffaf5f, F_LIGHTRED},        // token number (256 index 215)
	{0xd7d7af, 0xd7d7af, F_LIGHTMAGENTA},    // emphasis (256 index 187, spec: 0xd7d7af)
	{0x8787af, 0x8787af, F_CYAN},            // listmark (256 index 103, spec: 0x8787af)
}};

// The user file key of each role, in MdRole order.
const char *const kRoleKeys[MD_ROLE_COUNT] = {
	"body", "heading1", "heading2", "heading3", "strong", "link", "code", "quote", "rule", "art",
	"token-keyword", "token-string", "token-comment", "token-number",
	"emphasis", "listmark"
};

// The far palette entries an @Name may name; ColorsInit is private to farcolors.cpp, so the
// fork keeps its own list here.
struct PaletteName
{
	const char *name;
	unsigned int entry;
};

constexpr PaletteName kPaletteNames[] = {
	{"Viewer.Text",          COL_VIEWERTEXT},
	{"Help.Topic",           COL_HELPTOPIC},
	{"Help.Text.Highlight",  COL_HELPHIGHLIGHTTEXT},
	{"Panel.Text.Highlight", COL_PANELHIGHLIGHTTEXT},
};

// Today's look: the palette entry each role falls back to when the RGB defaults fail the
// contrast rule (spec, Resolution order step 2).
unsigned int TodayLookEntry(uint64_t role)
{
	switch (role) {
		case MD_ROLE_H1:
		case MD_ROLE_H2:
		case MD_ROLE_H3:
		case MD_ROLE_STRONG:
		case MD_ROLE_LINK: return COL_HELPTOPIC;
		case MD_ROLE_CODE: return COL_HELPHIGHLIGHTTEXT;
		case MD_ROLE_QUOTE:
		case MD_ROLE_RULE: return COL_VIEWERTEXT;
		case MD_ROLE_TOKEN_KEYWORD:
		case MD_ROLE_TOKEN_STRING:
		case MD_ROLE_TOKEN_COMMENT:
		case MD_ROLE_TOKEN_NUMBER: return COL_HELPHIGHLIGHTTEXT;   // spec: the code entry
		case MD_ROLE_EM:
		case MD_ROLE_LISTMARK: return COL_VIEWERTEXT;   // spec, New roles: today's look
	}
}

// Exactly three or six hex digits; expands `#rgb` the way the far expressions do, by doubling
// each digit.  Returns the value or -1; `consumed` receives the digits consumed.
long long ParseHex(const char *s, size_t len, size_t &consumed)
{
	consumed = 0;

	unsigned long long value = 0;
	while (consumed < len && consumed < 6) {
		const char c = s[consumed];
		unsigned int digit;
		if (c >= '0' && c <= '9') digit = c - '0';
		else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
		else break;
		value = (value << 4) | digit;
		++consumed;
	}

	// Only the two spec forms pass; `#f`, `#ff`, `#ffff` and five digits match none.
	if (consumed != 3 && consumed != 6)
		return -1;

	if (consumed == 3) {
		const unsigned long long expanded = value;
		for (size_t k = 0; k < consumed; ++k)
			value = (value << 4) | ((expanded >> (4 * (consumed - 1 - k))) & 0xf);
	}

	return (long long)value;
}

// One user override, parsed once per file load.  `NAME` keeps the palette entry unresolved so
// `MdRunColor` can call `FarColorToReal` on every draw: a theme change then shows on the next
// redraw.  `NONE` means the key is unset or bad: the role keeps its default.
struct RoleOverride
{
	enum Kind : uint8_t { NONE, LITERAL, NAME };

	Kind kind = NONE;
	uint64_t color = 0;      // LITERAL: the real color of the value
	unsigned int entry = 0;  // NAME: the palette entry to resolve at draw time
};

// The override value grammar, checked exactly: `foreground:<value>` or `<value>`, where
// `<value>` is `#rrggbb`, `#rgb`, an `F_*` color name, or `@Name` with the names above.  No
// quoted values, no other forms.  A bad value returns NONE, so the role keeps its default.
RoleOverride ParseOverrideValue(const std::string &value)
{
	RoleOverride out;

	std::string body = value;
	const size_t colon = body.find(':');
	if (colon != std::string::npos) {
		if (body.compare(0, colon, "foreground") != 0)
			return out;
		body.erase(0, colon + 1);
	}

	if (body.empty() || body.size() > 32)
		return out;

	if (body[0] == '@') {
		for (const PaletteName &named : kPaletteNames) {
			if (body.compare(1, std::string::npos, named.name) == 0) {
				out.kind = RoleOverride::NAME;
				out.entry = named.entry;
				return out;
			}
		}
		return out;
	}

	if (body[0] == '#') {
		size_t consumed = 0;
		const long long rgb = ParseHex(body.c_str() + 1, body.size() - 1, consumed);
		if (rgb < 0 || consumed != body.size() - 1)
			return out;
		out.kind = RoleOverride::LITERAL;
		out.color = TrueForeColor((uint32_t)rgb);
		return out;
	}

	// An F_* color name: the 16 names the color expressions accept.
	static const char *const kFNames[16] = {"F_BLACK", "F_BLUE", "F_GREEN", "F_CYAN",
		"F_RED", "F_MAGENTA", "F_BROWN", "F_LIGHTGRAY", "F_DARKGRAY", "F_LIGHTBLUE",
		"F_LIGHTGREEN", "F_LIGHTCYAN", "F_LIGHTRED", "F_LIGHTMAGENTA", "F_YELLOW", "F_WHITE"};
	for (unsigned int i = 0; i < 16; ++i) {
		if (body == kFNames[i]) {
			out.kind = RoleOverride::LITERAL;
			out.color = (uint64_t)i; // the 4-bit foreground layout of a palette entry
			return out;
		}
	}

	return out;
}

// The parsed ini of one draw-time batch, cached against the file's mtime.
struct PaletteOverrides
{
	time_t mtime = 0;
	bool no_file = false;
	RoleOverride roles[MD_ROLE_COUNT];
};

// Reads and validates the whole [markdown] section.
PaletteOverrides LoadOverrides()
{
	PaletteOverrides out;

	const std::string path = InMyConfig(MD_COLORS_INI);
	struct stat st {};
	if (stat(path.c_str(), &st) == -1) {
		out.no_file = true;
		return out;
	}

	// The mtime is recorded even when the load fails, so a corrupt or unreadable file does not
	// make every draw-time call re-parse it.
	out.mtime = st.st_mtime;

	KeyFileReadHelper kfh(path, MD_COLORS_SECTION);
	if (!kfh.IsLoaded()) {
		// The file vanished between the stat and the read: no file, all defaults.
		out.no_file = true;
		return out;
	}

	const KeyFileValues *section = kfh.GetSectionValues(MD_COLORS_SECTION);
	if (!section)
		return out; // no [markdown] section: all defaults, cached against the mtime

	for (size_t role = 0; role < MD_ROLE_COUNT; ++role) {
		if (!section->HasKey(kRoleKeys[role]))
			continue;
		out.roles[role] = ParseOverrideValue(section->GetString(kRoleKeys[role]));
	}

	return out;
}

// The cached parse of the user file.  Re-read when the mtime changed; a stat error counts as
// "no file" and clears the cache, so an edited-then-removed file never keeps stale colors.
const PaletteOverrides &ReadOverrides()
{
	static std::mutex s_mutex;
	static PaletteOverrides s_cached;

	std::lock_guard<std::mutex> lock(s_mutex);

	struct stat st {};
	if (stat(InMyConfig(MD_COLORS_INI).c_str(), &st) == -1) {
		if (!s_cached.no_file)
			s_cached = PaletteOverrides();
		s_cached.no_file = true;
		return s_cached;
	}

	if (s_cached.no_file || s_cached.mtime != st.st_mtime)
		s_cached = LoadOverrides();

	return s_cached;
}

// The RGB default of a role as a truecolor real color, with the 16-color index in the low bits,
// so a terminal that ignores RGB shows the index.
uint64_t RgbRealColor(const RoleDefault &def)
{
	return TrueForeColor(def.rgb) | def.fallback16;
}

// The verdict of the contrast rule for one batch: all or none at 3.0.  All eight RGB defaults
// share the same viewer background, so the whole set either passes or drops to today's look;
// a mid-luminance background must never show a mix of RGB defaults and today's look.
bool RgbDefaultsPass()
{
	RGB bg, fg;
	extractColor(FarColorToReal(COL_VIEWERTEXT), fg, bg);
	for (size_t i = 0; i < kRoleDefaults.size(); ++i) {
		if (i == MD_ROLE_ART - 1)
			continue;   // art has no RGB default row (the placeholder is not a color)
		RGB def_fg, def_bg;
		extractColor(RgbRealColor(kRoleDefaults[i]), def_fg, def_bg);
		if (wcagComputeContrast(def_fg, bg) < 3.0)
			return false;
	}
	return true;
}

// The contrast verdict, cached against the viewer background it was computed for: the verdict
// depends on COL_VIEWERTEXT, so it is recomputed when the background's real color differs from
// the cached one — a theme change re-evaluates the rule instead of keeping a stale verdict.
bool RgbDefaultsAllowed()
{
	static std::mutex s_mutex;
	static uint64_t s_bg_real = 0;
	static bool s_verdict = false;
	static bool s_computed = false;

	const uint64_t bg_real = FarColorToReal(COL_VIEWERTEXT);

	std::lock_guard<std::mutex> lock(s_mutex);
	if (!s_computed || s_bg_real != bg_real) {
		s_verdict = RgbDefaultsPass();
		s_bg_real = bg_real;
		s_computed = true;
	}
	return s_verdict;
}

// The Colorer look cache (spec, Cache): the ten rows under one scheme
// generation, fetched once per generation.  The 3:1 verdict is cached on the
// pair (viewer background, generation): a theme change and a scheme change
// both re-evaluate it.
struct LookCache
{
	unsigned long long generation = 0;
	MdRegionLook rows[10] = {};
	bool have[10] = {};
	bool valid = false;
};

LookCache &LookRows(unsigned long long gen, MdLookSource &src)
{
	static std::mutex s_mutex;
	static LookCache s_cache;

	std::lock_guard<std::mutex> lock(s_mutex);
	if (!s_cache.valid || s_cache.generation != gen) {
		int count = 0;
		const char *const *names = MdRegionNames(&count);
		const unsigned long long got = src.Fetch(names, count, s_cache.rows);
		s_cache.valid = (got != 0);
		s_cache.generation = got;
		for (int i = 0; i < count; ++i) {
			s_cache.have[i] = (s_cache.rows[i].fore != 0);
		}
	}
	return s_cache;
}

// The 3:1 verdict for the whole scheme batch, all or none, against the real
// viewer background: one row below 3.0 sends every role to the fork table.
bool LookAllowed(unsigned long long gen, const LookCache &cache)
{
	static std::mutex s_mutex;
	static uint64_t s_bg_real = 0;
	static unsigned long long s_gen = 0;
	static bool s_verdict = false;
	static bool s_computed = false;

	const uint64_t bg_real = FarColorToReal(COL_VIEWERTEXT);

	std::lock_guard<std::mutex> lock(s_mutex);
	if (!s_computed || s_bg_real != bg_real || s_gen != gen) {
		RGB row_fg, row_bg, bg;
		extractColor(bg_real, bg, bg);
		s_verdict = true;
		for (int i = 0; i < 10; ++i) {
			if (!cache.have[i]) {
				continue; // a missing region takes the fork default (spec, Fallback)
			}
			extractColor(TrueForeColor(cache.rows[i].fore), row_fg, row_bg);
			if (wcagComputeContrast(row_fg, bg) < 3.0) {
				s_verdict = false;
				break;
			}
		}
		s_bg_real = bg_real;
		s_gen = gen;
		s_computed = true;
	}
	return s_verdict;
}

// The index of a role's region in the ten-name table (spec, Region map);
// -1 for roles the Colorer look does not paint (art, the fence-token roles).
int RoleRegionIndex(uint64_t role)
{
	switch (role) {
		case MD_ROLE_H1:
		case MD_ROLE_H2:
		case MD_ROLE_H3: return 1;
		case MD_ROLE_LINK: return 2;
		case MD_ROLE_STRONG: return 3;
		case MD_ROLE_EM: return 4;
		case MD_ROLE_CODE: return 5;
		case MD_ROLE_QUOTE: return 6;
		case MD_ROLE_LISTMARK: return 7;
		case MD_ROLE_RULE: return 8;
		default: return -1;
	}
}

} // namespace

uint64_t MdRunColor(uint64_t role)
{
	const uint64_t body = FarColorToReal(COL_VIEWERTEXT);

	// Out-of-range roles take the body look, not the body override.
	if (role >= MD_ROLE_COUNT)
		return body;

	// 1. The user file.  An override has no contrast check.  An @Name resolves here, on every
	// call, so a theme change shows on the next redraw.
	const PaletteOverrides &overrides = ReadOverrides();
	const RoleOverride &ov = overrides.roles[role];
	if (ov.kind == RoleOverride::LITERAL)
		return (ov.color & ~kBackBits) | (body & kBackBits);
	if (ov.kind == RoleOverride::NAME)
		return (FarColorToReal(ov.entry) & ~kBackBits) | (body & kBackBits);

	// 2. The Colorer look: the scheme's fore and the style word's underline bit,
	// all or none at 3.0, cached on (viewer background, generation).  Below 8
	// bits of color the step does not run: the look carries 24-bit fore only.
	if (WINPORT(GetConsoleColorPalette)(NULL) >= 8) {
		MdLookSource &src = MdGetLookSource();
		const unsigned long long gen = src.Generation();
		if (gen != 0) {
			LookCache &cache = LookRows(gen, src);
			if (cache.valid && LookAllowed(gen, cache)) {
				int use = RoleRegionIndex(role);
				if (role == MD_ROLE_BODY) {
					// R2: body takes markdown:Text when its fore is non-zero,
					// def:Text otherwise.
					use = (cache.rows[9].fore != 0) ? 9 : 0;
				}
				if (use >= 0 && cache.have[use]) {
					const MdRegionLook &row = cache.rows[use];
					uint64_t bits = TrueForeColor(row.fore);
					if (row.style & 4) {
						bits |= COMMON_LVB_UNDERSCORE; // 0x8000 (spec, Underline)
					}
					return (bits & ~kBackBits) | (body & kBackBits);
				}
			}
		}
	}

	// body never takes an RGB value: outside a user override it stays the viewer text entry.
	if (role == MD_ROLE_BODY)
		return body;

	// art never takes an RGB value: it stays today's look in every step.
	if (role == MD_ROLE_ART)
		return (FarColorToReal(COL_PANELHIGHLIGHTTEXT) & ~kBackBits) | (body & kBackBits);

	const RoleDefault &def = kRoleDefaults[role - 1];

	uint64_t entry = 0;

	// 3. The RGB defaults, under the contrast rule: all or none at 3.0 against the real viewer
	// background, one batch verdict for the whole table.
	if (WINPORT(GetConsoleColorPalette)(NULL) >= 8 && RgbDefaultsAllowed())
		entry = RgbRealColor(def);

	// 2/4. Today's look when the RGB path did not run or failed the rule; the 16-color fallback
	// when the terminal has no RGB at all.
	if (!entry) {
		if (WINPORT(GetConsoleColorPalette)(NULL) >= 8)
			entry = FarColorToReal(TodayLookEntry(role));
		else
			entry = RgbRealColor(RoleDefault{def.def256, 0, def.fallback16});   // the 256-color look of the spec table, with the 16-color index in the low bits (R52)
	}

	return (entry & ~kBackBits) | (body & kBackBits);
}

// The 256-color look of a role, in the attribute form of the truecolor bits; the low index
// of the 16-color path, for a terminal that ignores RGB.
uint64_t MdRoleColor256(uint64_t role)
{
	if (role >= MD_ROLE_COUNT)
		return 0;
	if (role == MD_ROLE_BODY)
		return 0;
	if (role == MD_ROLE_ART)
		return 0;
	const RoleDefault &def = kRoleDefaults[role - 1];
	return RgbRealColor(RoleDefault{def.def256, 0, def.fallback16});
}
