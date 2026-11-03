import { cpSync, existsSync, mkdirSync, readdirSync } from 'node:fs';
import { execSync } from 'node:child_process';

const emitRoots = [
  '_build/default/packages/universal-reason-react/lucide-icons/test-browser/dist/node_modules',
  '_build_browser_tests/default/packages/universal-reason-react/lucide-icons/test-browser/dist/node_modules',
];

const emitRoot = emitRoots.find(existsSync);

function buildBindings() {
  try {
    execSync('opam exec -- dune build --build-dir _build_browser_tests @lucide-icons-browser-tests', { stdio: 'inherit' });
  } catch (_error) {
    console.log('Could not build melange runtime bindings; run a browser-test build first.');
    process.exit(0);
  }
}

if (!emitRoot) buildBindings();

const root = emitRoots.find(existsSync);
if (!root) {
  console.log('Melange runtime bindings not built yet; skipping.');
  process.exit(0);
}

mkdirSync('node_modules', { recursive: true });
for (const entry of readdirSync(root)) {
  const target = `node_modules/${entry}`;
  // Never overwrite pnpm-managed packages; only (re)install dune-emitted ones.
  if (existsSync(target)) continue;
  cpSync(`${root}/${entry}`, target, { recursive: true });
  console.log(`installed ${entry} into node_modules`);
}
