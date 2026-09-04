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
    await crew.getByRole('button', {name:'Buy for company'}).click();
    await owner.waitForFunction(() => document.querySelector('#coins').textContent === '38');
    assert.equal(await owner.locator('#cargo').textContent(), '1 / 12');
    const game = await b.newPage(), errors = [];
    game.on('pageerror', error => errors.push(error.message));
    await game.goto(origin + await crew.getByRole('link', {name:'Enter the 3D world'}).getAttribute('href'));
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready', undefined, {timeout:120000});
    const worldId = new URL(await crew.url()).hash.slice('#world='.length);
    const token = await crew.evaluate(() => localStorage.getItem('cc-coop-token'));
    async function state() {
      const response = await fetch(`${origin}/api/worlds/${worldId}/state`, {headers:{Authorization:`Bearer ${token}`}});
      assert(response.ok);
      return response.json();
    }
    const atTown = await state();
    await game.waitForFunction(hash => document.body.dataset.companyHash === hash, atTown.state.hash);
    await crew.getByRole('button', {name:'Road', exact:true}).click();
    await crew.getByRole('button', {name:'Depart', exact:true}).click();
    await owner.waitForFunction(() => document.querySelector('#location').textContent.includes('→'));
    await owner.getByRole('button', {name:'Pause world'}).click();
    const travelling = await state();
    assert.equal(travelling.state.journey.active, true);
    await game.waitForFunction(hash => document.body.dataset.companyHash === hash, travelling.state.hash);
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready', undefined, {timeout:120000});
    await game.waitForFunction(hash => document.body.dataset.companyHash === hash, travelling.state.hash);
    assert.deepEqual(errors, []);
    console.log('Two browser identities share cargo, departure, clock, and the C campaign across reload.');
  } finally {
    if (browser) await browser.close();
    host.kill('SIGTERM');
    if (host.exitCode === null) await new Promise(resolve => host.once('exit', resolve));
    await fs.rm(temp, {recursive:true, force:true});
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
