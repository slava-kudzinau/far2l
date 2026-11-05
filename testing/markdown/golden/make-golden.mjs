// Writes golden/<name>.txt for every golden/<name>.mmd from the beautiful-mermaid package
// itself (not from the bundle). The package must be installed first:
//   sh far2l/bootstrap/mermaid-ascii/build.sh
// then run: node testing/markdown/golden/make-golden.mjs
import { readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const pkgDir = join(here, '../../../far2l/bootstrap/mermaid-ascii/node_modules/beautiful-mermaid');
const { renderMermaidASCII } = await import(pathToFileURL(join(pkgDir, 'dist/index.js')).href);

// The first word of the source decides unsupported types (same list as entry.js).
const SUPPORTED = ['graph', 'flowchart', 'stateDiagram', 'stateDiagram-v2', 'sequenceDiagram',
    'classDiagram', 'erDiagram', 'xychart-beta'];

for (const mmd of readdirSync(here).filter((f) => f.endsWith('.mmd')).sort()) {
    const src = readFileSync(join(here, mmd), 'utf8');
    const word = src.trim().split(/[\s;]/)[0];
    let out;
    if (word !== '' && !SUPPORTED.includes(word)) {
        out = `unsupported: ${word}`;
    } else {
        try {
            out = renderMermaidASCII(src, { colorMode: 'none', useAscii: false })
                .split('\n').map((row) => row.replace(/\s+$/, '')).join('\n');
        } catch (e) {
            out = String(e.message).replace(/\s*\n\s*/g, ' ');
        }
    }
    writeFileSync(join(here, mmd.replace(/\.mmd$/, '.txt')), out + '\n');
    console.log(`${mmd}: ${out.split('\n')[0]}`);
}
