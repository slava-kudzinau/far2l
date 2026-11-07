// The F3 look of the Colorer scheme (story markdown-colorer-look, plan T5/T6).
// Needs the full build: the colorer plugin is loaded and the far2l.hrd scheme
// paints the runs; a run on the far2l-only build shows the fork colors.
mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"
config = profile + "/.config"

// A cell of the scheme look: the truecolor foreground (ForeTC is 0xBBGGRR)
// and the underline bit 0x8000 of the raw attributes (spec, Underline).
function CheckLook(name, x, y, tc, underlined) {
	var cell = ReadCell(x, y)
	if (cell.ForeTC != tc) {
		Log(name + ": foreTC " + cell.ForeTC.toString(16) + ", expected " + tc.toString(16))
		Panic(name + " shows the wrong foreground")
	}
	var raw = ReadCellRaw(x, y)
	var under = (raw.Attributes & 0x8000) != 0
	if (under != underlined) {
		Log(name + ": underline " + under + ", expected " + underlined)
		Panic(name + " shows the wrong underline")
	}
}

// A fresh profile shows the help screen on start; a reused one would not.
// The white Viewer.Text keeps the ten scheme rows above 3:1 (spec, Resolver
// order): the scheme look, not the fork table, paints the screen.

// 80x28: the fixture fits on one screen.
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 28);
ExpectString("Help - FAR2L");
TypeEscape()
ExpectString("OSC52");
TypeEscape()
TypeDown(1)
// Open the fixture in the viewer (F3; Markdown mode is the default).
TypeFKey(3)
ExpectString("left/fixture.md")
// The first render pays the scheme loads; the reopen re-renders from cache.
TypeEscape()
TypeFKey(3)
ExpectString("left/fixture.md")

// Session 1: the harness runs a truecolor terminal (COLORTERM), so the scheme is
// far2l.hrd, and the 3:1 batch verdict against the blue Viewer.Text fails
// (markdown:EmText) - one verdict decides for the whole view (story
// markdown-colorer-fences, spec Contrast): the scheme rows decide (the scheme
// rows are the driver's `look_real_hrd`); emphasis and body take the plain code
// look's cyan 0xffff00 with the theme underline.
// The table draws with box characters.
var h = ExpectString("Heading")
CheckLook("heading", h.X, h.Y, 0x5fffff, true)
var l = ExpectString("the link")
CheckLook("link", l.X, l.Y, 0x87ff87, true)
var st = ExpectString("strong")
CheckLook("strong", st.X, st.Y, 0xffffff, true)
var em = ExpectString("emphasis")
CheckLook("emphasis", em.X, em.Y, 0xffff00, true)
var co = ExpectString("inline code")
CheckLook("code", co.X, co.Y, 0xff87ff, true)
var q = ExpectString("quote text")
CheckLook("quote", q.X, q.Y, 0xd7afaf, true)
var it = ExpectString("item one")
// The bullet of a list item sits two cells left of the text (spec, New roles).
CheckLook("listmark", it.X - 2, it.Y, 0xaf8787, true)
CheckLook("body", it.X, it.Y, 0xffff00, true)
var bd = ExpectString("───┼───")
CheckLook("rule", bd.X, bd.Y, 0xaf8787, true)

// Session 2: F4 opens the editor on the same file (the F4/F6 parity of the
// goal), Escape returns, and the reopen shows the same look (the cache, spec
// "Cache").  A scheme change moving the look is the driver's `look_bridge_real`.
TypeFKey(4)
ExpectString("fixture.md")
TypeEscape()
TypeFKey(3)
var h2 = ExpectString("Heading")
CheckLook("heading after reopen", h2.X, h2.Y, 0xffff00, true)

TypeFKey(4)
ExpectString("fixture.md")
TypeFKey(6)
ExpectString("fixture.md")
TypeFKey(1)
ExpectString("Help - FAR2L")
TypeEscape()
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);

// --- Third run: the 16-color harness. Under --norgb the Colorer picks the
// scheme's console.hrd, and a 4-bit scheme always passes the 3:1 gate, so the
// fences print the scheme's 4-bit rows (V2's fg4); a cell with no scheme row
// keeps the code role's plain look in the truecolor bits (R52). The console
// scheme's attr carries the underline bit, so the scheme's rows ride underlined.
function CheckFence(name, x, y, fore, tc, underlined) {
 var cell = ReadCell(x, y)
 if (cell.Fore != fore || cell.ForeTC != tc) {
  Log(name + ": fore " + cell.Fore + "/" + cell.ForeTC.toString(16) + ", expected " + fore + "/" + tc.toString(16))
  Panic(name + " shows the wrong scheme row")
 }
 var raw = ReadCellRaw(x, y)
 var under = (raw.Attributes & 0x8000) != 0
 if (under != underlined) {
  Log(name + ": underline " + under + ", expected " + underlined)
  Panic(name + " shows the wrong underline")
 }
}

StartAppWithSize(["--tty", "--nodetect", "--norgb", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 28);
if (Exists(profile + "/.config/history/viewer.pos"))
 Remove(profile + "/.config/history/viewer.pos")
TypeDown(10)
TypeFKey(3)
ExpectString("left/fixture.md")
var f_kw = ExpectString("function tailSum")
CheckFence("js keyword", f_kw.X, f_kw.Y, 15, 0, true)
CheckFence("js function", f_kw.X + 9, f_kw.Y, 11, 0, true)
var f_str = ExpectString("\\lit")
CheckFence("js string", f_str.X - 1, f_str.Y, 14, 0, true)
CheckFence("js escape", f_str.X, f_str.Y, 14, 0, true)
var f_num = ExpectString("42 * true")
CheckFence("js number", f_num.X, f_num.Y, 10, 0, true)
CheckFence("js operator", f_num.X + 3, f_num.Y, 15, 0, true)
CheckFence("js constant", f_num.X + 6, f_num.Y, 11, 0, true)
var f_com = ExpectString("// note")
CheckFence("js comment", f_com.X, f_com.Y, 3, 0, true)
var f_call = ExpectString("tailSum(s, 7)")
CheckFence("js tail", f_call.X, f_call.Y, 13, 0xafd7d7, true)
CheckFence("js tail operator", f_call.X + 13, f_call.Y, 14, 0, true)
CheckFence("code padding", 60, f_kw.Y, 13, 0xafd7d7, true)
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
