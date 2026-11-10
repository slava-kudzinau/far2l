mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"

StartApp(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right]);
ExpectString("Help - FAR2L");
TypeEscape()

// Open the markdown sample in the viewer (F3 on panel).
TypeFKey(3)
ExpectString("sample.md")

// Switch viewer to Markdown render mode (Shift+F4).
TypeVK(0x10000073, 1)
ExpectString("Heading")
ExpectString("Code Fence Body")
ExpectString("Start")

TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);
0;
