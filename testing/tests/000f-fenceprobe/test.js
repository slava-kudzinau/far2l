mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
RemoveAll(profile)
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-view", left + "/fence-check.md"], 80, 24);
var h = ExpectString("HeadingOne");
var hc = ReadCell(h.X + 2, h.Y);
Log("heading tc=0x" + hc.ForeTC.toString(16) + " fore " + hc.Fore);
var bgc = ReadCell(70, 22);
Log("viewer bg tc=0x" + bgc.BackTC.toString(16) + " back " + bgc.Back + " isTC " + bgc.IsBackTC);
var a = ExpectString("// a comment");
var ca = ReadCell(a.X, a.Y);
Log("javascript comment tc=0x" + ca.ForeTC.toString(16) + " fore " + ca.Fore);
var c = ExpectString("const x");
var cc = ReadCell(c.X, c.Y);
Log("javascript keyword tc=0x" + cc.ForeTC.toString(16) + " fore " + cc.Fore);
TypeEscape();
TypeFKey(10);
TypeEnter();
ExpectAppExit(0);
