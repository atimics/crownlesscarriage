const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const shell = fs.readFileSync(path.join(__dirname, '../web/shell.html'), 'utf8');
const script = shell.match(/<script>([\s\S]*?)<\/script>/)[1];
function loadShell() {
  const document = {activeElement: null};
  const elements = new Map();
  for (const id of ['canvas', 'loading', 'status', 'progress', 'save-status', 'stage']) {
    elements.set('#' + id, {
      hidden: false, value: 0, dataset: {},
      focus() { document.activeElement = this; },
      addEventListener() {}
    });
  }
  document.querySelector = selector => elements.get(selector);
  const globals = {document, window: {addEventListener() {}}, console};
  vm.runInNewContext(script, globals);
  return {Module: globals.Module, document, elements};
}

const normal = loadShell();
normal.Module.setStatus('');
normal.Module.postRun[0]();
assert.equal(normal.elements.get('#loading').hidden, true);
assert.equal(normal.document.activeElement, normal.elements.get('#canvas'));
assert.equal(Boolean(normal.Module.crownlessRecoveryActive), false);

const failed = loadShell();
failed.Module.setStatus('Preparing… (1/2)');
failed.Module.showCrownlessStartupFailure();
// Emscripten completes startup even when main returns a failure status.
failed.Module.setStatus('');
failed.Module.postRun[0]();
assert.equal(failed.Module.crownlessRecoveryActive, true);
assert.equal(failed.elements.get('#loading').hidden, false);
assert.equal(failed.elements.get('#progress').hidden, true);
assert.equal(failed.elements.get('#progress').value, 0);
assert.equal(failed.document.activeElement, failed.elements.get('#loading'));
assert.equal(failed.elements.get('#status').textContent,
  'The game could not start its graphics. WebGL 2 is required. Check that browser graphics acceleration is enabled, then reload the page.');
console.log('Startup recovery visibility, focus, and normal startup passed.');
