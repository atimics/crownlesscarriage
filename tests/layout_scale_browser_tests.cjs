const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const {chromium} = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');
const {gameControls} = require('./game_controls.cjs');

async function main() {
  const root = path.resolve(process.argv[2]);
  const output = path.resolve(process.argv[3] || 'layout-scale-results');
  await fs.mkdir(output, {recursive: true});
  const server = http.createServer(async (request, response) => {
    const pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
    const file = path.resolve(root, '.' + (pathname === '/' ? '/index.html' : pathname));
    if (!file.startsWith(root + path.sep)) { response.writeHead(403).end(); return; }
    try {
      const types = {'.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm',
        '.data': 'application/octet-stream', '.png': 'image/png'};
      response.setHeader('Content-Type', types[path.extname(file)] || 'application/octet-stream');
      response.end(await fs.readFile(file));
    } catch { response.writeHead(404).end(); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch({args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader']});
  const results = [];

  async function selectSetting(page, index) {
    await page.locator('#canvas').focus();
    while (await page.evaluate(() => Module.crownlessMenuFocus) !== index) {
      const before = await page.evaluate(() => Module.crownlessMenuFocus);
      await page.keyboard.press('ArrowDown');
      await page.waitForFunction(value => Module.crownlessMenuFocus !== value, before);
    }
    await page.keyboard.press('Enter');
  }

  async function validateButtons(page, label) {
    const frame = await page.evaluate(() => ({
      title: Module.crownlessTouchFrame.title,
      reading: Module.crownlessTouchFrame.reading,
      buttons: Module.crownlessTouchFrame.buttons
    }));
    assert(frame.buttons.length > 0, `${label}: controls are present`);
    const [canvasWidth, canvasHeight] = await page.locator('#canvas').evaluate(node => [node.width, node.height]);
    for (const button of frame.buttons) {
      assert(button.x >= 0 && button.y >= 0, `${label}: ${button.label} begins inside the canvas`);
      assert(button.x + button.width <= canvasWidth && button.y + button.height <= canvasHeight,
        `${label}: ${button.label} fits the canvas`);
    }
    return frame;
  }

  async function runCase(width, height, large) {
    const size = large ? 'large' : 'normal';
    const name = `${width}x${height}-${size}`;
    const directory = path.join(output, name);
    await fs.mkdir(directory, {recursive: true});
    const context = await browser.newContext({viewport: {width, height}, hasTouch: true});
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    try {
      await page.goto(`http://127.0.0.1:${server.address().port}/`);
      await page.waitForFunction(() => window.Module?.crownlessScreen === 'title' &&
        document.querySelector('#loading').hidden, undefined, {timeout: 120000});
      const controls = gameControls(page, true);
      if (large) {
        await controls.button('Settings & controls').tap();
        await page.waitForFunction(() => Module.crownlessScreen === 'settings');
        await selectSetting(page, 0); // Body text size: standard -> large.
        await selectSetting(page, 1); // Caption size: standard -> large.
        await page.keyboard.press('Escape');
        await page.waitForFunction(() => Module.crownlessScreen === 'title');
      }
      await controls.button('Play').tap();
      await page.waitForFunction(() => Module.crownlessScreen === 'playing', undefined, {timeout: 45000});
      const town = await validateButtons(page, `${name} town actions`);
      await page.screenshot({path: path.join(directory, 'town-actions.png')});

      await controls.button(/^Book/).tap();
      await page.waitForFunction(() => Module.crownlessTouchFrame.title === 'Company Book', undefined, {timeout: 15000});
      const book = await validateButtons(page, `${name} Company Book`);
      await page.screenshot({path: path.join(directory, 'company-book.png')});
      await controls.button(/^Back/).tap();
      await page.waitForFunction(() => Module.crownlessScreen === 'playing', undefined, {timeout: 15000});

      await controls.button('Enter Granary hall').tap();
      await page.waitForFunction(() => Module.crownlessTouchFrame.reading.includes('Granary keeper'), undefined, {timeout: 30000});
      await controls.button(/^Trade .*Granary keeper/).tap();
      await page.waitForFunction(() => Module.crownlessTouchFrame.scene === 'trade', undefined, {timeout: 30000});
      const trade = await validateButtons(page, `${name} Granary trade`);
      const replay = trade.buttons.find(button => button.label === 'Replay F7');
      const skip = trade.buttons.find(button => button.label === 'Skip F8');
      assert(replay && skip, `${name}: keeper speech controls remain available on the trade panel`);
      assert(replay.y + replay.height <= 44 && skip.y + skip.height <= 44,
        `${name}: speech controls stay in the clear top margin above the trade panel`);
      assert.match(trade.reading, /For 1 Bread, the price is 4 crowns\./,
        `${name}: keeper caption remains available with the trade controls`);
      await page.screenshot({path: path.join(directory, 'granary-trade.png')});
      assert.deepEqual(errors, [], `${name}: browser reports no page errors`);
      const result = {viewport: `${width}x${height}`, text: size,
        town: town.title, book: book.title, trade: trade.title,
        townControls: town.buttons.length, bookControls: book.buttons.length,
        tradeControls: trade.buttons.length, reading: trade.reading};
      await fs.writeFile(path.join(directory, 'result.json'), JSON.stringify(result, null, 2) + '\n');
      results.push(result);
    } finally {
      await context.close();
    }
  }

  try {
    for (const [width, height] of [[1040, 620], [1200, 700], [1280, 720]]) {
      await runCase(width, height, false);
      await runCase(width, height, true);
    }
    await fs.writeFile(path.join(output, 'results.json'), JSON.stringify(results, null, 2) + '\n');
    console.log(`Layout controls passed at ${results.map(result => `${result.viewport} ${result.text}`).join(', ')}`);
  } finally {
    await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
