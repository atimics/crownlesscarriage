(function () {
  const worldId = typeof location !== 'undefined' ? new URLSearchParams(location.search).get('world') : null;
  const enabled = /^[a-f0-9]{32}$/.test(worldId || '');
  const preview = typeof location !== 'undefined' && new URLSearchParams(location.search).get('avatar') === '1';
  let avatar = CcAvatar.normalize();
  let state = null, pending = null, lastPoll = 0, inFlight = false, applying = false;
  const token = enabled ? localStorage.getItem('cc-coop-token') : '';
  const key = `cc-coop-pending-${worldId}`;
  let deleted = false;
  const sessionKey = `cc-coop-session-${worldId}`;
  let checkpoint = null, sessionSequence = 0, sessionSaved = 0, sessionFlight = false, lastSessionSave = 0, restoredSession = false;
  let status, pauseControl, startupError = '';
  let visit = '', poseFlight = false, poseScene = -1, lastPose = -Infinity, peersAt = 0, peers = [], leaving = false;
  function say(message) { if (status) status.textContent = startupError || message; }
  function accept(next, forceSnapshot = false) {
    if (deleted || (state && next.revision < state.revision)) return;
    if (!state || next.revision !== state.revision || forceSnapshot) pending = next.campaign;
    if (state && state.session_context !== next.session_context) peers = [];
    state = next;
    if (checkpoint && checkpoint.context !== next.session_context) checkpoint = null;
    avatar = CcAvatar.normalize(next.appearance);
    if (pauseControl) {
      pauseControl.hidden = !next.owner;
      pauseControl.textContent = next.paused ? 'Resume world' : 'Pause world';
    }
    say(next.paused ? 'Company paused' : `${next.crew.filter(p => p.online).length} aboard · shared carriage · saved`);
  }
  async function request(path, body) {
    const response = await fetch(`/api/worlds/${worldId}/${path}`, { method: body ? 'POST' : 'GET', headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' }, body: body ? JSON.stringify(body) : undefined, signal: AbortSignal.timeout(10000) });
    const result = await response.json();
    if (!response.ok) { const error = new Error(result.error || 'Reconnect to the carriage.'); error.status = response.status; throw error; }
    return result;
  }
  async function connect() {
    if (!/^[a-f0-9]{64}$/.test(token || '')) throw new Error('Join the company from the shared road book.');
    let next = await request('state?campaign=1&enter=1');
    while (next.catching_up) {
      say(`The world is catching up · ${next.away_clock.days_pending} days to settle…`);
      await new Promise(resolve => setTimeout(resolve, 500));
      next = await request('state?campaign=1&enter=1');
    }
    accept(next, true);
    visit = next.visit || '';
    if (next.session !== undefined) {
      let cached = null;
      try { cached = JSON.parse(localStorage.getItem(sessionKey) || 'null'); } catch {}
      const saved = next.session;
      sessionSequence = Math.max(saved?.sequence || 0, Number.isSafeInteger(cached?.sequence) ? cached.sequence : 0);
      sessionSaved = saved?.sequence || 0;
      const candidates = [saved, cached].filter(value => value && value.context === next.session_context && Number.isSafeInteger(value.sequence) && typeof value.session === 'string');
      candidates.sort((a, b) => b.sequence - a.sequence);
      checkpoint = candidates[0] || null;
      if (checkpoint) {
        FS.writeFile('/tmp/crownless-coop.ccsave.session', checkpoint.session);
        restoredSession = true;
      }
    }
  }
  function captureSession(session) {
    if (deleted || !enabled || !state?.session_context || checkpoint?.session === session) return;
    checkpoint = {context:state.session_context, sequence:++sessionSequence, session};
    localStorage.setItem(sessionKey, JSON.stringify(checkpoint));
    const place = session.split('\n')[1].split(' ');
    document.body.dataset.playerPosition = `${place[5]},${place[6]}`;
    document.body.dataset.playerScene = place[2];
  }
  async function saveSession(leaving = false) {
    if (deleted || !checkpoint || checkpoint.sequence <= sessionSaved || (sessionFlight && !leaving)) return;
    if (!leaving && performance.now() - lastSessionSave < 1000) return;
    const saved = checkpoint;
    lastSessionSave = performance.now(); sessionFlight = true;
    try {
      const response = await fetch(`/api/worlds/${worldId}/session`, {method:'POST',
        headers:{Authorization:`Bearer ${token}`, 'Content-Type':'application/json'},
        body:JSON.stringify(saved), keepalive:leaving, signal:AbortSignal.timeout(10000)});
      if (!response.ok) throw new Error('Your place is saved in this browser. Reconnect to sync it with the host.');
      sessionSaved = Math.max(sessionSaved, saved.sequence);
    } catch (error) { say(error.message); }
    finally { sessionFlight = false; }
  }
  async function apply(action, target, good, amount) {
    const run = async () => {
      applying = true;
      try {
        if (!state) await connect();
        let body = JSON.parse(localStorage.getItem(key) || 'null');
        const recoveringOtherAction = body && (body.action !== action ||
          String(body.target || '0') !== target || (body.good || 0) !== good || (body.amount || 0) !== amount);
        if (!body) {
          body = { protocol: 1, sequence: state.next_sequence, action_revision: state.action_revision, action, target, good, amount };
          localStorage.setItem(key, JSON.stringify(body));
        }
        const result = await request('command', { ...body, campaign: true });
        localStorage.removeItem(key);
        accept(result.world, true);
        if (recoveringOtherAction) throw new Error('Earlier company action recovered. Choose your next action again.');
        return result;
      } catch (error) {
        if (error.status && error.status < 500) localStorage.removeItem(key);
        say(error.message); throw error;
      } finally { applying = false; }
    };
    return navigator.locks ? navigator.locks.request(`cc-coop-command-${worldId}`, run) : run();
  }
  function poll() {
    if (deleted) return;
    saveSession();
    if (!enabled || inFlight || applying || performance.now() - lastPoll < 750) return;
    lastPoll = performance.now(); inFlight = true;
    request(`state?campaign=1&after=${state ? state.revision : -1}`).then(accept).catch(error => say(error.message)).finally(() => { inFlight = false; });
  }
  function exchange(scene, readPose) {
    if (!enabled || !state || !visit || leaving || deleted || startupError) return [];
    if (poseScene !== scene) { peers = []; poseScene = scene; }
    const now = performance.now();
    if (!poseFlight && now - lastPose >= 100) {
      const context = state.session_context, activeVisit = visit;
      lastPose = now; poseFlight = true;
      request('pose', {visit:activeVisit, context, scene, pose:readPose()}).then(next => {
        if (!leaving && activeVisit === visit && next.context === state.session_context && next.scene === poseScene) {
          peers = next.peers.slice(0, 7).map(peer => ({...peer, appearance:CcAvatar.pack(CcAvatar.normalize(peer.appearance))})); peersAt = performance.now();
        }
      }).catch(async error => {
        peers = [];
        if (error.status === 428 && !leaving && activeVisit === visit) {
          const fresh = await request('state?campaign=1&enter=1');
          if (!leaving) { accept(fresh, true); visit = fresh.visit; }
        }
        if (error.status === 409) lastPose = performance.now() + 900;
      }).catch(error => say(error.message)).finally(() => { poseFlight = false; });
    }
    return now - peersAt <= 3000 ? peers : [];
  }
  function exchangeMemory(scene, pose, crew, stride, name_offset, appearance_offset, pose_offset) {
    if (!Module.ccCoop) return 0;
    const peers = Module.ccCoop.exchange(scene, function() {
        return Array.from(HEAPF32.subarray(pose >> 2, (pose >> 2) + 83), function(value) {
            return Math.round(value * 1000) / 1000;
        });
    });
    peers.forEach(function(peer, i) {
        const base = crew + i * stride;
        stringToUTF8(peer.id, base, 17);
        stringToUTF8(peer.name, base + name_offset, 32);
        HEAPU32[(base + appearance_offset) >> 2] = peer.appearance;
        HEAPF32.set(peer.pose, (base + pose_offset) >> 2);
    });
    return peers.length;
  }
  function drawnMemory(crew, count, stride, name_offset, appearance_offset, pose_offset) {
    const drawn = [];
    for (let i = 0; i < count; ++i) {
        const base = crew + i * stride;
        drawn.push({id:UTF8ToString(base), name:UTF8ToString(base + name_offset),
            appearance:HEAPU32[(base + appearance_offset) >> 2],
            position:Array.from(HEAPF32.subarray((base + pose_offset) >> 2, ((base + pose_offset) >> 2) + 3))});
    }
    document.body.dataset.crewDrawn = JSON.stringify(drawn);
  }
  function leave() {
    if (leaving || !visit || !state) return;
    leaving = true; peers = [];
    fetch(`/api/worlds/${worldId}/pose`, {method:'POST', keepalive:true,
      headers:{Authorization:`Bearer ${token}`, 'Content-Type':'application/json'},
      body:JSON.stringify({visit, context:state.session_context, scene:Math.max(0, poseScene), pose:null})}).catch(() => {});
  }
  async function deleteWorld() {
    if (!state?.owner) throw new Error('The host can delete this world.');
    if (applying || localStorage.getItem(key)) throw new Error('Finish the pending company action first.');
    const result = await request('host', {action: 'delete'});
    if (result.deleted !== true) throw new Error('Check the company road book and try again.');
    deleted = true; pending = null; checkpoint = null; peers = [];
    localStorage.removeItem(sessionKey);
    localStorage.removeItem(key);
  }
  function openLobby() {
    location.assign(location.pathname.startsWith('/game/') ? '/' : 'https://crownless.ratimics.com/');
  }
  Module.ccCoop = { enabled, preview, connect, apply, poll, deleteWorld, openLobby,
    exchange, exchangeMemory, drawnMemory, leave, seat:() => Math.max(0, state?.crew.findIndex(member => member.id === state.member) || 0),
    avatar:() => CcAvatar.pack(avatar), checkpoint:captureSession, hasSession:() => restoredSession,
    owner() { return Boolean(state?.owner); },
    ready(error) { startupError = error; document.body.dataset.companyReady = error ? 'error' : 'ready'; if (error) say(error); },
    take() { const value = pending; pending = null; return value; } };
  if (enabled && typeof document !== 'undefined') {
    const attach = () => {
      const hint = document.getElementById('hint');
      if (hint) hint.textContent = Module.crownlessTouchEnabled ?
        'Tap the scene to walk or approach. Your company and clock are saved on the host.' :
        'Click the game, then use the mouse and keyboard. Your company and clock are saved on the host.';
      const bar = document.createElement('div');
      bar.id = 'company-bar';
      document.body.classList.add('shared-game');
      bar.style.cssText = 'position:fixed;top:0;left:0;right:0;z-index:9999;display:flex;justify-content:space-between;padding:7px 16px;background:#20392f;color:#f5f0e4;font:12px system-ui';
      status = document.createElement('span'); status.textContent = 'Joining the shared carriage…';
      pauseControl = document.createElement('button'); pauseControl.hidden = true;
      pauseControl.onclick = async () => {
        pauseControl.disabled = true;
        try {
          await request('host', {action:state.paused ? 'resume' : 'pause'});
          accept(await request('state?campaign=1'), true);
        }
        catch (error) { say(error.message); }
        finally { pauseControl.disabled = false; }
      };
      const link = document.createElement('a'); link.href = `/#world=${worldId}`; link.textContent = 'Your avatar & company ↗'; link.style.color = 'inherit';
      link.onclick = async event => { event.preventDefault(); Module._CcCoopCheckpointNow(); await saveSession(true); leave(); location.assign(link.href); };
      bar.append(status, pauseControl, link); document.body.prepend(bar);
      new ResizeObserver(() => {
        document.body.style.setProperty('--company-bar-height', `${bar.getBoundingClientRect().height}px`);
      }).observe(bar);
    };
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', attach); else attach();
    window.addEventListener('pagehide', () => { Module._CcCoopCheckpointNow?.(); saveSession(true); leave(); });
    document.addEventListener('visibilitychange', () => { if (document.visibilityState === 'hidden') { Module._CcCoopCheckpointNow?.(); saveSession(true); } });
  }
  if (preview && typeof window !== 'undefined') {
    window.addEventListener('message', event => {
      if (event.origin === location.origin && event.source === window.parent && event.data?.type === 'crownless-avatar-preview') avatar = CcAvatar.normalize(event.data.appearance);
    });
    const attachPreview = () => {
      document.body.classList.add('avatar-preview');
      window.parent.postMessage({type:'crownless-avatar-ready'}, location.origin);
    };
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', attachPreview); else attachPreview();
  }
})();
