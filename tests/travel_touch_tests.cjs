const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const handlers = {};
const documentHandlers = {};
let now = 100;
function element(tag) {
  return {
    tagName: tag.toUpperCase(), children: [], dataset: {}, hidden: false,
    append(...children) { this.children.push(...children); },
    addEventListener: (name, fn) => { handlers[`semantic:${name}`] = fn; },
    setAttribute(name, value) { this[name] = value; },
    focus() { globals.document.activeElement = this; },
    remove() {},
  };
}
const canvas = {
  width: 1040, height: 620,
  addEventListener: (name, fn) => { handlers[name] = fn; },
  getBoundingClientRect: () => ({left: 10, top: 20, width: 520, height: 310}),
  setPointerCapture: () => {},
  setAttribute: () => {},
};
const stage = element('div');
const loading = element('div');
loading.hidden = true;
const holds = [], taps = [];
const globals = {
  Module: {
    _CrownlessTouchHold: (...args) => holds.push(args),
    _CrownlessTouchTap: (...args) => taps.push(args),
    _CrownlessTouchActivate: () => {},
  },
  performance: {now: () => now},
  requestAnimationFrame: callback => callback(),
  document: {
    activeElement: canvas,
    body: element('body'),
    createElement: element,
    querySelector: selector => selector === '#canvas' ? canvas :
      selector === '#stage' ? stage : selector === '#loading' ? loading : null,
    addEventListener: (name, fn) => { documentHandlers[name] = fn; },
  },
  window: {addEventListener: (name, fn) => { handlers[name] = fn; }},
};
globals.globalThis = globals;
vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../web/diagnostics.js'), 'utf8'), globals);
vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../web/touch.js'), 'utf8'), globals);
assert.equal(stage.children[0].id, 'touch-actions');
assert.equal(stage.children[0].children.length, 4);
const point = {pointerType: 'touch', pointerId: 1, isPrimary: true,
  clientX: 200, clientY: 290, preventDefault() {}};
handlers.pointerdown(point);
assert.deepEqual(holds.at(-1), [380, 540, 1]);
handlers.pointerup(point);
assert.deepEqual(holds.at(-1), [0, 0, 0]);
assert.deepEqual(taps, [[380, 540]]);
for (const event of ['pointercancel', 'lostpointercapture', 'blur', 'resize', 'visibilitychange']) {
  handlers.pointerdown(point);
  (event === 'visibilitychange' ? documentHandlers : handlers)[event]();
  assert.equal(holds.at(-1)[2], 0, event);
  handlers.pointerup(point);
  assert.equal(taps.length, 1, event);
}
handlers.pointerdown(point);
handlers.pointermove({...point, clientX: 250});
assert.deepEqual(holds.at(-1), [480, 540, 1]);
handlers.pointerup(point);
assert.equal(taps.length, 1);
handlers.pointerdown(point);
handlers.pointerdown({...point, pointerId: 2, isPrimary: false});
assert.equal(holds.at(-1)[2], 0);

const diagnostics = globals.Module.crownlessDiagnostics;
const frame = {
  title: 'Mine yard', scene: 'mine', revision: 3,
  buttons: [{label: 'Open book', enabled: true, active: false}]
};
globals.Module.renderCrownlessTouch(frame);
const button = stage.children[0].children[2].children[0];
let stale = diagnostics.beginAction(3, frame);
documentHandlers.keydown({key: 'Escape'});
now += 20;
diagnostics.publishFrame({scene: 'town', revision: 4});
assert(!diagnostics.snapshot().entries.some(entry =>
  entry.stage === 'transition' && entry.action === 'touch-3'));
assert(diagnostics.finish(stale, {frame: {scene: 'town', revision: 4}}));

stale = diagnostics.beginAction(4, frame);
documentHandlers.pointerdown({});
button.onclick();
now += 20;
diagnostics.publishFrame({scene: 'book', revision: 5});
const transitions = diagnostics.snapshot().entries.filter(entry => entry.stage === 'transition');
assert(!transitions.some(entry => entry.action === 'touch-4'));
assert(transitions.some(entry => entry.action === 'touch-0'));
assert(diagnostics.finish(stale, {frame: {scene: 'book', revision: 5}}));
console.log('Touch hold, release, drag, cancellation and diagnostics attribution pass.');
