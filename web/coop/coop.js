'use strict';
const $ = id => document.getElementById(id);
const hex = count => Array.from(crypto.getRandomValues(new Uint8Array(count)), n => n.toString(16).padStart(2, '0')).join('');
let token = localStorage.getItem('cc-coop-token');
if (!/^[a-f0-9]{64}$/.test(token || '')) { token = hex(32); localStorage.setItem('cc-coop-token', token); }
let world = null, polling = false, gameAvailable = false, inviteParts = null;
const node = (tag, text, cls) => { const n = document.createElement(tag); if (text != null) n.textContent = text; if (cls) n.className = cls; return n; };
const button = (text, action, cls = '') => { const n = node('button', text, cls); n.type = 'button'; n.onclick = action; return n; };
function notice(text) { $('notice').hidden = false; $('notice').querySelector('span').textContent = text; }
$('dismiss').onclick = () => { $('notice').hidden = true; };
async function api(path, body) {
  const response = await fetch(path, {method:body === undefined ? 'GET':'POST', headers:{Authorization:`Bearer ${token}`, 'Content-Type':'application/json'}, body:body === undefined ? undefined:JSON.stringify(body), signal:AbortSignal.timeout(12000)});
  const data = await response.json();
  if (!response.ok) { const error = new Error(data.error || 'The host is catching up.'); error.status = response.status; throw error; }
  return data;
}
function accept(next) {
  if (world && (next.id !== world.id || next.revision < world.revision)) return;
  world = next; $('lobby').hidden = true; $('world').hidden = false;
  $('world-name').textContent = world.name;
  $('connection').textContent = world.recovery_required ? 'Host recovery needed' : world.paused ? 'World paused' : 'Company ready';
  $('invite').hidden = !world.owner;
  $('game-link').hidden = !gameAvailable; $('game-link').href = `/game/index.html?world=${world.id}`;
  $('crew').replaceChildren(...world.crew.map(p => { const n = node('span'); n.append(node('i', null, p.online ? 'online' : ''), document.createTextNode(p.name + (p.id === world.member ? ' · you' : ''))); return n; }));
}
async function openWorld(id) {
  try {
    world = null; accept(await api(`/api/worlds/${id}/state`));
    history.replaceState(null, '', `/#world=${id}`);
  } catch (error) { notice(error.message); }
}
async function lobby() {
  world = null; $('world').hidden = true; $('lobby').hidden = false;
  try {
    const data = await api('/api/worlds'); gameAvailable = data.game_available;
    $('world-list').replaceChildren(...data.worlds.map(w => button(`${w.name}  ↗`, () => openWorld(w.id), 'world-entry')));
    if (!data.worlds.length) $('world-list').append(node('p', 'Your saved journeys will appear here.', 'quiet'));
    $('connection').textContent = 'Host ready';
  } catch (error) { $('connection').textContent = 'Connecting to the host'; notice(error.message); }
}
$('create-form').onsubmit = async event => {
  event.preventDefault(); const form = event.currentTarget; const submit = form.querySelector('button'); submit.disabled = true;
  try {
    let pending = JSON.parse(localStorage.getItem('cc-coop-create') || 'null');
    if (!pending) { const fields = new FormData(form); pending = {id:hex(16), player:fields.get('player').trim(), name:fields.get('name').trim()}; localStorage.setItem('cc-coop-create', JSON.stringify(pending)); }
    const created = await api('/api/worlds', {...pending, world_pass:new FormData(form).get('world_pass').trim()});
    localStorage.removeItem('cc-coop-create'); form.elements.world_pass.value = ''; $('notice').hidden = true; await openWorld(created.id);
  } catch (error) { if (error.status && error.status < 500) localStorage.removeItem('cc-coop-create'); notice(error.message); }
  finally { submit.disabled = false; }
};
$('join-form').onsubmit = async event => {
  event.preventDefault(); if (!inviteParts) return;
  const submit = event.currentTarget.querySelector('button'); submit.disabled = true;
  try { await api(`/api/worlds/${inviteParts[0]}/join`, {player:new FormData(event.currentTarget).get('player').trim(), invite:inviteParts[1]}); await openWorld(inviteParts[0]); }
  catch (error) { notice(error.message); } finally { submit.disabled = false; }
};
$('leave').onclick = () => { history.replaceState(null, '', '/'); lobby(); };
$('invite').onclick = async () => {
  try { const data = await api(`/api/worlds/${world.id}/invite`); $('invite-url').value = `${location.origin}/#join=${world.id}.${data.invite}`; $('invite-dialog').showModal(); }
  catch (error) { notice(error.message); }
};
$('copy-invite').onclick = async () => { try { await navigator.clipboard.writeText($('invite-url').value); $('copy-invite').textContent = 'Copied'; } catch { $('invite-url').select(); } };
$('close-invite').onclick = () => { $('invite-dialog').close(); $('copy-invite').textContent = 'Copy invitation'; };
setInterval(async () => {
  if (!world || polling) return;
  const id = world.id; polling = true;
  try { const next = await api(`/api/worlds/${id}/state`); if (world?.id === id) accept(next); }
  catch { $('connection').textContent = 'Reconnecting…'; } finally { polling = false; }
}, 1500);
(async () => {
  const fragment = new URLSearchParams(location.hash.slice(1));
  const join = fragment.get('join');
  if (/^[a-f0-9]{32}\.[a-f0-9]{64}$/.test(join || '')) { inviteParts = join.split('.'); $('join-form').hidden = false; $('create-form').hidden = true; }
  await lobby();
  const id = fragment.get('world'); if (/^[a-f0-9]{32}$/.test(id || '')) await openWorld(id);
})();
