const { test } = require('node:test');
const assert = require('node:assert/strict');
const { createCrownlessVoiceClient } = require('../web/voice.js');
const record = { key: '0123456789abcdef', text: 'Eight boxes.' };
function wav() { const b = new Uint8Array(44); b.set(Buffer.from('RIFF')); b.set(Buffer.from('WAVE'), 8); return b; }
async function complete(client) {
  for (let i = 0; i < 200; i++) {
    const result = client.take();
    if (result) return result;
    await new Promise(resolve => setTimeout(resolve, 5));
  }
  throw new Error('Voice timed out');
}
test('packed speech is reused from memory and survives unavailable storage', async () => {
  let calls = 0;
  const client = createCrownlessVoiceClient({ fetch: async () => { calls++; return new Response(wav()); },
    storage: { open() { throw new Error('Full'); } } });
  assert.equal(client.start(record), true);
  assert.equal(client.start(record), false);
  assert.equal((await complete(client)).status, 1);
  client.start(record);
  assert.equal((await complete(client)).status, 1);
  assert.equal(calls, 1);
});
test('a missing pack submits exact text and waits for generated WAV', async () => {
  const calls = [];
  const client = createCrownlessVoiceClient({ endpoint: 'http://127.0.0.1:8766/', storage: null,
    fetch: async (url, options) => {
      calls.push([url, options]);
      if (calls.length === 1) return new Response('', { status: 404 });
      if (calls.length < 4) return new Response('', { status: 202 });
      return new Response(wav());
    } });
  client.start(record);
  assert.equal((await complete(client)).status, 1);
  assert.deepEqual(JSON.parse(calls[1][1].body), record);
  assert.equal(calls[3][0], 'http://127.0.0.1:8766/v1/speech/' + record.key);
});
test('cancellation discards late completion and permits the next turn', async () => {
  let finish;
  const client = createCrownlessVoiceClient({ storage: null, fetch: () => new Promise(resolve => { finish = resolve; }) });
  client.start(record);
  await new Promise(resolve => setTimeout(resolve, 0));
  client.cancel();
  const oldFinish = finish;
  client.start({ ...record, key: '1123456789abcdef' });
  await new Promise(resolve => setTimeout(resolve, 0));
  oldFinish(new Response(wav()));
  await new Promise(resolve => setTimeout(resolve, 0));
  assert.equal(client.take(), null);
  finish(new Response(wav()));
  assert.equal((await complete(client)).status, 1);
});
test('invalid and oversized audio fail within the download budget', async () => {
  for (const body of [new Uint8Array(44), new Uint8Array(1200101)]) {
    const client = createCrownlessVoiceClient({ storage: null, fetch: async () => new Response(body) });
    client.start(record);
    assert.equal((await complete(client)).status, -1);
  }
});
test('a worker failure ends this line and allows replay', async () => {
  const client = createCrownlessVoiceClient({ storage: null, endpoint: 'http://localhost',
    fetch: async url => new Response('', { status: url.startsWith('speech/') ? 404 : 503 }) });
  client.start(record);
  assert.equal((await complete(client)).status, -1);
  assert.equal(client.start(record), true);
  assert.equal((await complete(client)).status, -1);
});
test('persistent cache evicts old clips and supplies a fresh client', async () => {
  const entries = new Map();
  const cache = {
    async match(url) { return entries.get(url)?.clone(); },
    async put(url, response) { entries.set(url, response); },
    async delete(url) { return entries.delete(url); },
    async keys() { return [...entries.keys()]; },
  };
  const storage = { async open() { return cache; } };
  const client = createCrownlessVoiceClient({ storage, fetch: async () => new Response(wav()) });
  for (let i = 0; i < 66; i++) {
    client.start({ ...record, key: i.toString(16).padStart(16, '0') });
    assert.equal((await complete(client)).status, 1);
  }
  assert.equal(entries.size, 64);
  const fresh = createCrownlessVoiceClient({ storage, fetch: async () => { throw new Error('Offline'); } });
  fresh.start({ ...record, key: (65).toString(16).padStart(16, '0') });
  assert.equal((await complete(fresh)).status, 1);
});
