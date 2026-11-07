#pragma once
#include <stdint.h>
#include "MarkdownRender.hpp"
#include <stdlib.h>

struct ViewerPrinter
{
	void PrintSpaces(size_t cnt);
	void EnableBOMSkip();
	void SetSelection(bool selection);

	virtual ~ViewerPrinter() {};

	virtual int Length(const wchar_t *str, int limit = -1) = 0;
	virtual void Print(int skip_len, int print_len, const wchar_t *str) = 0;
	// One pass over the row just printed, at screen coordinates (the caller
	// knows them; Print works in window columns).  MarkdownPrinter restores the
	// underline bit the console drops for one-cell characters (story
	// markdown-colorer-look, spec Underline).  Empty for the plain printer.
	virtual void FinishRow(int screen_x1, int screen_y) {}

protected:

	inline bool ShouldSkip(wchar_t ch) const noexcept
	{
		if (_bom_skip && ch == 0xFEFF) {
			return true;
		}

		return false;
	}


	bool _bom_skip = false;
	bool _selection = false;
};

struct PlainViewerPrinter : ViewerPrinter
{
	PlainViewerPrinter(uint64_t color);
	virtual ~PlainViewerPrinter();

	virtual int Length(const wchar_t *str, int limit = -1);
	virtual void Print(int skip_len, int print_len, const wchar_t *str);
	// One GetText/PutText pair for the row, setting 0x8000 on the recorded
	// column spans (the mechanism of Viewer::ApplyUnderlineSegment, spec).

private:
	uint64_t _color;
};

struct MarkdownPrinter : ViewerPrinter
{
	// Draws styled markdown display lines (runs with palette roles, mapped to colors when drawn).
	// Print() is called once per screen row, in row order, consuming `_lines`
	// sequentially; the `str` argument (the concatenation of the runs, kept in
	// the viewer's Strings[] for poscache/length) is ignored in favour of runs.
	// `_groups` (story markdown-code-highlight): the fence token groups, indexed by
	// MdLine::tokens.  Null, or tokens == -1, draws the plain code role.
	MarkdownPrinter(uint64_t sel_color, const std::vector<MdLine> *lines,
		const std::vector<MdTokenGroup> *groups = nullptr);
	virtual ~MarkdownPrinter();

	virtual int Length(const wchar_t *str, int limit = -1);
	virtual void Print(int skip_len, int print_len, const wchar_t *str);
	// One GetText/PutText pair for the row, setting 0x8000 on the recorded
	// column spans (the mechanism of Viewer::ApplyUnderlineSegment, spec).
	virtual void FinishRow(int screen_x1, int screen_y) override;

private:
	// Print one run cell by cell, up to `cell_stop` (cell coordinates of the row).
	void Cells(const wchar_t *&p, int &pos, int &print_len, int64_t cell_stop);
	// Print one code run, coloring the token slices of `group`.
	void PrintCodeRuns(const MdTokenGroup &group, const std::wstring &text,
		int &pos, int skip_len, int &print_len);

	uint64_t _sel_color;
	const std::vector<MdLine> *_lines;
	const std::vector<MdTokenGroup> *_groups;
	size_t _row = 0;
	// Window-column spans [first, second) of the row whose color carries
	// COMMON_LVB_UNDERSCORE; filled by Print, consumed by FinishRow.
	std::vector<std::pair<int, int>> _spans;
};
