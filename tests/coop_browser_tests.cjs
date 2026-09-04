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
  let browser;
  try {
    for (let i = 0; ; i++) {
      try { if ((await fetch(origin + '/healthz')).ok) break; } catch {}
      assert(i < 100 && host.exitCode === null, 'Shared host must become ready');
      await new Promise(resolve => setTimeout(resolve, 100));
    }
    browser = await chromium.launch({args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader']});
    const a = await browser.newContext(), b = await browser.newContext();
    const owner = await a.newPage(), crew = await b.newPage();
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
    await crew.getByRole('button', {name:'Ocean coat', exact:true}).click();
    await crew.getByRole('button', {name:'Silver hair', exact:true}).click();
    await crew.getByRole('combobox', {name:'Hair style', exact:true}).selectOption('4');
    await crew.getByRole('button', {name:'Save appearance', exact:true}).click();
    await crew.waitForFunction(() => document.querySelector('#appearance-status').textContent === 'Saved');
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
    assert.equal(await game.locator('body').getAttribute('data-avatar'), '12576');
    assert.equal(await owner.locator('body').getAttribute('data-avatar'), '0');
    const purchased = await game.evaluate(() => Module.ccCoop.apply('trade', '0', 0, 1));
    assert.equal(purchased.accepted, true);
    assert.equal(purchased.world.state.company.cargo[0], 1);
    const atTown = await state();
    for (const page of [owner, game]) await page.waitForFunction(hash => document.body.dataset.companyHash === hash, atTown.state.hash);
    const start = await game.locator('body').getAttribute('data-player-position');
    const canvas = await game.locator('#canvas').boundingBox();
    await game.mouse.click(canvas.x + canvas.width * 0.62, canvas.y + canvas.height * 0.62);
    await game.waitForFunction(position => document.body.dataset.playerPosition !== position, start);
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready' && document.body.dataset.playerPosition, undefined, {timeout:120000});
    assert.notEqual(await game.locator('body').getAttribute('data-player-position'), start);
    assert.equal(await game.locator('body').getAttribute('data-avatar'), '12576');
    const destination = (await state()).state.travel[0].id;
    const departed = await game.evaluate(target => Module.ccCoop.apply('travel', target, 0, 0), destination);
    assert.equal(departed.accepted, true);
    await owner.getByRole('button', {name:'Pause world', exact:true}).click();
    await owner.getByRole('button', {name:'Resume world', exact:true}).waitFor();
    const travelling = await state();
    assert.equal(travelling.state.journey.active, true);
    for (const page of [owner, game]) await page.waitForFunction(hash => document.body.dataset.companyHash === hash, travelling.state.hash);
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready', undefined, {timeout:120000});
    await game.waitForFunction(hash => document.body.dataset.companyHash === hash, travelling.state.hash);
    assert.deepEqual(errors, []);
    console.log('Two player appearances persist; the avatar panel opens the world; reload keeps the saved place and shared campaign.');
  } finally {
    if (browser) await browser.close();
    host.kill('SIGTERM');
    if (host.exitCode === null) await new Promise(resolve => host.once('exit', resolve));
    await fs.rm(temp, {recursive:true, force:true});
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
