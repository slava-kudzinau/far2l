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

// Down moves the top by one rendered line: the heading row scrolls off, the next row stays.
TypeDown()
ExpectNoString("Long Start")
ExpectString("Row 001")

// PgDn moves by a screen minus one line: the first rows go, later rows show, never a blank screen.
TypePageDown()
ExpectNoString("Row 001")
ExpectString("Row 030")

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

// Home goes back to the first page.
TypeHome()
ExpectString("Long Start")
ExpectString("  0%")

TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);
0;
