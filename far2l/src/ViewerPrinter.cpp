#include "headers.hpp"
#include <algorithm>
#include "ViewerPrinter.hpp"
#include "interf.hpp"
#include "colors.hpp"
#include "farcolors.hpp"

static const wchar_t s_many_spaces[] = L"                                                                ";

void ViewerPrinter::PrintSpaces(size_t cnt)
{
	while (cnt) {
		size_t piece = std::min(cnt, sizeof(s_many_spaces)/sizeof(s_many_spaces[0]) - 1);
		Text(s_many_spaces, piece);
		cnt-= piece;
	}
}

void ViewerPrinter::EnableBOMSkip()
{
	_bom_skip = true;
}

void ViewerPrinter::SetSelection(bool selection)
{
	_selection = selection;
}

//

PlainViewerPrinter::PlainViewerPrinter(uint64_t color)
	: _color(color)
{
}

PlainViewerPrinter::~PlainViewerPrinter()
{
}

int PlainViewerPrinter::Length(const wchar_t *str, int limit)
{
	int out;
	bool joining = false;
	for (out = 0; *str && limit != 0; ++str, --limit) {
		if (static_cast<uint32_t>(*str) <= 0x7f) {
			if (!joining) ++out;
			joining = false;
		} else if (!ShouldSkip(*str)) {
			if (*str == CharClasses::ZERO_WIDTH_JOINER) {
				joining = true;
			} else if (CharClasses::IsFullWidth(str)) {
				if (!joining) out+= 2;
				joining = false;
			} else if (!CharClasses::IsXxxfix(*str)) {
				if (!joining) ++out;
				joining = false;
			}
		}
	}
	return out;
}

void PlainViewerPrinter::Print(int skip_len, int print_len, const wchar_t *str)
{
	SetColor(_selection ? FarColorToReal(COL_VIEWERSELECTEDTEXT) : _color);

	bool joining = false;
	for(; skip_len > 0 && *str; ++str) {
		if (!ShouldSkip(*str)) {
			if (*str == CharClasses::ZERO_WIDTH_JOINER) {
				joining = true;
			} else if (CharClasses::IsFullWidth(str)) {
				if (!joining) skip_len-= 2;
				joining = false;
			} else if (!CharClasses::IsXxxfix(*str)) {
				if (!joining) skip_len--;
				joining = false;
			}
		}
	}

	while (print_len > 0) {
		size_t piece = 0;
		while (str[piece] && !ShouldSkip(str[piece])) {
			++piece;
		}
		if (piece) {
			size_t cells = print_len;
			piece = StrSizeOfCells(str, piece, cells, false);
			if (piece) {
				Text(str, piece);
				print_len-= (int)cells;
			}
		}
		if (!str[piece]) {
			break;
		}
		str+= piece + 1;
	}

	if (print_len > 0) {
		PrintSpaces(print_len);
	}
}

// ---------------------------------------------------------------------------

namespace {

// The part of a palette entry that sets the background: its index nibble, the flag and the 24-bit color.
constexpr uint64_t kBackBits = 0xf0 | BACKGROUND_TRUECOLOR | (uint64_t(0xffffff) << 40);

// The color of a markdown run, from its palette role.  Read when the row is drawn, so a theme change
// shows on the next redraw.  The foreground comes from the role's entry; the background is always the
// viewer text background.
uint64_t MarkdownRunColor(uint64_t role)
{
	const uint64_t body = FarColorToReal(COL_VIEWERTEXT);
	uint64_t entry = body;
	switch (role) {
		case MD_ROLE_H1:
		case MD_ROLE_H2:
		case MD_ROLE_H3:
		case MD_ROLE_STRONG:
		case MD_ROLE_LINK: entry = FarColorToReal(COL_HELPTOPIC); break;
		case MD_ROLE_CODE: entry = FarColorToReal(COL_HELPHIGHLIGHTTEXT); break;
		case MD_ROLE_ART: entry = FarColorToReal(COL_PANELHIGHLIGHTTEXT); break;
		case MD_ROLE_RULE: entry = FarColorToReal(COL_VIEWERTEXT); break;
		default: return body;
	}
	return (entry & ~kBackBits) | (body & kBackBits);
}

}

MarkdownPrinter::MarkdownPrinter(uint64_t sel_color, const std::vector<MdLine> *lines)
	: _sel_color(sel_color), _lines(lines)
{
}

MarkdownPrinter::~MarkdownPrinter()
{
}

int MarkdownPrinter::Length(const wchar_t *str, int limit)
{
	int out;
	bool joining = false;
	for (out = 0; *str && limit != 0; ++str, --limit) {
		if (static_cast<uint32_t>(*str) <= 0x7f) {
			if (!joining) ++out;
			joining = false;
		} else if (!ShouldSkip(*str)) {
			if (*str == CharClasses::ZERO_WIDTH_JOINER) {
				joining = true;
			} else if (CharClasses::IsFullWidth(str)) {
				if (!joining) out += 2;
				joining = false;
			} else if (!CharClasses::IsXxxfix(*str)) {
				if (!joining) ++out;
				joining = false;
			}
		}
	}
	return out;
}

void MarkdownPrinter::Print(int skip_len, int print_len, const wchar_t * /*str*/)
{
	const MdLine *line = (_row < _lines->size()) ? &(*_lines)[_row] : nullptr;
	++_row;

	if (line) {
		for (const MdRun &run : line->runs) {
			SetColor(_selection ? _sel_color : MarkdownRunColor(run.role));

			const wchar_t *p = run.text.c_str();
			bool joining = false;

			// advance past the horizontal scroll offset (in cells)
			while (skip_len > 0 && *p) {
				if (static_cast<uint32_t>(*p) <= 0x7f) { if (!joining) skip_len--; joining = false; }
				else if (!ShouldSkip(*p)) {
					if (*p == CharClasses::ZERO_WIDTH_JOINER) joining = true;
					else if (CharClasses::IsFullWidth(p)) { if (!joining) skip_len -= 2; joining = false; }
					else if (!CharClasses::IsXxxfix(*p)) { if (!joining) skip_len--; joining = false; }
				}
				++p;
			}

			while (print_len > 0 && *p) {
				size_t piece = 0;
				while (p[piece] && !ShouldSkip(p[piece])) ++piece;
				if (piece) {
					size_t cells = print_len;
					piece = StrSizeOfCells(p, piece, cells, false);
					if (piece) {
						Text(p, piece);
						print_len -= (int)cells;
					}
				}
				if (!p[piece]) break;
				p += piece + 1;
			}
		}
	}

	// Blank cells (past the line end, or an empty line) take the viewer text color,
	// not whatever color the last run or the arrows left behind.
	if (print_len > 0) {
		SetColor(_selection ? _sel_color : FarColorToReal(COL_VIEWERTEXT));
		PrintSpaces(print_len);
	}
}
