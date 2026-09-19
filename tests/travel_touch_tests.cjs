const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const handlers = {};
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
  },
  document: {
    activeElement: canvas,
    body: element('body'),
    createElement: element,
    querySelector: selector => selector === '#canvas' ? canvas :
      selector === '#stage' ? stage : selector === '#loading' ? loading : null,
    addEventListener: (name, fn) => { handlers[name] = fn; },
  },
  window: {addEventListener: (name, fn) => { handlers[name] = fn; }},
};
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
  handlers[event]();
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
console.log('Touch hold, release, drag, cancellation and focus changes pass.');
