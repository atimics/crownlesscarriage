/* Music is fetched separately from the game's startup pack. */
(function () {
  const cacheName = 'crownless-music-v1';
  const audioLimit = 16 * 1024 * 1024;
  let warmJob = null, warmTimer = null, tracks = null, nextTrack = 0, stopped = false;
  async function cache() {
    try { return await caches.open(cacheName); } catch (_) { return null; }
  }
  async function match(storage, url) {
    try { return storage && await storage.match(url); } catch (_) { return null; }
  }
  async function read(response, limit) {
    if (!response || !response.ok || Number(response.headers.get('content-length')) > limit)
      throw new Error('Music unavailable');
    const reader = response.body.getReader();
    let size = 0;
    const chunks = [];
    try {
      for (;;) {
        const next = await reader.read();
        if (next.done) break;
        size += next.value.length;
        if (size > limit) throw new Error('Music file too large');
        chunks.push(next.value);
      }
    } finally { await reader.cancel().catch(() => {}); }
    const data = new Uint8Array(size);
    let offset = 0;
    for (const chunk of chunks) { data.set(chunk, offset); offset += chunk.length; }
    return data;
  }
  async function get(url, limit, signal, priority = 'high') {
    const audio = /\.mp3$/.test(url);
    const storage = audio ? await cache() : null;
    const saved = await match(storage, url);
    if (signal.aborted) throw new Error('Music request cancelled');
    if (saved) {
      try { return await read(saved, limit); } catch (_) { /* Refresh a broken entry. */ }
    }
    const data = await read(await fetch(url, {
      signal, priority, cache: audio ? 'default' : 'no-cache'
    }), limit);
    /* Playback can use the bytes while the browser writes its disk copy. */
    if (storage) void storage.put(url, new Response(data, {
      headers: { 'Content-Type': 'audio/mpeg', 'Content-Length': String(data.length) }
    })).catch(() => {});
    return data;
  }
  function scheduleWarm(delay = 250) {
    clearTimeout(warmTimer);
    if (stopped || net.request || warmJob || (tracks && nextTrack >= tracks.length)) return;
    warmTimer = setTimeout(() => { warmTimer = null; void warm(); }, delay);
  }
  async function warm() {
    if (stopped || net.request || warmJob) return;
    if (typeof navigator !== 'undefined' && navigator.onLine === false) return;
    const storage = await cache();
    if (!storage || stopped || net.request || warmJob) return;
    const url = tracks ? `assets/audio/music/${tracks[nextTrack].stem}.mp3` :
      'assets/audio/music/offline.json';
    const controller = new AbortController();
    const job = { url, controller, promise: null };
    warmJob = job;
    const timeout = setTimeout(() => controller.abort(), 60000);
    let retry = 250;
    job.promise = (async () => {
      if (!tracks) {
        const response = await fetch(url, { signal: controller.signal, priority: 'low' });
        const manifest = JSON.parse(new TextDecoder().decode(await read(response, 16384)));
        if (manifest.version !== 1 || !Array.isArray(manifest.tracks) || manifest.tracks.length > 27)
          throw new Error('Music manifest unavailable');
        tracks = manifest.tracks.filter(track => /^\d{2}-\d{2}$/.test(track.stem));
        return null;
      }
      const saved = await match(storage, url);
      if (saved) return null;
      return get(url, audioLimit, controller.signal, 'low');
    })();
    try {
      await job.promise;
      if (url.endsWith('.mp3')) nextTrack++;
    } catch (_) {
      /* An urgent song pauses warming at this file. Other failures retry later. */
      retry = controller.signal.aborted ? 250 : 60000;
    } finally {
      clearTimeout(timeout);
      if (warmJob === job) warmJob = null;
      scheduleWarm(retry);
    }
  }
  const net = Module.ccMusicNet = {
    request: null,
    start(url, limit) {
      if (this.request) return false;
      stopped = false;
      clearTimeout(warmTimer);
      /* Promote a matching transfer; urgent music takes priority over other files. */
      const shared = warmJob && warmJob.url === url ? warmJob : null;
      if (warmJob && !shared) warmJob.controller.abort();
      const controller = shared ? shared.controller : new AbortController();
      const request = { status: 0, controller };
      this.request = request;
      const timeout = setTimeout(() => controller.abort(), 30000);
      (shared ? shared.promise.then(data => data || get(url, limit, controller.signal)) :
        get(url, limit, controller.signal)).then(data => {
        if (controller.signal.aborted || !data || data.length > limit) throw new Error('Music unavailable');
        if (request.status === 0) { request.data = data; request.status = 1; }
      }).catch(() => { if (request.status === 0) request.status = -1; })
        .finally(() => clearTimeout(timeout));
      return true;
    },
    poll() {
      if (!this.request?.status) return null;
      const request = this.request;
      this.request = null;
      scheduleWarm();
      return request;
    },
    cancel() {
      if (this.request) {
        this.request.status = -1;
        this.request.controller.abort();
      }
    },
    stop() {
      stopped = true;
      clearTimeout(warmTimer);
      this.cancel();
      this.request = null;
      if (warmJob) warmJob.controller.abort();
    }
  };
})();

/* The browser decodes and plays full downloaded songs on its media thread. */
(function () {
  const streams = new Map();
  let nextId = 0;
  Module.ccMusicPlayback = {
    open(bytes) {
      if (streams.size >= 4 || !bytes || bytes.length < 1024 || bytes.length > 16 * 1024 * 1024) return 0;
      const id = ++nextId;
      const media = new Audio();
      const url = URL.createObjectURL(new Blob([bytes], { type: 'audio/mpeg' }));
      const item = { media, url, state: 0, primed: false, timer: null, hiddenPaused: false };
      streams.set(id, item);
      media.preload = 'auto';
      media.loop = true;
      media.volume = 0;
      media.hidden = true;
      media.dataset.crownlessMusic = String(id);
      document.body.appendChild(media);
      const fail = () => { item.state = -1; media.pause(); clearTimeout(item.timer); };
      media.addEventListener('error', fail, { once: true });
      item.timer = setTimeout(fail, 15000);
      media.src = url;
      media.play().then(() => {
        if (!streams.has(id)) return;
        media.pause();
        media.currentTime = 0;
        item.primed = true;
      }).catch(fail);
      return id;
    },
    ready(id) {
      const item = streams.get(id);
      if (!item) return -1;
      if (item.state === 0 && item.primed && !item.media.seeking && item.media.readyState >= 3) {
        item.state = 1;
        clearTimeout(item.timer);
      }
      return item.state;
    },
    length(id) { return streams.get(id)?.media.duration || 0; },
    position(id) { return streams.get(id)?.media.currentTime || 0; },
    volume(id, value) {
      const item = streams.get(id);
      if (item) item.media.volume = Number.isFinite(value) ? Math.max(0, Math.min(1, value)) : 0;
    },
    pause(id) {
      const item = streams.get(id);
      if (item) { item.hiddenPaused = false; item.media.pause(); }
    },
    resume(id) {
      const item = streams.get(id);
      if (!item) return;
      if (document.hidden) { item.hiddenPaused = true; return; }
      item.media.play().catch(() => { item.state = -1; });
    },
    close(id) {
      const item = streams.get(id);
      if (!item) return;
      streams.delete(id);
      clearTimeout(item.timer);
      item.media.pause();
      item.media.removeAttribute('src');
      item.media.load();
      item.media.remove();
      URL.revokeObjectURL(item.url);
    }
  };
  if (typeof document !== 'undefined') document.addEventListener('visibilitychange', () => {
    for (const [id, item] of streams) {
      if (document.hidden) {
        item.hiddenPaused = item.state === 1 && !item.media.paused;
        item.media.pause();
      } else if (item.hiddenPaused) {
        item.hiddenPaused = false;
        Module.ccMusicPlayback.resume(id);
      }
    }
  });
})();
