mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"
config = profile + "/.config"

// The harness runs a truecolor terminal (COLORTERM), so the Colorer scheme is far2l.hrd.
// Its 3:1 batch verdict against the blue Viewer.Text fails (markdown:EmText), and one
// verdict decides for the whole view (story markdown-colorer-fences, spec Contrast):
// every fence cell - token, tail, padding - takes the plain code role, the code RGB
// 0xff87ff with its 16-color fallback 13 (ForeTC is 0xBBGGRR, Fore the index).
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
CheckTok("js keyword", ReadCell(js_line.X, js_line.Y), 0xff87ff, 13)
var ts_kw = ExpectString("const y")
CheckTok("ts keyword", ReadCell(ts_kw.X, ts_kw.Y), 0xff87ff, 13)
var js_str = ExpectString("\"wjs")
CheckTok("js string", ReadCell(js_str.X, js_str.Y), 0xff87ff, 13)    
// The js scheme's '//' block reports its start leaf; the rest of the line
// keeps the code role (R49).
var js_num = ExpectString(" 41")
CheckTok("js number", ReadCell(js_num.X + 1, js_num.Y), 0xff87ff, 13)
var js_tail = ReadCell(js_num.X + 2, js_num.Y) // the ';' right after the number
CheckTok("js tail", js_tail, 0xff87ff, 13) // the code role, one verdict for the view (R49)
CheckTok("js padding", ReadCell(60, js_line.Y), 0xff87ff, 13) // the code role, as above (R49)

// ts: comment, string.
var ts_str = ExpectString("wts")
CheckTok("ts string", ReadCell(ts_str.X, ts_str.Y), 0xff87ff, 13) // the verdict puts every range on the code role (R49)
var ts_com = ExpectString("cts")
CheckTok("ts comment", ReadCell(ts_com.X, ts_com.Y), 0xff87ff, 13) // (R49)

// python: string, comment.
var py_str = ExpectString("wpy")
CheckTok("python string", ReadCell(py_str.X, py_str.Y), 0xff87ff, 13) // the verdict puts every range on the code role (R49)
var py_com = ExpectString("cpy")
CheckTok("python comment", ReadCell(py_com.X, py_com.Y), 0xff87ff, 13) // (R49)

// bash: keyword, string, comment.
var bash_kw = ExpectString("if [ -f")
CheckTok("bash keyword", ReadCell(bash_kw.X, bash_kw.Y), 0xff87ff, 13) // (R49: per-line tokenize, state restarts)
var bash_str = ExpectString("wbash")
CheckTok("bash string", ReadCell(bash_str.X, bash_str.Y), 0xff87ff, 13) // (R49)
var bash_com = ExpectString("b5")
CheckTok("bash comment", ReadCell(bash_com.X, bash_com.Y), 0xff87ff, 13) // (R49)

// json: string, number.
var json_str = ExpectString("key")
CheckTok("json string", ReadCell(json_str.X, json_str.Y), 0xff87ff, 13) // (R49)
var json_num = ExpectString("3.14")
CheckTok("json number", ReadCell(json_num.X, json_num.Y), 0xff87ff, 13)

// c: keyword, number, comment.
var c_kw = ExpectString("int c")
CheckTok("c keyword", ReadCell(c_kw.X, c_kw.Y), 0xff87ff, 13)
var c_num = ExpectString("44")
CheckTok("c number", ReadCell(c_num.X, c_num.Y), 0xff87ff, 13)        
var c_com = ExpectString("cc */")
CheckTok("c comment", ReadCell(c_com.X - 3, c_com.Y), 0xff87ff, 13)

// cpp: comment.
var cpp_com = ExpectString("cc2")
CheckTok("cpp comment", ReadCell(cpp_com.X, cpp_com.Y), 0xff87ff, 13) // (R49)

// go: string.
var go_str = ExpectString("wgo")
CheckTok("go string", ReadCell(go_str.X, go_str.Y), 0xff87ff, 13) // (R49)

// rust: string.
var rust_str = ExpectString("wrust")
CheckTok("rust string", ReadCell(rust_str.X, rust_str.Y), 0xff87ff, 13) // (R49)

// zsh: the keyword 'if' at the start of the zsh line.
var zsh_str = ExpectString("wzsh")
CheckTok("zsh keyword", ReadCell(zsh_str.X - 10, zsh_str.Y), 0xff87ff, 13) // (R49: per-line tokenize, state restarts)

// java: string.
var java_str = ExpectString("wjava")
CheckTok("java string", ReadCell(java_str.X, java_str.Y), 0xff87ff, 13) // (R49)

// css: number.
var css_num = ExpectString("10;")
CheckTok("css number", ReadCell(css_num.X, css_num.Y), 0xff87ff, 13)  

// The plain-code fallback: a fence with no language shows its text in the code role.
var plain_code = ExpectString("no language, no tokens")
CheckTok("plain fence code", ReadCell(plain_code.X, plain_code.Y), 0xff87ff, 13) // the code role (R49)

// The long fence: the token at its start keeps its role; cells at the far end of
// the visible prefix keep the plain code role (the screen shows the first 80
// cells of the line). One page down, then the first screen shows it.
TypePageDown()
var long_kw = ExpectString("const t = \"z")
CheckTok("long fence keyword", ReadCell(long_kw.X, long_kw.Y), 0xff87ff, 13) // const is the first token (R49)
CheckTok("long fence tail", ReadCell(78, long_kw.Y), 0xff87ff, 13) // the code role (R49)
// Horizontal scroll (spec.md, Core bridge): the row shows the slice at
// LeftPos, and every cell of the slice keeps the color of its token.  F2
// unwraps so Right moves LeftPos; four cells right the row starts at the
// 't' of const, inside the keyword token; the tail keeps the plain code role.
TypeFKey(2)
ExpectString("const t = \"z")   // F2 re-laid the page: the row is where it is now
TypeRight(4)
var scrolled = ExpectString("t = \"zzzzzzzz")   // 'const' is cut to its last cell
CheckTok("scrolled keyword", ReadCell(scrolled.X - 5, scrolled.Y), 0xff87ff, 13) // the 't' of const: cell 4, inside the keyword token
CheckTok("scrolled code", ReadCell(scrolled.X - 1, scrolled.Y), 0xff87ff, 13) // the gap before the string keeps the code role
CheckTok("scrolled string", ReadCell(scrolled.X + 4, scrolled.Y), 0xff87ff, 13) // the string quote takes the string role (R49)
CheckTok("scrolled tail", ReadCell(78, scrolled.Y), 0xff87ff, 13) // the code role, as above (R49)
TypeFKey(2)
TypePageUp()

TypeEscape()
TypeFKey(10)

ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;

// --- Second run: --norgb (16 colors) shows the scheme's 4-bit rows (V2's fg4). ---
StartAppWithSize(["--tty", "--nodetect", "--norgb", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 28);
if (Exists(profile + "/.config/history/viewer.pos"))
	Remove(profile + "/.config/history/viewer.pos")
TypeDown(10)
TypeFKey(3)
ExpectString("left/fixture.md")
var n_js = ExpectString("const x")
// The js scheme reports only its first line: strings, comments and numbers keep
// the code role (R49). The keyword above checks the scheme's 4-bit row - under
// --norgb the Colorer picks console.hrd, whose keywords are white (#F); the code
// cells below check the code role, which keeps its 256 look in the truecolor bits.
var n_str = ExpectString("wjs")
var n_com = ExpectString("cjs")
var n_num = ExpectString("41")
function CheckNorgb(name, cell, expected, tc) {
	// A 16-color attribute carries the 4-bit foreground; the code role keeps its
	// 256-color look in the truecolor bits (the printer's fallback, R52).
	if (cell.ForeTC != tc) {
		Log(name + ": foreTC " + cell.ForeTC.toString(16) + ", expected " + tc.toString(16))
		Panic(name + " shows the wrong truecolor under --norgb")
	}
	if (cell.Fore != expected) {
		Log(name + ": fore " + cell.Fore + ", expected " + expected)
		Panic(name + " shows the wrong 4-bit row")
	}
}
CheckNorgb("norgb keyword", ReadCell(n_js.X, n_js.Y), 15, 0) // console.hrd: keywords white (#F)
CheckNorgb("norgb string", ReadCell(n_str.X, n_str.Y), 14, 0) // the string role (#E yellow)
CheckNorgb("norgb comment", ReadCell(n_com.X, n_com.Y), 3, 0) // the comment role (#3 cyan)
CheckNorgb("norgb number", ReadCell(n_num.X, n_num.Y), 3, 0) // the number role (#3 cyan)
CheckNorgb("norgb code", ReadCell(n_num.X + 2, n_num.Y), 13, 0xafd7d7) // the code role (R52)
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;

// --- Third run: the ini overrides of the four token keys. The overrides are the
// V1 roles' knobs (story markdown-colors); fences print the scheme's V2 ranges,
// so with the view verdict failing every cell - token, tail - is the code role. ---
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
CheckTok("ini keyword", ReadCell(o_js.X, o_js.Y), 0xff87ff, 13)
CheckTok("ini string", ReadCell(o_str.X, o_str.Y), 0xff87ff, 13)
CheckTok("ini comment", ReadCell(o_com.X, o_com.Y), 0xff87ff, 13)
CheckTok("ini number", ReadCell(o_num.X, o_num.Y), 0xff87ff, 13)
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
