const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const {gameControls} = require('./game_controls.cjs');
const {chromium} = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');

async function main() {
  const root = path.resolve(process.argv[2]);
  const output = path.resolve(process.argv[3] || 'settings-browser-results');
  await fs.mkdir(output, {recursive: true});
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
  async function selectMenuItem(index) {
    await page.locator('#canvas').focus();
    while (await page.evaluate(() => Module.crownlessMenuFocus) !== index) {
      const previous = await page.evaluate(() => Module.crownlessMenuFocus);
      await page.keyboard.press('ArrowDown');
      await page.waitForFunction(value => Module.crownlessMenuFocus !== value, previous);
    }
    await page.keyboard.press('Enter');
  }
  async function prefs() {
    return page.evaluate(() => {
      const file = '/crownless-save/crownless_campaign.ccsave.preferences';
      return FS.analyzePath(file).exists ? FS.readFile(file, {encoding:'utf8'}) : '';
    });
  }
  async function waitForPrefs(...needles) {
    await page.waitForFunction(values => {
      const file = '/crownless-save/crownless_campaign.ccsave.preferences';
      if (!FS.analyzePath(file).exists) return false;
      const content = FS.readFile(file, {encoding:'utf8'});
      return values.every(value => content.includes(value));
    }, needles, {timeout:5000});
  }
  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.waitForFunction(() => window.Module && Module.crownlessScreen === 'title' && document.querySelector('#loading').hidden, undefined, {timeout:120000});
    await selectMenuItem(4);
    await page.waitForFunction(() => Module.crownlessScreen === 'settings');
    await page.screenshot({path:path.join(output,'settings-default.png')});
    await selectMenuItem(1);
    await waitForPrefs('caption_size 1');
    await selectMenuItem(0);
    await waitForPrefs('text_size 1');
    await page.screenshot({path:path.join(output,'settings-large.png')});
    await selectMenuItem(6);
    await page.waitForFunction(() => Module.crownlessTouchFrame?.buttons?.some(
      button => button.label === 'Press a key, or Escape to cancel'), undefined, {timeout:2000});
    await page.keyboard.press('i');
    await waitForPrefs('key_target_next 73');
    let saved = await prefs();
    assert.match(saved,/caption_size 1\n/);
    assert.match(saved,/text_size 1\n/);
    assert.match(saved,/key_target_next 73\n/);
    await selectMenuItem(10);
    await waitForPrefs('caption_size 0', 'text_size 0', 'key_target_next 69');
    await page.locator('#canvas').focus();
    await page.waitForFunction(() => Module.crownlessScreen === 'settings' &&
      document.activeElement === document.querySelector('#canvas'));
    const beforeEscape = await page.evaluate(() => ({
      screen: Module.crownlessScreen,
      focus: Module.crownlessMenuFocus,
      canvasFocused: document.activeElement === document.querySelector('#canvas'),
      bindingCaptureActive: Module.crownlessTouchFrame?.buttons?.some(
        button => button.label.includes('Press a key')) || false
    }));
    assert.equal(beforeEscape.screen, 'settings');
    assert.equal(beforeEscape.focus, 10);
    assert.equal(beforeEscape.canvasFocused, true);
    assert.equal(beforeEscape.bindingCaptureActive, false);
    await page.evaluate(() => {
      window.__settingsEscapeKeys = [];
      for (const name of ['keydown', 'keyup']) {
        document.addEventListener(name, event => {
          if (event.key === 'Escape') window.__settingsEscapeKeys.push(name);
        }, true);
      }
    });
    await page.keyboard.down('Escape');
    await page.evaluate(() => new Promise(resolve =>
      requestAnimationFrame(() => requestAnimationFrame(resolve))));
    await page.keyboard.up('Escape');
    try {
      await page.waitForFunction(() => window.__settingsEscapeKeys.includes('keydown') &&
        window.__settingsEscapeKeys.includes('keyup'), undefined, {timeout:5000});
      await page.waitForFunction(() => Module.crownlessScreen === 'title', undefined,
        {timeout:5000});
    } catch (error) {
      const state = await page.evaluate(() => ({
        screen: Module.crownlessScreen,
        focus: Module.crownlessMenuFocus,
        activeElement: document.activeElement && {
          tag: document.activeElement.tagName,
          id: document.activeElement.id,
          className: document.activeElement.className
        },
        canvasFocused: document.activeElement === document.querySelector('#canvas'),
        bindingCaptureActive: Module.crownlessTouchFrame?.buttons?.some(
          button => button.label.includes('Press a key')) || false,
        keyEvents: window.__settingsEscapeKeys
      }));
      throw new Error(`Escape did not return from settings: ${JSON.stringify(state)}; ${error.message}`);
    }
    await page.reload();
    await page.waitForFunction(() => window.Module && Module.crownlessScreen === 'title' && document.querySelector('#loading').hidden, undefined, {timeout:120000});
    saved = await prefs();
    assert.match(saved,/caption_size 0\n/);
    assert.match(saved,/key_target_next 69\n/);
    await context.close();
    const phone = await browser.newContext({viewport:{width:390,height:844},hasTouch:true,isMobile:true,deviceScaleFactor:3});
    const mobile = await phone.newPage();
    await mobile.goto(`http://127.0.0.1:${server.address().port}/`);
    await mobile.waitForFunction(() => window.Module?.crownlessScreen === 'title' && Module.crownlessTouchFrame?.buttons.length, undefined, {timeout:120000});
    const controls = gameControls(mobile,true);
    await controls.button('Settings & controls').tap();
    await mobile.waitForFunction(() => Module.crownlessScreen === 'settings');
    await controls.button(/^Caption size:/).tap();
    await mobile.waitForFunction(() => {
      const file = '/crownless-save/crownless_campaign.ccsave.preferences';
      return FS.analyzePath(file).exists && FS.readFile(file, {encoding:'utf8'}).includes('caption_size 1');
    }, undefined, {timeout:5000});
    await mobile.screenshot({path:path.join(output,'settings-touch.png')});
    await phone.close();
    console.log('Browser settings, remapping, defaults, and reload persistence passed');
  } finally {
    await context.close();
    await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
