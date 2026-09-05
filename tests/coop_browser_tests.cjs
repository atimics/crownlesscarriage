const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const os = require('node:os');
const {spawn} = require('node:child_process');
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
    await owner.goto(origin);
    await owner.getByRole('textbox', {name:'Your name'}).fill('Mara');
    await owner.getByRole('textbox', {name:'World name'}).fill('Shared test road');
    await owner.getByRole('button', {name:'Ready the carriage'}).click();
    await owner.getByRole('button', {name:'Invite crew', exact:true}).click();
    await crew.goto(await owner.getByRole('textbox', {name:'Invitation link'}).inputValue());
    await crew.getByRole('textbox', {name:'Your name'}).fill('Bren');
    await crew.getByRole('button', {name:'Join the company'}).click();
    await owner.getByRole('button', {name:'Close', exact:true}).click();
    assert.equal(await crew.getByRole('button', {name:/Buy|Sell|Depart|Rest|Pause/}).count(), 0);
    const worldId = new URL(await crew.url()).hash.slice('#world='.length);
    const token = await crew.evaluate(() => localStorage.getItem('cc-coop-token'));
    async function state() {
      const response = await fetch(`${origin}/api/worlds/${worldId}/state?campaign=1`, {headers:{Authorization:`Bearer ${token}`}});
      assert(response.ok);
      return response.json();
    }
    const game = crew, errors = [];
    game.on('pageerror', error => errors.push(error.message));
    await owner.getByRole('link', {name:'Enter the world'}).click();
    await game.getByRole('link', {name:'Enter the world'}).click();
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
    assert.equal(await game.locator('#touch-panel').isVisible(), true);
    const mobileLayout = await game.evaluate(() => ({
      overflow: document.documentElement.scrollWidth > innerWidth,
      canvasBottom: document.querySelector('#canvas').getBoundingClientRect().bottom,
      controlsTop: document.querySelector('#touch-panel').getBoundingClientRect().top
    }));
    assert.equal(mobileLayout.overflow, false);
    assert(mobileLayout.canvasBottom <= mobileLayout.controlsTop, JSON.stringify(mobileLayout));
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
    await game.getByRole('link', {name:'Enter the world'}).waitFor();
    await owner.waitForFunction(() => JSON.parse(document.body.dataset.crewDrawn || '[]').length === 0);
    await game.getByRole('link', {name:'Enter the world'}).click();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready', undefined, {timeout:120000});
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
    assert.deepEqual(errors, []);
    console.log('Desktop and phone players draw each other with their chosen appearance and moving poses; touch walking, leaving, rejoining, reload, and shared travel pass.');
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
