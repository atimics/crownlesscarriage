const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const {spawn, execFileSync} = require('node:child_process');
const {chromium} = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');
const {gameControls} = require('./game_controls.cjs');

async function main() {
  const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'crownless-care-browser-'));
  const database = path.join(temp, 'worlds.sqlite3');
  const port = 20000 + process.pid % 20000;
  const origin = `http://127.0.0.1:${port}`;
  const token = 'a'.repeat(64);
  const worldId = '1'.repeat(32);
  const host = spawn(process.env.CC_COOP_PYTHON || 'python3', [
    'tools/coop/server.py', '--library', path.resolve(process.argv[2]),
    '--database', database, '--port', String(port), '--game-dir', path.resolve(process.argv[3])
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
    const worldPass = execFileSync(process.env.CC_COOP_PYTHON || 'python3', [
      'tools/coop/server.py', '--database', database, '--issue-world-pass'
    ], {encoding: 'utf8'}).trim();
    await api('/api/worlds', {id: worldId, name: 'Care test road', player: 'Mara',
      seed: 0xc0a71a9e, world_pass: worldPass});
    async function view() {
      return api(`/api/worlds/${worldId}/state?campaign=1`);
    }
    async function command(action, target = '0') {
      const current = await view();
      return api(`/api/worlds/${worldId}/command`, {protocol: 1,
        sequence: current.next_sequence, action_revision: current.action_revision,
        action, target, good: 0, amount: 0});
    }
    const departure = await view();
    const route = departure.state.travel.find(option => option.available);
    assert(route, 'An open road reaches the staffed stable');
    assert((await command('travel', route.id)).accepted);
    for (let step = 0; step < 50; step++) {
      const current = (await view()).state;
      const journey = current.journey;
      if (!journey.active) break;
      let action, target = '0';
      if (journey.road_site) {
        action = 'pass_road_site'; target = journey.road_site.id;
      } else if (journey.phase === 4) {
        const position = current.road_position;
        const forward = position.next_legs.filter(leg => leg.direction === position.direction);
        assert(forward.length, 'A forward road leg exists');
        const leg = forward.find(leg => leg.kind === 1) || forward[0];
        action = 'road_leg'; target = leg.token;
      } else if (journey.phase === 3) {
        action = journey.stop === 1 ? 'break' : 'camp';
      } else {
        assert.equal(journey.phase, 1);
        action = 'skip_watch';
      }
      const result = await command(action, target);
      assert(result.accepted, `${action}: ${result.message}`);
    }
    const arrived = (await view()).state;
    assert.equal(arrived.journey.active, false);
    assert.equal(arrived.company.location, route.id);
    assert(arrived.horse_care.available);
    assert.equal(arrived.horse_care.source, 'stable market');
    browser = await chromium.launch({args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader']});
    const context = await browser.newContext({viewport: {width: 390, height: 844}, hasTouch: true, isMobile: true});
    await context.addInitScript(value => localStorage.setItem('cc-coop-token', value), token);
    const page = await context.newPage();
    const controls = gameControls(page, true);
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(`${origin}/game/index.html?world=${worldId}`);
    await page.waitForFunction(() => document.body.dataset.companyReady === 'ready', undefined, {timeout: 120000});
    await page.waitForFunction(() => Module.crownlessTouchFrame?.buttons?.length > 0, undefined, {timeout: 30000});
    await controls.button(/Care for horses:/).waitFor();
    const careButton = await controls.button(/Care for horses:/).read();
    assert(careButton.enabled, 'A parked team can use the stable');
    assert(careButton.label.includes(`${arrived.horse_care.cost} crowns`));
    assert(careButton.label.includes('1 Wheat from market / 1 day'));
    const semanticCare = page.locator('#touch-actions button').filter({hasText: 'Care for horses:'});
    assert.equal(await semanticCare.count(), 1, 'The care offer is in the browser touch controls');
    assert.equal(await semanticCare.textContent(), careButton.label);
    assert.equal(await page.evaluate(() => Module.crownlessScreen), 'playing');
    const output = process.argv[4];
    if (output) {
      await fs.mkdir(output, {recursive: true});
      await page.screenshot({path: path.join(output, 'stable-care-offer.png')});
    }
    await controls.button(/Care for horses:/).tap();
    await page.waitForFunction(hash => document.body.dataset.companyHash !== hash,
      arrived.hash, {timeout: 30000});
    const cared = (await view()).state;
    assert.equal(cared.day, arrived.day + 1);
    assert.equal(cared.company.coins, arrived.company.coins - arrived.horse_care.cost);
    assert(cared.events.some(event => event.text.includes('Care at ') &&
      event.text.includes('1 Wheat from the stable market') &&
      event.text.includes('1 day.')));
    await page.waitForFunction(hash => document.body.dataset.companyHash === hash,
      cared.hash, {timeout: 30000});
    await page.waitForFunction(() => Module.crownlessTouchFrame.buttons.some(button =>
      button.label.includes('already healthy and rested') && !button.enabled));
    assert.equal(await page.locator('#touch-actions button').filter({hasText: 'already healthy and rested'}).count(), 1);
    if (output) await page.screenshot({path: path.join(output, 'stable-care-receipt.png')});
    await page.reload();
    await page.waitForFunction(() => document.body.dataset.companyReady === 'ready', undefined, {timeout: 120000});
    await page.waitForFunction(hash => document.body.dataset.companyHash === hash,
      cared.hash, {timeout: 30000});
    await page.waitForFunction(() => Module.crownlessTouchFrame?.buttons?.some(button =>
      button.label.includes('already healthy and rested') && !button.enabled));
    assert.equal(await page.locator('#touch-actions button').filter({hasText: 'already healthy and rested'}).count(), 1);
    assert.equal((await view()).state.hash, cared.hash, 'The touch action survives browser reload');
    assert.deepEqual(errors, []);
    console.log(JSON.stringify({source: arrived.horse_care.source,
      cost: arrived.horse_care.cost, days: cared.day - arrived.day,
      touchOffer: careButton.label, receipt: cared.events.find(event =>
        event.text.includes('Care at ')).text, savedHash: cared.hash}));
  } finally {
    if (browser) await browser.close();
    host.kill();
    await fs.rm(temp, {recursive: true, force: true});
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
