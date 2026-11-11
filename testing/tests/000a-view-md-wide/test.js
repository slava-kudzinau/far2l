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

// Ctrl+Q opens quick view on wide.md: the viewer spans the screen, TAILWORD sits past the border.
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
ExpectString("Wide", 1, 1, 16, 1)
ExpectString("TAILWORD", 64, 2, 16, 1)
// Up onto plain.txt: quick view shows it and the half-screen split restores (box in right half).
TypeUp()
ExpectString("Plain", 41, 1, 16, 1)

// Down back onto wide.md: the wide layout applies again (acceptance 3).
TypeDown()
ExpectString("Wide", 1, 1, 16, 1)
ExpectString("TAILWORD", 64, 2, 16, 1)

// Ctrl+Q closes the quick view; quit.
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
