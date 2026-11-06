// Entry of far2l/bootstrap/mermaid-ascii.js, built by build.sh into one IIFE that sets
// the global renderAscii(src): the box-drawing art of a Mermaid diagram, one string,
// rows without trailing spaces. It throws Error('unsupported: <word>') for a diagram
// type outside SUPPORTED, and another Error (one line) for a parse failure. For graph and
// flowchart, edge operators written without spaces (A-->B) are spaced before the parse.
import { renderMermaidASCII } from './node_modules/beautiful-mermaid/src/ascii/index.ts';

const SUPPORTED = ['graph', 'flowchart', 'stateDiagram', 'stateDiagram-v2', 'sequenceDiagram',
    'classDiagram', 'erDiagram', 'xychart-beta'];

// Edge operators: --> --- -.-> -.- ==> ===, labelled -- text --> and -. text .-> forms, and
// the |label| after an operator. Each match is one operator token.
const EDGE = /<?(?:-{2,}>?|-\.-+>?|={2,}>?|-\.(?=\s)|\.-+>?)(?:\|[^|\n]*\|)?/g;
const MASK = '\u0000';

// The line with every character inside [] () {} or "" and everything after %% replaced by
// MASK, so an operator there is not matched. The length stays the same.
function maskProtected(line) {
    let out = '';
    let depth = 0;
    let quoted = false;
    for (let i = 0; i < line.length; i++) {
        const c = line[i];
        if (quoted) {
            out += MASK;
            if (c === '"') quoted = false;
        } else if (c === '%' && line[i + 1] === '%') {
            return out + MASK.repeat(line.length - i);
        } else if (c === '"') {
            quoted = true;
            out += MASK;
        } else if ('[({'.includes(c)) {
            depth++;
            out += MASK;
        } else if (depth > 0) {
            if (')]}'.includes(c)) depth--;
            out += MASK;
        } else {
            out += c;
        }
    }
    return out;
}

// One line with a space put on each side of every edge operator that touches other text.
// Spaced operators are left as they are.
function spaceEdges(line) {
    const masked = maskProtected(line);
    let out = '';
    let last = 0;
    let m;
    EDGE.lastIndex = 0;
    while ((m = EDGE.exec(masked)) !== null) {
        const start = m.index;
        const end = start + m[0].length;
        out += line.slice(last, start);
        if (start > 0 && !/\s/.test(line[start - 1])) out += ' ';
        out += line.slice(start, end);
        if (end < line.length && !/\s/.test(line[end])) out += ' ';
        last = end;
    }
    return out + line.slice(last);
}

globalThis.renderAscii = function renderAscii(src) {
    const text = String(src);
    const word = text.trim().split(/[\s;]/)[0];
    if (word !== '' && !SUPPORTED.includes(word)) {
        throw new Error('unsupported: ' + word);
    }
    const source = word === 'graph' || word === 'flowchart'
        ? text.split('\n').map(spaceEdges).join('\n') : text;
    let art;
    try {
        art = renderMermaidASCII(source, { colorMode: 'none', useAscii: false });
    } catch (e) {
        const msg = e && e.message !== undefined ? e.message : e;
        throw new Error(String(msg).replace(/\s*\n\s*/g, ' '));
    }
    return art.split('\n').map((row) => row.replace(/\s+$/, '')).join('\n');
};
