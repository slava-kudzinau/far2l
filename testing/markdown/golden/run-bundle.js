// Shared runner of ascii-golden.sh: prints renderAscii() of the sample __sample, or the
// error message on one line. The host defines __sample, __read(path) and __print(text)
// and loads mermaid-ascii.js before this file runs.
let out;
try {
    out = renderAscii(__read(__sample));
} catch (e) {
    out = String(e && e.message !== undefined ? e.message : e);
}
__print(out);
