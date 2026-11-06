---
description: Runtime view - open a Markdown file in F3, render and scroll, draw one mermaid fence as ASCII art, and open the file in the browser.
refs:
  - docs/architecture/features/markdown/markdown.overview.md
  - docs/architecture/features/markdown/markdown-viewer.spec.md
  - docs/architecture/features/markdown/markdown-browser-view.spec.md
  - docs/architecture/features/markdown/markdown.data.md
---

# View: Markdown runtime

Four flows matter. All F3 work runs on the UI thread. The browser flow is outside far2l after the command starts.

## Open a `.md` file in F3

```mermaid
sequenceDiagram
  actor User
  participant Panel as FileList
  participant V as Viewer
  participant Cache as ViewerPosCache
  participant Doc as MarkdownDoc
  User->>Panel: F3
  Panel->>V: OpenFile
  V->>Cache: read Param[3] if SavePos is on
  V->>V: MarkdownRuleMode, then MarkdownStartMode
  V->>V: ShowPage(SHOW_MARKDOWN)
  V->>V: SyncMarkdownTop, m_md_ready is false
  V->>V: ShowMarkdown reads the whole file
  V->>Doc: Render(text, Width)
  Doc-->>V: Lines()
  V->>V: MarkdownLineAtPos(FilePos) sets m_md_top
```

| Step | Who | Note |
|---|---|---|
| Rule | `MarkdownRuleMode` | Name match and `Markdown_Auto`. The result stays in `m_md_rule`. |
| Start mode | `MarkdownStartMode` | Saved 1 or 2 wins. Else the rule applies. |
| Parse | `Render` | md4c events go to `Builder`, which fills `MdLine` rows. |
| Close | `SavePosCache` | `Param[3]` stores only a deviation from `m_md_rule`. |

## Render plus scroll

```mermaid
sequenceDiagram
  actor User
  participant V as Viewer
  participant Doc as MarkdownDoc
  participant P as MarkdownPrinter
  User->>V: Down
  V->>V: SetMarkdownTop(m_md_top + 1) sets FilePos
  V->>V: ShowPage(SHOW_MARKDOWN)
  V->>V: SyncMarkdownTop
  alt width changed
    V->>Doc: Invalidate(Width)
    V->>V: find the top row by offset and place
  else FilePos moved by search or GoTo
    V->>V: MarkdownLineAtPos(FilePos)
  end
  loop each screen row
    V->>Doc: read row m_md_top + I
    V->>V: ReadStringMarkdown fills Strings and m_md_screen
  end
  V->>P: Print for each row
  P->>P: MarkdownRunColor(role) reads the palette
```

| Step | Who | Note |
|---|---|---|
| Move | `SetMarkdownTop` | Clamps the index and sets `FilePos` to the offset of that row. |
| Page | `MarkdownPage` | Moves by the screen height minus 1, up to `MarkdownMaxTop`. |
| Sync | `SyncMarkdownTop` | Three cases: new render, new width, or `FilePos` moved by other code. |
| Draw | `MarkdownPrinter::Print` | Ignores the text argument and draws the runs of `m_md_screen` in row order. |

## One mermaid fence to ASCII art

```mermaid
sequenceDiagram
  participant B as Builder
  participant Doc as MarkdownDoc
  participant A as AsciiMermaid
  participant JS as quickjs-ng
  B->>Doc: code rows and a Fence record
  Doc->>A: Render(src, lines, err)
  opt no runtime yet
    A->>A: locate_bundle, read the file
    A->>JS: JS_Eval(mermaid-ascii.js)
  end
  A->>JS: renderAscii(src)
  alt art returned
    JS-->>A: text
    A-->>Doc: rows
    Doc->>Doc: art rows replace the code rows
  else error or deadline
    JS-->>A: exception or interrupt
    A-->>Doc: false and err
    Doc->>Doc: code rows stay, hint row follows
  end
```

| Step | Who | Note |
|---|---|---|
| Collect | `cb_leave_block` | The fence body is the placeholder. |
| Bounds | `AsciiMermaid::Render` | A deadline, a 1 MB stack and a 128 MB limit on the one shared runtime. The deadline also covers a load of the bundle. |
| Timeout | `discard_engine` | Frees the runtime. The next call builds it again. |
| Splice | `MarkdownDoc::Render` | Runs in reverse order, so earlier indexes stay valid. |

## Open in the browser

```mermaid
sequenceDiagram
  actor User
  participant Panel as FileList
  participant Sh as md-open.sh
  participant OS as open or xdg-open
  participant Page as Browser page
  participant CDN as cdn.jsdelivr.net
  User->>Panel: Alt-F3 on a .md file
  Panel->>Sh: command of the association
  Sh->>Sh: escape the text, write the page
  Sh->>OS: page path
  OS->>Page: load the file URL
  Page->>Page: marked.parse, show the text
  opt the page has a mermaid block
    Page->>CDN: mermaid 12.3.0 with integrity hash
    alt loaded
      CDN-->>Page: script
      Page->>Page: parse each diagram, then draw
    else blocked
      Page->>Page: each diagram shows as code
    end
  end
```

| Step | Who | Note |
|---|---|---|
| Start | `ProcessLocalFileTypes` | Needs an entry that the user added by hand. |
| Write | `md-open.sh` | One page at `${TMPDIR:-/tmp}/far2l-md-<pid>.html`. |
| Show | The page script | The text shows before any network use. |
| Failure | `showCode` | A broken diagram shows its source and its error. Other diagrams still draw. |
