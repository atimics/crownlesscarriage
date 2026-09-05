import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const manifest = JSON.parse(readFileSync(new URL('../assets/audio/music/offline.json', import.meta.url)));
const saved = new Map();
let online = true, failCache = false, requests = 0;
const context = { Module: {}, Uint8Array, Response, TextDecoder, AbortController, AbortSignal, setTimeout, clearTimeout,
  navigator: { onLine: true },
  caches: { async open() {
    if (failCache) throw new Error('Storage full');
    return { async match(url) { return saved.get(url)?.clone(); },
      async put(url, response) { saved.set(url, response); } };
  } },
  async fetch(url, { signal } = {}) {
    requests++;
    if (!online || signal?.aborted) throw new Error('Offline');
    if (url.endsWith('offline.json')) return Response.json(manifest);
    if (url.endsWith('catalog.txt')) return new Response('CROWNLESS_MUSIC 1\n');
    if (url.endsWith('missing.mp3')) return new Response('Missing', { status: 404 });
    return new Response(new Uint8Array(2048), { headers: { 'content-length': '2048' } });
  }
};
vm.runInNewContext(readFileSync(new URL('../web/music.js', import.meta.url), 'utf8'), context);
const net = context.Module.ccMusicNet;
async function finish() {
  for (let i = 0; i < 100 && !net.request?.status; i++) await new Promise(r => setTimeout(r, 2));
  assert.ok(net.request?.status);
  return net.poll();
}
assert.ok(net.start('https://crownless-music.pages.dev/catalog.txt', 16384));
assert.equal(net.start('another', 1234), false);
assert.equal((await finish()).status, 1);
for (let i = 0; i < 2500 && saved.size < 27; i++) await new Promise(r => setTimeout(r, 5));
assert.equal(saved.size, 27, 'the 27 bundled web tracks finish caching');
online = false; context.navigator.onLine = false;
const before = requests;
for (const track of manifest.tracks) {
  assert.ok(net.start(`assets/audio/music/${track.stem}.mp3`, 16 * 1024 * 1024));
  const result = await finish();
  assert.equal(result.status, 1); assert.equal(result.data.length, 2048);
}
assert.equal(requests, before, 'all 27 play from storage with the network down');
assert.ok(net.start('https://crownless-music.pages.dev/catalog.txt', 16384));
assert.equal((await finish()).status, -1);
online = true; context.navigator.onLine = true;
assert.ok(net.start('https://crownless-music.pages.dev/catalog.txt', 16384));
assert.equal((await finish()).status, 1, 'catalog refresh works after reconnect');
assert.ok(net.start('missing.mp3', 4000)); assert.equal((await finish()).status, -1);
assert.ok(net.start('too-big.mp3', 1024)); assert.equal((await finish()).status, -1);
failCache = true;
assert.ok(net.start('no-storage.mp3', 4096)); assert.equal((await finish()).status, 1);
net.start('cancelled.mp3', 4096); net.stop(); assert.equal(net.request, null);
console.log('Web music: bounded loading, 27 offline tracks, reconnect and storage failure passed.');

async function until(test, message) {
  for (let i = 0; i < 1000 && !test(); i++) await new Promise(resolve => setTimeout(resolve, 2));
  assert.ok(test(), message);
}
function controlledNetwork({ slowStorage = false } = {}) {
  const pending = new Map(), calls = [], disk = new Map();
  const puts = [];
  const sandbox = { Module: {}, Uint8Array, Response, TextDecoder, AbortController, AbortSignal,
    setTimeout, clearTimeout, navigator: { onLine: true },
    caches: { async open() { return {
      async match(url) { return disk.get(url)?.clone(); },
      async put(url, response) {
        if (slowStorage) await new Promise(resolve => puts.push(resolve));
        disk.set(url, response);
      }
    }; } },
    fetch(url, { signal } = {}) {
      calls.push(url);
      if (url.endsWith('catalog.txt')) return Promise.resolve(new Response('CROWNLESS_MUSIC 1\n'));
      if (url.endsWith('offline.json')) return Promise.resolve(Response.json(manifest));
      return new Promise((resolve, reject) => {
        const request = { finish() { pending.delete(url); resolve(new Response(new Uint8Array(2048))); } };
        pending.set(url, request);
        signal.addEventListener('abort', () => { pending.delete(url); reject(new Error('Cancelled')); }, { once: true });
        if (signal.aborted) { pending.delete(url); reject(new Error('Cancelled')); }
      });
    }
  };
  vm.runInNewContext(readFileSync(new URL('../web/music.js', import.meta.url), 'utf8'), sandbox);
  return { net: sandbox.Module.ccMusicNet, pending, calls, puts, disk };
}
const firstWarm = `assets/audio/music/${manifest.tracks[0].stem}.mp3`;
for (const sameFile of [false, true]) {
  const test = controlledNetwork();
  test.net.start('catalog.txt', 16384);
  await until(() => test.net.request.status, 'catalog completes');
  test.net.poll();
  await until(() => test.pending.has(firstWarm), 'background download begins');
  const wanted = sameFile ? firstWarm : 'urgent.mp3';
  assert.ok(test.net.start(wanted, 4096));
  await until(() => test.pending.has(wanted), 'selected song takes priority');
  assert.equal(test.pending.size, 1, 'one audio transfer owns the connection');
  test.pending.get(wanted).finish();
  await until(() => test.net.request.status, 'selected song completes');
  assert.equal(test.net.poll().status, 1);
  if (sameFile) assert.equal(test.calls.filter(url => url === wanted).length, 1,
    'a selected song reuses its existing background transfer');
  test.net.stop();
}
const storageTest = controlledNetwork({ slowStorage: true });
storageTest.net.start('selected.mp3', 4096);
await until(() => storageTest.pending.has('selected.mp3'), 'audio fetch begins');
storageTest.pending.get('selected.mp3').finish();
await until(() => storageTest.net.request.status, 'playback bytes arrive while storage is busy');
assert.equal(storageTest.puts.length, 1);
assert.equal(storageTest.disk.size, 0);
assert.equal(storageTest.net.poll().status, 1);
storageTest.net.stop();
storageTest.puts[0]();

const cancelTest = controlledNetwork();
cancelTest.net.start('old.mp3', 4096);
await until(() => cancelTest.pending.has('old.mp3'), 'old download begins');
cancelTest.net.cancel();
assert.equal(cancelTest.net.poll().status, -1);
cancelTest.net.start('new.mp3', 4096);
await until(() => cancelTest.pending.has('new.mp3'), 'replacement begins after cancellation');
cancelTest.pending.get('new.mp3').finish();
await until(() => cancelTest.net.request.status, 'replacement completes');
assert.equal(cancelTest.net.poll().status, 1, 'late cancellation belongs to the old request');
cancelTest.net.stop();
console.log('Web music priority: promotion, preemption, slow storage and replacement requests passed.');
