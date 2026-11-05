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

private:
	uint64_t _color;
};

struct MarkdownPrinter : ViewerPrinter
{
	// Draws styled markdown display lines (runs with per-run FarColor attrs).
	// Print() is called once per screen row, in row order, consuming `_lines`
	// sequentially; the `str` argument (the concatenation of the runs, kept in
	// the viewer's Strings[] for poscache/length) is ignored in favour of runs.
	MarkdownPrinter(uint64_t sel_color, const std::vector<MdLine> *lines);
	virtual ~MarkdownPrinter();

	virtual int Length(const wchar_t *str, int limit = -1);
	virtual void Print(int skip_len, int print_len, const wchar_t *str);

private:
	uint64_t _sel_color;
	const std::vector<MdLine> *_lines;
	size_t _row = 0;
};
