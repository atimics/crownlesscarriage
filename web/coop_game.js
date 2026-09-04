(function () {
  const worldId = typeof location !== 'undefined' ? new URLSearchParams(location.search).get('world') : null;
  const enabled = /^[a-f0-9]{32}$/.test(worldId || '');
  let state = null, pending = null, lastPoll = 0, inFlight = false, applying = false;
  const token = enabled ? localStorage.getItem('cc-coop-token') : '';
  const key = `cc-coop-pending-${worldId}`;
  let status;
  function say(message) { if (status) status.textContent = message; }
  function accept(next) {
    if (state && next.revision < state.revision) return;
    if (!state || next.revision !== state.revision) pending = next.campaign;
    state = next;
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
    const next = await request('state?campaign=1'); accept(next); pending = next.campaign;
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
        accept(result.world); pending = result.world.campaign;
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
    if (!enabled || inFlight || applying || performance.now() - lastPoll < 750) return;
    lastPoll = performance.now(); inFlight = true;
    request(`state?campaign=1&after=${state ? state.revision : -1}`).then(accept).catch(error => say(error.message)).finally(() => { inFlight = false; });
  }
  Module.ccCoop = { enabled, connect, apply, poll, take() { const value = pending; pending = null; return value; } };
  if (enabled && typeof document !== 'undefined') {
    const attach = () => {
      const bar = document.createElement('div');
      bar.style.cssText = 'position:fixed;top:0;left:0;right:0;z-index:9999;display:flex;justify-content:space-between;padding:7px 16px;background:#20392f;color:#f5f0e4;font:12px system-ui';
      status = document.createElement('span'); status.textContent = 'Joining the shared carriage…';
      const link = document.createElement('a'); link.href = `/#world=${worldId}`; link.textContent = 'Company road book ↗'; link.style.color = 'inherit';
      bar.append(status, link); document.body.append(bar);
    };
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', attach); else attach();
  }
})();
