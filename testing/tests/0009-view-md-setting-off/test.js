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

// Setting off: Markdown_Auto=0 in the config far2l wrote on exit. The position cache is kept.
// sample.md has no saved choice, so with the setting off it must open plain, not Markdown.
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

TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
