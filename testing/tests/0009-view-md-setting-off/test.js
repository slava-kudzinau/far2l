mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"
config=profile + "/.config/settings/config.ini"

// Markdown view of sample.md: the heading shows without its "# " marker, with a double rule
// line under it.
function CheckMarkdownSample() {
	ExpectString("═")
	ExpectNoString("# Heading One")
	var heading = ExpectString("Heading One")
	CheckCellChar(heading.X, heading.Y + 1, "═")
}

// Plain view of sample.md: the "# " marker shows and no rule line is drawn under the heading.
function CheckPlainSample() {
	var heading = ExpectString("# Heading One")
	if (CellCharMatches(heading.X, heading.Y + 1, "═"))
		Panic("sample.md: plain heading has a rule line under it")
}

// Plain view of plain.txt: its "# plain" line shows as is, with no rule line under it.
function CheckPlainTxt() {
	var line = ExpectString("# plain")
	if (CellCharMatches(line.X, line.Y + 1, "═"))
		Panic("plain.txt: plain line has a rule line under it")
}

// A fresh profile shows the help screen on start; a reused one would not.
RemoveAll(profile)

// Setting on (the default): F3 on sample.md (two rows down) opens it in Markdown mode. Escape
// closes it in that mode, which is the file's default, so no choice is saved for it.
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("Help - FAR2L");
TypeEscape()
ExpectString("OSC52");
TypeEscape()

TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")
CheckMarkdownSample()
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)

// Setting off: the test writes config.ini with only [Viewer] Markdown_Auto=0, replacing the file
// the first start wrote. The position cache is kept. sample.md has no saved choice, so with the
// setting off it must open plain, not Markdown.
SaveTextFile(config, ["[Viewer]", "Markdown_Auto=0"])
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("sample.md")
TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")
CheckPlainSample()

// Hand change with the setting off: Shift+F4 switches sample.md to Markdown, a choice against
// the plain rule, and Escape saves it. F3 on sample.md opens Markdown again.
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
CheckMarkdownSample()
TypeEscape()
TypeFKey(3)
ExpectString("left/sample.md")
CheckMarkdownSample()
TypeEscape()

// plain.txt was never opened in this profile. The setting is off, so it opens plain. The Markdown
// mode Shift+F4 set above is not a start mode: it must not make plain.txt open in Markdown.
TypeUp()
TypeFKey(3)
ExpectString("left/plain.txt")
CheckPlainTxt()
TypeEscape()

TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
