const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

let now = 100;
const globals = {
  Module: {postRun: [], crownlessBuild: 'test-build', crownlessScreen: 'title'},
  performance: {now: () => now},
  console
};
globals.globalThis = globals;
vm.runInNewContext(
  fs.readFileSync(path.join(__dirname, '../web/diagnostics.js'), 'utf8'), globals);

now = 126.25;
globals.Module.postRun[0]();
const diagnostics = globals.Module.crownlessDiagnostics;
let token = diagnostics.begin('save', {action: 'campaign'});
now = 134.75;
assert(diagnostics.finish(token, {frame: {title: 'Mine yard', revision: 7}}));
token = diagnostics.beginAction(2, {title: 'Mine yard', revision: 7});
now = 151;
diagnostics.publishFrame({title: 'Company Book', revision: 8});
assert(diagnostics.finish(token, {frame: {title: 'Company Book', revision: 8}}));

for (let index = 0; index < 110; ++index) {
  token = diagnostics.begin('action', {action: 'private words must stay out'});
  now += 1;
  diagnostics.finish(token, {frame: {title: 'A person name', revision: index}});
}
const snapshot = diagnostics.snapshot();
assert.equal(snapshot.schema, 1);
assert.equal(snapshot.capacity, 96);
assert.equal(snapshot.entries.length, 96);
assert(snapshot.entries.some(entry => entry.action === 'other'));
for (const entry of snapshot.entries) {
  assert.deepEqual(Object.keys(entry),
    ['stage', 'duration_ms', 'build', 'scene', 'revision', 'action']);
  assert(Number.isFinite(entry.duration_ms) && entry.duration_ms >= 0);
  assert(Number.isSafeInteger(entry.revision));
}
const exported = globals.Module.exportCrownlessDiagnostics();
assert(!exported.includes('private words'));
assert(!exported.includes('A person name'));
assert(exported.length < 20000);
console.log('Bounded, secret-free browser timing diagnostics passed.');
