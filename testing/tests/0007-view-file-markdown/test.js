mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"

// Colors: a markdown cell must have the viewer background, and text the viewer foreground.
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
CheckFore("heading", ReadCell(heading.X, heading.Y))
CheckBack("blank viewer cell", ReadCell(60, heading.Y))
// A level-4 heading is drawn in the viewer colour like body text; its layout cue is the rule line below it.
var h4 = ExpectString("Heading Four")
CheckCellChar(h4.X, h4.Y + 1, "─")
CheckFore("heading four rule", ReadCell(h4.X, h4.Y + 1))
var code = ExpectString("Code Fence Body")
CheckBack("code", ReadCell(code.X, code.Y))
CheckFore("code", ReadCell(code.X, code.Y))
var table = ExpectString("│ cell1")
CheckBack("table box glyph", ReadCell(table.X, table.Y))
CheckBack("table cell", ReadCell(table.X + 2, table.Y))

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
// The markdown mode is kept per file, so switch this new file to it (Shift+F4).
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
ExpectString("Long Start")
ExpectNoString("# Long Start")
ExpectString("  0%")

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

// Down on the last page does not scroll past it: the last line keeps its row.
var last_row = ExpectString("Final paragraph")
TypeDown(3)
var still_last = ExpectString("Final paragraph")
if (still_last.Y != last_row.Y) {
	Log("Final paragraph moved from row " + last_row.Y + " to row " + still_last.Y);
	Panic("Down scrolled past the last full page");
}
ExpectString("100%")

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
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);
0;
