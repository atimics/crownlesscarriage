const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const {chromium} = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');

async function main() {
  const root = path.resolve(process.argv[2]);
  const server = http.createServer(async (request, response) => {
    const pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
    const file = path.resolve(root, '.' + (pathname === '/' ? '/index.html' : pathname));
    if (!file.startsWith(root + path.sep)) { response.writeHead(403).end(); return; }
    try {
      const types = {'.html':'text/html', '.js':'text/javascript', '.wasm':'application/wasm', '.data':'application/octet-stream'};
      response.setHeader('Content-Type', types[path.extname(file)] || 'application/octet-stream');
      response.end(await fs.readFile(file));
    } catch { response.writeHead(404).end(); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch({args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
  const context = await browser.newContext({viewport:{width:1040,height:620}});
  const page = await context.newPage();

  await page.addInitScript(() => {
    const preferencesPath = '/crownless-save/crownless_campaign.ccsave.preferences';
    const originalPut = IDBObjectStore.prototype.put;
    window.preferenceWriteFailures = 0;
    window.failNextPreferenceWrite = false;
    IDBObjectStore.prototype.put = function (value, key) {
      const request = originalPut.call(this, value, key);
      if (key === preferencesPath && window.failNextPreferenceWrite) {
        window.failNextPreferenceWrite = false;
        window.preferenceWriteFailures += 1;
        queueMicrotask(() => {
          try { this.transaction.abort(); } catch (_) {}
        });
      }
      return request;
    };
  });

  async function selectMenuItem(index) {
    await page.locator('#canvas').focus();
    while (await page.evaluate(() => Module.crownlessMenuFocus) !== index) {
      const previous = await page.evaluate(() => Module.crownlessMenuFocus);
      await page.keyboard.press('ArrowDown');
      await page.waitForFunction(value => Module.crownlessMenuFocus !== value, previous);
    }
    await page.keyboard.press('Enter');
  }

  async function preferences() {
    return page.evaluate(() => {
      const file = '/crownless-save/crownless_campaign.ccsave.preferences';
      return FS.analyzePath(file).exists ? FS.readFile(file, {encoding:'utf8'}) : '';
    });
  }

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.waitForFunction(() => window.Module && Module.crownlessScreen === 'title' &&
      document.querySelector('#loading').hidden, undefined, {timeout:120000});
    await selectMenuItem(4);
    await page.waitForFunction(() => Module.crownlessScreen === 'settings');

    await page.evaluate(() => { window.failNextPreferenceWrite = true; });
    await selectMenuItem(1); // Caption size changes in memory, then IndexedDB aborts.
    await page.waitForFunction(() => Module.crownlessTouchFrame?.detail?.includes(
      'The browser could not store client preferences.'), undefined, {timeout:10000});
    assert.equal(await page.evaluate(() => window.preferenceWriteFailures), 1,
      'the actual preferences IndexedDB transaction was aborted once');
    assert.match(await preferences(), /caption_size 1\n/,
      'the failed write leaves the active session preference usable');

    await selectMenuItem(0); // A later setting change retries and commits both values.
    await page.waitForFunction(() => Module.crownlessTouchFrame?.detail?.includes(
      'Settings saved.'), undefined, {timeout:10000});
    await page.waitForFunction(() => FS.readFile(
      '/crownless-save/crownless_campaign.ccsave.preferences', {encoding:'utf8'})
      .includes('text_size 1'));
    assert.match(await preferences(), /caption_size 1\n/);
    assert.match(await preferences(), /text_size 1\n/);

    await page.reload();
    await page.waitForFunction(() => window.Module && Module.crownlessScreen === 'title' &&
      document.querySelector('#loading').hidden, undefined, {timeout:120000});
    await selectMenuItem(4);
    await page.waitForFunction(() => Module.crownlessScreen === 'settings');
    const restored = await preferences();
    assert.match(restored, /caption_size 1\n/,
      'the retry persists caption size across reload');
    assert.match(restored, /text_size 1\n/,
      'the retry persists body size across reload');
    console.log('Browser settings storage failure feedback and retry passed');
  } finally {
    await context.close();
    await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
