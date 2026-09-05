import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
const name = 'crownless-host-check', volume = 'crownless-host-check-data';
const docker = (...args) => execFileSync('docker', args, {encoding:'utf8', stdio:['ignore','pipe','inherit']});
const origin = 'http://127.0.0.1:18788';
const token = 'a'.repeat(64), world = '1'.repeat(32);
async function ready() {
  for (let i=0; i<100; ++i) {
    try { const response = await fetch(origin + '/healthz'); if (response.ok) return response.json(); } catch {}
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  throw new Error('The container host must become ready.');
}
async function api(path, body) {
  const response = await fetch(origin + path, {method:body ? 'POST':'GET',
    headers:{Authorization:`Bearer ${token}`, 'Content-Type':'application/json'}, body:body ? JSON.stringify(body):undefined});
  assert.equal(response.status, 200, await response.clone().text());
  return response.json();
}
try {
  docker('run','--detach','--name',name,'--publish','127.0.0.1:18788:8787','--mount',`type=volume,source=${volume},target=/data`,process.argv[2]);
  const health = await ready();
  assert.equal(health.revision, process.env.GITHUB_SHA);
  assert.equal((await fetch(origin + '/game/index.wasm', {method:'HEAD'})).status, 200);
  const worldPass = docker('exec','--user','10001',name,'python','tools/coop/server.py',
    '--database','/data/worlds/worlds.sqlite3','--issue-world-pass').trim();
  let state = await api('/api/worlds', {id:world, name:'Saved carriage', player:'Mara', world_pass:worldPass});
  const command = {protocol:1, sequence:state.next_sequence, action_revision:state.action_revision, action:'trade', good:0, amount:1};
  const saved = await api(`/api/worlds/${world}/command`, command);
  assert.equal(saved.accepted, true);
  docker('stop',name);
  docker('start',name);
  await ready();
  state = await api(`/api/worlds/${world}/state`);
  assert.equal(state.state.hash, saved.world.state.hash);
  const retry = await api(`/api/worlds/${world}/command`, command);
  assert.equal(retry.duplicate, true);
  assert.equal(retry.world.state.company.cargo[0], 1);
  assert.equal(docker('exec',name,'python','-c',"import os; print(os.stat('/data/worlds/worlds.sqlite3').st_uid)").trim(), '10001');
  console.log('Release container serves the game and preserves company state and receipts across restart.');
} finally {
  docker('rm','--force',name);
  docker('volume','rm',volume);
}
