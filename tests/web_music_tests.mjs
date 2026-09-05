import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const manifest = JSON.parse(readFileSync(new URL('../assets/audio/music/offline.json', import.meta.url)));
const saved = new Map();
let online = true, failCache = false, requests = 0;
const context = { Module: {}, Uint8Array, Response, AbortController, AbortSignal, setTimeout, clearTimeout,
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
  const request = net.request; net.request = null; return request;
}
assert.ok(net.start('https://crownless-music.pages.dev/catalog.txt', 16384));
assert.equal(net.start('another', 1234), false);
assert.equal((await finish()).status, 1);
for (let i = 0; i < 100 && saved.size < 27; i++) await new Promise(r => setTimeout(r, 5));
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
