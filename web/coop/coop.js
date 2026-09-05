'use strict';
const $ = id => document.getElementById(id);
const hex = count => Array.from(crypto.getRandomValues(new Uint8Array(count)), n => n.toString(16).padStart(2, '0')).join('');
let token = localStorage.getItem('cc-coop-token');
if (!/^[a-f0-9]{64}$/.test(token || '')) { token = hex(32); localStorage.setItem('cc-coop-token', token); }
let world = null, polling = false, busy = false, gameAvailable = false, inviteParts = null;
let appearance = CcAvatar.normalize(), dirty = false;
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
function updatePreview() {
  for (const key of CcAvatar.fields) {
    if ($(key)) $(key).value = String(appearance[key]);
    for (const swatch of document.querySelectorAll(`[data-option="${key}"]`)) swatch.setAttribute('aria-pressed', String(Number(swatch.dataset.value) === appearance[key]));
  }
  $('avatar-preview').contentWindow?.postMessage({type:'crownless-avatar-preview', appearance}, location.origin);
  $('appearance-status').textContent = dirty ? 'Your changes are ready' : 'Saved';
}
for (const key of CcAvatar.fields) {
  CcAvatar.options[key].forEach(([name, colour], value) => {
    if ($(key)) { const option = node('option', name); option.value = value; $(key).append(option); }
    else {
      const swatch = button('', () => { appearance[key] = value; dirty = true; updatePreview(); }, 'swatch');
      swatch.dataset.option = key; swatch.dataset.value = value; swatch.style.setProperty('--swatch', colour);
      swatch.setAttribute('aria-label', `${name} ${key === 'coat' ? 'coat' : key === 'hair' ? 'hair' : 'skin'}`); swatch.title = name;
      $(key + '-options').append(swatch);
    }
  });
  if ($(key)) $(key).onchange = () => { appearance[key] = Number($(key).value); dirty = true; updatePreview(); };
}
window.addEventListener('message', event => {
  if (event.origin === location.origin && event.source === $('avatar-preview').contentWindow && event.data?.type === 'crownless-avatar-ready') {
    $('preview-status').hidden = true; updatePreview();
  }
});
function accept(next) {
  if (world && (next.id !== world.id || next.revision < world.revision)) return;
  world = next; $('lobby').hidden = true; $('world').hidden = false;
  $('world-name').textContent = world.name;
  $('connection').textContent = world.recovery_required ? 'Host recovery needed' : world.paused ? 'World paused' : 'Company ready';
  $('invite').hidden = !world.owner;
  $('game-link').hidden = !gameAvailable; $('game-link').href = `/game/index.html?world=${world.id}`;
  $('player-name').textContent = world.crew.find(p => p.id === world.member)?.name || 'Your traveller';
  $('crew').replaceChildren(...world.crew.map(p => { const n = node('span'); n.append(node('i', null, p.online ? 'online' : ''), document.createTextNode(p.name + (p.id === world.member ? ' · you' : ''))); return n; }));
  if (!dirty && !busy) { appearance = CcAvatar.normalize(world.appearance); updatePreview(); }
}
async function openWorld(id) {
  try {
    world = null; dirty = false; accept(await api(`/api/worlds/${id}/state`));
    history.replaceState(null, '', `/#world=${id}`);
    $('avatar-preview').hidden = !gameAvailable;
    $('preview-status').hidden = false;
    $('preview-status').textContent = gameAvailable ? 'Preparing your traveller…' : 'The host is preparing the avatar preview.';
    if (gameAvailable) $('avatar-preview').src = '/game/index.html?avatar=1';
  } catch (error) { notice(error.message); }
}
async function lobby() {
  world = null; $('world').hidden = true; $('lobby').hidden = false; $('avatar-preview').removeAttribute('src');
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
    const created = await api('/api/worlds', pending); localStorage.removeItem('cc-coop-create'); await openWorld(created.id);
  } catch (error) { if (error.status && error.status < 500) localStorage.removeItem('cc-coop-create'); notice(error.message); }
  finally { submit.disabled = false; }
};
$('join-form').onsubmit = async event => {
  event.preventDefault(); if (!inviteParts) return;
  const submit = event.currentTarget.querySelector('button'); submit.disabled = true;
  try { await api(`/api/worlds/${inviteParts[0]}/join`, {player:new FormData(event.currentTarget).get('player').trim(), invite:inviteParts[1]}); await openWorld(inviteParts[0]); }
  catch (error) { notice(error.message); } finally { submit.disabled = false; }
};
async function saveAppearance() {
  if (!world || busy) return false;
  if (!dirty) return true;
  const id = world.id, chosen = {...appearance}; busy = true; $('save-appearance').disabled = true;
  try {
    const result = await api(`/api/worlds/${id}/appearance`, {appearance:chosen});
    if (world?.id !== id) return false;
    dirty = CcAvatar.pack(chosen) !== CcAvatar.pack(appearance); accept(result); updatePreview(); return !dirty;
  } catch (error) { notice(error.message); return false; }
  finally { busy = false; $('save-appearance').disabled = false; }
}
$('appearance-form').onsubmit = event => { event.preventDefault(); saveAppearance(); };
$('game-link').onclick = async event => { event.preventDefault(); if (await saveAppearance()) location.assign($('game-link').href); };
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
