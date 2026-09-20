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
assert(diagnostics.finish(token, {frame: {title: 'Mine yard', scene: 'mine', revision: 7}}));
token = diagnostics.beginAction(2, {title: 'Mine yard', scene: 'mine', revision: 7});
now = 151;
diagnostics.publishFrame({title: 'Company Book', scene: 'book', revision: 8,
  buttons: [{enabled: true}]});
assert(diagnostics.finish(token, {frame: {title: 'Company Book', scene: 'book', revision: 8}}));
assert(diagnostics.snapshot().entries.some(entry => entry.stage === 'first-actionable'));

token = diagnostics.beginAction(3, {scene: 'book', revision: 9});
now = 5152;
diagnostics.publishFrame({scene: 'town', revision: 10});
assert(!diagnostics.snapshot().entries.some(entry =>
  entry.stage === 'transition' && entry.action === 'touch-3'));
assert(diagnostics.finish(token, {frame: {scene: 'town', revision: 10}}));

token = diagnostics.beginAction(4, {scene: 'town', revision: 11});
now = 5153;
diagnostics.clearPendingAction(); // Captured keyboard input clears a prior touch action.
now = 5154;
diagnostics.publishFrame({scene: 'mine', revision: 12});
assert(!diagnostics.snapshot().entries.some(entry =>
  entry.stage === 'transition' && entry.action === 'touch-4'));
assert(diagnostics.finish(token, {frame: {scene: 'mine', revision: 12}}));

token = diagnostics.beginAction(5, {scene: 'mine', revision: 13});
now = 5155;
diagnostics.clearPendingAction(); // Captured pointer input clears a prior touch action.
const replacement = diagnostics.beginAction(6, {scene: 'mine', revision: 13});
now = 5156;
diagnostics.publishFrame({scene: 'book', revision: 14});
const replacements = diagnostics.snapshot().entries.filter(entry => entry.stage === 'transition');
assert(!replacements.some(entry => entry.action === 'touch-5'));
assert(replacements.some(entry => entry.action === 'touch-6'));
assert(diagnostics.finish(token, {frame: {scene: 'book', revision: 14}}));
assert(diagnostics.finish(replacement, {frame: {scene: 'book', revision: 14}}));

for (let index = 0; index < 110; ++index) {
  token = diagnostics.begin('action', {action: 'private words must stay out'});
  now += 1;
  diagnostics.finish(token, {frame: {title: 'A person name', scene: 'town', revision: index}});
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
