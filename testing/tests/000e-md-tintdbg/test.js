mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"

RemoveAll(profile)
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("Help - FAR2L");
TypeEscape()
ExpectString("OSC52");
TypeEscape()
TypeDown(2)
TypeFKey(3)
ExpectString("left/sample.md")
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
ExpectString("# Heading One")
ToggleShift(true)
TypeFKey(4)
ToggleShift(false)
ExpectString("Heading")
for (r = 0; r < 24; r = r + 1) {
 var s = ""
 for (c = 0; c < 80; c = c + 1) s = s + ReadCell(c, r).Text
 Log("r" + r + " |" + s + "|")
}
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
