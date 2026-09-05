/* Music is fetched separately from the game's startup pack. */
(function () {
  const cacheName = 'crownless-music-v1';
  let warming = false;
  async function cache() {
    try { return await caches.open(cacheName); } catch (_) { return null; }
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
  async function get(url, limit, signal) {
    const audio = /\.mp3$/.test(url);
    const storage = audio ? await cache() : null;
    const saved = storage && await storage.match(url);
    if (saved) return read(saved, limit);
    const data = await read(await fetch(url, { signal, cache: audio ? 'default' : 'no-cache' }), limit);
    if (storage) await storage.put(url, new Response(data, {
      headers: { 'Content-Type': 'audio/mpeg', 'Content-Length': String(data.length) }
    })).catch(() => {});
    return data;
  }
  async function warm() {
    if (warming) return;
    warming = true;
    try {
      const response = await fetch('assets/audio/music/offline.json', { signal: AbortSignal.timeout(10000) });
      const manifest = await response.json();
      if (manifest.version !== 1 || !Array.isArray(manifest.tracks) || manifest.tracks.length > 27) return;
      /* One background file at a time keeps the current song and world responsive. */
      for (const track of manifest.tracks) {
        if (!/^\d{2}-\d{2}$/.test(track.stem)) continue;
        if (typeof navigator !== 'undefined' && navigator.onLine === false) break;
        await get(`assets/audio/music/${track.stem}.mp3`, 16 * 1024 * 1024,
                  AbortSignal.timeout(60000));
      }
    } catch (_) { warming = false; }
  }
  Module.ccMusicNet = {
    request: null,
    start(url, limit) {
      if (this.request) return false;
      const controller = new AbortController();
      const request = { status: 0, controller };
      this.request = request;
      const timeout = setTimeout(() => controller.abort(), 30000);
      get(url, limit, controller.signal).then(data => {
        request.data = data; request.status = 1;
      }).catch(() => { request.status = -1; }).finally(() => clearTimeout(timeout));
      void warm();
      return true;
    },
    stop() {
      if (this.request) this.request.controller.abort();
      this.request = null;
    }
  };
})();
