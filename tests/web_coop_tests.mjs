import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const source = await readFile(new URL('../web/coop_game.js', import.meta.url), 'utf8');
const persistence = await readFile(new URL('../web/persistence.js', import.meta.url), 'utf8');
const world = '1'.repeat(32), key = `cc-coop-pending-${world}`;
const storage = new Map([['cc-coop-token', 'a'.repeat(64)]]);
const requests = [], responses = [];
function client() {
  const context = vm.createContext({
    Module: {}, location: { search: `?world=${world}` }, URLSearchParams, AbortSignal,
    localStorage: { getItem: key => storage.get(key) ?? null, setItem: (key, value) => storage.set(key, value), removeItem: key => storage.delete(key) },
    navigator: { locks: { request: (_key, callback) => callback() } },
    performance: { now: () => 2000 },
    fetch: async (path, options) => {
      requests.push({path, ...options});
      const response = responses.shift();
      if (response instanceof Error) throw response;
      assert.ok(response, 'Every network request needs an expected response');
      return { ok: !response.error, status: response.error ? 409 : 200, json: async () => response };
    }
  });
  vm.runInContext(source, context);
  vm.runInContext(persistence, context);
  assert.equal(context.Module.crownlessCampaignAccess, 0);
  assert.equal(typeof context.Module.persistCrownlessSave, 'function');
  assert.equal(context.Module.preRun, undefined, 'Shared worlds acquire their campaign from the host');
  return context.Module.ccCoop;
}
const state = revision => ({ id: world, revision, action_revision: revision, next_sequence: revision + 1,
  campaign: `snapshot-${revision}`, crew: [{online:true}], paused:false });

let coop = client();
responses.push(state(0));
await coop.connect();
assert.equal(coop.take(), 'snapshot-0');
responses.push(new Error('Connection interrupted'));
await assert.rejects(coop.apply('trade', '0', 0, 1), /Connection interrupted/);
const saved = storage.get(key);
assert.ok(saved, 'An uncertain command survives a reload');

coop = client();
responses.push(state(1));
await coop.connect();
responses.push({accepted:true, duplicate:true, world:state(1)});
await coop.apply('trade', '0', 0, 1);
assert.deepEqual(JSON.parse(requests.at(-1).body), {...JSON.parse(saved), campaign:true});
assert.equal(storage.has(key), false);
assert.equal(coop.take(), 'snapshot-1');

storage.set(key, saved);
responses.push({accepted:true, duplicate:true, world:state(1)});
await assert.rejects(coop.apply('travel', '42', 0, 0), /Earlier company action recovered/);
assert.equal(JSON.parse(requests.at(-1).body).action, 'trade');
assert.equal(storage.has(key), false);
assert.equal(coop.take(), 'snapshot-1');

responses.push({error:'The company has changed.'});
await assert.rejects(coop.apply('travel', '42', 0, 0), /company has changed/);
assert.equal(storage.has(key), false, 'A rejected action can be chosen again after refresh');

responses.push(state(0));
coop.poll();
await new Promise(resolve => setImmediate(resolve));
assert.equal(coop.take(), null, 'An older poll preserves the latest campaign');
assert.ok(requests.at(-1).path.endsWith('after=1'));

coop = client();
responses.push(state(1));
await coop.connect();
coop.take();
let finishPoll, finishCommand;
responses.push(new Promise(resolve => { finishPoll = resolve; }));
coop.poll();
responses.push(new Promise(resolve => { finishCommand = resolve; }));
const applying = coop.apply('trade', '0', 0, 1);
finishPoll(state(3));
await new Promise(resolve => setImmediate(resolve));
finishCommand({accepted:true, world:state(2)});
await applying;
assert.equal(coop.take(), 'snapshot-3', 'A late action response preserves the newer C campaign');
assert.equal(responses.length, 0);
console.log('Shared browser recovery, ordering, and save ownership passed.');
