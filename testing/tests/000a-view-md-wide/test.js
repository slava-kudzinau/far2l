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
// Alphabetical list: /.., plain.txt, wide.md. Two rows down selects wide.md.
TypeDown(2)

// Ctrl+Q opens quick view on wide.md: a 20 column strip of the file list stays on the left
// (its border meets the quick view border at row 0), and the viewer fills the other 60 columns.
// "india" ends past column 70: on the old half screen the line wraps before it.
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
ExpectString("Wide", 21, 1, 16, 1)
ExpectString("india", 66, 2, 12, 1)
ExpectString("╗╔", 18, 0, 4, 1)
ExpectString("wide.md", 1, 4, 10, 1)
// Up onto plain.txt: quick view shows it and the half-screen split restores (box in right half).
TypeUp()
ExpectString("Plain", 41, 1, 16, 1)

// Down back onto wide.md: the wide layout applies again (acceptance 3).
TypeDown()
ExpectString("Wide", 21, 1, 16, 1)
ExpectString("india", 66, 2, 12, 1)

// Ctrl+Q closes the quick view while it is wide: the half-screen split restores.
// The border of the left panel meets the border of the right panel in the middle of row 0.
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
ExpectString("╗╔", 38, 0, 4, 1)

// Quit while the wide quick view is open: the left panel goes first, so the quick view destructor
// must not use it.
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
ExpectString("╗╔", 18, 0, 4, 1)

TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
