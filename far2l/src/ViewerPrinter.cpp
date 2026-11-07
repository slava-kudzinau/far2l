#include "headers.hpp"
#include <algorithm>
#include "ViewerPrinter.hpp"
#include "interf.hpp"
#include "colors.hpp"
#include "farcolors.hpp"
#include "markdown/MdPalette.hpp"

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

// The color of a markdown run, from its palette role.  A small dispatch: the palette module
// resolves the role at draw time (so a theme change shows on the next redraw), this keeps the
// background bits of the viewer text entry.
uint64_t MarkdownRunColor(uint64_t role)
{
	const uint64_t body = FarColorToReal(COL_VIEWERTEXT);
	return (MdRunColor(role) & ~kBackBits) | (body & kBackBits);
}


// The row fill of a tinted row (story markdown-code-tint): the viewer text color with
// its background bits replaced by the palette's code tint (packed 0xBBGGRR).  The
// foreground and the index nibble stay the viewer's: a backend that reads the 24-bit
// background reads the truecolor bits, which SET_RGB_BACK sets.
uint64_t TintFill(uint64_t tint_bgr)
{
	uint64_t attr = FarColorToReal(COL_VIEWERTEXT);
	SET_RGB_BACK(attr, tint_bgr);
	return attr;
}
}

MarkdownPrinter::MarkdownPrinter(uint64_t sel_color, const std::vector<MdLine> *lines,
	const std::vector<MdTokenGroup> *groups)
	: _sel_color(sel_color), _lines(lines), _groups(groups)
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
	// The selection overlay is a second Print of the same row (Viewer::ShowPage):
	// its spans join the row's underline spans, they do not replace them.
	if (!_selection)
		_spans.clear();

	// The row fill of a code block (story markdown-code-tint): a fence row paints the
	// block's tint as the row background for the full row width, before the runs; the
	// runs paint over it and keep their own colors, cells without a run keep the tint.
	// The palette resolves the tint for the live viewer background - a theme change
	// shows on the next redraw - and true color gates it (spec, States).  The fill is
	// a paint of the row's cells; the selection overlay rides it as the plain
	// background does.
	int64_t tint = (line && line->tint >= 0 && !_selection)
		? MdCodeTint(FarColorToReal(COL_VIEWERTEXT)) : -1;
	if (tint >= 0) {
		SHORT x, y;
		GetCursorPos(x, y);
		SetColor(TintFill((uint64_t)tint));
		PrintSpaces(print_len);
		GotoXY(x, y);
	}

	int pos = 0;   // cell coordinate in the row, counted over all runs
	if (line) {
		for (const MdRun &run : line->runs) {
			const bool tokenized = line->tokens >= 0 && _groups != nullptr &&
				(size_t)line->tokens < _groups->size() && run.role == MD_ROLE_CODE;
			const uint64_t run_color = _selection ? _sel_color : MarkdownRunColor(run.role);
			SetColor(run_color);
			if (tokenized) {
				PrintCodeRuns((*_groups)[line->tokens], run.text, pos, skip_len, print_len);
				continue;
			}

			const wchar_t *p = run.text.c_str();
			bool joining = false;

			// advance past the horizontal scroll offset (in cells)
			while (skip_len > 0 && *p) {
				const int before = skip_len;
				if (static_cast<uint32_t>(*p) <= 0x7f) { if (!joining) skip_len--; joining = false; }
				else if (!ShouldSkip(*p)) {
					if (*p == CharClasses::ZERO_WIDTH_JOINER) joining = true;
					else if (CharClasses::IsFullWidth(p)) { if (!joining) skip_len -= 2; joining = false; }
					else if (!CharClasses::IsXxxfix(*p)) { if (!joining) skip_len--; joining = false; }
				}
				pos += before - skip_len;
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
						pos += (int)cells;
						if (run_color & COMMON_LVB_UNDERSCORE) {
							_spans.emplace_back(pos - (int)cells, pos);
						}
					}
				}
				if (!p[piece]) break;
				p += piece + 1;
			}
		}
	}

	// Blank cells (past the line end, or an empty line) take the viewer text
	// color, not whatever color the last run or the arrows left behind.  A row
	// with a token group pads with the code role (spec.md, Token roles).
	if (print_len > 0) {
		if (tint >= 0) {
			// The fill covers the full row width: the cells past the last run take
			// the tint, not the viewer background.
			SetColor(TintFill((uint64_t)tint));
		} else {
			const bool tokenized_row = line && line->tokens >= 0 && _groups != nullptr &&
				(size_t)line->tokens < _groups->size();
			SetColor(_selection ? _sel_color :
				(tokenized_row ? MarkdownRunColor(MD_ROLE_CODE) : FarColorToReal(COL_VIEWERTEXT)));
		}
		PrintSpaces(print_len);
	}
}

// Print one run cell by cell, up to `cell_stop` (a cell coordinate of the row).
void MarkdownPrinter::Cells(const wchar_t *&p, int &pos, int &print_len, int64_t cell_stop)
{
	bool joining = false;
	while (print_len > 0 && *p && (int64_t)pos < cell_stop) {
		size_t piece = 0;
		while (p[piece] && !ShouldSkip(p[piece])) ++piece;
		if (piece) {
			size_t cells = print_len;
			if ((int64_t)cells > cell_stop - pos)
				cells = (size_t)(cell_stop - pos);
			piece = StrSizeOfCells(p, piece, cells, false);
			if (piece) {
				Text(p, piece);
				print_len -= (int)cells;
				pos += (int)cells;
			}
		}
		p += piece;
		if (ShouldSkip(*p)) ++p;   // skip the one skipped char the piece loop stopped at
		if (!*p) break;
	}
}

// Print one code run, coloring the token slices of `group` (plan T4).  Cells
// past the last token keep the code role; the row padding keeps the text role.
void MarkdownPrinter::PrintCodeRuns(const MdTokenGroup &group, const std::wstring &text,
	int &pos, int skip_len, int &print_len)
{
	const wchar_t *p = text.c_str();
	bool joining = false;
	while (skip_len > 0 && *p) {   // advance past the horizontal scroll offset
		const int before = skip_len;
		if (static_cast<uint32_t>(*p) <= 0x7f) { if (!joining) skip_len--; joining = false; }
		else if (!ShouldSkip(*p)) {
			if (*p == CharClasses::ZERO_WIDTH_JOINER) joining = true;
			else if (CharClasses::IsFullWidth(p)) { if (!joining) skip_len -= 2; joining = false; }
			else if (!CharClasses::IsXxxfix(*p)) { if (!joining) skip_len--; joining = false; }
		}
		pos += before - skip_len;
		++p;
	}

	size_t ti = 0;   // index over the group's tokens, ascending
	while (print_len > 0 && *p) {
		if (ti < group.tokens.size() && (int64_t)pos >= (int64_t)group.tokens[ti].start) {
			SetColor(_selection ? _sel_color : MarkdownRunColor(MdRoleForTokenClass(group.tokens[ti].cls)));
			Cells(p, pos, print_len, (int64_t)group.tokens[ti].end);
			SetColor(_selection ? _sel_color : MarkdownRunColor(MD_ROLE_CODE));
			++ti;
			continue;
		}
		const int64_t stop = ti < group.tokens.size() ? (int64_t)group.tokens[ti].start : INT64_MAX;
		Cells(p, pos, print_len, stop);
	}
}

// One GetText/PutText pair for the row: Console::Text drops the underline bit
// for one-cell characters, so the printer records the spans and restores the
// bit here, the way Viewer::ApplyUnderlineSegment does.  A row with no
// underlined run costs nothing (spec, Underline).
void MarkdownPrinter::FinishRow(int screen_x1, int screen_y)
{
	if (_spans.empty())
		return;

	int first = _spans.front().first;
	int last = _spans.back().second - 1;
	for (const auto &span : _spans) {
		if (span.first < first)
			first = span.first;
		if (span.second - 1 > last)
			last = span.second - 1;
	}
	const int width = last - first + 1;
	if (width <= 0)
		return;

	const int x1 = screen_x1 + first;
	const int x2 = screen_x1 + last;
	std::vector<CHAR_INFO> buf(static_cast<size_t>(width));
	GetText(x1, screen_y, x2, screen_y, buf.data(), width * (int)sizeof(CHAR_INFO));
	for (int i = 0; i < width; ++i) {
		const int col = first + i;
		for (const auto &span : _spans) {
			if (col >= span.first && col < span.second) {
				buf[static_cast<size_t>(i)].Attributes |= COMMON_LVB_UNDERSCORE;
				break;
			}
		}
	}
	PutText(x1, screen_y, x2, screen_y, buf.data());
	_spans.clear();
}
