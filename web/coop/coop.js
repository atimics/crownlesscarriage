'use strict';
const $ = id => document.getElementById(id);
const hex = count => Array.from(crypto.getRandomValues(new Uint8Array(count)), n => n.toString(16).padStart(2, '0')).join('');
let token = localStorage.getItem('cc-coop-token');
if (!/^[a-f0-9]{64}$/.test(token || '')) { token = hex(32); localStorage.setItem('cc-coop-token', token); }
let world = null, currentTab = 'market', good = 0, quantity = 1, polling = false, busy = false, gameAvailable = false;
let panelSignature = '', inviteParts = null;
const node = (tag, text, cls) => { const n = document.createElement(tag); if (text != null) n.textContent = text; if (cls) n.className = cls; return n; };
const button = (text, action, cls = '') => { const n = node('button', text, cls); n.type = 'button'; n.onclick = action; return n; };
function notice(text, retry = false) { $('notice').hidden = false; $('notice').querySelector('span').textContent = text; $('retry').hidden = !retry; }
$('dismiss').onclick = () => { $('notice').hidden = true; };
async function api(path, body) {
  const response = await fetch(path, { method: body === undefined ? 'GET' : 'POST', headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' }, body: body === undefined ? undefined : JSON.stringify(body), signal: AbortSignal.timeout(12000) });
  const data = await response.json();
  if (!response.ok) { const error = new Error(data.error || 'The host is catching up.'); error.status = response.status; throw error; }
  return data;
}
function accept(next) {
  if (world && next.id === world.id && next.revision < world.revision) return;
  world = next; $('lobby').hidden = true; $('world').hidden = false; render();
}
async function openWorld(id) {
  try { panelSignature = ''; accept(await api(`/api/worlds/${id}/state`)); history.replaceState(null, '', `/#world=${id}`); await retryPending(); }
  catch (error) { notice(error.message); }
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
    if (!pending) { const fields = new FormData(form); pending = { id: hex(16), player: fields.get('player').trim(), name: fields.get('name').trim() }; localStorage.setItem('cc-coop-create', JSON.stringify(pending)); }
    const created = await api('/api/worlds', pending); localStorage.removeItem('cc-coop-create'); await openWorld(created.id);
  } catch (error) { if (error.status && error.status < 500) localStorage.removeItem('cc-coop-create'); notice(error.message); }
  finally { submit.disabled = false; }
};
$('join-form').onsubmit = async event => {
  event.preventDefault(); if (!inviteParts) return;
  const submit = event.currentTarget.querySelector('button'); submit.disabled = true;
  try { await api(`/api/worlds/${inviteParts[0]}/join`, { player: new FormData(event.currentTarget).get('player').trim(), invite: inviteParts[1] }); await openWorld(inviteParts[0]); }
  catch (error) { notice(error.message); } finally { submit.disabled = false; }
};
$('leave').onclick = () => { history.replaceState(null, '', '/'); lobby(); };
async function withCommandLock(fn) { if (navigator.locks) return navigator.locks.request(`cc-coop-command-${world.id}`, fn); return fn(); }
async function retryPending() {
  if (!world || busy) return;
  const id = world.id, key = `cc-coop-pending-${id}`;
  const pending = JSON.parse(localStorage.getItem(key) || 'null'); if (!pending) return;
  busy = true;
  try {
    const result = await api(`/api/worlds/${id}/command`, pending);
    localStorage.removeItem(key); if (world?.id === id) accept(result.world);
    notice(result.message);
  } catch (error) {
    if (error.status && error.status < 500) { localStorage.removeItem(key); if (world?.id === id) accept(await api(`/api/worlds/${id}/state`)); }
    notice(error.message, localStorage.getItem(key) !== null);
  } finally { busy = false; }
}
async function command(action, values = {}) {
  if (!world || busy) return;
  const id = world.id;
  await withCommandLock(async () => {
    if (world?.id !== id) return;
    const key = `cc-coop-pending-${id}`;
    if (localStorage.getItem(key)) { await retryPending(); return; }
    localStorage.setItem(key, JSON.stringify({ protocol: 1, sequence: world.next_sequence, action_revision: world.action_revision, action, ...values }));
    await retryPending();
  });
}
$('retry').onclick = () => withCommandLock(retryPending);
$('pause').onclick = async () => { try { accept(await api(`/api/worlds/${world.id}/host`, { action: world.paused ? 'resume' : 'pause' })); } catch (e) { notice(e.message); } };
$('invite').onclick = async () => {
  try { const data = await api(`/api/worlds/${world.id}/invite`); $('invite-url').value = `${location.origin}/#join=${world.id}.${data.invite}`; $('invite-dialog').showModal(); }
  catch (error) { notice(error.message); }
};
$('copy-invite').onclick = async () => { try { await navigator.clipboard.writeText($('invite-url').value); $('copy-invite').textContent = 'Copied'; } catch { $('invite-url').select(); } };
$('close-invite').onclick = () => { $('invite-dialog').close(); $('copy-invite').textContent = 'Copy invitation'; };
for (const tab of document.querySelectorAll('[data-tab]')) tab.onclick = () => { currentTab = tab.dataset.tab; panelSignature = ''; render(); };
function render() {
  const s = world.state, c = s.company, j = s.journey;
  $('world-name').textContent = world.name; $('connection').textContent = world.recovery_required ? 'Host recovery needed' : world.paused ? 'World paused' : 'Together · saved';
  $('invite').hidden = !world.owner; $('pause').hidden = !world.owner; $('pause').textContent = world.paused ? 'Resume world' : 'Pause world';
  $('game-link').hidden = !gameAvailable; $('game-link').href = `/game/index.html?world=${world.id}`;
  $('coins').textContent = c.coins; $('cargo').textContent = `${c.cargo_used} / ${c.capacity}`;
  $('day').textContent = `Day ${s.day} · ${String(Math.floor(s.minute / 60)).padStart(2, '0')}:${String(s.minute % 60).padStart(2, '0')}`;
  const names = new Map(s.settlements.map(t => [t.id, t.name]));
  $('location').textContent = j.active ? `${names.get(j.origin) || 'The road'} → ${names.get(j.destination) || 'Next town'}` : names.get(c.location) || 'At the carriage';
  $('crew').replaceChildren(...world.crew.map(p => { const n = node('span'); n.append(node('i', null, p.online ? 'online' : ''), document.createTextNode(p.name + (p.id === world.member ? ' · you' : ''))); return n; }));
  $('journey-label').textContent = j.active ? `Watch ${j.watch} / ${j.watches}` : 'The carriage is at rest';
  renderMap(s); renderJourney(j);
  for (const tab of document.querySelectorAll('[data-tab]')) tab.setAttribute('aria-selected', String(tab.dataset.tab === currentTab));
  const signature = JSON.stringify([currentTab, world.paused, j.phase, s.market, c.cargo, c.coins, s.charters, currentTab === 'history' ? s.events : null, s.travel]);
  if (signature !== panelSignature && !['INPUT', 'SELECT'].includes(document.activeElement?.tagName)) { panelSignature = signature; renderPanel(s); }
}
function svg(tag, attrs) { const n = document.createElementNS('http://www.w3.org/2000/svg', tag); for (const [k, v] of Object.entries(attrs)) n.setAttribute(k, v); return n; }
function renderMap(s) {
  const items = [], towns = new Map(s.settlements.map(t => [t.id, t]));
  for (const r of s.roads) {
    const a = towns.get(r.from), b = towns.get(r.to); if (!a || !b) continue;
    items.push(svg('path', { d: `M ${a.x} ${a.y} Q ${(a.x+b.x)/2-22} ${(a.y+b.y)/2+25} ${b.x} ${b.y}`, class: r.closed ? 'closed' : '' }));
  }
  for (const t of towns.values()) { items.push(svg('circle', { cx: t.x, cy: t.y, r: 8, class: 'town' })); const name = svg('text', { x: t.x, y: t.y+29, 'text-anchor': 'middle' }); name.textContent = t.name; items.push(name); }
  let place = towns.get(s.company.location);
  if (s.journey.active) { const a = towns.get(s.journey.origin), b = towns.get(s.journey.destination); if (a && b) { const t = s.journey.progress/1000; place = {x: (1-t)*(1-t)*a.x+2*(1-t)*t*((a.x+b.x)/2-22)+t*t*b.x, y:(1-t)*(1-t)*a.y+2*(1-t)*t*((a.y+b.y)/2+25)+t*t*b.y}; } }
  if (place) { const carriage = svg('g', { transform:`translate(${place.x},${place.y-14})` }); carriage.append(svg('rect',{x:-12,y:-16,width:24,height:18,rx:3,class:'carriage'}),svg('circle',{cx:-8,cy:5,r:4,class:'carriage'}),svg('circle',{cx:8,cy:5,r:4,class:'carriage'})); items.push(carriage); }
  $('map').replaceChildren(...items);
}
function renderJourney(j) {
  const container = $('journey-controls'); container.replaceChildren(); if (!j.active) return;
  let text, choices;
  if (j.phase === 2) { text = 'The carriage has stopped at a road encounter. Choose together.'; choices = [['Pay passage', 'negotiate'], ['Offer provisions', 'provisions'], ['Withdraw', 'withdraw']]; }
  else if (j.phase === 3) { text = j.stop === 2 ? 'The day’s watch is over. Find a place for the company to rest.' : 'The horses need a midday stop.'; choices = j.stop === 2 ? [['Make camp', 'camp'], ['Press on', 'press_on']] : [['Take a break', 'break'], ['Press on', 'press_on']]; if (j.lodge) choices.unshift(['Lodge at the road house','lodge']); }
  else { text = 'The whole company travels on the same clock.'; choices = [['Careful','pace',{amount:0}], ['Steady','pace',{amount:1}], ['Push','pace',{amount:2}]]; }
  container.append(node('p', text, 'journey-note')); const actions = node('div', null, 'journey-actions');
  for (const [label, action, values] of choices) { const b = button(label, () => command(action, values)); b.disabled = world.paused; actions.append(b); } container.append(actions);
}
function renderPanel(s) {
  const panel = $('panel'); panel.replaceChildren();
  if (currentTab === 'market') {
    panel.append(node('h2', s.market ? `${s.market.name} market` : 'Cargo on the road'));
    if (!s.market) { panel.append(node('p','The company can trade when the carriage reaches a town.')); for (let i=0;i<s.goods.length;i++) if(s.company.cargo[i]) panel.append(node('p',`${s.goods[i]} · ${s.company.cargo[i]}`)); return; }
    const fields=node('div',null,'market-fields'), goodLabel=node('label','Good'), select=node('select'), countLabel=node('label','Quantity'), count=node('input');
    select.setAttribute('aria-label','Good'); s.goods.forEach((g,i)=>{const o=node('option',g);o.value=i;select.append(o);}); select.value=good; select.onchange=()=>{good=Number(select.value);price();};
    count.type='number';count.min=1;count.max=1000;count.value=quantity;count.setAttribute('aria-label','Quantity');count.oninput=()=>{quantity=Number(count.value);price();};
    goodLabel.append(select);countLabel.append(count);fields.append(goodLabel,countLabel);panel.append(fields);const info=node('div',null,'price-line');panel.append(info);
    function price(){info.textContent=`${s.market.stock[good]} at market · ${s.company.cargo[good]} aboard. Buy ${s.market.prices[good]} crowns each; sell ${Math.max(1,Math.floor(s.market.prices[good]*3/4))}.`;}
    price();const actions=node('div',null,'tools');for(const [label,sign]of[['Buy for company',1],['Sell cargo',-1]]){const b=button(label,()=>{if(Number.isInteger(quantity)&&quantity>=1&&quantity<=1000)command('trade',{good,amount:quantity*sign});else notice('Choose a whole quantity from 1 to 1000.');});b.disabled=world.paused;actions.append(b);}panel.append(actions,node('p','Purchases and deliveries use the company’s common purse and cargo.'));
  } else if(currentTab==='road') {
    panel.append(node('h2','Choose the next road'));
    for(const trip of s.travel){const row=node('div',null,'row'),desc=node('div',trip.name);desc.append(node('small',trip.available?`${trip.watches} watches · ${trip.fare} crowns`:trip.reason||'The carriage is on the road.'));row.append(desc);const b=button(trip.closed?'Repair · 18 crowns':'Depart',()=>command(trip.closed?'repair':'travel',{target:trip.closed?trip.route:trip.id,amount:trip.closed?2:0}));b.disabled=world.paused||s.journey.active||(!trip.closed&&!trip.available);row.append(b);panel.append(row);}if(!s.travel.length)panel.append(node('p','Explore the company’s charts for the next route.'));
  } else if(currentTab==='charters') {
    panel.append(node('h2','Promises we carry'));
    for(const charter of s.charters){const row=node('div',null,'row'),desc=node('div',charter.kind);desc.append(node('small',`${charter.sponsor} · ${charter.quantity} ${s.goods[charter.good]} · ${charter.reward} crowns`));row.append(desc);const b=button(charter.accepted?'Abandon':'Accept',()=>command(charter.accepted?'abandon':'accept',{target:charter.id}));b.disabled=world.paused||s.journey.active;row.append(b);panel.append(row);}if(!s.charters.length)panel.append(node('p','Speak with people at the next stop to learn what the company can do.'));
  } else {
    panel.append(node('h2','A shared history'));const list=node('ol',null,'timeline');for(const event of s.events){const li=node('li');li.append(node('small',`DAY ${event.day}`),document.createTextNode(event.text));list.append(li);}panel.append(list);
  }
}
setInterval(async()=>{if(!world||polling||busy)return;polling=true;const id=world.id;try{const next=await api(`/api/worlds/${id}/state`);if(world?.id===id)accept(next);}catch(error){$('connection').textContent='Reconnecting · progress saved';if(error.status===403){notice(error.message);await lobby();}}finally{polling=false;}},1000);
(async()=>{const hash=location.hash;await lobby();const join=/^#join=([a-f0-9]{32})\.([a-f0-9]{64})$/.exec(hash);if(join){inviteParts=join.slice(1);$('join-form').hidden=false;$('create-form').hidden=true;}else{const existing=/^#world=([a-f0-9]{32})$/.exec(hash);if(existing)await openWorld(existing[1]);}})();
