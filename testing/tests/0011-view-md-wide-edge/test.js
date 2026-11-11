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

// Directories first: /.., dir.md, plain.txt, wide.md.
TypeDown(2)

// The left panel grows over the command line.
ToggleLCtrl(true)
ToggleShift(true)
TypeDown()
ToggleShift(false)
ToggleLCtrl(false)
Sleep(500)

// With the command line covered, quick view on plain.txt, then down onto wide.md and close it.
// The wide layout applies from inside a redraw of the panels; closing must still restore the
// half screen split and the two column list.
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
ExpectString("Plain", 41, 1, 16, 1)
TypeDown()
ExpectString("Wide", 21, 1, 16, 1)
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
ExpectString("╗╔", 38, 0, 4, 1)
ExpectString("║║", 38, 4, 4, 1)

TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
