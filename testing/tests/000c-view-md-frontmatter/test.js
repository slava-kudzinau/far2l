mydir=WorkDir()
profile=mydir + "/profile"
left=mydir + "/left"
right=mydir + "/right"

// Front matter shows both delimiter lines as rule rows: the opening `---` on row 0,
// the closing `---` directly under the last key line (story markdown-front-matter-rule).
RemoveAll(profile)
StartAppWithSize(["--tty", "--nodetect", "--mortal", "-u", profile, "-cd", left, "-cd", right], 80, 24);
ExpectString("Help - FAR2L");
TypeEscape()
ExpectString("OSC52");
TypeEscape()

// Open frontmatter.md in the viewer: the cursor starts on "/..", the file is the entry below.
TypeDown(1)
TypeFKey(3)
ExpectString("left/frontmatter.md")

// The title bar sits on row 0; the opening rule is the first viewer row below it.
var title = ExpectString("left/frontmatter.md")
var opening = ExpectString("──────", 0, title.Y + 1, 80, 1)
if (opening.Y != title.Y + 1) {
	Log("opening rule on row " + opening.Y + ", title on row " + title.Y);
	Panic("the opening rule does not follow the title bar");
}

// The keys show under the opening rule, in order.
var key_first = ExpectString("name: architect")
if (key_first.Y != opening.Y + 1) {
	Log("first key on row " + key_first.Y + ", opening rule on row " + opening.Y);
	Panic("the first key does not follow the opening rule");
}
var key_last = ExpectString("disable-model-invocation: true")
if (key_last.Y != key_first.Y + 2) {
	Log("last key on row " + key_last.Y + ", first key on row " + key_first.Y);
	Panic("the keys are not one per row");
}

// The closing rule sits directly under the last key line.
CheckCellChar(0, key_last.Y + 1, "─")
CheckCellChar(40, key_last.Y + 1, "─")

// The heading follows the closing rule after its blank row.
var head = ExpectString("Architect")
if (head.Y != key_last.Y + 3) {
	Log("heading on row " + head.Y + ", closing rule on row " + (key_last.Y + 1));
	Panic("the heading does not follow the closing rule and its blank row");
}
ExpectString("Body line after front matter")

TypeEscape()
TypeFKey(10)
ExpectString("Do you want to quit FAR?")
TypeEnter()
ExpectAppExit(0);
0;
