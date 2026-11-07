// One console row per printed Markdown row, with fences straddling the scroll
// edge (story markdown-code-tint, T1 print model).  The text cell and its
// row's fill cell must agree, and the fill must reach the right edge, on rows
// that scrolled in.
//
// The viewer opens through `far2l -view <file>`: the panel cursor race of the
// F3-from-panel path does not affect the rows a tint test measures.
mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"

function CheckRowFill(what, text_x, y) {
 var t = ReadCell(text_x, y)
 var f = ReadCell(70, y)
 var e = ReadCell(79, y)
 if (t.BackTC != f.BackTC || t.Back != f.Back || t.IsBackTC != f.IsBackTC) {
  Log(what + " text " + t.Back + " tc " + t.IsBackTC + " " + t.BackTC.toString(16)
   + " fill " + f.Back + " tc " + f.IsBackTC + " " + f.BackTC.toString(16))
  Panic(what + ": text cell and row fill disagree")
 }
 if (f.BackTC != e.BackTC || f.Back != e.Back) {
  Log(what + " fill " + f.BackTC.toString(16) + " edge " + e.BackTC.toString(16))
  Panic(what + ": the fill stops before the right edge")
 }
}

// long.md, 137 source lines, one cpp fence at source line 69 (rendered line
// 39).  Page down twice: the fence body straddles the scroll edge, rows
// printed after a page-in keep one console row per Markdown row (the T1
// one-pass fill).
RemoveAll(profile)
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-view", left + "/long.md"], 80, 24);
TypePageDown()
TypePageDown()
TypePageDown()
var body = ExpectString("Code 061 verbatim line")
CheckRowFill("scrolled-in fence", body.X, body.Y)
TypePageDown()
var table = ExpectString("Cell 076")
CheckRowFill("table row after page down", table.X, table.Y)
TypeEscape()
ExpectAppExit(0)

// spacing.md: a text fence inside the spacing shapes, on the first page.
// The fence row and the rows around it keep the same row geometry.
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-view", left + "/spacing.md"], 80, 24);
var fence = ExpectString("Spacing fence line")
CheckRowFill("spacing fence", fence.X, fence.Y)
TypeEscape()
ExpectAppExit(0)
0;
