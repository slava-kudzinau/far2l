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
// Headings take their own role colors, not the body foreground.
var heading_cell = ReadCell(heading.X, heading.Y)
CheckForeOther("heading", heading_cell, plain, "viewer")
CheckBack("blank viewer cell", ReadCell(60, heading.Y))
// The strong span in the first paragraph takes its own color, distinct from heading and body.
var strong_text = ExpectString("strong")
var strong_cell = ReadCell(strong_text.X, strong_text.Y)
CheckForeOther("strong", strong_cell, plain, "viewer")
CheckForeOther("strong", strong_cell, heading_cell, "heading")
// A level-4 heading shares the h3 color (H3 to H6 are one role); the list comes on the row
// right under it.
var h4 = ExpectString("Heading Four")
CheckForeOther("heading four", ReadCell(h4.X, h4.Y), heading_cell, "h1")
var h3 = ExpectString("Heading Three")
var h3_cell = ReadCell(h3.X, h3.Y)
CheckForeSame("heading four", ReadCell(h4.X, h4.Y), h3_cell, "h3")
CheckForeOther("h3", h3_cell, heading_cell, "h1")
var bullet = ExpectString("bullet one")
CheckRowOf("list right under the h3 heading", bullet, h3.Y + 1)
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
// Table header text takes the strong color; the borders take the rule color, read from the line
// glyph of the separator row under the header.
var table_head = ExpectString("Col A")
CheckBack("table header", ReadCell(table_head.X, table_head.Y))
CheckForeSame("table header", ReadCell(table_head.X, table_head.Y), strong_cell, "strong")
CheckCellChar(table.X + 1, table.Y - 1, "─")
// The borders take the rule color: the separator line, the bars of the header row and the
// bars of the data rows all share it, and it differs from strong and from the body fore.
var border_cell = ReadCell(table.X + 1, table.Y - 1)
CheckForeOther("table border", border_cell, strong_cell, "strong")
CheckForeOther("table border", border_cell, plain, "viewer")
var table_bar = ReadCell(table.X, table.Y)
CheckForeSame("table bar", table_bar, border_cell, "table border")
// A code span in the table header keeps the code role: the header markup is strong for plain
// text, but spans of their own win over it (the spec's precedence).
var hcode = ExpectString("hcode")
var hcode_cell = ReadCell(hcode.X, hcode.Y)
CheckForeSame("code span in table header", hcode_cell, code_cell, "code")

// Scroll to the bottom, where the mermaid fences are.
TypeEnd()
ExpectNoString("Heading One")
ExpectString("Start")
ExpectString("Finish")
ExpectNoString("```mermaid")
// The flowchart is drawn as box art in the art role, with no source text left on screen.
var flow = ExpectString("└───────┘     └────────┘")
var flow_art = ReadCell(flow.X, flow.Y)
CheckForeOther("flowchart art", flow_art, border_cell, "table border")
CheckForeOther("flowchart art", flow_art, plain, "viewer")
ExpectNoString("graph LR")
// The swimlane block is not drawn: its code shows, and the hint row follows the code.
var swim_last = ExpectString("a1[Start] --> a2[Finish]")
var hint = ExpectString("(diagram not drawn here - Alt-F3 opens it in the browser)")
if (hint.Y != swim_last.Y + 2) {
	Log("swimlane code on row " + swim_last.Y + ", hint on row " + hint.Y);
	Panic("the hint does not follow the swimlane code after its end line");
}
CheckFore("diagram hint", ReadCell(hint.X, hint.Y))
// Quote text and the quote bar take the quote role: today the viewer text, so the check reads
// the first quote and keeps the cell for the later pairs.
var quote = ExpectString("│ block quote line")
var quote_cell = ReadCell(quote.X + 2, quote.Y)
CheckForeOther("block quote", quote_cell, border_cell, "table border")
CheckForeOther("block quote", quote_cell, plain, "viewer")


// --- T4: the rest of the palette roles, checked against the cells read above. ---
// H3, H4, H5 and H6 share one color; each heading level differs from the ones next to it.
var h2 = ExpectString("Heading Two")
var h2_cell = ReadCell(h2.X, h2.Y)
var h3r = ExpectString("Heading Three Renamed")
var h3r_cell = ReadCell(h3r.X, h3r.Y)
var h5 = ExpectString("Heading Five")
CheckForeSame("heading five", ReadCell(h5.X, h5.Y), h3_cell, "h3")
var h6 = ExpectString("Heading Six")
CheckForeSame("heading six", ReadCell(h6.X, h6.Y), h3_cell, "h3")
CheckForeOther("h2", h2_cell, heading_cell, "h1")
CheckForeOther("h2", h2_cell, h3_cell, "h3")
CheckForeSame("renamed h3", h3r_cell, h3_cell, "h3")
CheckBack("h2", h2_cell)
CheckBack("renamed h3", h3r_cell)
CheckBack("heading five", ReadCell(h5.X, h5.Y))
CheckBack("heading six", ReadCell(h6.X, h6.Y))
// A labeled link takes the link role: its own color, different from strong, code and body.
var llink = ExpectString("labeled link")
var link_cell = ReadCell(llink.X, llink.Y)
CheckForeOther("labeled link", link_cell, strong_cell, "strong")
CheckForeOther("labeled link", link_cell, code_cell, "code")
CheckForeOther("labeled link", link_cell, plain, "viewer")
CheckBack("labeled link", link_cell)
// The autolink URL cell is recolored by the viewer's own link highlighting, not by the palette,
// so it can only be asserted against the body text around it: it differs from the body cell.
var linkpara = ExpectString("https://example.com")
CheckForeOther("autolink url", ReadCell(linkpara.X, linkpara.Y), plain, "viewer")
CheckBack("autolink url", ReadCell(linkpara.X, linkpara.Y))
// A bold span in a heading shows the strong color, not the heading color; a quote keeps its
// quote color under strong and code spans (the spec's precedence).
var hbold = ExpectString("bold head")
CheckForeSame("bold span in h2", ReadCell(hbold.X, hbold.Y), strong_cell, "strong")
CheckBack("bold span in h2", ReadCell(hbold.X, hbold.Y))
var qbold = ExpectString("bold quote span")
CheckForeSame("bold span in quote", ReadCell(qbold.X, qbold.Y), strong_cell, "strong")
CheckBack("bold span in quote", ReadCell(qbold.X, qbold.Y))
var qcode = ExpectString("quote code span")
CheckForeSame("code span in quote", ReadCell(qcode.X, qcode.Y), code_cell, "code")
CheckBack("code span in quote", ReadCell(qcode.X, qcode.Y))
var quote2 = ExpectString("block quote line")
CheckForeSame("quote text", ReadCell(quote2.X + 2, quote2.Y), quote_cell, "quote")
CheckBack("quote text", ReadCell(quote2.X + 2, quote2.Y))
// A setext-style `---` under a paragraph renders as an H2: the h2 color, and no rule row
// under it (the paragraph text moves up into the heading row, the quote follows after a blank).
var setext_h2 = ExpectString("setext-style break")
var setext_h2_cell = ReadCell(setext_h2.X, setext_h2.Y)
CheckForeSame("setext break as h2", setext_h2_cell, h2_cell, "h2")
CheckForeSame("row after the setext break", ReadCell(0, setext_h2.Y + 1), quote_cell, "quote")
CheckCellChar(0, setext_h2.Y + 1, "│")
CheckBack("setext break as h2", setext_h2_cell)
// The trailing thematic break shares the rule color of the table borders, and the quote text
// differs from it: close the viewer and read the rule row on the last page.
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);

StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
if (Exists(profile + "/.config/history/viewer.pos"))
	Remove(profile + "/.config/history/viewer.pos")
TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")
TypeEnd()
var rule_row = ExpectString("────────────────────────────────────────────────────────────────────────────────")
var rule_cell = ReadCell(rule_row.X, rule_row.Y)
CheckForeSame("thematic break", rule_cell, border_cell, "rule")
CheckForeOther("quote vs rule", quote_cell, rule_cell, "rule")
CheckBack("thematic break", rule_cell)
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);

// Scrolling by line index on long.md: several screens of rendered lines, so each
// step lands on text that is not on the screen before it. The position cache removal makes
// the panel the start screen again; the cursor sits on "/..", long.md is one row down.
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
if (Exists(profile + "/.config/history/viewer.pos"))
	Remove(profile + "/.config/history/viewer.pos")
ExpectString("sample.md")
TypeDown()
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

// --- T4: --norgb and the settings/markdown-colors.ini overrides, each in a fresh far2l. ---
// Every restart below reopens the first page of sample.md; the first screen (h1, strong,
// code fence, body) is all these cases need.

// --norgb: the truecolor palette must not reach the screen; every role falls back to its
// readable 16-color index (IsForeTC false, a non-black Fore). A reused profile shows no
// help screen, so the start waits for the panels.
StartAppWithSize(["--tty", "--nodetect", "--norgb", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
if (Exists(profile + "/.config/history/viewer.pos"))
	Remove(profile + "/.config/history/viewer.pos")
TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")
var norgb_heading = ExpectString("Heading One")
var norgb_h3 = ExpectString("Heading Three")
var norgb_strong = ExpectString("strong")
var norgb_code = ExpectString("Code Fence Body")
// The exact 4-bit index of each spec fallback: F_YELLOW 14, F_LIGHTRED 12, F_LIGHTGRAY 7,
// F_WHITE 15, F_LIGHTGREEN 10, F_LIGHTMAGENTA 13, F_CYAN 3. A wrong index fails here.
function CheckNorgb(name, cell, expected) {
	if (cell.IsForeTC) {
		Log(name + ": IsForeTC is true, foreTC " + cell.ForeTC)
		Panic(name + " still shows a 24-bit color under --norgb")
	}
	if (cell.Fore != expected) {
		Log(name + ": fore " + cell.Fore + ", expected " + expected)
		Panic(name + " shows the wrong 16-color fallback")
	}
}
// The palette-driven roles must show their readable 16-color fallback; the plain body text
// keeps the viewer's own COL_VIEWERTEXT truecolor, which the palette module does not control.
CheckNorgb("norgb h1", ReadCell(norgb_heading.X, norgb_heading.Y), 14)
CheckNorgb("norgb h3", ReadCell(norgb_h3.X, norgb_h3.Y), 7)
CheckNorgb("norgb strong", ReadCell(norgb_strong.X, norgb_strong.Y), 15)
CheckNorgb("norgb code", ReadCell(norgb_code.X, norgb_code.Y), 13)
// The H2, link and quote cells sit on the last page; the thematic break is the last row.
TypeEnd()
var norgb_h2 = ExpectString("Heading Two")
CheckNorgb("norgb h2", ReadCell(norgb_h2.X, norgb_h2.Y), 12)
var norgb_link = ExpectString("labeled link")
CheckNorgb("norgb link", ReadCell(norgb_link.X, norgb_link.Y), 10)
var norgb_quote = ExpectString("block quote line")
CheckNorgb("norgb quote", ReadCell(norgb_quote.X + 2, norgb_quote.Y), 3)
var norgb_rule = ExpectString("────────────────────────────────────────────────────────────────────────────────")
// The rule takes the spec's 16-color fallback F_CYAN (3), the same index as quote.
CheckNorgb("norgb rule", ReadCell(norgb_rule.X, norgb_rule.Y), 3)
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);

// heading1=foreground:#00FF00: the h1 cell must show the override, the other roles keep their
// RGB defaults (strong stays white, not green).
var config = profile + "/.config"
SaveTextFile(config + "/settings/markdown-colors.ini", ["[markdown]", "heading1=foreground:#00FF00"])
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
if (Exists(profile + "/.config/history/viewer.pos"))
	Remove(profile + "/.config/history/viewer.pos")
TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")
var ovr_heading = ExpectString("Heading One")
var ovr_strong = ExpectString("strong")
var ovr_heading_cell = ReadCell(ovr_heading.X, ovr_heading.Y)
var ovr_strong_cell = ReadCell(ovr_strong.X, ovr_strong.Y)
if (ovr_heading_cell.ForeTC != 0x00ff00) {
	Log("h1 foreTC " + ovr_heading_cell.ForeTC)
	Panic("the heading1 override did not reach the h1 cell")
}
CheckForeOther("overridden h1", ovr_heading_cell, ovr_strong_cell, "strong")
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);

// link=@Help.Topic: a palette entry resolves at draw time. If the override were dropped, the
// cell would show the default link color, so it is compared with the first run's link cell.
SaveTextFile(config + "/settings/markdown-colors.ini", ["[markdown]", "link=@Help.Topic"])
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
if (Exists(profile + "/.config/history/viewer.pos"))
	Remove(profile + "/.config/history/viewer.pos")
TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")
// The link paragraph is below the fold on the first screen: page down to it.
TypePageDown()
var ovr_link = ExpectString("labeled link")
var ovr_link_cell = ReadCell(ovr_link.X, ovr_link.Y)
Log("link override foreTC " + ovr_link_cell.ForeTC.toString(16) + " fore " + ovr_link_cell.Fore + " isTC " + ovr_link_cell.IsForeTC)
var ovr_body = ReadCell(70, ovr_link.Y)
// Compared with the default labeled link read in the first run: if the override is dropped,
// the cell shows the default link color and this fails. The exact @Help.Topic color follows
// the theme and is not pinned; it can equal the body fore, so that pair is not asserted.
CheckForeOther("link override vs default link", ovr_link_cell, link_cell, "default link")
// An ignored @Name would fall back to the body fore: this pair catches it.
CheckForeOther("link override vs body", ovr_link_cell, ovr_body, "viewer")
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);

// Hostile ini: unknown key, bad value, a very long value line. The viewer opens with defaults
// for the bad entries; the h1 fore must equal its default and the code fence must too.
SaveTextFile(config + "/settings/markdown-colors.ini", [
	"[markdown]",
	"unknown=foreground:#123456",
	"heading1=not-a-color",
	"code=" + new Array(40).join("foreground:#abcdef "),
])
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
if (Exists(profile + "/.config/history/viewer.pos"))
	Remove(profile + "/.config/history/viewer.pos")
TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")
var bad_heading = ExpectString("Heading One")
var bad_code = ExpectString("Code Fence Body")
var bad_heading_cell = ReadCell(bad_heading.X, bad_heading.Y)
CheckForeSame("hostile ini h1", bad_heading_cell, heading_cell, "default h1")
CheckForeSame("hostile ini code", ReadCell(bad_code.X, bad_code.Y), code_cell, "default code")
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);

// An empty ini file: all defaults again.
SaveTextFile(config + "/settings/markdown-colors.ini", [])
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
if (Exists(profile + "/.config/history/viewer.pos"))
	Remove(profile + "/.config/history/viewer.pos")
TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")
var empty_heading = ExpectString("Heading One")
CheckForeSame("empty ini h1", ReadCell(empty_heading.X, empty_heading.Y), heading_cell, "default h1")
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);

// A light viewer background: the white and yellow RGB defaults fail the 3:1 contrast rule, so
// the all-or-none rule sends every role to today's look. The h1 and h2 cells must then differ
// from their RGB defaults (ffff5f and ffaf5f): a per-role or missing verdict fails here.
SaveTextFile(config + "/settings/farcolors.ini", [
	"[farcolors]",
	"Viewer.Text=background:#FFFFFF foreground:#000000, B_WHITE | F_BLACK",
])
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
if (Exists(profile + "/.config/history/viewer.pos"))
	Remove(profile + "/.config/history/viewer.pos")
TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")
var light_h1 = ExpectString("Heading One")
var light_h1_cell = ReadCell(light_h1.X, light_h1.Y)
if (light_h1_cell.ForeTC == 0xffff5f) {
	Log("light h1 foreTC " + light_h1_cell.ForeTC.toString(16))
	Panic("light background: h1 kept its RGB default, the contrast rule did not fire")
}
TypeEnd()
var light_h2 = ExpectString("Heading Two")
var light_h2_cell = ReadCell(light_h2.X, light_h2.Y)
if (light_h2_cell.ForeTC == 0xffaf5f) {
	Log("light h2 foreTC " + light_h2_cell.ForeTC.toString(16))
	Panic("light background: h2 kept its RGB default, the contrast rule did not fire")
}
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);
0;
