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

// Two cells share a background: the code row and the blank cell past its text ride
// the block's tint (story markdown-code-tint), so they are compared with each other.
function CheckSameBack(name, cell, ref, refName) {
	if (cell.Back != ref.Back || cell.IsBackTC != ref.IsBackTC || cell.BackTC != ref.BackTC) {
		Log(name + ": back " + cell.Back + " tc " + cell.IsBackTC + " " + cell.BackTC + ", " + refName + " back " + ref.Back + " tc " + ref.IsBackTC + " " + ref.BackTC)
		Panic(name + " background differs from the " + refName + " background")
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
ExpectString("4Hex   ", 0, 23, 80, 1)
ToggleShift(true)
ExpectString("4Text  ", 0, 23, 80, 1)
ToggleShift(false)
// F3 on a .md file opens Markdown mode by default: switch sample.md to plain first.
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
var plain_line = ExpectString("# Heading One")
var plain = ReadCell(60, plain_line.Y)
ExpectString("4Hex   ", 0, 23, 80, 1)
ToggleShift(true)
ExpectString("4Markdo", 0, 23, 80, 1)
ToggleShift(false)

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
// The look resolves from the Colorer scheme first (spec, Resolver): with the plugin
// loaded the viewer paints the scheme's rows, not the fork table.  HeadText parents
// def:URI of far2l.hrd - #8888ff as 0xBBGGRR reads 0xff8888, with the style word
// 4, so the heading underlines.  The V1 look carries no 4-bit index: fore reads 0.
if (heading_cell.ForeTC != 0xff8888 || heading_cell.Fore != 0) {
	Log("h1 foreTC " + heading_cell.ForeTC.toString(16) + " fore " + heading_cell.Fore)
	Panic("h1 is not the scheme's #8888ff def:URI row")
}
if (!heading_cell.Underscore) {
	Log("h1 underline " + heading_cell.Underscore)
	Panic("h1 lost the def:URI underline")
}
CheckBack("blank viewer cell", ReadCell(60, heading.Y))
// The strong span in the first paragraph takes its own color, distinct from heading and body.
var strong_text = ExpectString("strong")
var strong_cell = ReadCell(strong_text.X, strong_text.Y)
CheckForeOther("strong", strong_cell, plain, "viewer")
CheckForeOther("strong", strong_cell, heading_cell, "heading")
// A level-4 heading shares the h3 color (H3 to H6 are one role); the list comes on the row
// after the heading and its blank row.  The scheme maps every heading level to one HeadText
// region, so the h4 and h3 cells share the h1 color too.
var h4 = ExpectString("Heading Four")
CheckForeSame("heading four", ReadCell(h4.X, h4.Y), heading_cell, "h1")
var h3 = ExpectString("Heading Three")
var h3_cell = ReadCell(h3.X, h3.Y)
CheckForeSame("heading four", ReadCell(h4.X, h4.Y), h3_cell, "h3")
CheckForeSame("h3", h3_cell, heading_cell, "h1")
var bullet = ExpectString("bullet one")
var body_cell = ReadCell(bullet.X, bullet.Y)
CheckRowOf("list after the h3 heading and its blank row", bullet, h3.Y + 2)
// Code takes its own palette entry, different from the heading and from the body.
var code = ExpectString("Code Fence Body")
// The fence row rides the code tint: the text cell and the cell past the text share the
// row's tinted background, one eighth of the code color into the viewer background.
var code_cell = ReadCell(code.X, code.Y)
CheckSameBack("code", code_cell, ReadCell(70, code.Y), "code row fill")
CheckForeOther("code", code_cell, plain, "viewer")
CheckForeOther("code", code_cell, heading_cell, "heading")
var table = ExpectString("│ cell1")
CheckBack("table box glyph", ReadCell(table.X, table.Y))
CheckBack("table cell", ReadCell(table.X + 2, table.Y))
// Table body text carries the body role: the scheme's def:Text row (#99EEEE -> 0xeeee99),
// the color of the viewer's body text, not the plain-mode blank cell.
CheckForeSame("table cell text", ReadCell(table.X + 2, table.Y), body_cell, "body")
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

// Scroll down to the mermaid fences.
TypePageDown()
ExpectNoString("Heading One")
ExpectString("Start")
ExpectString("Finish")
ExpectNoString("```mermaid")
// The flowchart is drawn as box art in the art role, with no source text left on screen.
var flow = ExpectString("└───────┘     └────────┘")
var flow_art = ReadCell(flow.X, flow.Y)
ExpectNoString("graph LR")
// The swimlane block is not drawn: its code shows, and the hint row follows the code.
var swim_last = ExpectString("a1[Start] --> a2[Finish]")
var hint = ExpectString("(diagram not drawn here - Alt-F3 opens it in the browser)")
if (hint.Y != swim_last.Y + 2) {
	Log("swimlane code on row " + swim_last.Y + ", hint on row " + hint.Y);
	Panic("the hint does not follow the swimlane code after its end line");
}
// The hint row is body text: the scheme's def:Text row, like all body cells.
CheckForeSame("diagram hint", ReadCell(hint.X, hint.Y), body_cell, "body")
TypeEnd()
// Quote text and the quote bar take the quote role: today the viewer text, so the check reads
// the first quote and keeps the cell for the later pairs.
var quote = ExpectString("│ block quote line")
var quote_cell = ReadCell(quote.X + 2, quote.Y)
CheckForeOther("block quote", quote_cell, border_cell, "table border")
CheckForeOther("block quote", quote_cell, plain, "viewer")


// --- T4: the rest of the palette roles, checked against the cells read above. ---
// The scheme colors H1 to H6 through one HeadText region, so every heading level - renamed
// or setext included - shares the h1 color; level checks collapse to equality.
var h2 = ExpectString("Heading Two")
var h2_cell = ReadCell(h2.X, h2.Y)
var h3r = ExpectString("Heading Three Renamed")
var h3r_cell = ReadCell(h3r.X, h3r.Y)
var h5 = ExpectString("Heading Five")
CheckForeSame("heading five", ReadCell(h5.X, h5.Y), h3_cell, "h3")
var h6 = ExpectString("Heading Six")
CheckForeSame("heading six", ReadCell(h6.X, h6.Y), h3_cell, "h3")
CheckForeSame("h2", h2_cell, heading_cell, "h1")
CheckForeSame("h2", h2_cell, h3_cell, "h3")
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
CheckForeSame("row after the setext break", ReadCell(0, setext_h2.Y + 2), quote_cell, "quote")
CheckCellChar(0, setext_h2.Y + 2, "│")
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

// After three pages the top row is Item 059, a list item of long.md, so the round trip below tests a list row.
TypePageDown(3)
CheckTop("list item in markdown", ExpectString("Item 059"))
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
CheckTop("list item in plain", ExpectString("- Item 059 list text for the scrolling tests."))
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
ExpectNoString("- Item 059")
CheckTop("list item back in markdown", ExpectString("Item 059"))

// Hex with the Markdown flag still set: Down moves by one 16-byte row, not by a markdown line.
TypeHome()
TypeFKey(4)
CheckTop("hex first row", ExpectString("0000000000:"))
ExpectString("4Text  ", 0, 23, 80, 1)
ToggleShift(true)
ExpectString("4Text  ", 0, 23, 80, 1)
ToggleShift(false)
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
// Page 1: the title, the lead paragraph, the lists, the table, the quote.
// The leading empty line on line 1 produces a blank row above the title.
CheckBlankRow("blank row before the title", 1);
var title = ExpectString("Spacing Title");
CheckRowOf("title as the first content row", title, 2);
CheckBlankRow("blank row after the title", title.Y + 1);
var lead = ExpectString("Lead paragraph of the spacing test.");
CheckRowOf("lead paragraph after the title", lead, title.Y + 2);
var heading = ExpectString("Spacing Heading");
CheckBlankRow("blank row before the heading", heading.Y - 1);
CheckRowOf("heading after the lead paragraph", heading, lead.Y + 2);

// Spacing Heading then paragraph: one blank row between them from the source empty line.
var p1 = ExpectString("Paragraph before two source blanks.");
CheckBlankRow("blank row between heading and paragraph", heading.Y + 1);
CheckRowOf("paragraph after the heading and its blank row", p1, heading.Y + 2);

// Two source blanks between paragraphs: two blank rows.
var p2 = ExpectString("Paragraph after two source blanks.");
CheckBlankRow("first blank row between two paragraphs", p1.Y + 1);
CheckBlankRow("second blank row between two paragraphs", p1.Y + 2);
CheckRowOf("two source blank lines give two blank rows", p2, p1.Y + 3);

// Items of a list stay tight; the nested list gets no blank row; source empty line before the list.
var item1 = ExpectString("Spacing item one");
CheckBlankRow("blank row before the list", item1.Y - 1);
CheckRowOf("list after the paragraph and its blank row", item1, p2.Y + 2);
var nested = ExpectString("Spacing nested item");
var item2 = ExpectString("Spacing item two");
CheckRowOf("nested item right under its item", nested, item1.Y + 1);
CheckRowOf("next item right under the nested item", item2, item1.Y + 2);

// The table header has a blank row before it from the source empty line. The table has one rule.
var table_sp = ExpectString("Spacing A");
CheckBlankRow("blank row before the table", table_sp.Y - 1);
CheckRowOf("table header after the list and its blank row", table_sp, item2.Y + 2);
CheckCellChar(table_sp.X - 2, table_sp.Y + 1, "├");
CheckRowOf("table body row under the rule", ExpectString("spacing 1"), table_sp.Y + 2);

// The quote has a blank row before it that keeps its lead.
CheckCellChar(0, table_sp.Y + 3, "│");
var quote_lead_blank = BoundedLine(1, table_sp.Y + 3, 79, " \t");
if (quote_lead_blank != "") {
	Log("quote blank row has text: " + quote_lead_blank);
	Panic("quote blank row is not blank after the lead");
}
var quote_sp = ExpectString("Spacing quote line.");
CheckRowOf("quote after the table and its blank row", quote_sp, table_sp.Y + 4);
CheckBlankRow("blank row after the quote", quote_sp.Y + 1);
CheckNoBorder();

// Page 2: the code fences, a fence whose last source line is empty, a list with a fence,
// a loose list, and paragraphs around them.
TypePageDown();
CheckNoBorder();

var fence = ExpectString("Spacing fence line.");
CheckBlankRow("blank row before the code fence", fence.Y - 1);
CheckBlankRow("blank row after the code fence", fence.Y + 1);
var after_fence = ExpectString("Paragraph after the fence.");
CheckRowOf("paragraph after the code fence", after_fence, fence.Y + 2);
CheckBlankRow("blank row after the paragraph", after_fence.Y + 1);

var empty_last = ExpectString("Fence with an empty last line.");
CheckRowOf("code fence with an empty last line", empty_last, after_fence.Y + 2);
CheckBlankRow("empty last source line inside the fence", empty_last.Y + 1);
CheckBlankRow("blank row after the fence with an empty last line", empty_last.Y + 2);
var after_empty = ExpectString("Paragraph after the empty last line.");
CheckRowOf("paragraph after that fence", after_empty, empty_last.Y + 3);
CheckBlankRow("blank row after the paragraph", after_empty.Y + 1);

// The fence in a list item sits tight under the item text because there is no source empty line;
// the paragraph after the list keeps its blank row from the source empty line.
var item_fence = ExpectString("Spacing item with a fence:");
CheckRowOf("list after the paragraph and its blank row", item_fence, after_empty.Y + 2);
var item_code = ExpectString("Spacing item fence line.");
CheckRowOf("fence in the item tight under its item text", item_code, item_fence.Y + 1);
CheckBlankRow("blank row after the fence in the item", item_code.Y + 1);
var after_list = ExpectString("Spacing paragraph after the list.");
CheckRowOf("paragraph after the list, from the source blank", after_list, item_code.Y + 2);
CheckBlankRow("blank row after the paragraph", after_list.Y + 1);

var item_p = ExpectString("Spacing item with a paragraph.");
CheckRowOf("loose list after the paragraph and its blank row", item_p, after_list.Y + 2);
CheckBlankRow("blank row between the item's paragraphs", item_p.Y + 1);
var item_p2 = ExpectString("Spacing item paragraph two.");
CheckRowOf("second paragraph in the item", item_p2, item_p.Y + 2);
CheckBlankRow("blank row after the list item", item_p2.Y + 1);

var last = ExpectString("Spacing last paragraph.");
CheckRowOf("paragraph after the list item and its blank row", last, item_p2.Y + 2);
CheckBlankRow("blank row after the paragraph", last.Y + 1);
var c_para = ExpectString("Compact para before list.");
CheckRowOf("compact paragraph before list", c_para, last.Y + 2);
CheckNoBorder();

// Page 3: the start of the compact blocks, headings of levels 1, 2 and 3, and fences around lists.
TypePageDown();
CheckNoBorder();

// Compact layout: list sits tight right after paragraph when no source blank line separates them.
var c_para_p3 = ExpectString("Compact para before list.");
var c_item = ExpectString("Compact item after para.");
CheckRowOf("list right after the paragraph", c_item, c_para_p3.Y + 1);
CheckBlankRow("blank row between list and paragraph", c_item.Y + 1);
var c_after_list = ExpectString("Compact para after list.");
CheckRowOf("paragraph after the list and its blank row", c_after_list, c_item.Y + 2);

// R1 and R2: a heading of level 1 keeps one blank row before it and one after it from source empty lines.
var c_top = ExpectString("Compact Top");
CheckBlankRow("blank row before the level 1 heading", c_top.Y - 1);
CheckRowOf("level 1 heading after the list and its blank row", c_top, c_after_list.Y + 2);
CheckBlankRow("blank row after the level 1 heading", c_top.Y + 1);
var c_top_item = ExpectString("Compact item after top.");
CheckRowOf("list after the heading and its blank row", c_top_item, c_top.Y + 2);

// R1 and R3: a heading of level 2 keeps blank rows where source empty lines sit.
var c_second = ExpectString("Compact Second");
CheckBlankRow("blank row before a level 2 heading", c_second.Y - 1);
CheckBlankRow("blank row after a level 2 heading", c_second.Y + 1);
var c_second_item = ExpectString("Compact item after second.");
CheckRowOf("list after a level 2 heading and its blank row", c_second_item, c_second.Y + 2);

// C1: a heading after a heading keeps one blank row from source. R3: paragraph after heading keeps its blank row.
var c_upper = ExpectString("Compact Upper");
CheckBlankRow("blank row before heading", c_upper.Y - 1);
CheckRowOf("heading after the list and its blank row", c_upper, c_second_item.Y + 2);
var c_lower = ExpectString("Compact Lower");
CheckBlankRow("blank row between two headings", c_upper.Y + 1);
CheckRowOf("heading under a heading with a blank row", c_lower, c_upper.Y + 2);
CheckBlankRow("blank row after heading", c_lower.Y + 1);
var c_after_lower = ExpectString("Compact para after lower.");
CheckRowOf("paragraph after heading and its blank row", c_after_lower, c_lower.Y + 2);

// C2: a code fence after heading keeps its blank row from source. C5: a list after a code fence has a blank row.
var c_fence_head = ExpectString("Compact Before Fence");
CheckBlankRow("blank row before heading", c_fence_head.Y - 1);
CheckRowOf("heading after the paragraph and its blank row", c_fence_head, c_after_lower.Y + 2);
CheckBlankRow("blank row before code fence", c_fence_head.Y + 1);
var c_fence_line = ExpectString("Compact fence after heading.");
CheckRowOf("code fence after heading and its blank row", c_fence_line, c_fence_head.Y + 2);
CheckNoBorder();

// Page 4: the list after the fence, headings of levels 2 and 6, the setext heading, and list items.
TypePageDown();
CheckNoBorder();

var c_fence_line_p4 = ExpectString("Compact fence after heading.");
CheckBlankRow("blank row after the fence", c_fence_line_p4.Y + 1);
var c_fence_item = ExpectString("Compact item after fence.");
CheckRowOf("list after a code fence with a blank row", c_fence_item, c_fence_line_p4.Y + 2);

// C6: a code fence after a list has a blank row. C7: a heading after a list has a blank row.
var c_fence_after_item = ExpectString("Compact fence after item.");
CheckBlankRow("blank row between the list item and the fence", c_fence_item.Y + 1);
CheckRowOf("code fence after the list item and its blank row", c_fence_after_item, c_fence_item.Y + 2);
CheckBlankRow("blank row between the fence and the list item", c_fence_after_item.Y + 1);
var c_item_before_head = ExpectString("Compact item before heading.");
CheckRowOf("list item after the fence and its blank row", c_item_before_head, c_fence_after_item.Y + 2);
CheckBlankRow("blank row before the heading after the list", c_item_before_head.Y + 1);
var c_after_item = ExpectString("Compact After Item");
CheckRowOf("heading after a list with a blank row", c_after_item, c_item_before_head.Y + 2);
CheckBlankRow("blank row after the heading", c_after_item.Y + 1);
var c_fence_item_one = ExpectString("Compact fence item one:");
CheckRowOf("list after a heading and its blank row", c_fence_item_one, c_after_item.Y + 2);

// The fence in a list item: tight under the item text with no blank row, then the next item tight under the fence.
var c_fence_in_item = ExpectString("Compact fence in item.");
CheckRowOf("fence in item right under item text", c_fence_in_item, c_fence_item_one.Y + 1);
var c_fence_item_two = ExpectString("Compact fence item two.");
CheckRowOf("next item right under the fence", c_fence_item_two, c_fence_in_item.Y + 1);
CheckBlankRow("blank row between lists", c_fence_item_two.Y + 1);
var c_ordered = ExpectString("Compact ordered item.");
CheckRowOf("ordered list after the bullet list and its blank row", c_ordered, c_fence_item_two.Y + 2);

// R3 for heading level 6: blank rows from source before and after.
var c_six = ExpectString("Compact Six");
CheckBlankRow("blank row before level 6 heading", c_six.Y - 1);
CheckRowOf("level 6 heading after the list and its blank row", c_six, c_ordered.Y + 2);
CheckBlankRow("blank row after level 6 heading", c_six.Y + 1);
var c_after_six = ExpectString("Compact para after six.");
CheckRowOf("paragraph after level 6 heading and its blank row", c_after_six, c_six.Y + 2);

// A setext heading of level 2 keeps its source blank rows.
var c_setext = ExpectString("Compact Setext");
CheckBlankRow("blank row before a setext heading", c_setext.Y - 1);
CheckRowOf("setext heading after the paragraph and its blank row", c_setext, c_after_six.Y + 2);
CheckBlankRow("blank row after a setext heading", c_setext.Y + 1);
CheckNoBorder();

// Page 5: the paragraph after setext, setext heading of level 1, the thematic break, the table,
// and the list item with a heading.
TypePageDown();
CheckNoBorder();

var c_after_setext = ExpectString("Compact para after setext.");
CheckRowOf("paragraph after a setext heading and its blank row", c_after_setext, 2);

// A setext heading of level 1 keeps one blank row before and after it from source blanks.
var c_setext_top = ExpectString("Compact Setext Top");
CheckBlankRow("blank row before a setext heading of level 1", c_setext_top.Y - 1);
CheckRowOf("setext heading of level 1", c_setext_top, c_after_setext.Y + 2);
CheckBlankRow("blank row after a setext heading of level 1", c_setext_top.Y + 1);
var c_above_break = ExpectString("Compact para above break.");
CheckRowOf("paragraph after a level 1 heading and its blank row", c_above_break, c_setext_top.Y + 2);

// Thematic break HR scan: empty line before the break shows a blank row above it.
CheckBlankRow("blank row before the thematic break", c_above_break.Y + 1);
CheckCellChar(0, c_above_break.Y + 2, "─");
CheckCellChar(70, c_above_break.Y + 2, "─");
var c_break = ExpectString("Compact para below break.");
CheckBlankRow("blank row after the thematic break", c_above_break.Y + 3);
CheckRowOf("paragraph after the thematic break and its blank row", c_break, c_above_break.Y + 4);

// R6 and R7: the table has a blank row before it from source and one rule under its header row.
var c_head = ExpectString("Compact A");
CheckBlankRow("blank row before the table", c_head.Y - 1);
CheckRowOf("table header after the paragraph and its blank row", c_head, c_break.Y + 2);
CheckCellChar(c_head.X - 2, c_head.Y + 1, "├");
var c_cell = ExpectString("compact 1");
CheckRowOf("body row under the header rule", c_cell, c_head.Y + 2);
CheckCellChar(c_cell.X - 2, c_cell.Y - 1, "├");
CheckBlankRow("blank row after the table", c_cell.Y + 1);
var c_after_table = ExpectString("Compact para after table.");
CheckRowOf("paragraph after the table and its blank row", c_after_table, c_cell.Y + 2);

// C13: a heading as the first block of a list item stays on the row of the bullet.
var c_item_head = ExpectString("Compact Item Heading");
CheckBlankRow("blank row before the heading item", c_item_head.Y - 1);
CheckRowOf("list item after the paragraph and its blank row", c_item_head, c_after_table.Y + 2);
CheckCellChar(c_item_head.X - 2, c_item_head.Y, "•");
CheckNoBorder();

// Page 6: the first review blocks, right after the heading item.
TypePageDown();
CheckNoBorder();

// C7: a heading after a list keeps its blank row from source. An empty item (`-` with no text)
// shows no row of its own.
var r_item1 = ExpectString("review item one");
var r_head_empty = ExpectString("Review Head After Empty");
CheckBlankRow("blank row between the item and the heading", r_item1.Y + 1);
CheckRowOf("heading after the list with an empty item and its blank row", r_head_empty, r_item1.Y + 2);
CheckBlankRow("blank row after heading", r_head_empty.Y + 1);
var r_para2 = ExpectString("Review para two.");
CheckRowOf("paragraph after heading and its blank row", r_para2, r_head_empty.Y + 2);

// C6: a code block after a list keeps its blank row from source, also after an empty item.
var r_item2 = ExpectString("review item two");
CheckBlankRow("blank row before item", r_item2.Y - 1);
CheckRowOf("list item after paragraph and its blank row", r_item2, r_para2.Y + 2);
var r_code_empty = ExpectString("review code after empty");
CheckBlankRow("blank row between the item and the code block", r_item2.Y + 1);
CheckRowOf("code block after the list with an empty item and its blank row", r_code_empty, r_item2.Y + 2);
CheckBlankRow("blank row after code block", r_code_empty.Y + 1);
var r_para3 = ExpectString("Review para three.");
CheckRowOf("paragraph after code block and its blank row", r_para3, r_code_empty.Y + 2);

// R5: an HTML block has blank rows where source empty lines sit.
var r_html1 = ExpectString("review html one");
CheckBlankRow("blank row before the html block", r_para3.Y + 1);
CheckRowOf("html block after the paragraph and its blank row", r_html1, r_para3.Y + 2);
CheckBlankRow("blank row after the html block", r_html1.Y + 1);
var r_para4 = ExpectString("Review para four.");
CheckRowOf("paragraph after the html block and its blank row", r_para4, r_html1.Y + 2);

// Review Head Over Html
var r_head_html = ExpectString("Review Head Over Html");
CheckBlankRow("blank row before heading over html", r_head_html.Y - 1);
CheckRowOf("heading after the paragraph and its blank row", r_head_html, r_para4.Y + 2);
CheckBlankRow("blank row between heading and html", r_head_html.Y + 1);
var r_html2 = ExpectString("review html two");
CheckRowOf("html block after heading and its blank row", r_html2, r_head_html.Y + 2);
CheckBlankRow("blank row after html block", r_html2.Y + 1);
var r_para5 = ExpectString("Review para five.");
CheckRowOf("paragraph after the html block and its blank row", r_para5, r_html2.Y + 2);
CheckBlankRow("blank row after paragraph five", r_para5.Y + 1);
CheckNoBorder();

// Page 7, the last page: the fence in an item, the heading and the html block as the first
// block of a list item.
TypePageDown();
CheckNoBorder();

var r_para5_p7 = ExpectString("Review para five.");
CheckBlankRow("blank row before the fence item", r_para5_p7.Y + 1);

// C8 and C13: a fence as the first block of an item stays on the row of the bullet.
var r_fence_first = ExpectString("review fence first");
CheckRowOf("fence as first block of item after paragraph and its blank row", r_fence_first, r_para5_p7.Y + 2);
CheckBlankRow("blank row after fence item", r_fence_first.Y + 1);
var r_para6 = ExpectString("Review para six.");
CheckRowOf("paragraph after fence item and its blank row", r_para6, r_fence_first.Y + 2);
CheckBlankRow("blank row before item", r_para6.Y + 1);
var r_item3 = ExpectString("review item three");
CheckRowOf("item after paragraph and its blank row", r_item3, r_para6.Y + 2);

// C13: a heading of level 1 as the first block of an item stays on the row of the bullet.
// C3: it keeps one blank row after it from source.
var r_top = ExpectString("Review Item Top");
CheckBlankRow("blank row before heading in item", r_top.Y - 1);
CheckRowOf("heading in item after item text and its blank row", r_top, r_item3.Y + 2);
CheckBlankRow("blank row after heading in item", r_top.Y + 1);
var r_item4 = ExpectString("review item four");
CheckRowOf("item after heading in item and its blank row", r_item4, r_top.Y + 2);

// C13 and R5: an html block as the first block of an item stays on the row of the bullet,
// and keeps its blank row after it from source.
var r_html3 = ExpectString("review html three");
CheckBlankRow("blank row before html in item", r_html3.Y - 1);
CheckRowOf("html block in item after item text and its blank row", r_html3, r_item4.Y + 2);
CheckBlankRow("blank row after html block in the item", r_html3.Y + 1);
var r_item5 = ExpectString("review item five");
CheckRowOf("item after the html block in the item", r_item5, r_html3.Y + 2);
CheckBlankRow("blank row after item", r_item5.Y + 1);
var r_para7 = ExpectString("Review para seven.");
CheckRowOf("paragraph after item and its blank row", r_para7, r_item5.Y + 2);

// R4 and R5: a list has blank rows where source empty lines sit, and the html block after it
// keeps its blank row.
var r_item6 = ExpectString("review item six");
CheckBlankRow("blank row before item", r_item6.Y - 1);
CheckRowOf("item after paragraph and its blank row", r_item6, r_para7.Y + 2);
var r_html4 = ExpectString("review html four");
CheckRowOf("html block after the list and its blank row", r_html4, r_item6.Y + 2);
CheckBlankRow("blank row between the list and the html block", r_item6.Y + 1);
CheckNoBorder();


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
function CheckNorgb(name, cell, tc, expected) {
	// --norgb keeps the 256-color look in the attribute (R52); a 256-color
	// terminal paints ForeTC, a 16-color terminal paints Fore.
	if (cell.ForeTC != tc) {
		Log(name + ": foreTC " + cell.ForeTC.toString(16) + ", expected " + tc.toString(16))
		Panic(name + " shows the wrong 256-color look under --norgb")
	}
	if (cell.Fore != expected) {
		Log(name + ": fore " + cell.Fore + ", expected " + expected)
		Panic(name + " shows the wrong 16-color fallback")
	}
}
// The palette-driven roles must show their readable 16-color fallback; the plain body text
// keeps the viewer's own COL_VIEWERTEXT truecolor, which the palette module does not control.
CheckNorgb("norgb h1", ReadCell(norgb_heading.X, norgb_heading.Y), 0x5fffff, 14)
CheckNorgb("norgb h3", ReadCell(norgb_h3.X, norgb_h3.Y), 0xffaf87, 7)
CheckNorgb("norgb strong", ReadCell(norgb_strong.X, norgb_strong.Y), 0xffffff, 15)
CheckNorgb("norgb code", ReadCell(norgb_code.X, norgb_code.Y), 0xafd7d7, 13)
// The H2, link and quote cells sit on the last page; the thematic break is the last row.
TypeEnd()
var norgb_h2 = ExpectString("Heading Two")
CheckNorgb("norgb h2", ReadCell(norgb_h2.X, norgb_h2.Y), 0x5fafff, 12)
var norgb_link = ExpectString("labeled link")
CheckNorgb("norgb link", ReadCell(norgb_link.X, norgb_link.Y), 0x87ff87, 10)
var norgb_quote = ExpectString("block quote line")
CheckNorgb("norgb quote", ReadCell(norgb_quote.X + 2, norgb_quote.Y), 0xd7afaf, 3)
var norgb_rule = ExpectString("────────────────────────────────────────────────────────────────────────────────")
// The rule takes the spec's 16-color fallback F_CYAN (3), the same index as quote.
CheckNorgb("norgb rule", ReadCell(norgb_rule.X, norgb_rule.Y), 0xaf8787, 3) // spec 256 index 103 (R52)
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);

// heading1=foreground:#FF8000: the h1 cell must show the override, the other roles keep their
// RGB defaults (strong stays white). The value has unequal R and B, so a swap fails the check.
var config = profile + "/.config"
SaveTextFile(config + "/settings/markdown-colors.ini", ["[markdown]", "heading1=foreground:#FF8000"])
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
if (ovr_heading_cell.ForeTC != 0x0080ff) {
	Log("h1 foreTC " + ovr_heading_cell.ForeTC.toString(16))
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
// from their RGB defaults (#FFFF5F and #FFAF5F, read as 5fffff and 5fafff): a per-role or missing verdict fails here.
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
if (light_h1_cell.ForeTC == 0x5fffff) {
	Log("light h1 foreTC " + light_h1_cell.ForeTC.toString(16))
	Panic("light background: h1 kept its RGB default, the contrast rule did not fire")
}
TypeEnd()
var light_h2 = ExpectString("Heading Two")
var light_h2_cell = ReadCell(light_h2.X, light_h2.Y)
if (light_h2_cell.ForeTC == 0x5fafff) {
	Log("light h2 foreTC " + light_h2_cell.ForeTC.toString(16))
	Panic("light background: h2 kept its RGB default, the contrast rule did not fire")
}
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);
0;
