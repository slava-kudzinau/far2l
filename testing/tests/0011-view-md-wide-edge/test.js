mydir=WorkDir()
profile=mydir + "/profile"
config=profile + "/.config/settings/config.ini"
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
// A directory named dir.md is not a Markdown file: quick view keeps the half screen split
// (its box starts at column 40, and the file list keeps two columns).
TypeDown()
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
ExpectString("╗╔", 38, 0, 4, 1)
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
TypeDown()

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
TypeTab()
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

// Shift+F9 saves the setup while the quick view is wide. The save gives back the half screen
// layout for a moment: the screen must be repainted in the wide layout afterwards. A stale two
// column list would show its column separator inside the 20 column strip.
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("wide.md")
TypeDown(3)
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
ExpectString("╗╔", 18, 0, 4, 1)
TypeTab()
ToggleShift(true)
TypeFKey(9)
ToggleShift(false)
ExpectString("══ Save setup ══")
TypeEnter()
ExpectNoString("══ Save setup ══")
Sleep(500)
ExpectString("╗╔", 18, 0, 4, 1)
ExpectNoString("│", 1, 3, 17, 5)
TypeTab()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)

// The saved setup holds the user layout, not the strip layout.
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("wide.md")
ExpectString("║║", 38, 4, 4, 1)
ExpectString("│", 2, 3, 36, 4)
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)

// Stacked panels (PanelsDisposition=1) have no side strip. The wide layout must not change the
// view mode of the file list: it keeps its two column separator.
SaveTextFile(config, ["[Layout]", "PanelsDisposition=1"])
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("wide.md")
TypeDown(3)
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
ExpectString("Wide")
ExpectString("│", 0, 2, 80, 3)
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
