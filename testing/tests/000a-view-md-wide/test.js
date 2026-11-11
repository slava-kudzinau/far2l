mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"
config=profile + "/.config/settings/config.ini"

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
// The strip shows one name column: a 17 character name is shown whole.
ExpectString("zz-strip-name.txt", 1, 3, 18, 8)
// Up onto plain.txt: quick view shows it and the half-screen split restores (box in right half).
TypeUp()
ExpectString("Plain", 41, 1, 16, 1)

// Down back onto wide.md: the wide layout applies again (acceptance 3).
TypeDown()
ExpectString("Wide", 21, 1, 16, 1)
ExpectString("india", 66, 2, 12, 1)

// Ctrl+P hides the quick view: the file list gets the half screen back (its right border is in
// the middle of row 0). Ctrl+P shows it again: the wide layout applies again.
ToggleLCtrl(true)
TypeText("p")
ToggleLCtrl(false)
ExpectString("╗", 38, 0, 4, 1)
ToggleLCtrl(true)
TypeText("p")
ToggleLCtrl(false)
ExpectString("╗╔", 18, 0, 4, 1)
ExpectString("india", 66, 2, 12, 1)

// Ctrl+O hides both panels and shows them again: the wide layout is back.
ToggleLCtrl(true)
TypeText("o")
ToggleLCtrl(false)
ToggleLCtrl(true)
TypeText("o")
ToggleLCtrl(false)
ExpectString("╗╔", 18, 0, 4, 1)
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

// Saving the setup while the quick view is wide must not save the strip layout. AutoSaveSetup
// saves it on exit: the first run exits with a wide quick view on wide.md, the second run
// starts from the saved setup. The panels must be split in the middle (row 0 is not checked:
// a quick view title with a long path covers it) and the list must keep its two column mode.
SaveTextFile(config, ["[System]", "AutoSaveSetup=1"])
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("wide.md")
TypeDown(2)
ToggleLCtrl(true)
TypeText("q")
ToggleLCtrl(false)
ExpectString("╗╔", 18, 0, 4, 1)
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)

StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("wide.md")
ExpectString("║║", 38, 4, 4, 1)
ExpectString("│", 2, 3, 36, 4)
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
