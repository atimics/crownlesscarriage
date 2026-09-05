const result = document.querySelector('#result');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
function check(condition, message) { if (!condition) throw new Error(message); }
function tone() {
  const rate = 48000, frames = rate * 12;
  const bytes = new Uint8Array(44 + frames * 2);
  const view = new DataView(bytes.buffer);
  function text(offset, value) { for (let i = 0; i < value.length; i++) bytes[offset + i] = value.charCodeAt(i); }
  text(0, 'RIFF'); view.setUint32(4, bytes.length - 8, true); text(8, 'WAVEfmt ');
  view.setUint32(16, 16, true); view.setUint16(20, 1, true); view.setUint16(22, 1, true);
  view.setUint32(24, rate, true); view.setUint32(28, rate * 2, true);
  view.setUint16(32, 2, true); view.setUint16(34, 16, true); text(36, 'data');
  view.setUint32(40, frames * 2, true);
  for (let i = 0; i < frames; i++) view.setInt16(44 + i * 2, Math.sin(i * 2 * Math.PI * 440 / rate) * 2000, true);
  return bytes;
}
async function prepared(player, id) {
  for (let i = 0; i < 500 && player.ready(id) === 0; i++) await delay(10);
  check(player.ready(id) === 1, 'The browser opens and buffers the song');
}
document.querySelector('#run').onclick = async () => {
  result.textContent = 'Running';
  result.dataset.state = 'running';
  const context = new AudioContext();
  const player = Module.ccMusicPlayback;
  const opened = [];
  try {
    await context.resume();
    await context.audioWorklet.addModule('/tests/fixtures/music-meter.js');
    const bytes = tone();
    const id = player.open(bytes); opened.push(id);
    await prepared(player, id);
    const media = document.querySelector(`[data-crownless-music="${id}"]`);
    check(media.paused && media.currentTime < 0.05, 'Prepared song waits at the start');
    const source = context.createMediaElementSource(media);
    const meter = new AudioWorkletNode(context, 'music-meter');
    source.connect(meter).connect(context.destination); // The meter emits silence.
    function message(command) {
      return new Promise(resolve => { meter.port.onmessage = event => resolve(event.data); meter.port.postMessage(command); });
    }
    player.volume(id, 1);
    player.resume(id);
    await delay(250);
    await message('start');
    const before = player.position(id);
    const end = performance.now() + 700;
    while (performance.now() < end) { /* Deliberately stall the page. */ }
    await delay(150);
    const report = await message('report');
    const advanced = player.position(id) - before;
    check(advanced >= 0.65, 'Playback time advances through the page stall');
    check(report.frames >= context.sampleRate * 0.65, 'The audio thread processes the stalled interval');
    check(report.longestSilence < context.sampleRate * 0.02, 'The signal stays continuous through the stall');
    player.pause(id);
    const pausedAt = player.position(id);
    await delay(100);
    check(Math.abs(player.position(id) - pausedAt) < 0.02, 'Paused music holds its position');
    player.resume(id);
    await delay(100);
    check(player.position(id) > pausedAt + 0.03, 'Resumed music continues from that position');
    player.pause(id);
    for (let i = 0; i < 3; i++) opened.push(player.open(bytes));
    check(opened.every(Boolean) && player.open(bytes) === 0, 'Four streams bound music memory');
    for (const stream of opened) player.close(stream);
    check(document.querySelectorAll('[data-crownless-music]').length === 0, 'Closing music releases its media elements');
    const broken = player.open(new Uint8Array(2048)); opened.push(broken);
    for (let i = 0; i < 500 && player.ready(broken) === 0; i++) await delay(10);
    check(player.ready(broken) < 0, 'A damaged file fails before its fade can begin');
    player.close(broken);
    result.textContent = JSON.stringify({ status: 'passed', stalledMs: 700, playbackAdvanced: advanced,
      measuredFrames: report.frames, longestSilentMs: report.longestSilence / context.sampleRate * 1000 }, null, 2);
    result.dataset.state = 'passed';
  } catch (error) {
    result.textContent = String(error.stack || error);
    result.dataset.state = 'failed';
  } finally {
    for (const id of opened) player.close(id);
    Module.ccMusicNet.stop();
    await context.close();
  }
};
