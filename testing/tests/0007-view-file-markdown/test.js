mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"

// Colors: a markdown cell must have the viewer background. Body text has the viewer foreground;
// headings, code and table art take palette entries of their own (checked below).
// plain is a blank cell of the plain-mode viewer, read before the markdown switch.
function CheckBack(name, cell) {
	if (cell.Back != plain.Back || cell.IsBackTC != plain.IsBackTC || cell.BackTC != plain.BackTC) {
		Log(name + ": back " + cell.Back + " tc " + cell.IsBackTC + " " + cell.BackTC + ", plain back " + plain.Back + " tc " + plain.IsBackTC + " " + plain.BackTC)
		Panic(name + " background differs from the viewer background")
	}
}

function CheckFore(name, cell) {
	if (cell.Fore != plain.Fore || cell.IsForeTC != plain.IsForeTC || cell.ForeTC != plain.ForeTC) {
		Log(name + ": fore " + cell.Fore + " tc " + cell.IsForeTC + " " + cell.ForeTC + ", plain fore " + plain.Fore + " tc " + plain.IsForeTC + " " + plain.ForeTC)
		Panic(name + " foreground differs from the viewer foreground")
	}
}

// Foreground of a cell against a reference cell: the same palette entry, or a different one.
function SameFore(a, b) {
	return a.Fore == b.Fore && a.IsForeTC == b.IsForeTC && a.ForeTC == b.ForeTC;
}

function CheckForeSame(name, cell, ref, refName) {
	if (!SameFore(cell, ref)) {
		Log(name + ": fore " + cell.Fore + " tc " + cell.IsForeTC + " " + cell.ForeTC + ", " + refName + " fore " + ref.Fore + " tc " + ref.IsForeTC + " " + ref.ForeTC)
		Panic(name + " foreground differs from the " + refName + " foreground")
	}
}

function CheckForeOther(name, cell, ref, refName) {
	if (SameFore(cell, ref)) {
		Log(name + ": fore " + cell.Fore + " tc " + cell.IsForeTC + " " + cell.ForeTC + ", " + refName + " fore is the same")
		Panic(name + " foreground is the same as the " + refName + " foreground")
	}
}

// A fresh profile shows the help screen on start; a reused one would not.
RemoveAll(profile)

// 80x24: the sample does not fit on one screen, so the mermaid art sits below the fold.
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("Help - FAR2L");
TypeEscape()
ExpectString("OSC52");
TypeEscape()

// Open the markdown sample in the viewer (F3 on panel; the cursor starts on "/..").
// long.md sorts before sample.md, so the sample is the second file below "/..".
TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")
// F3 on a .md file opens Markdown mode by default: switch sample.md to plain first.
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
var plain_line = ExpectString("# Heading One")
var plain = ReadCell(60, plain_line.Y)

// Switch viewer to Markdown render mode (Shift+F4).
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
ExpectString("Heading")
ExpectString("Code Fence Body")

var heading = ExpectString("Heading One")
CheckBack("heading", ReadCell(heading.X, heading.Y))
// Headings take the heading palette entry, not the body foreground.
var heading_cell = ReadCell(heading.X, heading.Y)
CheckForeOther("heading", heading_cell, plain, "viewer")
CheckBack("blank viewer cell", ReadCell(60, heading.Y))
// A level-4 heading takes the heading entry too; its layout cue is the rule line below it.
var h4 = ExpectString("Heading Four")
CheckCellChar(h4.X, h4.Y + 1, "─")
CheckForeSame("heading four rule", ReadCell(h4.X, h4.Y + 1), heading_cell, "heading")
// Code takes its own palette entry, different from the heading and from the body.
var code = ExpectString("Code Fence Body")
CheckBack("code", ReadCell(code.X, code.Y))
var code_cell = ReadCell(code.X, code.Y)
CheckForeOther("code", code_cell, plain, "viewer")
CheckForeOther("code", code_cell, heading_cell, "heading")
var table = ExpectString("│ cell1")
CheckBack("table box glyph", ReadCell(table.X, table.Y))
CheckBack("table cell", ReadCell(table.X + 2, table.Y))
CheckFore("table cell text", ReadCell(table.X + 2, table.Y))
// Table header text takes the heading entry; the borders take the art entry, read from the
// line glyph of the separator row under the header.
var table_head = ExpectString("Col A")
CheckBack("table header", ReadCell(table_head.X, table_head.Y))
CheckForeSame("table header", ReadCell(table_head.X, table_head.Y), heading_cell, "heading")
CheckCellChar(table.X + 1, table.Y - 1, "─")
var art_cell = ReadCell(table.X + 1, table.Y - 1)
CheckForeSame("table border", ReadCell(table.X, table.Y), art_cell, "table art")
CheckForeOther("table art", art_cell, plain, "viewer")
CheckForeOther("table art", art_cell, heading_cell, "heading")
CheckForeOther("table art", art_cell, code_cell, "code")

// Scroll to the bottom, where the mermaid fence is.
TypeEnd()
ExpectNoString("Heading One")
ExpectString("Start")
ExpectString("Finish")
ExpectString("┌──────────────────┐")
ExpectNoString("```mermaid")
var quote = ExpectString("│ block quote line")
CheckFore("block quote", ReadCell(quote.X + 2, quote.Y))

TypeEscape()

// Scrolling by line index on long.md: several screens of rendered lines, so each
// step lands on text that is not on the screen before it.
TypeUp()
TypeFKey(3)
ExpectString("left/long.md")
ExpectString("Long Start")
ExpectNoString("# Long Start")
ExpectString("  0%")

// A row of the markdown view must be blank: the text of its 80 cells is empty.
function CheckBlankRow(name, y) {
	var text = BoundedLine(0, y, 80, " \t");
	if (text != "") {
		Log(name + ": row " + y + " is '" + text + "'");
		Panic(name + " is not a blank row");
	}
}

// The found text must be on row y.
function CheckRowOf(name, found, y) {
	if (found.Y != y) {
		Log(name + ": found on row " + found.Y + ", expected row " + y);
		Panic(name + " is not on the expected row");
	}
}

// Spacing: a blank row follows the heading and its rule, then the first paragraph.
var start_row = ExpectString("Long Start");
CheckBlankRow("blank row after the heading", start_row.Y + 2);
var first_para = ExpectString("Row 001");
if (first_para.Y != start_row.Y + 3) {
	Log("heading on row " + start_row.Y + ", first paragraph on row " + first_para.Y);
	Panic("the first paragraph does not follow the heading after one blank row");
}

// The found text must be on the top row of the viewer.
function CheckTop(name, found) {
	if (found.Y != 1) {
		Log(name + ": found on row " + found.Y);
		Panic(name + " is not on the top row");
	}
}

// Down moves the top by one rendered line: the heading row scrolls off, the next row stays.
TypeDown()
ExpectNoString("Long Start")
ExpectString("Row 001")

// Up moves the top back by one line and stops at the first line.
TypeUp(3)
CheckTop("up to the first line", ExpectString("Long Start"))

// PgDn moves by a screen minus one line: the first rows go, later rows show, never a blank screen.
TypePageDown()
ExpectNoString("Row 001")
var row30 = ExpectString("Row 030")

// Up moves the top by exactly one line: the row below it moves down one row.
TypeUp()
var row30_up = ExpectString("Row 030")
if (row30_up.Y != row30.Y + 1) {
	Log("Row 030 moved from row " + row30.Y + " to row " + row30_up.Y);
	Panic("Up did not move the top by one line");
}

// PgUp moves back by the same step: the first rows return.
TypePageUp()
ExpectString("Row 001")
ExpectNoString("Row 030")

// End shows the last page: the final heading is drawn, not its raw "# " markup.
TypeEnd()
ExpectString("Long End")
ExpectString("Final paragraph")
ExpectNoString("# Long End")
ExpectNoString("Row 030")
ExpectString("100%")

// Down on the last page does not scroll past it: the last page keeps its rows.
var last_row = ExpectString("Final paragraph")
TypeDown(3)
var still_last = ExpectString("Final paragraph")
if (still_last.Y != last_row.Y) {
	Log("Final paragraph moved from row " + last_row.Y + " to row " + still_last.Y);
	Panic("Down scrolled past the last full page");
}
ExpectString("100%")

// The last heading has one blank row after it, then its paragraph.
var end_head = ExpectString("Long End")
CheckBlankRow("blank row after the last heading", end_head.Y + 2)
if (ExpectString("Final paragraph").Y != end_head.Y + 3) {
	Log("Long End on row " + end_head.Y + ", final paragraph is not on row " + (end_head.Y + 3));
	Panic("the final paragraph does not follow the heading after one blank row");
}

// Home goes back to the first page.
TypeHome()
CheckTop("home", ExpectString("Long Start"))
ExpectString("  0%")

// Markdown -> plain -> markdown keeps the top row. Each row's file position is its line start,
// so the plain view of the heading keeps its "# " and the list item keeps its "- ".
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
CheckTop("heading in plain", ExpectString("# Long Start"))
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
ExpectNoString("# Long Start")
CheckTop("heading back in markdown", ExpectString("Long Start"))

// The blank rows of the spacing moved the list down: after three pages the top row is Item 057.
TypePageDown(3)
CheckTop("list item in markdown", ExpectString("Item 057"))
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
CheckTop("list item in plain", ExpectString("- Item 057 list text for the scrolling tests."))
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
ExpectNoString("- Item 057")
CheckTop("list item back in markdown", ExpectString("Item 057"))

// Hex with the Markdown flag still set: Down moves by one 16-byte row, not by a markdown line.
TypeHome()
TypeFKey(4)
CheckTop("hex first row", ExpectString("0000000000:"))
TypeDown()
CheckTop("hex second row", ExpectString("0000000010:"))
TypeFKey(4)
ExpectNoString("0000000010:")
CheckTop("back from hex", ExpectString("Row 001"))

TypeEscape()

// Spacing, block by block, in spacing.md. It sorts after sample.md, so the cursor steps above still
// reach long.md and sample.md. Its first page is the top of the file; its last page is End.
TypeDown(2)
TypeFKey(3)
ExpectString("left/spacing.md")

var title = ExpectString("Spacing Title");
CheckRowOf("title as the first row", title, 1);
CheckBlankRow("blank row after the title", title.Y + 2);
var lead = ExpectString("Lead paragraph of the spacing test.");
CheckRowOf("lead paragraph after the title", lead, title.Y + 3);
var heading = ExpectString("Spacing Heading");
CheckBlankRow("blank row before the heading", heading.Y - 1);
CheckRowOf("heading after the lead paragraph", heading, lead.Y + 2);
CheckBlankRow("blank row after the heading", heading.Y + 2);
var p1 = ExpectString("Paragraph before two source blanks.");
CheckRowOf("paragraph after the heading", p1, heading.Y + 3);
var p2 = ExpectString("Paragraph after two source blanks.");
CheckBlankRow("blank row between two paragraphs", p1.Y + 1);
CheckRowOf("two source blank lines give one blank row", p2, p1.Y + 2);

// Items of a list stay tight; the nested list gets no blank row; the list has one before and after.
var item1 = ExpectString("Spacing item one");
CheckBlankRow("blank row before the list", item1.Y - 1);
var nested = ExpectString("Spacing nested item");
var item2 = ExpectString("Spacing item two");
CheckRowOf("nested item right under its item", nested, item1.Y + 1);
CheckRowOf("next item right under the nested item", item2, item1.Y + 2);
CheckBlankRow("blank row after the list", item2.Y + 1);

var table = ExpectString("Spacing A");
CheckBlankRow("blank row before the table", table.Y - 2);
CheckBlankRow("blank row after the table", table.Y + 4);

// The rest of the file: the last page (End) holds the quote to the last paragraph.
TypeEnd();
var quote = ExpectString("Spacing quote line.");
CheckBlankRow("blank row after the quote", quote.Y + 1);
var fence = ExpectString("Spacing fence line.");
CheckRowOf("code fence after the quote", fence, quote.Y + 2);
CheckBlankRow("blank row after the code fence", fence.Y + 1);
var after_fence = ExpectString("Paragraph after the fence.");
CheckRowOf("paragraph after the code fence", after_fence, fence.Y + 2);

// A fence whose last source line is empty: its own empty code row, then the blank row after the fence.
var empty_last = ExpectString("Fence with an empty last line.");
CheckRowOf("code fence with an empty last line", empty_last, after_fence.Y + 2);
CheckBlankRow("empty last source line inside the fence", empty_last.Y + 1);
CheckBlankRow("blank row after the fence with an empty last line", empty_last.Y + 2);
var after_empty = ExpectString("Paragraph after the empty last line.");
CheckRowOf("paragraph after that fence", after_empty, empty_last.Y + 3);

// Blocks inside list items: a fence after the item's text, and a second paragraph in a loose item.
var item_fence = ExpectString("Spacing item with a fence:");
CheckRowOf("list after the paragraph", item_fence, after_empty.Y + 2);
CheckBlankRow("blank row before the fence in the item", item_fence.Y + 1);
var item_code = ExpectString("Spacing item fence line.");
CheckRowOf("fence in the item after its text and a blank row", item_code, item_fence.Y + 2);
CheckBlankRow("blank row after the fence in the item", item_code.Y + 1);
var after_list = ExpectString("Spacing paragraph after the list.");
CheckRowOf("paragraph after the list", after_list, item_code.Y + 2);
var item_p = ExpectString("Spacing item with a paragraph.");
CheckRowOf("loose list after the paragraph", item_p, after_list.Y + 2);
CheckBlankRow("blank row between the item's paragraphs", item_p.Y + 1);
var item_p2 = ExpectString("Spacing item paragraph two.");
CheckRowOf("second paragraph in the item", item_p2, item_p.Y + 2);
CheckBlankRow("blank row after the list item", item_p2.Y + 1);
var last = ExpectString("Spacing last paragraph.");
CheckRowOf("paragraph after the list item", last, item_p2.Y + 2);

TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);
0;
