mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"
config = profile + "/.config"

// Colors: a token cell carries the role's RGB (ForeTC is 0xBBGGRR) and its 16-color
// fallback (Fore). A cell of the plain code role is the reference for tails and padding.
function CheckTok(name, cell, tc, fore) {
	if (cell.ForeTC != tc) {
		Log(name + ": foreTC " + cell.ForeTC.toString(16) + ", expected " + tc.toString(16))
		Panic(name + " shows the wrong truecolor")
	}
	if (cell.Fore != fore) {
		Log(name + ": fore " + cell.Fore + ", expected " + fore)
		Panic(name + " shows the wrong 16-color index")
	}
}

// A fresh profile shows the help screen on start; a reused one would not.
RemoveAll(profile)

// 80x28: all 13 fences (12 languages and the plain-code one) fit on one screen.
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

// js: keyword, string, comment, number, and the tail after the last token.
var js_line = ExpectString("const x")
CheckTok("js keyword", ReadCell(js_line.X, js_line.Y), 0xffaf87, 7)   // spec 256 index 111 (R52)
var ts_kw = ExpectString("const y")
CheckTok("ts keyword", ReadCell(ts_kw.X, ts_kw.Y), 0xffaf87, 7)
var js_str = ExpectString("\"wjs")
CheckTok("js string", ReadCell(js_str.X, js_str.Y), 0x87ff87, 10)     // spec 256 index 120 (R52)
// The js scheme's '//' block reports its start leaf; the rest of the line
// keeps the code role (R49).
var js_num = ExpectString(" 41")
CheckTok("js number", ReadCell(js_num.X + 1, js_num.Y), 0xff87ff, 13)
var js_tail = ReadCell(js_num.X + 2, js_num.Y) // the ';' right after the number
CheckTok("js tail", js_tail, 0xff87ff, 13) // the code role shows its 256-color look under --norgb (R49)
CheckTok("js padding", ReadCell(60, js_line.Y), 0xff87ff, 13) // the code role, as above (R49)

// ts: comment, string.
var ts_str = ExpectString("wts")
CheckTok("ts string", ReadCell(ts_str.X, ts_str.Y), 0xff87ff, 13) // the ts scheme draws strings plain (R49)
var ts_com = ExpectString("cts")
CheckTok("ts comment", ReadCell(ts_com.X, ts_com.Y), 0xff87ff, 13) // and comments (R49)

// python: string, comment.
var py_str = ExpectString("wpy")
CheckTok("python string", ReadCell(py_str.X, py_str.Y), 0xff87ff, 13) // the python scheme draws strings plain (R49)
var py_com = ExpectString("cpy")
CheckTok("python comment", ReadCell(py_com.X, py_com.Y), 0xff87ff, 13) // and comments (R49)

// bash: keyword, string, comment.
var bash_kw = ExpectString("if [ -f")
CheckTok("bash keyword", ReadCell(bash_kw.X, bash_kw.Y), 0xaf8787, 3) // the line begins inside a cached comment (R49)
var bash_str = ExpectString("wbash")
CheckTok("bash string", ReadCell(bash_str.X, bash_str.Y), 0xff87ff, 13) // strings inside a cached comment stay plain (R49)
var bash_com = ExpectString("b5")
CheckTok("bash comment", ReadCell(bash_com.X, bash_com.Y), 0xff87ff, 13) // inside a cached comment (R49)

// json: string, number.
var json_str = ExpectString("key")
CheckTok("json string", ReadCell(json_str.X, json_str.Y), 0xff87ff, 13) // plain: the json keys carry the string class (R49)
var json_num = ExpectString("3.14")
CheckTok("json number", ReadCell(json_num.X, json_num.Y), 0x5fafff, 12) // spec 256 index 215 (R52)

// c: keyword, number, comment.
var c_kw = ExpectString("int c")
CheckTok("c keyword", ReadCell(c_kw.X, c_kw.Y), 0xffaf87, 7)
var c_num = ExpectString("44")
CheckTok("c number", ReadCell(c_num.X, c_num.Y), 0x5fafff, 12)         // spec 256 index 215 (R52)
var c_com = ExpectString("cc */")
CheckTok("c comment", ReadCell(c_com.X - 3, c_com.Y), 0xaf8787, 3)

// cpp: comment.
var cpp_com = ExpectString("cc2")
CheckTok("cpp comment", ReadCell(cpp_com.X, cpp_com.Y), 0xff87ff, 13) // plain: the line begins inside a cached comment (R49)

// go: string.
var go_str = ExpectString("wgo")
CheckTok("go string", ReadCell(go_str.X, go_str.Y), 0xff87ff, 13) // plain: the go scheme draws strings plain (R49)

// rust: string.
var rust_str = ExpectString("wrust")
CheckTok("rust string", ReadCell(rust_str.X, rust_str.Y), 0xff87ff, 13) // plain (R49)

// zsh: the keyword 'if' at the start of the zsh line.
var zsh_str = ExpectString("wzsh")
CheckTok("zsh keyword", ReadCell(zsh_str.X - 10, zsh_str.Y), 0xaf8787, 3) // the line begins inside a cached comment (R49)

// java: string.
var java_str = ExpectString("wjava")
CheckTok("java string", ReadCell(java_str.X, java_str.Y), 0xff87ff, 13) // plain (R49)

// css: number.
var css_num = ExpectString("10;")
CheckTok("css number", ReadCell(css_num.X, css_num.Y), 0x5fafff, 12)   // spec 256 index 215 (R52)

// The plain-code fallback: a fence with no language shows its text in the code role.
var plain_code = ExpectString("no language, no tokens")
CheckTok("plain fence code", ReadCell(plain_code.X, plain_code.Y), 0xff87ff, 13) // the code role (R49)

// The long fence: the token at its start keeps its role; cells at the far end of
// the visible prefix keep the plain code role (the screen shows the first 80
// cells of the line). One page down, then the first screen shows it.
TypePageDown()
var long_kw = ExpectString("const t = \"z")
CheckTok("long fence keyword", ReadCell(long_kw.X, long_kw.Y), 0xffaf87, 7) // const is the first token (R49)
CheckTok("long fence tail", ReadCell(78, long_kw.Y), 0xff87ff, 13) // the code role (R49)
TypePageUp()

TypeEscape()
TypeFKey(10)

ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;

// --- Second run: --norgb shows the 16-color fallbacks of the spec table. ---
StartAppWithSize(["--tty", "--nodetect", "--norgb", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 28);
if (Exists(profile + "/.config/history/viewer.pos"))
	Remove(profile + "/.config/history/viewer.pos")
TypeDown(10)
TypeFKey(3)
ExpectString("left/fixture.md")
var n_js = ExpectString("const x")
// The js scheme reports only its first line: strings, comments and numbers keep
// the code role (R49). The keyword above checks the keyword's 256 look; the code
// cells below check the code role's 256 look (R52).
var n_str = ExpectString("wjs")
var n_com = ExpectString("cjs")
var n_num = ExpectString("41")
function CheckNorgb(name, cell, tc, expected) {
	// --norgb keeps the 256-color look in the attribute (R52); a 256-color
	// terminal paints ForeTC, a 16-color terminal paints Fore.
	if (cell.ForeTC != tc) {
		Log(name + ": foreTC " + cell.ForeTC.toString(16) + ", expected " + tc.toString(16))
		Panic(name + " shows the wrong 256-color look under --norgb")
	}
	if (cell.Fore != expected) {
		Log(name + ": fore " + cell.Fore + ", expected " + expected)
		Panic(name + " shows the wrong 16-color fallback")
	}
}
CheckNorgb("norgb keyword", ReadCell(n_js.X, n_js.Y), 0xffaf87, 7)   // spec 256 index 111
CheckNorgb("norgb string", ReadCell(n_str.X, n_str.Y), 0xafd7d7, 13) // the code role: spec 256 index 213 (R52)
CheckNorgb("norgb comment", ReadCell(n_com.X, n_com.Y), 0xafd7d7, 13) // the code role, as above (R52)
CheckNorgb("norgb number", ReadCell(n_num.X, n_num.Y), 0xafd7d7, 13) // the code role, as above (R52)
CheckNorgb("norgb code", ReadCell(n_num.X + 2, n_num.Y), 0xafd7d7, 13) // the code role, as above (R52)
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;

// --- Third run: the ini overrides of the four token keys. ---
// A literal override carries no 16-color index: the low bits stay 0 (R52).
SaveTextFile(config + "/settings/markdown-colors.ini", ["[markdown]",
	"token-keyword=foreground:#FF8000",
	"token-string=foreground:#00FF00",
	"token-comment=foreground:#0000FF",
	"token-number=foreground:#FF00FF"])
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 28);
if (Exists(profile + "/.config/history/viewer.pos"))
	Remove(profile + "/.config/history/viewer.pos")
TypeDown(10)
TypeFKey(3)
ExpectString("left/fixture.md")
var o_js = ExpectString("const x")
var o_str = ExpectString("wjs")
var o_com = ExpectString("cjs")
var o_num = ExpectString("41")
CheckTok("ini keyword", ReadCell(o_js.X, o_js.Y), 0x0080ff, 0)
CheckTok("ini string", ReadCell(o_str.X, o_str.Y), 0xff87ff, 13)
CheckTok("ini comment", ReadCell(o_com.X, o_com.Y), 0xff87ff, 13)
CheckTok("ini number", ReadCell(o_num.X, o_num.Y), 0xff87ff, 13)
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
