// Copies the WASM demo into test-results so the existing CI upload
// (playwright-artifacts) always has a zip people can download.
const fs = require('fs');
const path = require('path');

module.exports = async () => {
  const root = path.join(__dirname, '..', '..', '..');
  const dest = path.join(root, 'test-results', 'browser-demo');
  fs.mkdirSync(dest, { recursive: true });
  fs.cpSync(path.join(root, 'web', 'demo'), dest, { recursive: true });
};
