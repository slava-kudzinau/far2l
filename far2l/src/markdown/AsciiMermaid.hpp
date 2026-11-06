#ifndef FAR2L_ASCII_MERMAID_HH
#define FAR2L_ASCII_MERMAID_HH

#include <string>
#include <vector>

// Draws a Mermaid diagram as box-drawing rows.  The engine is mermaid-ascii.js (far2l/bootstrap,
// built from beautiful-mermaid), run in one quickjs runtime for the process.
class AsciiMermaid {
public:
    // Renders `src` into `lines`, one art row each, without trailing spaces or trailing empty rows,
    // and returns true.  On failure returns false with `lines` empty and `err` set to one of:
    // "unsupported: <type>", the parser's one-line message, "timeout", "bundle missing",
    // "empty output" or "js: <message>".  Runs on the calling thread and may block up to the
    // render deadline (2 s, or FAR2L_MD_DEADLINE_MS).
    static bool Render(const std::string &src, std::vector<std::string> &lines, std::string &err);
};

#endif // FAR2L_ASCII_MERMAID_HH
