mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"

// A fresh profile shows the help screen on start; a reused one would not.
RemoveAll(profile)

StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("Help - FAR2L");
TypeEscape()
ExpectString("OSC52");
TypeEscape()

// The panel lists "/..", then plain.txt, then sample.md: F3 on sample.md (two rows down).
TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")

// No Shift+F4: a .md file opens in Markdown mode. The heading shows without its "# " marker,
// with a double rule line under it.
var heading = ExpectString("Heading One")
ExpectNoString("# Heading One")
CheckCellChar(heading.X, heading.Y + 1, "═")

// Shift+F4 at the top of the file switches to plain text. The choice is saved on close (even
// with the top position), so F3 on sample.md again opens plain, not Markdown.
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
ExpectString("# Heading One")
TypeEscape()
TypeFKey(3)
ExpectString("left/sample.md")
ExpectString("# Heading One")
TypeEscape()

// plain.txt is the row above sample.md. The last viewer showed Markdown, but a .txt file
// opens plain: its "# plain" line stays as is.
TypeUp()
TypeFKey(3)
ExpectString("left/plain.txt")
ExpectString("# plain")

TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
