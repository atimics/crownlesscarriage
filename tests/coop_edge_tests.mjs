import assert from 'node:assert/strict';
import edge from '../tools/coop/edge.mjs';

const origin = {ORIGIN:'https://crownless-ratimics.fly.dev'};
globalThis.fetch = async (request, options) => {
  assert.equal(request.url, 'https://crownless-ratimics.fly.dev/api/worlds/123/command?campaign=1');
  assert.equal(request.headers.get('Host'), 'crownless-ratimics.fly.dev');
  assert.equal(request.headers.get('Origin'), 'https://crownless.ratimics.com');
  assert.equal(request.headers.get('Authorization'), 'Bearer test-session');
  assert.equal(request.method, 'POST');
  assert.equal(await request.text(), '{"action":"trade"}');
  assert.equal(options.redirect, 'manual');
  return new Response('{"saved":true}', {headers:{'Cache-Control':'no-store'}});
};
const request = new Request('https://crownless.ratimics.com/api/worlds/123/command?campaign=1', {
  method:'POST', body:'{"action":"trade"}',
  headers:{Host:'crownless.ratimics.com', Origin:'https://crownless.ratimics.com', Authorization:'Bearer test-session'}
});
const response = await edge.fetch(request, origin);
assert.equal(await response.text(), '{"saved":true}');
assert.equal(response.headers.get('Cache-Control'), 'no-store');
console.log('Domain proxy preserves the shared command, session, and origin.');
