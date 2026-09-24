const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const {spawn, execFileSync} = require('node:child_process');
const {chromium} = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');
const {gameControls} = require('./game_controls.cjs');

async function main() {
  const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'crownless-road-block-browser-'));
  const database = path.join(temp, 'worlds.sqlite3');
  const port = 20000 + process.pid % 20000;
  const origin = `http://127.0.0.1:${port}`;
  const token = 'b'.repeat(64);
  const library = path.resolve(process.argv[2]);
  const site = path.resolve(process.argv[3]);
  const fixtureTool = path.resolve(process.argv[4]);
  const output = path.resolve(process.argv[5] || path.join(temp, 'screenshots'));
  const python = process.env.CC_COOP_PYTHON || 'python3';
  const host = spawn(python, [
    'tools/coop/server.py', '--library', library, '--database', database,
    '--port', String(port), '--game-dir', site
  ], {stdio: ['ignore', 'inherit', 'inherit']});
  let browser;
  try {
    for (let i = 0; ; i++) {
      try { if ((await fetch(`${origin}/healthz`)).ok) break; } catch {}
      assert(i < 100 && host.exitCode === null, 'The shared host starts');
      await new Promise(resolve => setTimeout(resolve, 100));
    }
    let lastRequest = 0;
    async function api(route, body) {
      const pause = Math.max(0, 60 - (Date.now() - lastRequest));
      if (pause) await new Promise(resolve => setTimeout(resolve, pause));
      lastRequest = Date.now();
      const response = await fetch(`${origin}${route}`, {
        method: body ? 'POST' : 'GET',
        headers: {Authorization: `Bearer ${token}`, 'Content-Type': 'application/json'},
        body: body ? JSON.stringify(body) : undefined
      });
      const data = await response.json();
      assert(response.ok, `${route}: ${response.status} ${JSON.stringify(data)}`);
      return data;
    }
    const worldIds = {warning: '3'.repeat(32), blocked: '4'.repeat(32)};
    for (const [mode, worldId] of Object.entries(worldIds)) {
      const worldPass = execFileSync(python, [
        'tools/coop/server.py', '--database', database, '--issue-world-pass'
      ], {encoding: 'utf8'}).trim();
      await api('/api/worlds', {id: worldId, name: `Road ${mode} test`,
        player: 'Mara', seed: 0x5ca17, world_pass: worldPass});
      const fixture = path.join(temp, `${mode}.bin`);
      execFileSync(fixtureTool, [mode, fixture]);
      execFileSync(python, [
        'tests/road_block_browser_seed.py', library, database, worldId, fixture
      ]);
    }
    const warning = (await api(`/api/worlds/${worldIds.warning}/state?campaign=1`)).state;
    const blocked = (await api(`/api/worlds/${worldIds.blocked}/state?campaign=1`)).state;
    const warningEvent = warning.events.find(event =>
      event.text.includes('Scouts spot riders from '));
    assert(warningEvent && warningEvent.text.includes('The Ditch Parliament'));
    assert.equal(blocked.journey.phase, 2);
    browser = await chromium.launch({args: [
      '--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'
    ]});
    const context = await browser.newContext({viewport: {width: 390, height: 844},
      hasTouch: true, isMobile: true});
    await context.addInitScript(value => localStorage.setItem('cc-coop-token', value), token);
    const page = await context.newPage();
    const controls = gameControls(page, true);
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(`${origin}/game/index.html?world=${worldIds.warning}`);
    await page.waitForFunction(() => document.body.dataset.companyReady === 'ready',
      undefined, {timeout: 120000});
    await page.waitForFunction(() => Module.crownlessTouchFrame?.detail?.includes(
      'The Ditch Parliament'), undefined, {timeout: 30000});
    const warningDetail = await page.evaluate(() => Module.crownlessTouchFrame.detail);
    assert(warningDetail.includes('Scouts:'));
    assert(warningDetail.includes('likely'));
    assert(warningDetail.includes('crowns'));
    assert(warningDetail.includes('Careful pace may bypass'));
    await fs.mkdir(output, {recursive: true});
    await page.screenshot({path: path.join(output, 'road-warning.png')});

    await page.goto(`${origin}/game/index.html?world=${worldIds.blocked}`);
    await page.waitForFunction(() => document.body.dataset.companyReady === 'ready',
      undefined, {timeout: 120000});
    await page.waitForFunction(() => Module.crownlessTouchFrame?.buttons?.length > 0,
      undefined, {timeout: 30000});
    await controls.button('Return to carriage').tap();
    await controls.button(/Withdraw to .*: 0 crowns/).waitFor();
    const retreat = await controls.button(/Withdraw to .*: 0 crowns/).read();
    assert(retreat.enabled);
    assert(retreat.label.includes('0 crowns'));
    const blockReading = await page.evaluate(() => Module.crownlessTouchFrame.reading);
    const blockDetail = await page.evaluate(() => Module.crownlessTouchFrame.detail);
    assert(blockDetail.includes('Demand: 2 Bread or 8 crowns'));
    assert(blockReading.includes('The Ditch Parliament'));
    assert(blockReading.includes('DEMAND'));
    assert(blockReading.includes('CURRENT TIME'));
    await page.screenshot({path: path.join(output, 'road-block-choice.png')});
    await controls.button(/Withdraw to .*: 0 crowns/).tap();
    await page.waitForFunction(hash => document.body.dataset.companyHash !== hash,
      blocked.hash, {timeout: 30000});
    const returned = (await api(`/api/worlds/${worldIds.blocked}/state?campaign=1`)).state;
    assert.equal(returned.journey.active, false);
    assert.equal(returned.company.location, blocked.journey.origin);
    assert.equal(returned.company.coins, blocked.company.coins);
    assert.deepEqual(returned.company.cargo, blocked.company.cargo);
    assert.equal(returned.day, blocked.day);
    assert(returned.events.some(event => event.text.includes('refuses the fight and returns')));
    await page.waitForFunction(hash => document.body.dataset.companyHash === hash,
      returned.hash, {timeout: 30000});
    await controls.button(/^Book(?: B)?$/).tap();
    await page.waitForFunction(() => Module.crownlessTouchFrame?.reading?.includes(
      'refuses the fight and returns'), undefined, {timeout: 30000});
    await page.locator('#touch-actions').evaluate(element => {
      element.scrollTop = element.scrollHeight;
    });
    await page.screenshot({path: path.join(output, 'road-return-receipt.png')});
    await page.reload();
    await page.waitForFunction(() => document.body.dataset.companyReady === 'ready',
      undefined, {timeout: 120000});
    await page.waitForFunction(hash => document.body.dataset.companyHash === hash,
      returned.hash, {timeout: 30000});
    assert.equal((await api(`/api/worlds/${worldIds.blocked}/state?campaign=1`)).state.hash,
      returned.hash);
    await page.waitForFunction(() => Module.crownlessTouchFrame?.buttons?.length > 0,
      undefined, {timeout: 30000});
    await controls.button(/^Book(?: B)?$/).tap();
    await page.waitForFunction(() => Module.crownlessTouchFrame?.reading?.includes(
      'refuses the fight and returns'), undefined, {timeout: 30000});
    const returnState = await api(`/api/worlds/${worldIds.blocked}/state?campaign=1`);
    const revisit = returnState.state.travel.find(route =>
      route.id === blocked.journey.destination && route.available);
    assert(revisit, 'The company can plan a second crossing of the saved road');
    const secondDeparture = await api(`/api/worlds/${worldIds.blocked}/command`, {
      protocol: 1, sequence: returnState.next_sequence,
      action_revision: returnState.action_revision,
      action: 'travel', target: revisit.id, good: 0, amount: 0
    });
    assert(secondDeparture.accepted);
    assert.equal(secondDeparture.world.state.journey.route, blocked.journey.route);
    assert.equal(secondDeparture.world.state.journey.active, true);
    assert.deepEqual(errors, []);
    console.log(JSON.stringify({claimant: 'The Ditch Parliament',
      warning: warningEvent.text, retreat: retreat.label,
      returnReceipt: returned.events.find(event =>
        event.text.includes('refuses the fight and returns')).text,
      savedHash: returned.hash}));
  } finally {
    if (browser) await browser.close();
    host.kill();
    await fs.rm(temp, {recursive: true, force: true});
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
