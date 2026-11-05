// Entry of far2l/bootstrap/mermaid-ascii.js, built by build.sh into one IIFE that sets
// the global renderAscii(src): the box-drawing art of a Mermaid diagram, one string,
// rows without trailing spaces. It throws Error('unsupported: <word>') for a diagram
// type outside SUPPORTED, and another Error (one line) for a parse failure.
import { renderMermaidASCII } from './node_modules/beautiful-mermaid/src/ascii/index.ts';

const SUPPORTED = ['graph', 'flowchart', 'stateDiagram', 'stateDiagram-v2', 'sequenceDiagram',
    'classDiagram', 'erDiagram', 'xychart-beta'];

globalThis.renderAscii = function renderAscii(src) {
    const text = String(src);
    const word = text.trim().split(/[\s;]/)[0];
    if (word !== '' && !SUPPORTED.includes(word)) {
        throw new Error('unsupported: ' + word);
    }
    let art;
    try {
        art = renderMermaidASCII(text, { colorMode: 'none', useAscii: false });
    } catch (e) {
        const msg = e && e.message !== undefined ? e.message : e;
        throw new Error(String(msg).replace(/\s*\n\s*/g, ' '));
    }
    return art.split('\n').map((row) => row.replace(/\s+$/, '')).join('\n');
};
