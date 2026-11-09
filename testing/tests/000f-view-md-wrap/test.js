mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"

function CheckNotBlank(name, y) {
	var text = BoundedLine(0, y, 80, " \t")
	if (text == "") {
		Panic(name + " row is blank")
	}
}

function CheckBlank(name, y) {
	var text = BoundedLine(0, y, 80, " \t")
	if (text != "") {
		Log(name + ": row " + y + " is '" + text + "'")
		Panic(name + " row is not blank")
	}
}

// A fresh profile shows the help screen on start; a reused one would not.
RemoveAll(profile)

// Open the fixture (80x24: the table header does not fit in 80 cells, so the prose
// wrapping and the table-layout preservation show at the same size).
// far2l opens the panels (F3 on the panels opens the viewer).
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("Help - FAR2L");
TypeEscape()
ExpectString("OSC52");
TypeEscape()
TypeDown()
TypeFKey(3)
ExpectString("left/wrap.md")

// The long paragraph takes two rows: its own row and a continuation row, then one blank row.
var para = ExpectString("Alpha bravo")
CheckNotBlank("paragraph continuation", para.Y + 1)
CheckBlank("gap after the paragraph", para.Y + 2)

// F2 turns wrap off: the paragraph is one row, so the gap moves up to the row under it.
TypeFKey(2)
var unwrapped = ExpectString("Alpha bravo")
if (unwrapped.Y != para.Y) {
	Log("F2 moved the paragraph from row " + para.Y + " to row " + unwrapped.Y)
	Panic("F2 did not keep the top of the document")
}
CheckBlank("gap after the unwrapped paragraph", unwrapped.Y + 1)

// With wrap off, Right scrolls the row: two cells drop "Al".
TypeRight(2)
var scrolled = BoundedLine(0, unwrapped.Y, 80, " \t")
// The row is scrolled by two cells; the left-arrow marker is drawn over the first cell.
if (scrolled.substring(0, 9) != "«ha bravo") {
	Log("after two Right: row is '" + scrolled + "'")
	Panic("Right did not scroll the unwrapped paragraph")
}
TypeLeft(2)

// F2 turns wrap on again: two rows and the blank gap below them.
TypeFKey(2)
var rewrapped = ExpectString("Alpha bravo")
CheckNotBlank("paragraph continuation after F2", rewrapped.Y + 1)
CheckBlank("gap after the rewrapped paragraph", rewrapped.Y + 2)

var head = ExpectString("Column A")
var rule = BoundedLine(0, head.Y + 1, 80, " \t")
if (rule.substring(0, 1) != "├") {
    Log("the row under the header is '" + rule + "'")
    Panic("the table header row is split by wrap")
}
if (BoundedLine(0, head.Y, 80, "").substring(79, 80) == "»") {
    Panic("table unexpectedly scrolls horizontally")
}
var row1 = BoundedLine(0, head.Y + 2, 80, " ")
var row2 = BoundedLine(0, head.Y + 3, 80, " ")
if (row1.indexOf("aaaaaaaaaaaaaaaaaaaaaaaa") < 0 || row2.indexOf("aaaaaaaa") < 0) {
    Log("row one is '" + row1 + "', row two is '" + row2 + "'")
    Panic("the long table word was not split at its column width")
}
if (row1.indexOf("cccc dddd") < 0 || row2.indexOf("hhhh") < 0 || row1.indexOf("hhhh") >= 0) {
    Log("row one is '" + row1 + "', row two is '" + row2 + "'")
    Panic("the table cell did not prefer a whitespace break")
}

var weightedHead = ExpectString("Detailed content column")
var weightedHeadLine = BoundedLine(0, weightedHead.Y, 80, "")
var bars = []
for (var b = 0; b < weightedHeadLine.length; ++b) {
	if (weightedHeadLine.substring(b, b + 1) == "│") bars.push(b)
}
if (bars.length < 4) {
	Log("weighted table header is '" + weightedHeadLine + "'")
	Panic("the weighted table header lost its column borders")
}
var firstWidth = bars[1] - bars[0]
var finalWidth = bars[3] - bars[2]
if (finalWidth <= firstWidth) {
	Log("weighted table header is '" + weightedHeadLine + "'")
	Panic("the final content column did not receive more width than the short first column")
}
for (var i = 0; i < 6; ++i) {
	var weightedLine = BoundedLine(0, weightedHead.Y + i, 80, "")
	if (weightedLine.length > 80) {
		Log("weighted table row is '" + weightedLine + "'")
		Panic("a weighted table row exceeded the viewer width")
	}
}
ExpectString("first column.")

// A list item wraps under its lead: the bullet rides the first row only, and the continuation
// starts at the 2-cell list indent.
var item = ExpectString("• Alpha bravo")
CheckNotBlank("list continuation", item.Y + 1)
if (BoundedLine(0, item.Y + 1, 80, " \t").indexOf("•") >= 0) {
	Log("list continuation is '" + BoundedLine(0, item.Y + 1, 80, " \t") + "'")
	Panic("the list mark repeated on the wrapped row")
}
CheckCellChar(2, item.Y + 1, "abcdefghijklmnopqrstuvwxyz")

var wide = ExpectString("界")
var wideLine = BoundedLine(0, wide.Y, 80, "")
if (wideLine.substring(wide.X + 2, wide.X + 4) != "  ") {
    Log("the wide-glyph table row is '" + wideLine + "'")
    Panic("the wide glyph has incorrect table padding")
}

// The fences, the final paragraph and the mermaid art sit below the first screen.
TypePageDown()
var fence2 = ExpectString("second fence line")
var longFence = BoundedLine(0, fence2.Y - 1, 80, "")
if (longFence.substring(longFence.length - 1) != "»") {
	Log("the long fence row is '" + longFence + "'")
	Panic("the long code fence row was split")
}
// The mermaid box keeps one row per box row: its top border runs past the right edge
// (its right-arrow marker shows), so the wide art row was not split by wrap.
var boxTop = ExpectString("┌")
var topRow = BoundedLine(0, boxTop.Y, 80, "")
if (topRow.substring(topRow.length - 1) != "»") {
	Log("the mermaid top border is '" + topRow + "'")
	Panic("the mermaid box row was split by wrap")
}
// Leave the viewer through the standard confirmation path used by the Markdown smoke tests.
TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0)
0;
