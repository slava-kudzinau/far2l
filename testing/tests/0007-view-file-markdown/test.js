mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"

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

// Switch viewer to Markdown render mode (Shift+F4).
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
ExpectString("Heading")
ExpectString("Code Fence Body")

// Scroll to the bottom, where the mermaid fence is. End is the scroll key that
// re-renders the markdown view; PgDn and the arrow keys do not yet.
TypeEnd()
ExpectNoString("Heading One")
ExpectString("Start")
ExpectString("Finish")
ExpectString("┌──────────────────┐")
ExpectNoString("```mermaid")

TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);
0;
