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
// A level-4 heading takes the heading entry too; the list comes on the row right under it.
var h4 = ExpectString("Heading Four")
CheckForeSame("heading four", ReadCell(h4.X, h4.Y), heading_cell, "heading")
var bullet = ExpectString("bullet one")
CheckRowOf("list right under the heading", bullet, h4.Y + 1)
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

// Scroll to the bottom, where the mermaid fences are.
TypeEnd()
ExpectNoString("Heading One")
ExpectString("Start")
ExpectString("Finish")
ExpectNoString("```mermaid")
// The flowchart is drawn as box art in the art role, with no source text left on screen.
var flow = ExpectString("┌───────┐     ┌────────┐")
CheckForeSame("flowchart art", ReadCell(flow.X, flow.Y), art_cell, "table art")
ExpectNoString("graph LR")
// The swimlane block is not drawn: its code shows, and the hint row follows the code.
var swim_last = ExpectString("a1[Start] --> a2[Finish]")
var hint = ExpectString("(diagram not drawn here - Alt-F3 opens it in the browser)")
if (hint.Y != swim_last.Y + 2) {
	Log("swimlane code on row " + swim_last.Y + ", hint on row " + hint.Y);
	Panic("the hint does not follow the swimlane code after its end line");
}
CheckFore("diagram hint", ReadCell(hint.X, hint.Y))
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

// R7: a table has one rule, under its header row, and no top or bottom border. No page of the
// spacing file shows a corner glyph of a border.
function CheckNoBorder() {
	ExpectNoString("┌", 0, 0, 80, 24, 1000)
	ExpectNoString("└", 0, 0, 80, 24, 1000)
}

// Spacing: a blank row follows the heading, then the first paragraph.
var start_row = ExpectString("Long Start");
CheckBlankRow("blank row after the heading", start_row.Y + 1);
var first_para = ExpectString("Row 001");
if (first_para.Y != start_row.Y + 2) {
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

// The last heading keeps one blank row after it, then its paragraph.
var end_head = ExpectString("Long End")
CheckBlankRow("blank row after the last heading", end_head.Y + 1);
CheckRowOf("paragraph after the last heading", ExpectString("Final paragraph"), end_head.Y + 2);

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

// After three pages the top row is Item 060, a list item of long.md, so the round trip below tests a list row.
TypePageDown(3)
CheckTop("list item in markdown", ExpectString("Item 060"))
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
CheckTop("list item in plain", ExpectString("- Item 060 list text for the scrolling tests."))
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
ExpectNoString("- Item 060")
CheckTop("list item back in markdown", ExpectString("Item 060"))

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

// Page 1: the title, the lead paragraph, the lists, the table, the quote and the code fences.
var title = ExpectString("Spacing Title");
CheckRowOf("title as the first row", title, 1);
CheckBlankRow("blank row after the title", title.Y + 1);
var lead = ExpectString("Lead paragraph of the spacing test.");
CheckRowOf("lead paragraph after the title", lead, title.Y + 2);
var heading = ExpectString("Spacing Heading");
CheckBlankRow("blank row before the heading", heading.Y - 1);
CheckRowOf("heading after the lead paragraph", heading, lead.Y + 2);
var p1 = ExpectString("Paragraph before two source blanks.");
CheckRowOf("paragraph after the heading", p1, heading.Y + 1);
var p2 = ExpectString("Paragraph after two source blanks.");
CheckBlankRow("blank row between two paragraphs", p1.Y + 1);
CheckRowOf("two source blank lines give one blank row", p2, p1.Y + 2);

// Items of a list stay tight; the nested list gets no blank row; a list has none before or after.
var item1 = ExpectString("Spacing item one");
CheckRowOf("list right after the paragraph", item1, p2.Y + 1);
var nested = ExpectString("Spacing nested item");
var item2 = ExpectString("Spacing item two");
CheckRowOf("nested item right under its item", nested, item1.Y + 1);
CheckRowOf("next item right under the nested item", item2, item1.Y + 2);

// The table header is right after the list. The table has one rule and no top or bottom border.
var table_sp = ExpectString("Spacing A");
CheckRowOf("table header right after the list", table_sp, item2.Y + 1);
CheckCellChar(table_sp.X - 2, table_sp.Y + 1, "├");
CheckRowOf("table body row under the rule", ExpectString("spacing 1"), table_sp.Y + 2);
CheckBlankRow("blank row after the table", table_sp.Y + 3);
var quote_sp = ExpectString("Spacing quote line.");
CheckRowOf("quote after the table and its blank row", quote_sp, table_sp.Y + 4);
CheckBlankRow("blank row after the quote", quote_sp.Y + 1);
var fence = ExpectString("Spacing fence line.");
CheckRowOf("code fence after the quote", fence, quote_sp.Y + 2);
CheckBlankRow("blank row after the code fence", fence.Y + 1);
var after_fence = ExpectString("Paragraph after the fence.");
CheckRowOf("paragraph after the code fence", after_fence, fence.Y + 2);
CheckRowOf("code fence with an empty last line", ExpectString("Fence with an empty last line."), after_fence.Y + 2);
CheckNoBorder();

// Page 2: a fence whose last source line is empty, a list with a fence, a loose list, and the start
// of the compact blocks.
TypePageDown();
CheckNoBorder();
var empty_last = ExpectString("Fence with an empty last line.");
CheckBlankRow("empty last source line inside the fence", empty_last.Y + 1);
CheckBlankRow("blank row after the fence with an empty last line", empty_last.Y + 2);
var after_empty = ExpectString("Paragraph after the empty last line.");
CheckRowOf("paragraph after that fence", after_empty, empty_last.Y + 3);

// The fence in a list item keeps a blank row after the item text, and the paragraph after the list
// keeps a blank row from the fence.
var item_fence = ExpectString("Spacing item with a fence:");
CheckRowOf("list right after the paragraph", item_fence, after_empty.Y + 1);
CheckBlankRow("blank row after the item text", item_fence.Y + 1);
var item_code = ExpectString("Spacing item fence line.");
CheckRowOf("fence in the item after its text and a blank row", item_code, item_fence.Y + 2);
CheckBlankRow("blank row after the fence in the item", item_code.Y + 1);
var after_list = ExpectString("Spacing paragraph after the list.");
CheckRowOf("paragraph after the list, from the fence", after_list, item_code.Y + 2);
var item_p = ExpectString("Spacing item with a paragraph.");
CheckRowOf("loose list right after the paragraph", item_p, after_list.Y + 1);
CheckBlankRow("blank row between the item's paragraphs", item_p.Y + 1);
var item_p2 = ExpectString("Spacing item paragraph two.");
CheckRowOf("second paragraph in the item", item_p2, item_p.Y + 2);
var last = ExpectString("Spacing last paragraph.");
CheckRowOf("paragraph right after the list item", last, item_p2.Y + 1);

// Compact layout: a list sits right after the paragraph before it, and the paragraph after the list
// sits right under it.
var c_para = ExpectString("Compact para before list.");
var c_item = ExpectString("Compact item after para.");
CheckRowOf("list right after the paragraph", c_item, c_para.Y + 1);
var c_after_list = ExpectString("Compact para after list.");
CheckRowOf("paragraph right after the list", c_after_list, c_item.Y + 1);

// R1 and R2: a heading of level 1 keeps one blank row before it and one after it.
var c_top = ExpectString("Compact Top");
CheckRowOf("level 1 heading after the list and its blank row", c_top, c_after_list.Y + 2);
CheckBlankRow("blank row before the level 1 heading", c_top.Y - 1);
CheckBlankRow("blank row after the level 1 heading", c_top.Y + 1);
var c_top_item = ExpectString("Compact item after top.");
CheckRowOf("list after the heading and its blank row", c_top_item, c_top.Y + 2);
CheckNoBorder();

// Page 3: the level 2 headings, the level 3 heading, the fences around the lists.
TypePageDown();
CheckNoBorder();

// R1 and R3: a heading of level 2 keeps its blank row before it and none after it.
var c_second = ExpectString("Compact Second");
CheckBlankRow("blank row before a level 2 heading", c_second.Y - 1);
var c_second_item = ExpectString("Compact item after second.");
CheckRowOf("list right after a level 2 heading", c_second_item, c_second.Y + 1);

// C1: a heading after a heading of level 2 keeps one blank row.  R3: a paragraph right under a heading.
var c_upper = ExpectString("Compact Upper");
CheckRowOf("heading after the list and its blank row", c_upper, c_second_item.Y + 2);
var c_lower = ExpectString("Compact Lower");
CheckRowOf("heading under a heading with a blank row", c_lower, c_upper.Y + 2);
CheckBlankRow("blank row between two headings", c_upper.Y + 1);
var c_after_lower = ExpectString("Compact para after lower.");
CheckRowOf("paragraph right under a heading", c_after_lower, c_lower.Y + 1);

// C2: a code fence right under a heading of level 2.  C5: a list after a code fence has a blank row.
var c_fence_head = ExpectString("Compact Before Fence");
CheckRowOf("heading after the paragraph and its blank row", c_fence_head, c_after_lower.Y + 2);
var c_fence_line = ExpectString("Compact fence after heading.");
CheckRowOf("code fence right under a heading", c_fence_line, c_fence_head.Y + 1);
var c_fence_item = ExpectString("Compact item after fence.");
CheckRowOf("list after a code fence with a blank row", c_fence_item, c_fence_line.Y + 2);
CheckBlankRow("blank row between the fence and the list", c_fence_line.Y + 1);

// C6: a code fence after a list has a blank row.  C7: a heading after a list has a blank row.
var c_fence_after_item = ExpectString("Compact fence after item.");
CheckRowOf("code fence after the list item and its blank row", c_fence_after_item, c_fence_item.Y + 2);
CheckBlankRow("blank row between the list item and the fence", c_fence_item.Y + 1);
var c_item_before_head = ExpectString("Compact item before heading.");
CheckRowOf("list item after the fence and its blank row", c_item_before_head, c_fence_after_item.Y + 2);
var c_after_item = ExpectString("Compact After Item");
CheckRowOf("heading after a list with a blank row", c_after_item, c_item_before_head.Y + 2);
CheckBlankRow("blank row before the heading after the list", c_item_before_head.Y + 1);
var c_fence_item_one = ExpectString("Compact fence item one:");
CheckRowOf("list right after a heading", c_fence_item_one, c_after_item.Y + 1);

// The fence in a list item: a blank row after the item text, then the fence.
var c_fence_in_item = ExpectString("Compact fence in item.");
CheckBlankRow("blank row after the item text", c_fence_item_one.Y + 1);
CheckRowOf("fence in the item after its text and a blank row", c_fence_in_item, c_fence_item_one.Y + 2);
CheckNoBorder();

// Page 4: the next item after the fence, the ordered list, the headings of levels 6 and 1, and the
// thematic break.
TypePageDown();
CheckNoBorder();
var c_fence_in_item_p4 = ExpectString("Compact fence in item.");
var c_fence_item_two = ExpectString("Compact fence item two.");
CheckRowOf("next item after the fence with a blank row", c_fence_item_two, c_fence_in_item_p4.Y + 2);
CheckBlankRow("blank row between the fence and the next item", c_fence_in_item_p4.Y + 1);
var c_ordered = ExpectString("Compact ordered item.");
CheckRowOf("ordered list right after the bullet list", c_ordered, c_fence_item_two.Y + 1);

// R3 for heading level 6: the paragraph sits right under it.
var c_six = ExpectString("Compact Six");
CheckRowOf("level 6 heading after the list and its blank row", c_six, c_ordered.Y + 2);
var c_after_six = ExpectString("Compact para after six.");
CheckRowOf("paragraph right under a level 6 heading", c_after_six, c_six.Y + 1);

// A setext heading of level 2 has no rule row under it: the paragraph is on the next row.
var c_setext = ExpectString("Compact Setext");
CheckRowOf("setext heading after the paragraph and its blank row", c_setext, c_after_six.Y + 2);
CheckBlankRow("blank row before a setext heading", c_setext.Y - 1);
var c_after_setext = ExpectString("Compact para after setext.");
CheckRowOf("paragraph right under a setext heading", c_after_setext, c_setext.Y + 1);

// A setext heading of level 1 keeps one blank row after it, and the thematic break keeps its line.
var c_setext_top = ExpectString("Compact Setext Top");
CheckBlankRow("blank row before a setext heading of level 1", c_setext_top.Y - 1);
CheckBlankRow("blank row after a setext heading of level 1", c_setext_top.Y + 1);
var c_above_break = ExpectString("Compact para above break.");
CheckRowOf("paragraph after a level 1 heading and its blank row", c_above_break, c_setext_top.Y + 2);
CheckBlankRow("blank row before the thematic break", c_above_break.Y + 1);
CheckCellChar(0, c_above_break.Y + 2, "─");
CheckCellChar(70, c_above_break.Y + 2, "─");
var c_break = ExpectString("Compact para below break.");
CheckRowOf("paragraph right after the thematic break and its blank row", c_break, c_above_break.Y + 4);
CheckBlankRow("blank row after the thematic break", c_break.Y - 1);
CheckNoBorder();

// Page 5: four lines down from page 4, the table, its rules and the list item with a heading.
// The review blocks follow on pages 6 and 7.
TypeDown(4);
CheckNoBorder();
var c_break_end = ExpectString("Compact para below break.");
// R6 and R7: the table has a blank row before it and one rule under its header row.
var c_head = ExpectString("Compact A");
CheckBlankRow("blank row before the table", c_head.Y - 1);
CheckRowOf("table header after the paragraph and its blank row", c_head, c_break_end.Y + 2);
CheckCellChar(c_head.X - 2, c_head.Y + 1, "├");
var c_cell = ExpectString("compact 1");
CheckRowOf("body row right under the header rule", c_cell, c_head.Y + 2);
CheckCellChar(c_cell.X - 2, c_cell.Y - 1, "├");
CheckBlankRow("blank row after the table", c_cell.Y + 1);
var c_after_table = ExpectString("Compact para after table.");
CheckRowOf("paragraph after the table and its blank row", c_after_table, c_cell.Y + 2);

// C13: a heading as the first block of a list item stays on the row of the bullet.
var c_item_head = ExpectString("Compact Item Heading");
CheckRowOf("list item right after the paragraph", c_item_head, c_after_table.Y + 1);
CheckCellChar(c_item_head.X - 2, c_item_head.Y, "•");

// Page 6: the first review blocks, right after the heading item.
TypePageDown();
CheckNoBorder();

// C7: a heading after a list keeps its blank row. An empty item (`-` with no text) shows no
// row of its own.
var r_item1 = ExpectString("review item one");
var r_head_empty = ExpectString("Review Head After Empty");
CheckRowOf("heading after the list with an empty item", r_head_empty, r_item1.Y + 2);
CheckBlankRow("blank row between the item and the heading", r_item1.Y + 1);

// C6: a code block after a list keeps its blank row, also after an empty item.
var r_item2 = ExpectString("review item two");
var r_code_empty = ExpectString("review code after empty");
CheckRowOf("code block after the list with an empty item", r_code_empty, r_item2.Y + 2);
CheckBlankRow("blank row between the item and the code block", r_item2.Y + 1);

// R5: an HTML block has one blank row before it and one after it.
var r_para3 = ExpectString("Review para three.");
var r_html1 = ExpectString("review html one");
CheckRowOf("html block after the paragraph and its blank row", r_html1, r_para3.Y + 2);
CheckBlankRow("blank row before the html block", r_para3.Y + 1);
var r_para4 = ExpectString("Review para four.");
CheckRowOf("paragraph after the html block and its blank row", r_para4, r_para3.Y + 4);
CheckBlankRow("blank row after the html block", r_para3.Y + 3);

// Page 7, the last page: the heading over an html block, the fence in an item, the heading
// and the html block as the first block of a list item.
TypePageDown();
CheckNoBorder();

// C2: an html block right under a heading of level 2. R5: it keeps its blank row after it.
var r_head_html = ExpectString("Review Head Over Html");
var r_html2 = ExpectString("review html two");
CheckRowOf("html block right under a heading", r_html2, r_head_html.Y + 1);
var r_para5 = ExpectString("Review para five.");
CheckRowOf("paragraph after the html block and its blank row", r_para5, r_head_html.Y + 3);
CheckBlankRow("blank row between the heading block and the paragraph", r_head_html.Y + 2);

// C8 and C13: a list sits right after the paragraph, and a fence as the first block of an
// item stays on the row of the bullet.
var r_fence_first = ExpectString("review fence first");
if (r_fence_first.Y != r_para5.Y + 1 && r_fence_first.Y != r_para5.Y + 2) {
	Log("paragraph five on row " + r_para5.Y + ", fence on row " + r_fence_first.Y);
	Panic("the fence item is not one or two rows under the paragraph");
}
if (BoundedLine(0, r_para5.Y + 1, 80, " \t") == "") {
	Log("row " + (r_para5.Y + 1) + " is blank");
	Panic("the list after the paragraph has a blank row before it");
}

// C13: a heading of level 1 as the first block of an item stays on the row of the bullet.
// C3: it keeps one blank row after it.
var r_top = ExpectString("Review Item Top");
var r_item4 = ExpectString("review item four");
CheckRowOf("item after the heading item and its blank row", r_item4, r_top.Y + 2);
CheckBlankRow("blank row after the heading item", r_top.Y + 1);

// C13 and R5: an html block as the first block of an item stays on the row of the bullet,
// and keeps its blank row after it.
var r_html3 = ExpectString("review html three");
var r_item5 = ExpectString("review item five");
CheckRowOf("item after the html block in the item", r_item5, r_html3.Y + 2);
CheckBlankRow("blank row after the html block in the item", r_html3.Y + 1);

// R4 and R5: a list has no blank row before it, and the html block after it keeps its
// blank row.
var r_item6 = ExpectString("review item six");
var r_html4 = ExpectString("review html four");
CheckRowOf("html block after the list and its blank row", r_html4, r_item6.Y + 2);
CheckBlankRow("blank row between the list and the html block", r_item6.Y + 1);


TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);
0;
