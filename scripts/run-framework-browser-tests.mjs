import { spawn } from 'node:child_process';

const server = spawn('./_build_browser_tests/default/demos/todo/server/src/server.exe', {
  cwd: process.cwd(),
  env: {
    ...process.env,
    TODO_DOC_ROOT: './_build_browser_tests/default/demos/todo/ui/src',
    SERVER_INTERFACE: '127.0.0.1',
  },
  stdio: 'inherit',
});

const waitForServer = async () => {
  for (let attempt = 0; attempt < 50; attempt += 1) {
    try {
      const response = await fetch('http://127.0.0.1:8080/');
      if (response.ok) {
        return;
      }
    } catch (_error) {
      // server not ready yet
    }

    await new Promise(resolve => setTimeout(resolve, 200));
  }

  throw new Error('Timed out waiting for todo browser test server');
};

const runNode = script => new Promise((resolve, reject) => {
  const child = spawn('node', [script], {
    cwd: process.cwd(),
    stdio: 'inherit',
    env: process.env,
  });

  child.on('exit', code => {
    if (code === 0) {
      resolve();
    } else {
      reject(new Error(`Command failed for ${script} with exit code ${code}`));
    }
  });

  child.on('error', reject);
});

try {
  await waitForServer();
  await runNode('./packages/universal-reason-react/router/test-browser/generated/RouterBrowserTest.mjs');
  await runNode('./packages/universal-reason-react/store/test-browser/generated/StoreBrowserTest.mjs');
  await runNode('./packages/universal-reason-react/components/test-browser/generated/ComponentsBrowserTest.mjs');
  await runNode('./packages/universal-reason-react/lucide-icons/test-browser/generated/LucideIconsBrowserTest.mjs');
  await runNode('./packages/universal-reason-react/sonner/test-browser/generated/SonnerBrowserTest.mjs');
  await runNode('./packages/universal-reason-react/intl/test-browser/generated/IntlBrowserTest.mjs');
  await runNode('./demos/todo/tests/browser/generated/TodoBrowserTest.mjs');
} finally {
  server.kill('SIGTERM');
}
