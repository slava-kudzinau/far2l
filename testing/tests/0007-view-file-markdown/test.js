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
TypeDown()
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
var code = ExpectString("Code Fence Body")
CheckBack("code", ReadCell(code.X, code.Y))
CheckFore("code", ReadCell(code.X, code.Y))
var table = ExpectString("│ cell1")
CheckBack("table box glyph", ReadCell(table.X, table.Y))
CheckBack("table cell", ReadCell(table.X + 2, table.Y))

// Scroll to the bottom, where the mermaid fence is. End is the scroll key that
// re-renders the markdown view; PgDn and the arrow keys do not yet.
TypeEnd()
ExpectNoString("Heading One")
ExpectString("Start")
ExpectString("Finish")
ExpectString("┌──────────────────┐")
ExpectNoString("```mermaid")
var quote = ExpectString("│ block quote line")
CheckFore("block quote", ReadCell(quote.X + 2, quote.Y))

TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);
0;
