/* Small, local-only timings for finding slow browser journeys. */
(function (root) {
  const module = root.Module = root.Module || {};
  const clock = root.performance || {now: () => Date.now()};
  const capacity = 96;
  const entries = [];
  const pending = new Map();
  const transitions = [];
  let nextToken = 1;
  let firstActionableRecorded = false;

  function safeBuild(value) {
    return String(value || '0.1.0-web').replace(/[^A-Za-z0-9._-]/g, '').slice(0, 40) || 'web';
  }
  function safeAction(value) {
    const action = String(value || 'none');
    if (/^touch-\d+$/.test(action)) return action.slice(0, 24);
    return ['runtime', 'campaign', 'new-campaign', 'none'].includes(action) ? action : 'other';
  }
  function currentScene(frame) {
    const scene = String(frame?.scene || 'startup');
    return ['startup', 'menu', 'town', 'road', 'mine', 'book', 'dungeon',
      'carriage', 'conversation', 'trade'].includes(scene) ? scene : 'other';
  }
  function revision(frame) {
    const value = frame?.revision;
    return Number.isSafeInteger(value) ? value : 0;
  }
  function append(stage, started, metadata) {
    const duration = Math.max(0, clock.now() - started);
    entries.push({
      stage,
      duration_ms: Math.round(duration * 10) / 10,
      build: safeBuild(module.crownlessBuild),
      scene: currentScene(metadata?.frame),
      revision: revision(metadata?.frame),
      action: safeAction(metadata?.action)
    });
    if (entries.length > capacity) entries.splice(0, entries.length - capacity);
  }
  function begin(stage, metadata) {
    const token = nextToken++;
    pending.set(token, {stage, started: clock.now(), metadata: metadata || {}});
    return token;
  }
  function finish(token, metadata) {
    const item = pending.get(token);
    if (!item) return false;
    pending.delete(token);
    append(item.stage, item.started, {...item.metadata, ...metadata});
    return true;
  }
  function beginAction(index, frame) {
    const action = `touch-${Math.max(0, Number(index) || 0)}`;
    const started = clock.now();
    const token = begin('action', {action, frame});
    transitions.length = 0;
    transitions.push({started, action, scene: currentScene(frame)});
    return token;
  }
  function publishFrame(frame) {
    const now = clock.now();
    const scene = currentScene(frame);
    if (!firstActionableRecorded && frame?.buttons?.some(button => button.enabled)) {
      append('first-actionable', 0, {action: 'runtime', frame});
      firstActionableRecorded = true;
    }
    for (let i = transitions.length - 1; i >= 0; --i) {
      const item = transitions[i];
      if (scene !== item.scene) {
        append('transition', item.started, {action: item.action, frame});
        transitions.splice(i, 1);
      } else if (now - item.started > 5000) transitions.splice(i, 1);
    }
  }
  function snapshot() {
    return {schema: 1, capacity, entries: entries.map(entry => ({...entry}))};
  }
  function exportJson() { return JSON.stringify(snapshot(), null, 2); }
  function download() {
    const blob = new Blob([exportJson()], {type: 'application/json'});
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = 'crownless-local-timings.json';
    link.click();
    URL.revokeObjectURL(url);
  }

  module.crownlessBuild = module.crownlessBuild || 'unknown-web-build';
  module.crownlessDiagnostics = {begin, finish, beginAction, publishFrame, snapshot};
  module.exportCrownlessDiagnostics = exportJson;
  module.downloadCrownlessDiagnostics = download;
  module.postRun = module.postRun || [];
  module.postRun.push(() => append('runtime-ready', 0,
    {action: 'runtime', frame: module.crownlessTouchFrame}));

})(typeof globalThis !== 'undefined' ? globalThis : this);
