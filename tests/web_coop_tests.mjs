import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const source = await readFile(new URL('../web/coop_game.js', import.meta.url), 'utf8');
const avatar = await readFile(new URL('../web/coop/avatar.js', import.meta.url), 'utf8');
const persistence = await readFile(new URL('../web/persistence.js', import.meta.url), 'utf8');
const world = '1'.repeat(32), key = `cc-coop-pending-${world}`;
const storage = new Map([['cc-coop-token', 'a'.repeat(64)]]);
const requests = [], responses = [];
const files = new Map();
let now = 2000;
function client() {
  files.clear();
  const context = vm.createContext({
    Module: {}, console,
    FS: { mkdirTree() {}, readFile: path => files.get(path), writeFile: (path, text) => files.set(path, text) },
    location: { search: `?world=${world}` }, URLSearchParams, AbortSignal,
    document: {readyState:'loading', addEventListener() {}, body:{dataset:{}}},
    window: {addEventListener() {}},
    localStorage: { getItem: key => storage.get(key) ?? null, setItem: (key, value) => storage.set(key, value), removeItem: key => storage.delete(key) },
    navigator: { locks: { request: (_key, callback) => callback() } },
    performance: { now: () => now },
    fetch: async (path, options) => {
      requests.push({path, ...options});
      const response = responses.shift();
      if (response instanceof Error) throw response;
      assert.ok(response, 'Every network request needs an expected response');
      return { ok: !response.error, status: response.status || (response.error ? 409 : 200), json: async () => response };
    }
  });
  vm.runInContext(avatar, context);
  vm.runInContext(source, context);
  vm.runInContext(persistence, context);
  assert.equal(context.Module.crownlessCampaignAccess, 0);
  assert.equal(typeof context.Module.persistCrownlessSave, 'function');
  context.Module.preRun.forEach(run => run());
  const preferencesPath = '/tmp/crownless-coop.ccsave.preferences';
  assert.equal(files.get(preferencesPath), storage.get('cc-coop-preferences'));
  const preferences = 'CROWNLESS_PREFERENCES 2\nreduced_motion 1\naudio_mode 2\n';
  files.set(preferencesPath, preferences);
  context.Module.persistCrownlessPreferences(preferencesPath);
  assert.equal(storage.get('cc-coop-preferences'), preferences);
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

const sessionKey = `cc-coop-session-${world}`;
const session = sequence => ({sequence, context:'town', session:`CROWNLESS_SESSION 7\n1 2 0 0 0 ${sequence} 29 0 1 3\n`});
storage.set(sessionKey, JSON.stringify(session(8)));
coop = client();
responses.push({...state(4), session_context:'town', session:session(7)});
await coop.connect();
assert.equal(coop.hasSession(), true);
assert.equal(files.get('/tmp/crownless-coop.ccsave.session'), session(8).session,
  'The newest local checkpoint wins when a page closes before its host request finishes');
responses.push({}, {...state(4), session_context:'town'});
coop.poll();
await new Promise(resolve => setImmediate(resolve));
assert.deepEqual(JSON.parse(requests.at(-2).body), session(8));
coop.checkpoint(session(9).session);
assert.equal(JSON.parse(storage.get(sessionKey)).sequence, 9);
assert.equal(JSON.parse(storage.get(sessionKey)).session, session(9).session,
  'Movement is saved in the browser before its next host request');

coop = client();
responses.push({...state(4), session_context:'town', session:session(10)});
await coop.connect();
assert.equal(files.get('/tmp/crownless-coop.ccsave.session'), session(10).session,
  'A newer host checkpoint restores the place saved by another browser');
files.clear();
coop = client();
responses.push({...state(5), session_context:'road', session:session(10)});
await coop.connect();
assert.equal(coop.hasSession(), false);
assert.equal(files.has('/tmp/crownless-coop.ccsave.session'), false, 'A shared carriage move starts in the current scene');
assert.equal(responses.length, 0);
console.log('Shared browser recovery, ordering, and save ownership passed.');

// Player poses update independently of shared campaign saves.
storage.delete(sessionKey);
coop = client();
responses.push({...state(5), session_context:'town', visit:'visit-one'});
await coop.connect();
const pose = Array(83).fill(0);
const peer = {id:'b'.repeat(16), name:'Bren', appearance:{coat:3, hair:2}, pose};
responses.push({context:'town', scene:0, peers:[peer]});
assert.equal(coop.exchange(0, () => pose).length, 0);
await new Promise(resolve => setImmediate(resolve));
assert.equal(coop.exchange(0, () => { throw Error('Pose reads are throttled'); })[0].name, 'Bren');
assert.equal(typeof coop.exchange(0, () => pose)[0].appearance, 'number');
assert.equal(JSON.parse(requests.at(-1).body).visit, 'visit-one');
assert.equal(coop.exchange(1, () => pose).length, 0, 'Changing rooms clears the prior crew');
now += 4000;
responses.push({context:'town', scene:1, peers:[peer]});
assert.equal(coop.exchange(1, () => pose).length, 0, 'Expired poses stay hidden');
await new Promise(resolve => setImmediate(resolve));
now += 100;
responses.push({error:'Host restarted', status:428}, {...state(6), session_context:'town', visit:'visit-two'});
coop.exchange(1, () => pose);
await new Promise(resolve => setImmediate(resolve));
now += 100;
responses.push({context:'town', scene:1, peers:[]});
coop.exchange(1, () => pose);
await new Promise(resolve => setImmediate(resolve));
assert.equal(JSON.parse(requests.at(-1).body).visit, 'visit-two', 'A host restart renews presence');
responses.push({});
coop.leave();
assert.equal(JSON.parse(requests.at(-1).body).pose, null);
assert.equal(requests.at(-1).keepalive, true);
assert.equal(coop.exchange(1, () => pose).length, 0);
assert.equal(responses.length, 0);
console.log('Crew movement, room changes, expiry, reconnect, and leave passed.');
