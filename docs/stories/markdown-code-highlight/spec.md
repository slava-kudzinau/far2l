# Spec: Syntax colors in fenced code blocks

Two units are under test: the Colorer plugin export `plugins/colorer/src/pcolorer2/ColorerExport.cpp` and the Markdown side `far2l/src/markdown/MdColorer.cpp` with its use in `MarkdownRender.cpp` and `ViewerPrinter.cpp`.

## Plugin export: `ColorerTokenizeV1`

```cpp
extern "C" int ColorerTokenizeV1(const wchar_t *lang, const wchar_t *const *lines, int count,
    void (*emit)(void *ctx, int line, int start, int end, int token_class),
    void *ctx, int deadline_ms);
```

| Call | Result |
|---|---|
| `ColorerTokenizeV1(L"js", lines, 3, emit, ctx, 40)` on `var s = "x"; // c` | Returns 0. Emits, in source order, one event per token, half-open `[start, end)` in wide characters: a keyword event, a string event, a comment event. |
| Any call before the plugin created `editorSet` | Returns -1. Emits nothing. Does not create `editorSet`. |
| `lang` = `L"nosuchlang9"` | Returns -2. Emits nothing. |
| A Colorer `Exception` during setup or parse | Caught. Returns -3. Events emitted before the exception stay. |
| The deadline passes during a call | Calls `breakParse()`. Emits the spans of the lines finished before the deadline. Returns 1. |
| `count` = 0 | Returns 0. Emits nothing. |

The token classes are fixed integers, stable across calls and processes:

| Class | Value | `def:*` regions that map to it |
|---|---|---|
| keyword | 1 | `def:Keyword` |
| string | 2 | `def:String`, `def:Character` |
| comment | 3 | `def:Comment` |
| number | 4 | `def:Number` |

The export maps a region to a class with `Region::isAncestorOf(scheme, L"def", <name>)`, the pattern the plugin's own coloring uses. A region with none of them emits no event, so the text keeps the code role.

## Core bridge: `far2l/src/markdown/MdColorer`

| Call | Result |
|---|---|
| `MdGetTokenSource()` the first time in a process, with Colorer enabled | Returns a source whose `Tokenize` calls the plugin. The lookup runs `PluginManager::FindPlugin(FAR_PID_COLORER)`, `Plugin::Load`, `dlopen(GetModuleName(), RTLD_LAZY \| RTLD_NOLOAD)`, then `dlsym("ColorerTokenizeV1")`, one time. |
| `MdGetTokenSource()` with no Colorer plugin, a disabled plugin, a plugin without the export, or a `dlopen` failure | Returns a source whose `Tokenize` returns false for every fence. No message, no modal dialog. |
| `Tokenize(lang, lines, budget_ms, out)` with a real source | Returns true when the export returned 0 or 1. Appends each token to `out`, in source order, in wide-character coordinates of the fence source text. |
| `Tokenize(lang, lines, budget_ms, out)` after `MdSetTokenSourceForTest` | Appends the canned tokens of that test. No plugin, no `dlopen`. |
| A code line longer than the screen, with a horizontal scroll | The row shows the slice at `LeftPos`, and each cell keeps the color of its token. |
| A language name in the alias table | The alias is used for the file type lookup. `c++` and `zsh` reach a Colorer file type. |
| A language name of 33 characters, or with a character outside `[A-Za-z0-9+#_-]` | No parse. The fence keeps the code role. |

## Language names

The core picks the file type by the fake file name `x.<lang>`. The alias table is complete and exact:

| Fence name | File name sent to Colorer |
|---|---|
| `c++` | `x.cpp` |
| `zsh` | `x.sh` |

The table also maps the long slug of every language that Colorer knows
by a shorter name: `javascript` to `js`, `typescript` to `ts`, `python`
to `py`, `bash` and `shell` to `sh`, `rust` to `rs`, `csharp` to `cs`,
`golang` to `go`, `kotlin` to `kt`, `powershell` to `ps1`, `docker` to
`dockerfile`, `latex` to `tex`, `plaintext` to `text`, `perl` to `pl`,
`elixir` to `ex`, `erlang` to `erl`, `haskell` to `hs`, `julia` to
`jl`, `objectivec`, `objc` and `obj-c` to `m`, `fsharp` to `fs`,
`vbnet` to `vb`, `assembly` to `asm`, `pascal` to `pas`, `fortran` to
`f`, `make` to `makefile`, `gitignore` to `ignore`, `apache` and
`apacheconf` to `conf`, `blade` to `php`, `handlebars` to `hbs`,
`brainfuck` to `bf`, `graphviz` to `dot` and `terraform` to `hcl`. A
slug absent from the table is sent as it is. The `slug_probe` driver
case checks every row against the installed plugin (owner list
2026-11-07). The languages without a scheme in the bundle keep the
code role: `matlab`, `clojure`, `nim`, `scheme`, `prolog`,
`actionscript`, `applescript`, `nginx`, `http`, `liquid`, `ejs`,
`mermaid`.

The file type `default` means no colors: the fence keeps the code role. These twelve names reach all four classes in the fixture test: `js`, `ts`, `python`, `bash`, `c`, `cpp`, `go`, `rust`, `java`, `sql`. `json` and `yaml` reach the classes their HRC scheme defines, at least string and number, and are accepted with fewer classes.

## Token roles and colors

Four new roles join the table in `far2l/src/markdown/MdPalette.cpp`. They follow the rules of story markdown-palette: user file, then the palette entry, then the RGB default under the contrast rule, then the 16-color fallback.

| Role | `markdown-colors.ini` key | 256 index | RGB | 16-color fallback |
|---|---|---|---|---|
| `MD_ROLE_TOKEN_KEYWORD` | `token-keyword` | 111 | `#87AFFF` | `F_LIGHTGRAY` |
| `MD_ROLE_TOKEN_STRING` | `token-string` | 120 | `#87FF87` | `F_LIGHTGREEN` |
| `MD_ROLE_TOKEN_COMMENT` | `token-comment` | 103 | `#8787AF` | `F_CYAN` |
| `MD_ROLE_TOKEN_NUMBER` | `token-number` | 215 | `#FFAF5F` | `F_LIGHTRED` |

| Case | Result |
|---|---|
| 24-bit console, dark viewer background | Each role shows its RGB value. |
| 24-bit console, light viewer background (contrast under 3:1) | All four roles show `COL_HELPHIGHLIGHTTEXT`, the code entry. The contrast verdict is cached on the viewer background, as for the other roles. |
| `--norgb` | Each role shows the 16-color fallback of the table. No RGB bits are in the cell attribute. |
| A token on a cell past the end of the source text, on a wide screen | The cell keeps the code role and the plain background. |

## Input limits

Markdown is untrusted input. The limits are exact:

| Limit | Value | Effect when it is hit |
|---|---|---|
| Characters per source line sent to the parser | 1000 | The rest of that line keeps the code role. |
| Lines per fence | 400 | Lines from 401 on keep the code role. |
| Source bytes per fence | 65536 | Bytes from there on keep the code role. |
| Time for all fences in one render | 40 ms | The fences that did not finish keep the code role, and the fences not yet started are skipped. |

A token run never parses again on a resize. `MarkdownDoc::Render` keeps a fence cache, keyed by the fence language and a 64-bit hash of the fence source text, in the form of the existing mermaid cache. A resize re-renders the layout, takes a cache hit, and rebuilds each line's token group, in the cell coordinates of that line's code run, with no `ColorerTokenizeV1` call. A code line with no tokens keeps `tokens` = -1.

## Layout invariance

| Case | Result |
|---|---|
| Any fixture, before and after this story | The rendered text, the row count and the `nFilePos` of each row do not change. Only the foreground color of cells inside a fence can change. |
| A mermaid fence, drawn and not drawn | Unchanged. |
| A file open in F3 while the same file is open in F4 | The editor colors do not change. The two use the same `ParserFactory` and do not share parse state. |

## Validation

| Call | Result |
|---|---|
| Python fence, one line of 1 MB of `'` | Renders in at most 120 ms wall clock. The line keeps the code role past 1000 characters. The viewer stays responsive. |
| A fence with an unclosed string, deep nesting, many small fences, a fence name of 32 characters | Rendered within the limits above, with no crash and no hang. |
| A fence of 401 lines | The first 400 lines are colored, the rest keep the code role. |
| A multi-line block comment | The comment color covers every row of the comment, and the state does not leak past the fence. |

## Done

`BUILD=../far2l.markdown-code-highlight.build` "The check" of `AGENTS.md`, with `TESTS="0007-view-file-markdown 0008-view-md-default 0009-view-md-setting-off 000c-view-md-highlight"`, plus `sh testing/markdown/ascii-golden.sh`, `sh testing/markdown/ascii-driver.sh` and `sh testing/markdown/highlight-driver.sh`, all exit 0. Every table row above is covered by a case in `testing/tests/000c-view-md-highlight/test.js` or in `testing/markdown/highlight-driver.cpp`. The full build (`cmake --build ../far2l.markdown-code-highlight.build -j8`) builds the `colorer` target and installs the plugin module next to the binary, where the smoke run finds it. The new test reads cells with `ReadCell(x, y)` of the smoke harness, the way `0007-view-file-markdown` does: `Fore` gives the 16-color index, and `IsForeTC` with `ForeTC` gives the 24-bit color.

## Out of scope

- A link from the core to the Colorer library or to libxml2.
- HRD colors, the plugin's own F3 coloring (`ViewerColoring`), a built-in tokenizer.
- Indented code blocks, HTML blocks, code spans, mermaid fences.
- A worker thread. Colorer parsing runs on the main thread.
