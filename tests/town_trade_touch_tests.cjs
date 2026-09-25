const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const {chromium} = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');
const {gameControls} = require('./game_controls.cjs');

async function main() {
  const root = path.resolve(process.argv[2]);
  const output = path.resolve(process.argv[3] || 'town-trade-touch-results');
  await fs.mkdir(output, {recursive: true});
  const server = http.createServer(async (request, response) => {
    const pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
    const file = path.resolve(root, '.' + (pathname === '/' ? '/index.html' : pathname));
    if (!file.startsWith(root + path.sep)) { response.writeHead(403).end(); return; }
    try {
      const types = {'.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.png': 'image/png'};
      response.setHeader('Content-Type', types[path.extname(file)] || 'application/octet-stream');
      response.end(await fs.readFile(file));
    } catch { response.writeHead(404).end(); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch({args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader']});

  async function startGame(page) {
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.waitForFunction(() => window.Module && document.querySelector('#loading').hidden,
      undefined, {timeout: 120000});
    await page.waitForFunction(() => Module.crownlessScreen === 'title');
    await page.locator('#canvas').focus();
    await page.keyboard.press('Enter');
    await page.waitForFunction(() => Module.crownlessScreen === 'playing',
      undefined, {timeout: 120000});
  }

  async function chooseBakery(controls, page) {
    for (let attempt = 0; attempt < 4; ++attempt) {
      const buttons = await controls.buttons();
      if (buttons.some(button => button.label === 'Enter Bakery')) {
        await controls.button('Enter Bakery').tap();
        return;
      }
      assert(buttons.some(button => button.label === 'More objects'),
        'The town tray must give a page to the bakery');
      const before = await page.evaluate(() => Module.crownlessTouchFrame.revision);
      await controls.button('More objects').tap();
      // Read the next page only after the tray has redrawn; a slow renderer can
      // still show the old page, and a second tap would skip past the bakery.
      await page.waitForFunction(revision =>
        Module.crownlessTouchFrame.revision !== revision, before, {timeout: 15000});
    }
    assert.fail('The town tray must list the bakery');
  }

  async function enterTrade(page) {
    const controls = gameControls(page, true);
    if (await page.evaluate(() => Module.crownlessTouchFrame.scene !== 'trade')) {
      if (!await controls.button(/^Trade .*Baker/).read()) await chooseBakery(controls, page);
      await page.waitForFunction(() => Module.crownlessTouchFrame.buttons
        .some(button => /^Trade .*Baker/.test(button.label)),
      undefined, {timeout: 45000});
      await controls.button(/^Trade .*Baker/).tap();
      await page.waitForFunction(() => Module.crownlessTouchFrame.scene === 'trade',
        undefined, {timeout: 45000});
    }
  }

  async function runViewport(name, width, height) {
    const context = await browser.newContext({viewport: {width, height}, hasTouch: true});
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await startGame(page);
    const controls = gameControls(page, true);
    const start = await page.evaluate(() => Module.crownlessLocalNavigation);
    if (width === 390) {
      const mobileDetailSize = await page.locator('#touch-actions .touch-detail')
        .evaluate(node => getComputedStyle(node).fontSize);
      assert.equal(mobileDetailSize, '16px', 'mobile trade detail stays at the readable 16px touch size');
    }
    const before = await controls.reading();
    assert.match(before, /2 Lantern\s*\/\s*42 crowns\s*\/\s*Cargo 0\/12/,
      `${name}: fresh campaign calendar, purse, and cargo: ${before}`);

    await fs.mkdir(path.join(output, name), {recursive: true});
    await page.screenshot({path: path.join(output, name, 'town.png')});
    await chooseBakery(controls, page);
    await page.waitForFunction(() => Module.crownlessTouchFrame.buttons
      .some(button => /^Trade .*Baker/.test(button.label)),
    undefined, {timeout: 45000});
    const arrival = await page.evaluate(() => Module.crownlessLocalNavigation);
    assert(Math.hypot(arrival.x - start.x, arrival.z - start.z) > 2,
      `${name}: entering the bakery walks across town: ${JSON.stringify({start, arrival})}`);
    await controls.button(/^Trade .*Baker/).tap();
    await page.waitForFunction(() => Module.crownlessTouchFrame.scene === 'trade',
      undefined, {timeout: 45000});
    await page.waitForTimeout(300);
    const bread = (await controls.buttons()).find(button => /^Bread: \d+$/.test(button.label));
    assert(bread && bread.enabled, `${name}: stocked Bread must be selectable`);
    await controls.button(/^Bread: \d+$/).tap();
    await controls.button('+').tap();
    await page.waitForFunction(() => Module.crownlessTouchFrame.reading.includes('Buy: 2 Bread'),
      undefined, {timeout: 10000});
    const quote = await controls.reading();
    assert.match(quote, /Buy: 2 Bread/);
    assert.match(quote, /Purse 42\s*\|\s*Cargo 0\/12/);
    const quotedCost = Number(quote.match(/Total cost (\d+) crowns/)[1]);
    const buyName = `Buy 2 Bread. Total cost: ${quotedCost} crowns.`;
    const buyControl = page.getByRole('button', {name: buyName, exact: true});
    await buyControl.focus();
    assert.equal(await page.evaluate(() => document.activeElement?.textContent), buyName,
      `${name}: keyboard focus speaks the complete Buy offer`);
    assert.equal(await buyControl.getAttribute('aria-disabled'), 'false');
    await controls.button('Sell').tap();
    const blockedSell = page.getByRole('button',
      {name: /^Sell 2 Bread\. Sale value: \d+ crowns\. Bring the full quantity to the counter\.$/});
    await blockedSell.evaluate(node => node.previousElementSibling.focus());
    await page.keyboard.press('Tab');
    await page.waitForFunction(() => document.activeElement?.textContent?.startsWith('Sell 2 Bread.'),
      undefined, {timeout: 3000});
    await page.keyboard.press('Shift+Tab');
    assert.equal(await page.evaluate(() => document.activeElement?.textContent), '+',
      `${name}: Shift+Tab returns to the quantity control`);
    await page.keyboard.press('Tab');
    assert.match(await page.evaluate(() => document.activeElement?.textContent),
      /^Sell 2 Bread\. Sale value: \d+ crowns\. Bring the full quantity to the counter\.$/,
      `${name}: keyboard focus includes the precise disabled Sell reason`);
    assert.equal(await blockedSell.getAttribute('aria-disabled'), 'true');
    await page.screenshot({path: path.join(output, name, 'trade-blocked-sell.png')});
    const blockedState = await page.evaluate(() => ({revision: Module.crownlessTouchFrame.revision,
      detail: Module.crownlessTouchFrame.detail}));
    await blockedSell.press('Enter');
    assert.deepEqual(await page.evaluate(() => ({revision: Module.crownlessTouchFrame.revision,
      detail: Module.crownlessTouchFrame.detail})), blockedState,
      `${name}: the disabled Sell action keeps the same offer and game state`);
    await controls.button('Buy').tap();
    await buyControl.focus();
    await page.screenshot({path: path.join(output, name, 'trade-before.png')});
    const revision = await page.evaluate(() => Module.crownlessSaveRevision);
    await buyControl.press('Enter');
    await page.waitForFunction(() => Module.crownlessTouchFrame.reading.includes('Bought 2 Bread'),
      undefined, {timeout: 20000});
    await page.keyboard.press('F5');
    await page.waitForFunction(previous => Module.crownlessSaveRevision > previous,
      revision, {timeout: 20000});
    const after = await controls.reading();
    assert.match(after, /Bought 2 Bread\./);
    assert.match(after, /Total cost 8 crowns/, `${name}: completed offer keeps its confirmed price beside the receipt`);
    assert.match(after, /Purse 34\s*\|\s*Cargo 2\/12/);
    assert.match(after, /Carriage 2/);
    const completedBuyEnabled = await page.evaluate(() =>
      Module.crownlessTouchFrame.buttons.find(button => /^Buy 2 Bread\. Total cost:/.test(button.label))?.enabled);
    assert.equal(completedBuyEnabled, false,
      `${name}: the completed purchase stays disabled until the player changes the offer`);
    assert.match(await page.getByRole('button', {name: /^Buy 2 Bread\. Total cost:/}).textContent(),
      /Trade complete\. Choose another good or quantity for your next trade\.$/,
      `${name}: completed Buy explains why it is disabled`);
    const receipt = after.match(/Bought 2 Bread\. -(\d+) crowns\. Purse: (\d+)\./);
    assert(receipt, `${name}: receipt reports exact price and balance: ${after}`);
    const charged = Number(receipt[1]);
    const receiptBalance = Number(receipt[2]);
    assert.equal(charged, quotedCost, `${name}: confirmed price matches the displayed quote`);
    assert.equal(receiptBalance, 42 - quotedCost, `${name}: receipt balance matches quoted spend`);
    await page.screenshot({path: path.join(output, name, 'trade-receipt.png')});

    await page.reload();
    await page.waitForFunction(() => window.Module && Module.crownlessScreen === 'title',
      undefined, {timeout: 120000});
    await page.locator('#canvas').focus();
    await page.keyboard.press('Enter');
    await page.waitForFunction(() => Module.crownlessScreen === 'playing',
      undefined, {timeout: 120000});
    await enterTrade(page);
    const restored = await gameControls(page, true).reading();
    assert.match(restored, /Purse 34\s*\|\s*Cargo 2\/12/,
      `${name}: reload purse and cargo total: ${restored}`);
    assert.match(restored, /Bread\s+78 units/,
      `${name}: reload retained remaining Bread stock: ${restored}`);
    assert.match(restored, /Carriage 2/,
      `${name}: reload retained two Bread in carriage: ${restored}`);
    const restoredBalance = Number(restored.match(/Purse (\d+)\s*\|\s*Cargo/)[1]);
    assert.equal(restoredBalance, receiptBalance, `${name}: reload retained charged balance`);
    await gameControls(page, true).button('Sell').tap();
    await page.waitForFunction(() => /Sale value: \d+ crowns/.test(Module.crownlessTouchFrame?.detail || ''),
      undefined, {timeout: 10000});
    const sellQuote = await page.evaluate(() => Module.crownlessTouchFrame.detail);
    const sellValue = Number(sellQuote.match(/Sale value: (\d+) crowns/)[1]);
    const sellName = `Sell 1 Bread. Sale value: ${sellValue} crowns.`;
    const sellControl = page.getByRole('button', {name: sellName, exact: true});
    await sellControl.focus();
    assert.equal(await page.evaluate(() => document.activeElement?.textContent), sellName,
      `${name}: a saved cargo load has a complete, focusable Sell offer`);
    assert.equal(await sellControl.getAttribute('aria-disabled'), 'false');
    await gameControls(page, true).button('Buy').tap();
    await page.waitForFunction(() => /Total cost: \d+ crowns/.test(Module.crownlessTouchFrame?.detail || ''),
      undefined, {timeout: 10000});
    await page.screenshot({path: path.join(output, name, 'trade-after-reload.png')});

    await gameControls(page, true).button('+').tap();
    await page.waitForFunction(() => Module.crownlessTouchFrame.reading.includes('Buy: 2 Bread') &&
      Module.crownlessTouchFrame.reading.includes('Total cost 10 crowns'),
    undefined, {timeout: 10000});
    const nextOffer = await gameControls(page, true).reading();
    assert.match(nextOffer, /Purse 34\s*\|\s*Cargo 2\/12/);
    assert.equal(await page.getByRole('button',
      {name: 'Buy 2 Bread. Total cost: 10 crowns.', exact: true}).count(), 1,
    `${name}: the reloaded Buy control uses the changed saved price`);
    await page.screenshot({path: path.join(output, name, 'trade-next-quote.png')});
    const secondRevision = await page.evaluate(() => Module.crownlessSaveRevision);
    await page.getByRole('button',
      {name: 'Buy 2 Bread. Total cost: 10 crowns.', exact: true}).press('Enter');
    await page.waitForFunction(() => Module.crownlessTouchFrame.reading.includes('Bought 2 Bread'),
      undefined, {timeout: 20000});
    const secondReceiptText = await gameControls(page, true).reading();
    assert.match(secondReceiptText, /Total cost 10 crowns/);
    assert.match(secondReceiptText, /Purse 24\s*\|\s*Cargo 4\/12/);
    const secondReceipt = secondReceiptText.match(/Bought 2 Bread\. -(\d+) crowns\. Purse: (\d+)\./);
    assert(secondReceipt, `${name}: repeated purchase receipt reports its price and balance: ${secondReceiptText}`);
    assert.equal(Number(secondReceipt[1]), 10, `${name}: repeated purchase charges the newly displayed quote`);
    assert.equal(Number(secondReceipt[2]), 24, `${name}: repeated purchase receipt matches purse`);
    await page.screenshot({path: path.join(output, name, 'trade-next-receipt.png')});
    await page.keyboard.press('F5');
    await page.waitForFunction(previous => Module.crownlessSaveRevision > previous,
      secondRevision, {timeout: 20000});
    await page.reload();
    await page.waitForFunction(() => window.Module && Module.crownlessScreen === 'title',
      undefined, {timeout: 120000});
    await page.locator('#canvas').focus();
    await page.keyboard.press('Enter');
    await page.waitForFunction(() => Module.crownlessScreen === 'playing',
      undefined, {timeout: 120000});
    await enterTrade(page);
    const secondReload = await gameControls(page, true).reading();
    assert.match(secondReload, /Purse 24\s*\|\s*Cargo 4\/12/);
    assert.match(secondReload, /Bread\s+76 units/);
    assert.match(secondReload, /Carriage 4/);
    await page.screenshot({path: path.join(output, name, 'trade-next-after-reload.png')});
    if (name === 'desktop-1040x620') {
      await page.locator('#touch-actions summary').focus();
      await page.keyboard.press('Tab');
      assert.equal(await page.evaluate(() =>
        document.querySelector('#touch-actions').contains(document.activeElement)), false,
      'Tab after the last action leaves the semantic action list');
      await page.locator('#touch-actions .touch-buttons button').first().focus();
      await page.keyboard.press('Shift+Tab');
      assert.equal(await page.evaluate(() =>
        document.querySelector('#touch-actions').contains(document.activeElement)), false,
      'Shift+Tab before the first action leaves the semantic action list');
      await page.locator('#canvas').focus();
      await page.keyboard.press('Tab');
      await page.waitForFunction(() => Module.crownlessTouchFrame?.title === 'Company Book',
        undefined, {timeout: 10000});
    }
    assert.deepEqual(errors, [], `${name}: browser runtime errors`);
    const result = {viewport: `${width}x${height}`, before: {crowns: 42, cargo: {}},
      quoteCrowns: quotedCost, chargedCrowns: charged,
      bought: {good: 'Bread', quantity: 2}, after: {crowns: 34, cargo: {Bread: 2}},
      reload: {crowns: 34, cargo: {Bread: 2}}, nextQuoteCrowns: 10,
      nextChargedCrowns: Number(secondReceipt[1]),
      nextReload: {crowns: 24, cargo: {Bread: 4}},
      savedRevision: await page.evaluate(() => Module.crownlessSaveRevision)};
    await fs.writeFile(path.join(output, name, 'result.json'), JSON.stringify(result, null, 2));
    await context.close();
    return result;
  }

  try {
    const results = [
      await runViewport('desktop-1040x620', 1040, 620),
      await runViewport('mobile-390x844', 390, 844)
    ];
    await fs.writeFile(path.join(output, 'results.json'), JSON.stringify(results, null, 2));
    console.log(JSON.stringify(results, null, 2));
  } finally {
    await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
