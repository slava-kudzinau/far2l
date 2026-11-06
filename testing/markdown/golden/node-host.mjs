// Node leg of ascii-golden.sh: loads the bundle and the shared runner for one sample.
// usage: node node-host.mjs <mermaid-ascii.js> <run-bundle.js> <sample.mmd>
import { readFileSync } from 'node:fs';

const [bundle, runner, sample] = process.argv.slice(2);
(0, eval)(readFileSync(bundle, 'utf8'));
globalThis.__sample = sample;
globalThis.__read = (path) => readFileSync(path, 'utf8');
globalThis.__print = (text) => process.stdout.write(text + '\n');
(0, eval)(readFileSync(runner, 'utf8'));
