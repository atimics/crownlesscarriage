import assert from 'node:assert/strict';
import edge from '../tools/coop/edge.mjs';

const origin = {ORIGIN:'https://crownless-ratimics.fly.dev'};
for (const domain of ['crownless.ca', 'crownless.ratimics.com']) {
  globalThis.fetch = async (request, options) => {
    assert.equal(request.url, 'https://crownless-ratimics.fly.dev/api/worlds/123/command?campaign=1');
    assert.equal(request.headers.get('Host'), 'crownless-ratimics.fly.dev');
    assert.equal(request.headers.get('Origin'), `https://${domain}`);
    assert.equal(request.headers.get('Authorization'), 'Bearer test-session');
    assert.equal(request.method, 'POST');
    assert.equal(await request.text(), '{"action":"trade"}');
    assert.equal(options.redirect, 'manual');
    return new Response('{"saved":true}', {headers:{'Cache-Control':'no-store'}});
  };
  const request = new Request(`https://${domain}/api/worlds/123/command?campaign=1`, {
    method:'POST', body:'{"action":"trade"}',
    headers:{Host:domain, Origin:`https://${domain}`, Authorization:'Bearer test-session'}
  });
  const response = await edge.fetch(request, origin);
  assert.equal(await response.text(), '{"saved":true}');
  assert.equal(response.headers.get('Cache-Control'), 'no-store');
}
console.log('Both public domains preserve the shared command, session, and origin.');
