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

// A cell of the code block's tint (story markdown-code-tint): the fill's
// blended background rides the row - one eighth of the code role's magenta in
// the viewer's blue (0x8f101f, the driver's BlendEighth of the same pair).
// The scheme's 4-bit index rides along under the truecolor field.
function CheckTint(name, x, y) {
	var cell = ReadCell(x, y)
	if (cell.BackTC != 0x8f101f || cell.Back != 1 || !cell.IsBackTC) {
		Log(name + ": back " + cell.Back + " tc " + cell.IsBackTC + " " + cell.BackTC.toString(16))
		Panic(name + " shows the wrong tint background")
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

// Session 1: with the plugin loaded the viewer shows the Colorer look: the
// scheme's regions paint the runs and the scheme's style words underline them
// (the harness runs a 16-color terminal, so the scheme is console.hrd - the
// 4-bit path of the resolver, spec "Resolver order"; the truecolor rows are
// the driver's `look_real_hrd`). The table draws with box characters.
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

// The javascript fence's code row (story markdown-code-tint): the block's tint
// fills the whole row - the code cell, the cell past the end of the text and
// the right edge alike (the fill reaches the edge).  The fence follows the
// heading: its code row sits two rows below the heading row; the code text
// starts four cells in (spec, layout) and ends at the ';' on column 15.
var fence_y = h.Y + 2
CheckTint("code cell", 4, fence_y)
CheckTint("past the text", 16, fence_y)
CheckTint("right edge", 79, fence_y)
// The blank gap row inside the block and the row after it are plain: the
// viewer's own blue background, as on the rows outside the block.
var gap = ReadCell(2, fence_y + 1)
if (gap.BackTC != 0x800000 || gap.Back != 1 || !gap.IsBackTC) {
	Log("gap back " + gap.Back + " tc " + gap.IsBackTC + " " + gap.BackTC.toString(16))
	Panic("the gap row of the fence lost the viewer background")
}
var plain = ReadCell(2, fence_y + 2)
if (plain.BackTC != 0x800000 || plain.Back != 1 || !plain.IsBackTC) {
	Log("plain back " + plain.Back + " tc " + plain.IsBackTC + " " + plain.BackTC.toString(16))
	Panic("the row after the fence lost the viewer background")
}

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

// --- Second run: --norgb shows the plain code background. ---
StartAppWithSize(["--tty", "--nodetect", "--norgb", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 28);
if (Exists(profile + "/.config/history/viewer.pos"))
	Remove(profile + "/.config/history/viewer.pos")
TypeDown(1)
TypeFKey(3)
ExpectString("left/fixture.md")
// --norgb gates the tint (spec, Contrast): the fence keeps the scheme look on
// the 4-bit path.  A plain code cell (past the last token's text) carries
// console.hrd's Code - #d7d7af on 4-bit index 13 - on the viewer's own blue
// background.  A cell under a token carries that region's
// own 4-bit color (the V2 path, story markdown-code-tint).
var n_code = ExpectString("const z")
var ncell = ReadCell(n_code.X + 12, n_code.Y)
if (ncell.Fore != 13 || ncell.ForeTC != 0xafd7d7) {
	Log("norgb fore " + ncell.Fore + " ftc " + ncell.ForeTC.toString(16))
	Panic("--norgb code cell lost the scheme's code foreground")
}
if (ncell.BackTC != 0x800000 || ncell.Back != 1 || !ncell.IsBackTC) {
	Log("norgb back " + ncell.Back + " tc " + ncell.IsBackTC + " " + ncell.BackTC.toString(16))
	Panic("--norgb code cell shows a tinted background")
}
var n_tail = ReadCell(n_code.X + 12, n_code.Y)
if (n_tail.BackTC != 0x800000 || n_tail.Back != 1 || !n_tail.IsBackTC) {
	Log("norgb tail back " + n_tail.Back + " tc " + n_tail.IsBackTC + " " + n_tail.BackTC.toString(16))
	Panic("--norgb shows a tint past the text")
}
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
