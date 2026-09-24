const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const {chromium} = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');

async function main() {
  const root = path.resolve(process.argv[2]);
  const output = path.resolve(process.argv[3] || 'landscape-touch-results');
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
  const context = await browser.newContext({viewport: {width: 844, height: 390},
    hasTouch: true, isMobile: true, deviceScaleFactor: 2});
  const page = await context.newPage();
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));

  async function layout(stage) {
    const measurements = await page.evaluate(() => {
      const canvas = document.querySelector('#canvas').getBoundingClientRect();
      const panel = document.querySelector('#touch-actions');
      const panelBounds = panel.getBoundingClientRect();
      const save = document.querySelector('#save-status').getBoundingClientRect();
      return {
        canvas: {x: canvas.x, y: canvas.y, width: canvas.width, height: canvas.height},
        panel: {x: panelBounds.x, y: panelBounds.y, width: panelBounds.width, height: panelBounds.height,
          display: getComputedStyle(panel).display, clipPath: getComputedStyle(panel).clipPath,
          fontSize: getComputedStyle(panel).fontSize},
        save: {x: save.x, y: save.y, width: save.width, height: save.height},
        buttons: [...panel.querySelectorAll('.touch-buttons button')].map(button => {
          const box = button.getBoundingClientRect();
          return {label: button.textContent.trim(), width: box.width, height: box.height,
            enabled: !button.disabled};
        })
      };
    });
    assert(measurements.panel.width >= 250 && measurements.panel.height >= 180,
      `${stage}: touch action area is visible: ${JSON.stringify(measurements.panel)}`);
    assert.notEqual(measurements.panel.clipPath, 'inset(50%)',
      `${stage}: touch action area is not clipped`);
    assert(measurements.panel.x >= measurements.canvas.x + measurements.canvas.width - 1,
      `${stage}: touch actions sit beside the canvas: ${JSON.stringify(measurements)}`);
    assert(measurements.panel.y + measurements.panel.height <= measurements.save.y + 1,
      `${stage}: touch actions stay above save status: ${JSON.stringify(measurements)}`);
    assert(measurements.buttons.length > 0, `${stage}: at least one action is listed`);
    assert(measurements.buttons.every(button => button.width >= 44 && button.height >= 44),
      `${stage}: every semantic action meets the 44px touch target: ${JSON.stringify(measurements.buttons)}`);
    assert(Number.parseFloat(measurements.panel.fontSize) >= 16,
      `${stage}: touch action text remains at least 16px`);
    assert(Math.abs(measurements.canvas.width / measurements.canvas.height - 16 / 9) < 0.01,
      `${stage}: game canvas keeps its 16:9 shape`);
    return measurements;
  }

  async function tap(pattern, stage) {
    const buttons = page.locator('#touch-actions .touch-buttons button');
    const button = buttons.filter({hasText: pattern}).first();
    await button.waitFor({state: 'visible', timeout: 30000});
    await button.scrollIntoViewIfNeeded();
    assert(await button.isEnabled(), `${stage}: ${pattern} is enabled`);
    const box = await button.boundingBox();
    assert(box.width >= 44 && box.height >= 44,
      `${stage}: ${pattern} has a touch-size target: ${JSON.stringify(box)}`);
    await button.tap();
  }

  async function enterTrade(stage) {
    await page.waitForFunction(() => Module.crownlessScreen === 'playing', undefined, {timeout: 45000});
    await layout(`${stage} town`);
    const frame = await page.evaluate(() => Module.crownlessTouchFrame);
    if (frame.scene !== 'trade' && !frame.reading.includes('Granary keeper')) {
      await tap('Enter Granary hall', `${stage} town`);
      await page.waitForFunction(() => Module.crownlessTouchFrame.reading.includes('Granary keeper'),
        undefined, {timeout: 30000});
    }
    if (frame.scene !== 'trade') await tap(/Trade .*Granary keeper/, `${stage} keeper`);
    await page.waitForFunction(() => Module.crownlessTouchFrame.scene === 'trade',
      undefined, {timeout: 30000});
    return layout(`${stage} trade`);
  }

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.waitForFunction(() => window.Module?.crownlessScreen === 'title' &&
      document.querySelector('#loading').hidden, undefined, {timeout: 120000});
    await layout('title');
    await page.screenshot({path: path.join(output, 'title-844x390.png')});

    await tap('Play', 'title');
    const tradeLayout = await enterTrade('first visit');
    let reading = page.locator('#touch-actions .touch-reading');
    assert.match(await reading.textContent(), /Purse 42\s*\|\s*Cargo 0\/12/,
      'fresh company starts with the expected purse and cargo');
    await tap(/^Bread: \d+$/, 'trade stock');
    await tap('+', 'trade quantity');
    await page.waitForFunction(() => Module.crownlessTouchFrame.reading.includes('Buy: 2 Bread'),
      undefined, {timeout: 10000});
    await page.screenshot({path: path.join(output, 'trade-quote-844x390.png')});
    const quote = await reading.textContent();
    assert.match(quote, /Total cost 8 crowns/);

    await tap(/^Buy\s+Enter$/, 'trade confirmation');
    await page.waitForFunction(() => Module.crownlessTouchFrame.reading.includes('Bought 2 Bread'),
      undefined, {timeout: 20000});
    let receipt = await reading.textContent();
    assert.match(receipt, /Bought 2 Bread\. -8 crowns\. Purse: 34\./);
    assert.match(receipt, /Purse 34\s*\|\s*Cargo 2\/12/);
    const revision = await page.evaluate(() => Module.crownlessSaveRevision);
    await page.screenshot({path: path.join(output, 'trade-receipt-844x390.png')});
    await tap(/^Back(?:\s|$)/, 'leave the trade panel');
    await page.waitForFunction(() => Module.crownlessTouchFrame.scene !== 'trade',
      undefined, {timeout: 20000});
    await page.waitForFunction(() => Module.crownlessTouchFrame.buttons.some(button => /^Save/.test(button.label)),
      undefined, {timeout: 20000});
    await tap(/^Save/, 'save action');
    await page.waitForFunction(previous => Module.crownlessSaveRevision > previous,
      revision, {timeout: 20000});
    await page.screenshot({path: path.join(output, 'town-save-844x390.png')});

    await page.reload();
    await page.waitForFunction(() => window.Module?.crownlessScreen === 'title' &&
      document.querySelector('#loading').hidden, undefined, {timeout: 120000});
    await tap('Play', 'reloaded title');
    await enterTrade('reloaded visit');
    reading = page.locator('#touch-actions .touch-reading');
    const restored = await reading.textContent();
    assert.match(restored, /Purse 34\s*\|\s*Cargo 2\/12/);
    assert.match(restored, /Carriage 2/);
    await page.screenshot({path: path.join(output, 'trade-reload-844x390.png')});
    assert.deepEqual(errors, [], 'browser reports no page errors');
    await fs.writeFile(path.join(output, 'result.json'), JSON.stringify({
      viewport: '844x390', title: 'visible action area', trade: 'Granary Hall',
      canvasCss: tradeLayout.canvas,
      actionPanelCss: tradeLayout.panel,
      minimumActionButtonCss: {
        width: Math.min(...tradeLayout.buttons.map(button => button.width)),
        height: Math.min(...tradeLayout.buttons.map(button => button.height))
      },
      confirmedQuote: '8 crowns for 2 Bread', receipt: 'Purse 42 to 34; cargo 0 to 2 Bread',
      reloaded: 'Purse 34; carriage holds 2 Bread'
    }, null, 2) + '\n');
    console.log('Landscape touch action panel, Granary trade, save, and reload passed at 844x390');
  } finally {
    await context.close();
    await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
