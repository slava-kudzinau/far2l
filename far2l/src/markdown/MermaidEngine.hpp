#ifndef FAR2L_MERMAID_ENGINE_HH
#define FAR2L_MERMAID_ENGINE_HH

#include <string>

// Drives embedded mermaid.min.js inside a singleton quickjs-ng runtime.
// Renders a mermaid diagram source into an SVG string.
class MermaidEngine {
public:
    // Lazy init; false on shim/parse failure (caller falls back to raw fence).
    static bool Available();

    // Renders `mermaid_src` into `svg_out` via mermaid.render().  `id` is the
    // SVG element id.  `width_px` is currently unused: mermaid sizes the SVG itself.
    // On failure, fills `err` (cleared on entry) and returns false.
    static bool Render(const std::string &mermaid_src, const std::string &id,
                       int width_px, std::string &svg_out, std::string &err);
};

#endif // FAR2L_MERMAID_ENGINE_HH
