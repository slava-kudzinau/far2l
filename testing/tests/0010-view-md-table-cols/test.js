mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"

RemoveAll(profile)

// 80x24: the table's natural width is far past the screen, so each column must
// keep at least the width of its longest word.
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("Help - FAR2L");
TypeEscape()
ExpectString("OSC52");
TypeEscape()

// tablecols.md sorts after sample.md and spacing.md: four steps down from "/..".
TypeDown(4)
TypeFKey(3)
ExpectString("left/tablecols.md")

// Anchor on the unique table header, not the file heading "Stage moves".
var head = ExpectString("Why it works")
var headLine = BoundedLine(0, head.Y, 80, "")
var bars = []
for (var b = 0; b < headLine.length; ++b) {
	if (headLine.substring(b, b + 1) == "│") bars.push(b)
}
if (bars.length != 4) {
	Panic("table header lost a column border: " + bars.length + " bars in '" + headLine + "'")
}
if (bars[3] != 79) {
	Panic("table right border at " + bars[3] + ", not 79")
}
var separator = BoundedLine(0, head.Y + 1, 80, "")
if (separator.substring(79, 80) != "┤") {
    Panic("table separator border is wrong at column 79: '" + separator + "'")
}
var w0 = bars[1] - bars[0] - 3
var w1 = bars[2] - bars[1] - 3
var w2 = bars[3] - bars[2] - 3
if (w0 < 7 || w1 < 12 || w2 < 10) {
    Panic("column floor too narrow: " + w0 + " " + w1 + " " + w2)
}
if (w1 <= w0 || w2 <= w0) {
    Panic("prose column not wider than label: " + w0 + " " + w1 + " " + w2)
}

// The symptom: the first body-row label shows as one word, not "An/al/yz/e".
var body = BoundedLine(0, head.Y + 2, 80, "")
if (body.indexOf("Analyze") < 0) {
	Panic("first column crushed, row reads '" + body + "'")
}

TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);
0;
