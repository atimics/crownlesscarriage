(function () {
  const worldId = typeof location !== 'undefined' ? new URLSearchParams(location.search).get('world') : null;
  const enabled = /^[a-f0-9]{32}$/.test(worldId || '');
  const preview = typeof location !== 'undefined' && new URLSearchParams(location.search).get('avatar') === '1';
  let avatar = CcAvatar.normalize();
  let state = null, pending = null, lastPoll = 0, inFlight = false, applying = false;
  const token = enabled ? localStorage.getItem('cc-coop-token') : '';
  const key = `cc-coop-pending-${worldId}`;
  const sessionKey = `cc-coop-session-${worldId}`;
  let checkpoint = null, sessionSequence = 0, sessionSaved = 0, sessionFlight = false, lastSessionSave = 0, restoredSession = false;
  let status, pauseControl, startupError = '';
  function say(message) { if (status) status.textContent = startupError || message; }
  function accept(next, forceSnapshot = false) {
    if (state && next.revision < state.revision) return;
    if (!state || next.revision !== state.revision || forceSnapshot) pending = next.campaign;
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
    let next = await request('state?campaign=1');
    while (next.catching_up) {
      say(`The world is catching up · ${next.away_clock.days_pending} days to settle…`);
      await new Promise(resolve => setTimeout(resolve, 500));
      next = await request('state?campaign=1');
    }
    accept(next, true);
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
    if (!enabled || !state?.session_context || checkpoint?.session === session) return;
    checkpoint = {context:state.session_context, sequence:++sessionSequence, session};
    localStorage.setItem(sessionKey, JSON.stringify(checkpoint));
    const place = session.split('\n')[1].split(' ');
    document.body.dataset.playerPosition = `${place[5]},${place[6]}`;
    document.body.dataset.playerScene = place[2];
  }
  async function saveSession(leaving = false) {
    if (!checkpoint || checkpoint.sequence <= sessionSaved || (sessionFlight && !leaving)) return;
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
    saveSession();
    if (!enabled || inFlight || applying || performance.now() - lastPoll < 750) return;
    lastPoll = performance.now(); inFlight = true;
    request(`state?campaign=1&after=${state ? state.revision : -1}`).then(accept).catch(error => say(error.message)).finally(() => { inFlight = false; });
  }
  Module.ccCoop = { enabled, preview, connect, apply, poll, avatar:() => CcAvatar.pack(avatar),
    checkpoint:captureSession, hasSession:() => restoredSession,
    ready(error) { startupError = error; document.body.dataset.companyReady = error ? 'error' : 'ready'; if (error) say(error); },
    take() { const value = pending; pending = null; return value; } };
  if (enabled && typeof document !== 'undefined') {
    const attach = () => {
      const hint = document.getElementById('hint');
      if (hint) hint.textContent = 'Click the game, then use the mouse and keyboard. Your company and clock are saved on the host.';
      const bar = document.createElement('div');
      bar.style.cssText = 'position:fixed;top:0;left:0;right:0;z-index:9999;display:flex;justify-content:space-between;padding:7px 16px;background:#20392f;color:#f5f0e4;font:12px system-ui';
      status = document.createElement('span'); status.textContent = 'Joining the shared carriage…';
      pauseControl = document.createElement('button'); pauseControl.hidden = true;
      pauseControl.onclick = async () => {
        pauseControl.disabled = true;
        try { accept(await request('host', {action:state.paused ? 'resume' : 'pause'})); }
        catch (error) { say(error.message); }
        finally { pauseControl.disabled = false; }
      };
      const link = document.createElement('a'); link.href = `/#world=${worldId}`; link.textContent = 'Your avatar & company ↗'; link.style.color = 'inherit';
      link.onclick = async event => { event.preventDefault(); Module._CcCoopCheckpointNow(); await saveSession(true); location.assign(link.href); };
      bar.append(status, pauseControl, link); document.body.append(bar);
    };
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', attach); else attach();
    window.addEventListener('pagehide', () => { Module._CcCoopCheckpointNow?.(); saveSession(true); });
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
