/* Exact speech downloads share a small cache across scene changes. */
function createCrownlessVoiceClient(options = {}) {
  const fetchVoice = options.fetch || globalThis.fetch.bind(globalThis);
  const endpoint = (options.endpoint || "").replace(/\/$/, "");
  const storage = options.storage === undefined ? globalThis.caches : options.storage;
  const origin = options.origin || globalThis.location?.origin || "http://localhost";
  const limit = 1200100;
  const memory = new Map();
  const client = { request: null };

  async function bytes(response) {
    if (!response.ok || Number(response.headers.get("Content-Length")) > limit) throw new Error("Voice download failed");
    const reader = response.body.getReader();
    const chunks = [];
    let length = 0;
    try {
      while (true) {
        const chunk = await reader.read();
        if (chunk.done) break;
        length += chunk.value.length;
        if (length > limit) throw new Error("Voice download exceeds its budget");
        chunks.push(chunk.value);
      }
    } catch (error) {
      await reader.cancel();
      throw error;
    } finally {
      reader.releaseLock();
    }
    const data = new Uint8Array(length);
    let offset = 0;
    for (const chunk of chunks) { data.set(chunk, offset); offset += chunk.length; }
    if (length < 44 || String.fromCharCode(...data.subarray(0, 4)) !== "RIFF" ||
        String.fromCharCode(...data.subarray(8, 12)) !== "WAVE") throw new Error("Voice file is invalid");
    return data;
  }

  async function cacheRead(key) {
    if (memory.has(key)) {
      const data = memory.get(key);
      memory.delete(key); memory.set(key, data);
      return data;
    }
    if (!storage) return null;
    try {
      const cache = await storage.open("crownless-speech-v1");
      const response = await cache.match(`${origin}/__crownless_speech/${key}`);
      return response ? await bytes(response) : null;
    } catch (_) { return null; }
  }

  async function cacheWrite(key, data) {
    memory.set(key, data);
    while (memory.size > 32) memory.delete(memory.keys().next().value);
    if (!storage) return;
    try {
      const cache = await storage.open("crownless-speech-v1");
      const url = `${origin}/__crownless_speech/${key}`;
      await cache.delete(url);
      await cache.put(url, new Response(data, { headers: { "Content-Type": "audio/wav" } }));
      const keys = await cache.keys();
      for (const old of keys.slice(0, Math.max(0, keys.length - 64))) await cache.delete(old);
    } catch (_) { /* Memory caching remains available when storage is full. */ }
  }

  async function resolve(record, request) {
    const timeout = setTimeout(() => request.controller.abort(), 45000);
    const signal = request.controller.signal;
    try {
      let data = await cacheRead(record.key);
      if (!data) {
        const packed = await fetchVoice(`speech/${record.key}.wav`, { signal });
        if (packed.ok) data = await bytes(packed);
        else if (packed.status !== 404) throw new Error("Voice pack request failed");
      }
      if (!data && endpoint && !signal.aborted) {
        const submitted = await fetchVoice(`${endpoint}/v1/speech`, {
          method: "POST", headers: { "Content-Type": "application/json" },
          body: JSON.stringify(record), signal,
        });
        if (submitted.status !== 200 && submitted.status !== 202) throw new Error("Voice job failed");
        while (!signal.aborted) {
          const result = await fetchVoice(`${endpoint}/v1/speech/${record.key}`, { signal });
          if (result.status === 200) { data = await bytes(result); break; }
          if (result.status !== 202) throw new Error("Voice generation failed");
          await new Promise(resolveWait => setTimeout(resolveWait, 250));
        }
      }
      if (!data || signal.aborted) throw new Error("Voice unavailable");
      await cacheWrite(record.key, data);
      if (client.request === request) Object.assign(request, { status: 1, data });
    } catch (_) {
      if (client.request === request) request.status = -1;
    } finally { clearTimeout(timeout); }
  }

  client.start = record => {
    if (client.request || !/^[0-9a-f]{16}$/.test(record.key)) return false;
    const request = { status: 0, controller: new AbortController() };
    client.request = request;
    void resolve(record, request);
    return true;
  };
  client.take = () => {
    const request = client.request;
    if (!request || request.status === 0) return null;
    client.request = null;
    return request;
  };
  client.cancel = () => {
    if (client.request) client.request.controller.abort();
    client.request = null;
  };
  return client;
}

if (typeof module !== "undefined" && module.exports) module.exports = { createCrownlessVoiceClient };
if (typeof Module !== "undefined") {
  Module.ccVoiceNet = createCrownlessVoiceClient({ endpoint: globalThis.CROWNLESS_VOICE_URL || "" });
}
