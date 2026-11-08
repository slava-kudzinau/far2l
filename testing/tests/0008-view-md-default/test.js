mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"

// Markdown view of sample.md: the heading shows without its "# " marker, and its paragraph sits
// on the blank row under it.
function CheckMarkdownSample() {
	var heading = ExpectString("Heading One")
	ExpectNoString("# Heading One")
	CheckCellChar(heading.X, heading.Y + 2, "A")
}

// Plain view of sample.md: the "# " marker shows.
function CheckPlainSample() {
	ExpectString("# Heading One")
}

// Markdown view of plain.txt: its "# plain" line renders as a heading, at the left edge.
function CheckMarkdownPlainTxt() {
	ExpectNoString("# plain")
	var line = ExpectString("plain", 0, 1, 80, 1)
	if (line.X != 0) {
		Log("plain.txt: the heading starts at column " + line.X);
		Panic("plain.txt: the heading is not at the left edge");
	}
}

// A fresh profile shows the help screen on start; a reused one would not.
RemoveAll(profile)

StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("Help - FAR2L");
TypeEscape()
ExpectString("OSC52");
TypeEscape()

// The setting is on (the default). The panel lists "/..", then plain.txt, then sample.md: F3 on
// sample.md (two rows down) opens it in Markdown mode. Escape closes it in that mode, which is
// the file's default, so no choice is saved.
TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")
CheckMarkdownSample()
TypeEscape()

// plain.txt is the row above sample.md. The last viewer closed in Markdown mode, but a .txt file
// opens plain: its "# plain" line stays as is.
TypeUp()
TypeFKey(3)
ExpectString("left/plain.txt")
ExpectString("# plain")
TypeEscape()

// Back on sample.md, with no saved choice: the rule opens it in Markdown again. Shift+F4 at the
// top switches it to plain, and Escape saves that choice (a plain choice against the Markdown rule).
TypeDown()
TypeFKey(3)
ExpectString("left/sample.md")
CheckMarkdownSample()
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
CheckPlainSample()
TypeEscape()

// F3 on sample.md opens plain: the saved choice wins over the rule.
TypeFKey(3)
ExpectString("left/sample.md")
CheckPlainSample()
TypeEscape()

// Hand change on plain.txt: Shift+F4 switches it to Markdown, a choice against the plain rule,
// and Escape saves it. F3 on plain.txt opens Markdown again.
TypeUp()
TypeFKey(3)
ExpectString("left/plain.txt")
ExpectString("# plain")
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
CheckMarkdownPlainTxt()
TypeEscape()
TypeFKey(3)
ExpectString("left/plain.txt")
CheckMarkdownPlainTxt()
TypeEscape()

// Quick View (Ctrl+Q on the right panel, then Tab to focus Quick View):
// plain.txt has a saved Markdown choice, so Quick View opens it in Markdown mode.
// Main F4 says Hex; Shift+F4 says Text, and after Shift+F4 toggles to plain, Shift+F4 says Markdown.
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
TypeTab()
ExpectString("4Hex   ", 0, 23, 80, 1)
ToggleShift(true)
ExpectString("4Text  ", 0, 23, 80, 1)
TypeFKey(4)
ExpectString("4Markdo", 0, 23, 80, 1)
ToggleShift(false)
ExpectString("4Hex   ", 0, 23, 80, 1)
TypeTab()
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)

TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
