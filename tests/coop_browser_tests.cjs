const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const os = require('node:os');
const {spawn, execFileSync} = require('node:child_process');
const {gameControls} = require('./game_controls.cjs');
const {chromium} = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');

async function main() {
  const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'crownless-coop-browser-'));
  const origin = 'http://127.0.0.1:8788';
  const host = spawn(process.env.CC_COOP_PYTHON || 'python3', [
    'tools/coop/server.py', '--library', path.resolve(process.argv[2]),
    '--database', path.join(temp, 'worlds.sqlite3'), '--port', '8788',
    '--game-dir', path.resolve(process.argv[3])
  ], {stdio: ['ignore', 'inherit', 'inherit']});
  let browser, owner, crew;
  try {
    for (let i = 0; ; i++) {
      try { if ((await fetch(origin + '/healthz')).ok) break; } catch {}
      assert(i < 100 && host.exitCode === null, 'Shared host must become ready');
      await new Promise(resolve => setTimeout(resolve, 100));
    }
    browser = await chromium.launch({args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader']});
    const a = await browser.newContext(), b = await browser.newContext({
      viewport: {width: 390, height: 844}, hasTouch: true, isMobile: true});
    owner = await a.newPage(); crew = await b.newPage();
    const ownerControls = gameControls(owner), crewControls = gameControls(crew, true);
    /* The host hands the invitation link to the share sheet. Headless Chromium has none, so record it. */
    await owner.addInitScript(() => {
      Object.defineProperty(navigator, 'share', {configurable: true, value: async data => { window.sharedInvitation = data.url; }});
    });
    await owner.goto(origin);
    assert.equal(await owner.getByRole('link').count(), 1);
    await owner.getByRole('link', {name:'Start', exact:true}).click();
    await owner.waitForFunction(() => window.Module?.crownlessScreen === 'title', undefined, {timeout:120000});
    await ownerControls.button('Online').click();
    await owner.waitForFunction(() => Module.crownlessTouchFrame.buttons.some(button => button.label === 'Online' && button.active));
    await ownerControls.button('Play').click();
    await owner.waitForFunction(() => Module.crownlessScreen === 'worlds');
    await ownerControls.button('Create world').click();
    await owner.getByRole('textbox', {name:'Your name', exact:true}).fill('Mara');
    await owner.getByRole('textbox', {name:'World name', exact:true}).fill('Shared test road');
    await owner.getByLabel('World pass', {exact:true}).fill('0'.repeat(64));
    await ownerControls.button('Create world').click();
    await owner.waitForFunction(() => Module.crownlessTouchFrame.detail.includes('fresh world pass'));
    const worldPass = execFileSync(process.env.CC_COOP_PYTHON || 'python3', [
      'tools/coop/server.py', '--database', path.join(temp, 'worlds.sqlite3'), '--issue-world-pass'
    ], {encoding:'utf8'}).trim();
    await owner.getByLabel('World pass', {exact:true}).fill(worldPass);
    await ownerControls.button('Create world').click();
    await owner.waitForFunction(() => Module.crownlessScreen === 'company');
    assert.equal(await owner.locator('input').count(), 0);
    await ownerControls.button('Invite crew').click();
    await owner.waitForFunction(() => typeof window.sharedInvitation === 'string');
    const invitation = await owner.evaluate(() => window.sharedInvitation);
    assert.equal(await owner.evaluate(() => Module.crownlessScreen), 'company');
    await owner.waitForFunction(() => Module.crownlessTouchFrame.detail.includes('Invitation shared'));
    const worldId = new URL(invitation).hash.slice('#join='.length).split('.')[0];
    assert.match(invitation, /^http:\/\/127\.0\.0\.1:8788\/#join=[a-f0-9]{32}\.[a-f0-9]{64}$/);
    await fs.mkdir('browser-results', {recursive:true});
    await owner.screenshot({path:'browser-results/in-game-invitation.png'});
    /* Without a share sheet or clipboard the link is still shown in full for hand copying. */
    await owner.evaluate(() => {
      Object.defineProperty(navigator, 'share', {configurable: true, value: undefined});
      Object.defineProperty(navigator, 'clipboard', {configurable: true, value: {writeText: async () => { throw new Error('blocked'); }}});
    });
    await ownerControls.button('Invite crew').click();
    await owner.waitForFunction(() => Module.crownlessScreen === 'invitation');
    assert.equal(await owner.getByRole('textbox', {name:'Invitation', exact:true}).inputValue(), invitation);
    await owner.screenshot({path:'browser-results/in-game-invitation-link.png'});
    await crew.goto(invitation);
    await crew.getByRole('link', {name:'Start', exact:true}).click();
    await crew.waitForFunction(() => window.Module?.crownlessScreen === 'join', undefined, {timeout:120000});
    await crew.getByRole('textbox', {name:'Your name', exact:true}).fill('Bren');
    await crewControls.button('Join company').tap();
    await crew.waitForFunction(() => Module.crownlessScreen === 'company');
    await ownerControls.button('Back').click();
    await ownerControls.button('Refresh crew').click();
    await owner.waitForFunction(() => Module.crownlessTouchFrame.buttons.some(button => button.label.includes('Bren')));
    await owner.screenshot({path:'browser-results/in-game-company.png'});
    const token = await crew.evaluate(() => localStorage.getItem('cc-coop-token'));
    async function state() {
      const response = await fetch(`${origin}/api/worlds/${worldId}/state?campaign=1`, {headers:{Authorization:`Bearer ${token}`}});
      assert(response.ok, `Shared state returned ${response.status}`);
      return response.json();
    }
    const game = crew, errors = [];
    game.on('pageerror', error => errors.push(error.message));
    await ownerControls.button('Enter world').click();
    await crewControls.button('Enter world').tap();
    for (const page of [owner, game]) {
      await page.waitForFunction(() => document.body.dataset.companyReady === 'ready' && document.body.dataset.playerPosition, undefined, {timeout:120000});
    }
    async function selectMenuItem(page, index) {
      await page.locator('#canvas').focus();
      for (let step = 0; await page.evaluate(() => Module.crownlessMenuFocus) !== index; ++step) {
        assert(step < 16, 'Menu item must be reachable');
        const previous = await page.evaluate(() => Module.crownlessMenuFocus);
        await page.keyboard.press('ArrowDown');
        await page.waitForFunction(value => Module.crownlessMenuFocus !== value, previous);
      }
      const draft = await page.evaluate(() => Module.crownlessScreen === 'avatar' ? Module.crownlessAvatarDraft : null);
      await page.keyboard.press('Enter');
      if (draft !== null && index < 5) await page.waitForFunction(value => Module.crownlessAvatarDraft !== value, draft);
    }
    await game.locator('#canvas').focus();
    await game.keyboard.press('Escape');
    await game.waitForFunction(() => Module.crownlessScreen === 'paused');
    await selectMenuItem(game, 9);
    await game.waitForFunction(() => Module.crownlessScreen === 'avatar');
    await selectMenuItem(game, 4);
    await selectMenuItem(game, 4);
    await selectMenuItem(game, 4);
    await selectMenuItem(game, 1);
    await selectMenuItem(game, 1);
    await selectMenuItem(game, 1);
    await selectMenuItem(game, 1);
    await selectMenuItem(game, 2);
    await selectMenuItem(game, 2);
    await selectMenuItem(game, 2);
    await selectMenuItem(game, 2);
    await selectMenuItem(game, 5);
    await game.waitForFunction(() => Module.crownlessScreen === 'paused');
    assert.deepEqual((await state()).appearance, {skin:0, hair:4, style:4, face:0, coat:3});
    await selectMenuItem(game, 0);
    await game.waitForFunction(() => Module.crownlessScreen === 'playing');
    assert.equal(await game.locator('body').getAttribute('data-avatar'), '12576');
    assert.equal(await game.locator('#touch-panel').count(), 0);
    assert(await game.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
    assert.equal(await owner.locator('body').getAttribute('data-avatar'), '0');
    for (const [page, name, appearance] of [[owner, 'Bren', 12576], [game, 'Mara', 0]]) {
      await page.waitForFunction(({name, appearance}) => {
        const drawn = JSON.parse(document.body.dataset.crewDrawn || '[]');
        return drawn.length === 1 && drawn[0].name === name && drawn[0].appearance === appearance;
      }, {name, appearance}, {timeout:30000});
    }
    await fs.mkdir('browser-results', {recursive:true});
    await owner.screenshot({path:'browser-results/visible-crew.png'});
    const remoteStart = JSON.parse(await owner.locator('body').getAttribute('data-crew-drawn'))[0].position;
    const purchased = await game.evaluate(() => Module.ccCoop.apply('trade', '0', 0, 1));
    assert.equal(purchased.accepted, true);
    assert.equal(purchased.world.state.company.cargo[0], 1);
    const atTown = await state();
    for (const page of [owner, game]) await page.waitForFunction(hash => document.body.dataset.companyHash === hash, atTown.state.hash);
    const start = await game.locator('body').getAttribute('data-player-position');
    const canvas = await game.locator('#canvas').boundingBox();
    // The road crosses the middle of this view; the lower edge is a cliff.
    await game.touchscreen.tap(canvas.x + canvas.width * 0.68, canvas.y + canvas.height * 0.50);
    await game.waitForFunction(position => document.body.dataset.playerPosition !== position, start);
    await owner.waitForFunction(position => {
      const peer = JSON.parse(document.body.dataset.crewDrawn || '[]')[0];
      return peer && Math.hypot(peer.position[0] - position[0], peer.position[2] - position[2]) > 0.1;
    }, remoteStart);
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready' && document.body.dataset.playerPosition, undefined, {timeout:120000});
    assert.notEqual(await game.locator('body').getAttribute('data-player-position'), start);
    assert.equal(await game.locator('body').getAttribute('data-avatar'), '12576');
    await owner.waitForFunction(() => JSON.parse(document.body.dataset.crewDrawn || '[]')[0]?.name === 'Bren');
    await game.locator('#canvas').focus();
    await game.keyboard.press('Escape');
    await game.waitForFunction(() => Module.crownlessScreen === 'paused');
    await selectMenuItem(game, 11);
    await game.waitForFunction(() => Module.crownlessScreen === 'company');
    assert(await crewControls.button('Pause shared world').isDisabled());
    await crewControls.button('Return to the road').tap();
    await game.waitForFunction(() => Module.crownlessScreen === 'playing');
    await owner.waitForFunction(() => JSON.parse(document.body.dataset.crewDrawn || '[]')[0]?.name === 'Bren');
    const destination = (await state()).state.travel[0].id;
    const departed = await game.evaluate(target => Module.ccCoop.apply('travel', target, 0, 0), destination);
    assert.equal(departed.accepted, true);
    await owner.locator('#canvas').focus();
    await owner.keyboard.press('Escape');
    await owner.waitForFunction(() => Module.crownlessScreen === 'paused');
    await selectMenuItem(owner, 12);
    await owner.waitForFunction(() => Module.ccCoop.paused());
    const travelling = await state();
    assert.equal(travelling.state.journey.active, true);
    for (const page of [owner, game]) await page.waitForFunction(hash => document.body.dataset.companyHash === hash, travelling.state.hash);
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready', undefined, {timeout:120000});
    await game.waitForFunction(hash => document.body.dataset.companyHash === hash, travelling.state.hash);
    await owner.evaluate(() => Module.ccCoop.togglePause());
    for (const page of [owner, game]) await page.waitForFunction(() => !Module.ccCoop.paused());
    const firstStop = await owner.evaluate(() => Module.ccCoop.apply('skip_watch', '0', 0, 0));
    assert.equal(firstStop.accepted, true);
    assert(firstStop.world.state.journey.road_site, 'A nearby stop is offered');
    const firstJourney = firstStop.world.state.journey;
    // Leaving the option alone keeps the shared carriage moving.
    await game.waitForFunction(progress => {
      const reading = Module.crownlessTouchFrame?.reading || '';
      return !reading.includes('Press on') && !reading.includes('Fast forward') &&
        document.body.dataset.companyHash;
    }, firstJourney.progress);
    await game.waitForTimeout(1800);
    const moving = (await state()).state.journey;
    assert(moving.progress > firstJourney.progress || !moving.active, JSON.stringify({firstJourney,moving}));
    assert((await crewControls.buttons()).every(button => !/Press on|Fast forward|Continue on the road/.test(button.label)));
    // Pause for a stable reload check; the company keeps the same route and pace.
    await owner.evaluate(() => Module.ccCoop.togglePause());
    for (const page of [owner, game]) await page.waitForFunction(() => Module.ccCoop.paused());
    const savedRoad = await state();
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready', undefined, {timeout:120000});
    await game.waitForFunction(hash => document.body.dataset.companyHash === hash, savedRoad.state.hash);
    await owner.evaluate(() => Module.ccCoop.togglePause());
    for (const page of [owner, game]) await page.waitForFunction(() => !Module.ccCoop.paused());
    // The C client reports each death and resets both players after one jump.
    await owner.evaluate(() => Module.ccCoop.togglePause());
    for (const page of [owner, game]) await page.waitForFunction(() => Module.ccCoop.paused());
    const beforeDeath = await state();
    await owner.evaluate(() => Module.ccCoop.life(true));
    await owner.waitForFunction(() => Module.ccCoop.dead());
    await game.waitForTimeout(1000);
    assert.equal((await state()).state.day, beforeDeath.state.day);
    await game.evaluate(() => Module.ccCoop.life(true));
    await owner.evaluate(() => Module.ccCoop.togglePause());
    for (const page of [owner, game]) await page.waitForFunction(() => !Module.ccCoop.paused());
    for (const page of [owner, game]) {
      await page.waitForFunction(() => Module.ccCoop.partyWipes() === 1 &&
        !Module.ccCoop.dead(), undefined, {timeout:30000});
    }
    const nextCompany = await state();
    assert.equal(nextCompany.state.day, beforeDeath.state.day + 7300);
    assert.equal(nextCompany.state.journey.active, false);
    for (const page of [owner, game]) {
      await page.waitForFunction(hash => document.body.dataset.companyHash === hash,
        nextCompany.state.hash);
    }
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready' &&
      Module.ccCoop.partyWipes() === 1 && !Module.ccCoop.dead(), undefined, {timeout:120000});
    assert.equal((await state()).state.day, nextCompany.state.day);
    assert.deepEqual(errors, []);
    console.log('Desktop and phone players draw each other with their chosen appearance and moving poses; touch walking, leaving, rejoining, reload, continuous shared travel, optional stops, and the twenty-year party death jump pass.');
  } catch (error) {
    await fs.mkdir('browser-results', {recursive:true});
    for (const [name, page] of [['owner', owner], ['crew', crew]]) {
      if (page && !page.isClosed()) {
        await page.screenshot({path:`browser-results/crew-failure-${name}.png`}).catch(() => {});
        console.error(name, await page.locator('body').getAttribute('data-crew-drawn'));
      }
    }
    throw error;
  } finally {
    if (browser) await browser.close();
    host.kill('SIGTERM');
    if (host.exitCode === null) await new Promise(resolve => host.once('exit', resolve));
    await fs.rm(temp, {recursive:true, force:true});
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
