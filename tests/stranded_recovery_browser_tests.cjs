const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const {spawn, execFileSync} = require('node:child_process');
const {chromium} = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');
const {gameControls} = require('./game_controls.cjs');

async function main() {
  const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'crownless-stranded-browser-'));
  const database = path.join(temp, 'worlds.sqlite3');
  const port = 20000 + process.pid % 20000;
  const origin = `http://127.0.0.1:${port}`;
  const token = 'd'.repeat(64);
  const worlds = {recover: '5'.repeat(32), foaling: '6'.repeat(32), tap: '7'.repeat(32)};
  const library = path.resolve(process.argv[2]);
  const site = path.resolve(process.argv[3]);
  const fixtureTool = path.resolve(process.argv[4]);
  const output = process.argv[5] && path.resolve(process.argv[5]);
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
    async function api(route, body, immediate = false) {
      const pause = immediate ? 0 :
        Math.max(0, 180 - (Date.now() - lastRequest));
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
    async function view(world) {
      return api(`/api/worlds/${world}/state`);
    }
    async function command(world, action, target = '0') {
      const current = await view(world);
      return api(`/api/worlds/${world}/command`, {protocol: 1,
        sequence: current.next_sequence, action_revision: current.action_revision,
        action, target, good: 0, amount: 0});
    }
    async function holdMovingRoad(receipt) {
      const current = receipt.world;
      const journey = current.state.journey;
      if (!journey.active || ![1, 3].includes(journey.phase)) return;
      const hold = await api(`/api/worlds/${worlds.recover}/command`, {
        protocol: 1, sequence: current.next_sequence,
        action_revision: current.action_revision, action: 'stop_travel',
        target: journey.route, good: 0, amount: 0
      }, true);
      assert(hold.accepted, `The host holds the road: ${hold.message}`);
    }
    for (const [mode, world] of Object.entries(worlds)) {
      const worldPass = execFileSync(python, [
        'tools/coop/server.py', '--database', database, '--issue-world-pass'
      ], {encoding: 'utf8'}).trim();
      await api('/api/worlds', {id: world, name: `Synthetic ${mode} road`,
        player: 'Mara', seed: 0xc0a71a9e, world_pass: worldPass});
      const fixture = path.join(temp, `${mode}.bin`);
      execFileSync(fixtureTool, [mode === 'tap' ? 'recover' : mode, fixture]);
      execFileSync(python, [
        'tests/road_block_browser_seed.py', library, database, world, fixture
      ]);
    }

    const unavailable = (await view(worlds.foaling)).state;
    const foalingReason = 'A mare near foaling must remain at the stable.';
    assert.equal(unavailable.travel.length, 1);
    assert.equal(unavailable.travel[0].available, false);
    assert.equal(unavailable.travel[0].reason, foalingReason);
    const rejected = await command(worlds.foaling, 'travel', unavailable.travel[0].id);
    assert.equal(rejected.accepted, false);
    assert.equal(rejected.message, foalingReason);
    assert.equal((await view(worlds.foaling)).state.hash, unavailable.hash);

    const start = (await view(worlds.recover)).state;
    assert.equal(start.market.abandoned, true);
    assert.equal(start.market.residents, 0);
    assert.equal(start.market.visitors, 0);
    assert.equal(start.market.services.length, 0);
    assert.equal(start.market.civilian_food_rations, 0);
    assert.equal(start.market.animal_feed_rations, 0);
    assert.equal(start.company.location, start.team.carriage_location);
    assert.equal(start.company.cargo[6], 2, 'Wood remains in company custody');
    assert.equal(start.horse_care.available, false);
    assert(start.horse_care.reason.includes('no staffed stable'));
    assert.equal(start.travel.length, 1);
    assert(start.travel[0].available);

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
    async function loadWorld(world) {
      await page.goto(`${origin}/game/index.html?world=${world}`);
      await page.waitForFunction(() => document.body.dataset.companyReady === 'ready',
        undefined, {timeout: 120000});
      await page.waitForFunction(() => Module.crownlessTouchFrame?.buttons?.length > 0,
        undefined, {timeout: 30000});
    }
    await loadWorld(worlds.foaling);
    const blockedRoad = await controls.button(/^Choose a road/).read();
    assert(blockedRoad && !blockedRoad.enabled);
    assert(blockedRoad.label.includes(foalingReason));
    assert(await page.locator('#touch-actions button')
      .filter({hasText: foalingReason}).isDisabled());
    if (output) {
      await fs.mkdir(output, {recursive: true});
      await page.screenshot({path: path.join(output, 'foaling-road-choice.png')});
    }
    await loadWorld(worlds.tap);
    const passableRoad = await controls.button(/^Choose a road/).read();
    assert(passableRoad && passableRoad.enabled);
    await controls.button(/^Choose a road/).tap();
    await page.waitForFunction(() => Module.crownlessTouchFrame?.scene === 'road' &&
      Module.crownlessTouchFrame.buttons.some(button =>
        button.label.startsWith('Drive to ')), undefined, {timeout: 45000});
    const roadChoices = (await controls.buttons()).filter(button =>
      button.label.startsWith('Drive to '));
    assert.equal(roadChoices.length, 2);
    assert(roadChoices.some(button => button.label.includes(unavailable.market.name)));
    if (output) await page.screenshot({path: path.join(output, 'abandoned-road-choice.png')});
    await loadWorld(worlds.recover);
    const originButtons = await controls.buttons();
    assert(originButtons.some(button =>
      button.label.includes('town is abandoned') && !button.enabled));
    assert(originButtons.some(button => button.label.startsWith('Choose a road') &&
      button.enabled));
    assert(!originButtons.some(button => button.label.startsWith('Talk ')));
    if (output) {
      await fs.mkdir(output, {recursive: true});
      await page.screenshot({path: path.join(output, 'abandoned-town-care.png')});
    }
    await page.goto(`${origin}/healthz`);

    async function driveTo(destination) {
      const before = (await view(worlds.recover)).state;
      const route = before.travel.find(option => option.id === destination);
      assert(route && route.available, 'The next saved road is passable');
      const started = await command(worlds.recover, 'travel', destination);
      assert(started.accepted);
      await holdMovingRoad(started);
      for (let step = 0; step < 100; step++) {
        const current = (await view(worlds.recover)).state;
        const journey = current.journey;
        if (!journey.active) break;
        let action, target = '0';
        if (journey.road_site) {
          action = 'pass_road_site'; target = journey.road_site.id;
        } else if (journey.phase === 4) {
          const position = current.road_position;
          const forward = position.next_legs.filter(leg =>
            leg.direction === position.direction);
          assert(forward.length, 'A forward road leg exists');
          const leg = forward.find(option => option.kind === 1) || forward[0];
          action = 'road_leg'; target = leg.token;
        } else if (journey.phase === 3) {
          action = journey.stop === 1 ? 'break' : 'camp';
        } else {
          assert.equal(journey.phase, 1);
          action = 'skip_watch';
        }
        const result = await command(worlds.recover, action, target);
        assert(result.accepted, `${action}: ${result.message}`);
        await holdMovingRoad(result);
      }
      const arrived = (await view(worlds.recover)).state;
      assert.equal(arrived.journey.active, false);
      assert.equal(arrived.company.location, destination);
      assert.equal(arrived.team.carriage_location, destination);
      assert.equal(arrived.company.cargo[6], 2);
      assert.equal(arrived.company.coins, before.company.coins);
      assert(arrived.day > before.day || arrived.minute > before.minute);
      return arrived;
    }
    const middle = await driveTo(start.travel[0].id);
    assert.equal(middle.horse_care.available, false);
    assert(middle.horse_care.reason.includes('no staffed stable'));
    const onward = middle.travel.find(option =>
      option.id !== start.company.location && option.available);
    assert(onward, 'The second saved road reaches a staffed town');
    const arrived = await driveTo(onward.id);
    assert(arrived.market.residents > 0);
    assert(arrived.market.services.includes('Stable'));
    assert(arrived.market.stock[7] >= 1);
    assert(arrived.horse_care.available);
    assert.equal(arrived.horse_care.source, 'stable market');
    await loadWorld(worlds.recover);
    await page.waitForFunction(hash => document.body.dataset.companyHash === hash,
      arrived.hash, {timeout: 120000});
    await controls.button(/Care for horses:/).waitFor();
    const care = await controls.button(/Care for horses:/).read();
    assert(care.enabled);
    assert(care.label.includes(`${arrived.horse_care.cost} crowns`));
    assert(care.label.includes('1 Wheat from market / 1 day'));
    if (output) await page.screenshot({path: path.join(output, 'staffed-stable-care.png')});
    await controls.button(/Care for horses:/).tap();
    await page.waitForFunction(hash => document.body.dataset.companyHash !== hash,
      arrived.hash, {timeout: 30000});
    const cared = (await view(worlds.recover)).state;
    assert.equal(cared.day, arrived.day + 1);
    assert.equal(cared.company.coins,
      arrived.company.coins - arrived.horse_care.cost);
    assert.equal(cared.company.cargo[6], 2);
    assert.equal(cared.company.location, onward.id);
    assert.equal(cared.team.carriage_location, onward.id);
    assert(cared.team.horses[0].health > arrived.team.horses[0].health);
    assert(cared.team.horses[0].hunger <= arrived.team.horses[0].hunger);
    assert(cared.events.some(event => event.text.includes('Care at ') &&
      event.text.includes('1 Wheat from the stable market') &&
      event.text.includes('1 day.')));
    await page.reload();
    await page.waitForFunction(hash => document.body.dataset.companyHash === hash,
      cared.hash, {timeout: 120000});
    assert.equal((await view(worlds.recover)).state.hash, cared.hash);
    assert.deepEqual(errors, []);
    console.log(JSON.stringify({start: start.market.name, middle: middle.market.name,
      stable: arrived.market.name, blockedReason: foalingReason,
      care: care.label, days: cared.day - arrived.day,
      coins: [arrived.company.coins, cared.company.coins],
      horseHealth: [arrived.team.horses[0].health, cared.team.horses[0].health],
      horseHunger: [arrived.team.horses[0].hunger, cared.team.horses[0].hunger],
      marketWheat: [arrived.market.stock[7], cared.market.stock[7]],
      receipt: cared.events.find(event => event.text.includes('Care at ')).text,
      savedHash: cared.hash}));
  } finally {
    if (browser) await browser.close();
    host.kill();
    await fs.rm(temp, {recursive: true, force: true});
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
