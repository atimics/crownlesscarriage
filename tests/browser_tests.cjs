const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const {chromium} = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');

async function main() {
  const root = path.resolve(process.argv[2]);
  const output = path.resolve(process.argv[3] || 'browser-results');
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
  const context = await browser.newContext({viewport: {width: 1280, height: 900}});
  const page = await context.newPage();
  async function selectMenuItem(index) {
    await page.locator('#canvas').focus();
    while (await page.evaluate(() => Module.crownlessMenuFocus) !== index) {
      const previous = await page.evaluate(() => Module.crownlessMenuFocus);
      await page.keyboard.press('ArrowDown');
      await page.waitForFunction(value => Module.crownlessMenuFocus !== value, previous);
    }
    const draft = await page.evaluate(() => Module.crownlessScreen === 'avatar' ? Module.crownlessAvatarDraft : null);
    await page.keyboard.press('Enter');
    if (draft !== null && index < 5) await page.waitForFunction(value => Module.crownlessAvatarDraft !== value, draft);
  }
  const errors = [];
  let rejectedWrite = false;
  page.on('pageerror', error => errors.push(error.message));
  page.on('console', message => {
    if (message.type() === 'error' && !(rejectedWrite && message.text().includes('Could not store Crownless Carriage saves'))) errors.push(message.text());
  });
  await page.addInitScript(() => {
    window.shaderLinks = [];
    const prototype = WebGL2RenderingContext.prototype;
    const link = prototype.linkProgram;
    prototype.linkProgram = function(program) {
      link.call(this, program);
      const vertex = this.getAttachedShaders(program).find(shader => this.getShaderParameter(shader, this.SHADER_TYPE) === this.VERTEX_SHADER);
      const source = vertex ? this.getShaderSource(vertex) : '';
      if (source.includes('boneMatrices')) {
        const fragment = this.getAttachedShaders(program).find(shader => this.getShaderParameter(shader, this.SHADER_TYPE) === this.FRAGMENT_SHADER);
        window.skinSources = {vertex: source, fragment: this.getShaderSource(fragment)};
      }
      let vectors = 0;
      for (let i = 0; i < this.getProgramParameter(program, this.ACTIVE_UNIFORMS); i++) {
        const uniform = this.getActiveUniform(program, i);
        const name = uniform.name.replace(/\[0\]$/, '');
        if (new RegExp('uniform\\s+\\w+\\s+' + name + '\\b').test(source)) {
          vectors += uniform.size * (uniform.type === this.FLOAT_MAT4 ? 4 : uniform.type === this.FLOAT_MAT3 ? 3 : uniform.type === this.FLOAT_MAT2 ? 2 : 1);
        }
      }
      window.shaderLinks.push({linked: this.getProgramParameter(program, this.LINK_STATUS), log: this.getProgramInfoLog(program), vectors, skinned: source.includes('boneMatrices'), limit: this.getParameter(this.MAX_VERTEX_UNIFORM_VECTORS)});
    };
  });
  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.waitForFunction(() => window.Module && Module.crownlessCampaignAccess === 0 && document.querySelector('#loading').hidden && window.shaderLinks.some(link => link.skinned), {timeout: 120000});
    await page.waitForFunction(() => Module.crownlessScreen === 'title');
    assert.equal(await page.evaluate(() => Module.crownlessSaveRevision), 0);
    await page.screenshot({path: path.join(output, 'title.png')});
    assert.equal(await page.locator('header, footer, iframe').count(), 0);
    await selectMenuItem(3);
    await page.waitForFunction(() => Module.crownlessScreen === 'avatar');
    await selectMenuItem(4); // Moss coat, preview only.
    await page.keyboard.press('Escape');
    await page.waitForFunction(() => Module.crownlessScreen === 'title');
    await selectMenuItem(3);
    await page.waitForFunction(() => Module.crownlessScreen === 'avatar');
    await selectMenuItem(4);
    await page.screenshot({path: path.join(output, 'traveller-editor.png')});
    await selectMenuItem(5);
    await page.waitForFunction(() => Module.crownlessScreen === 'title');
    await page.reload();
    await page.waitForFunction(() => window.Module && Module.crownlessScreen === 'title');
    const appearance = await page.evaluate(() => FS.readFile('/crownless-save/crownless_campaign.ccsave.preferences', {encoding:'utf8'}));
    assert.match(appearance, /avatar 4096\n/);
    await page.locator('#canvas').focus();
    await page.keyboard.press('Enter');
    await page.waitForFunction(() => Module.crownlessScreen === 'playing' && Module.crownlessSaveRevision > 0);
    await page.screenshot({path: path.join(output, 'opening.png')});
    const shaders = await page.evaluate(() => window.shaderLinks);
    assert(shaders.every(shader => shader.linked), JSON.stringify(shaders));
    assert(shaders.every(shader => shader.vectors <= 256), JSON.stringify(shaders));
    assert(shaders.some(shader => shader.skinned && shader.vectors === 140), JSON.stringify(shaders));
    await fs.writeFile(path.join(output, 'shaders.json'), JSON.stringify(shaders, null, 2));
    const minimumBudget = await page.evaluate(() => {
      const gl = document.createElement('canvas').getContext('webgl2');
      const limit = gl.getParameter(gl.MAX_VERTEX_UNIFORM_VECTORS);
      const reserved = limit - 256;
      const sources = window.skinSources;
      function linkWithPalette(bones) {
        let vertex = sources.vertex.replace('boneMatrices[32]', `boneMatrices[${bones}]`);
        if (reserved > 0) {
          vertex = vertex.replace('void main()', `uniform vec4 reservedVertexBudget[${reserved}];\nvoid main()`)
            .replace(/}\s*$/, `gl_Position += reservedVertexBudget[int(abs(vertexPosition.x)) % ${reserved}];\n}`);
        }
        const program = gl.createProgram();
        const compiled = [];
        for (const [type, source] of [[gl.VERTEX_SHADER, vertex], [gl.FRAGMENT_SHADER, sources.fragment]]) {
          const shader = gl.createShader(type);
          gl.shaderSource(shader, source);
          gl.compileShader(shader);
          compiled.push(gl.getShaderParameter(shader, gl.COMPILE_STATUS));
          gl.attachShader(program, shader);
        }
        gl.linkProgram(program);
        const result = {compiled, linked: gl.getProgramParameter(program, gl.LINK_STATUS), log: gl.getProgramInfoLog(program)};
        gl.deleteProgram(program);
        return result;
      }
      return {limit, reserved, available: 256, shipped: linkWithPalette(32), oversized: linkWithPalette(64)};
    });
    assert(minimumBudget.shipped.linked, JSON.stringify(minimumBudget));
    assert(!minimumBudget.oversized.linked, JSON.stringify(minimumBudget));
    await fs.writeFile(path.join(output, 'minimum-uniform-budget.json'), JSON.stringify(minimumBudget, null, 2));
    for (const [width, height] of [[800, 600], [1280, 720], [600, 900]]) {
      await page.setViewportSize({width, height});
      await page.locator('#canvas').focus();
      await page.keyboard.press('Escape');
      await page.waitForFunction(() => Module.crownlessScreen === 'paused');
      await selectMenuItem(10);
      await page.waitForFunction(() => document.fullscreenElement !== null);
      const bounds = await page.locator('#canvas').boundingBox();
      assert(Math.abs(bounds.width / bounds.height - 16 / 9) < 0.01);
      assert(Math.abs(bounds.x * 2 + bounds.width - width) < 2);
      assert(Math.abs(bounds.y * 2 + bounds.height - height) < 2);
      await page.evaluate(() => {
        window.lastCanvasPointer = null;
        document.querySelector('#canvas').addEventListener('pointerdown', event => {
          const canvas = event.currentTarget;
          const bounds = canvas.getBoundingClientRect();
          window.lastCanvasPointer = [(event.clientX - bounds.x) * canvas.width / bounds.width, (event.clientY - bounds.y) * canvas.height / bounds.height];
        }, {once: true});
      });
      await page.mouse.click(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);
      const pointer = await page.evaluate(() => window.lastCanvasPointer);
      const intrinsic = await page.locator('#canvas').evaluate(canvas => [canvas.width, canvas.height]);
      assert(Math.abs(intrinsic[0] / intrinsic[1] - 16 / 9) < 0.01, JSON.stringify(intrinsic));
      assert(Math.abs(pointer[0] - intrinsic[0] / 2) < 2 && Math.abs(pointer[1] - intrinsic[1] / 2) < 2, JSON.stringify({pointer, intrinsic, bounds}));
      await page.screenshot({path: path.join(output, `fullscreen-${width}x${height}.png`)});
      await page.evaluate(() => document.exitFullscreen());
      await page.waitForFunction(() => document.fullscreenElement === null);
      await selectMenuItem(0);
      await page.waitForFunction(() => Module.crownlessScreen === 'playing');
    }
    await page.setViewportSize({width: 1280, height: 900});
    await page.keyboard.press('Control+s');
    await page.waitForFunction(() => Module.crownlessSaveRevision > 0);
    const revision = await page.evaluate(() => Module.crownlessSaveRevision);
    await page.reload();
    await page.waitForFunction(() => window.Module && Module.crownlessCampaignRestored && document.querySelector('#loading').hidden);
    assert.equal(await page.evaluate(() => Module.crownlessSaveRevision), revision);
    await page.waitForFunction(() => Module.crownlessScreen === 'title');
    await page.locator('#canvas').focus();
    await page.keyboard.press('Enter');
    await page.waitForFunction(() => Module.crownlessScreen === 'playing');
    rejectedWrite = true;
    await page.evaluate(() => {
      const transaction = IDBDatabase.prototype.transaction;
      window.originalSaveTransaction = transaction;
      IDBDatabase.prototype.transaction = function(names, mode, ...rest) {
        if (mode === 'readwrite') throw new DOMException('Injected storage failure', 'QuotaExceededError');
        return transaction.call(this, names, mode, ...rest);
      };
      window.saveRejectionSeen = false;
      const persist = Module.persistCrownlessSave;
      Module.persistCrownlessSave = async (...args) => {
        try { return await persist(...args); }
        catch (error) { window.saveRejectionSeen = true; throw error; }
      };
    });
    await page.keyboard.press('Control+s');
    await page.waitForFunction(() => window.saveRejectionSeen);
    await page.waitForTimeout(250);
    assert.equal(await page.evaluate(() => Module.crownlessSaveRevision), revision);
    await page.screenshot({path: path.join(output, 'rejected-save.png')});
    await page.evaluate(() => { IDBDatabase.prototype.transaction = window.originalSaveTransaction; });
    rejectedWrite = false;
    await page.keyboard.press('Escape');
    await page.waitForFunction(() => Module.crownlessScreen === 'paused');
    await page.screenshot({path: path.join(output, 'pause-menu.png')});
    await selectMenuItem(5);
    await page.waitForFunction(() => Module.crownlessScreen === 'delete');
    await page.screenshot({path: path.join(output, 'delete-confirmation.png')});
    await page.locator('#canvas').focus();
    await page.keyboard.press('Enter'); // Keep world is selected first.
    await page.waitForFunction(() => Module.crownlessScreen === 'paused');
    assert.equal(await page.evaluate(() => Module.crownlessSaveRevision), revision);
    await selectMenuItem(5);
    await page.waitForFunction(() => Module.crownlessScreen === 'delete');
    await selectMenuItem(1);
    await page.waitForFunction(() => Module.crownlessScreen === 'title' && Module.crownlessSaveRevision > 0);
    assert.equal(await page.evaluate(() => Module.crownlessSaveRevision), revision + 1);
    await page.reload();
    await page.waitForFunction(() => window.Module && Module.crownlessScreen === 'title');
    assert.equal(await page.evaluate(() => Module.crownlessCampaignRestored), false);
    await page.locator('#canvas').focus();
    await page.keyboard.press('Enter');
    await page.waitForFunction(() => Module.crownlessScreen === 'playing');
    assert.equal(await page.evaluate(() => Module.crownlessSaveRevision), revision + 2);
    const phone = await browser.newContext({viewport: {width: 390, height: 844},
      hasTouch: true, isMobile: true, deviceScaleFactor: 3});
    const mobile = await phone.newPage();
    mobile.on('pageerror', error => errors.push(error.message));
    try {
      await mobile.goto(`http://127.0.0.1:${server.address().port}/`);
      await mobile.waitForFunction(() => window.Module?.crownlessScreen === 'title' &&
        document.querySelector('#touch-actions button'));
      assert.equal(await mobile.locator('#touch-panel').isVisible(), true);
      async function mobileLayout(width, height) {
        await mobile.setViewportSize({width, height});
        await mobile.waitForTimeout(100);
        const layout = await mobile.evaluate(() => ({
          width: window.innerWidth, scroll: document.documentElement.scrollWidth,
          buttons: Array.from(document.querySelectorAll('#touch-actions button'), b => {
            const r = b.getBoundingClientRect();
            return {width: r.width, height: r.height};
          })
        }));
        assert(layout.scroll <= layout.width + 1, JSON.stringify(layout));
        assert(layout.buttons.length > 0 && layout.buttons.every(b => b.width >= 44 && b.height >= 44), JSON.stringify(layout));
        const canvas = await mobile.locator('#canvas').boundingBox();
        assert(Math.abs(canvas.width / canvas.height - 16 / 9) < 0.01, JSON.stringify(canvas));
        if (width >= 600 && width > height && height <= 600) {
          const stage = await mobile.locator('#stage').boundingBox();
          assert(stage.y + stage.height <= height + 1, JSON.stringify(stage));
        }
        await mobile.screenshot({path: path.join(output, `mobile-${width}x${height}.png`)});
      }
      for (const size of [[320, 740], [390, 844], [667, 375], [844, 390], [1024, 768]]) await mobileLayout(...size);
      await mobile.setViewportSize({width: 390, height: 844});
      // A real touch on the scaled canvas must reach the same Play control once.
      await mobile.evaluate(() => {
        window.touchTaps = [];
        const tap = Module._CrownlessTouchTap;
        Module._CrownlessTouchTap = (x, y) => { window.touchTaps.push([x, y]); tap(x, y); };
      });
      let canvas = await mobile.locator('#canvas').boundingBox();
      await mobile.touchscreen.tap(canvas.x + canvas.width / 2, canvas.y + canvas.height * 394 / 720);
      await mobile.waitForFunction(() => Module.crownlessScreen === 'playing');
      const taps = await mobile.evaluate(() => window.touchTaps);
      assert.equal(taps.length, 1);
      assert(Math.abs(taps[0][0] - 640) < 5 && Math.abs(taps[0][1] - 394) < 5, JSON.stringify(taps));
      const buttons = mobile.locator('#touch-actions');
      const oldControl = await buttons.getByRole('button', {name: 'Menu', exact: true}).evaluate(button => ({
        index: Array.from(button.parentElement.children).indexOf(button), revision: Number(button.dataset.revision)
      }));
      await buttons.getByRole('button', {name: 'Book', exact: true}).tap();
      await mobile.getByRole('heading', {name: 'Company Book', exact: true}).waitFor();
      await mobile.evaluate(({index, revision}) => Module._CrownlessTouchActivate(index, revision), oldControl);
      await mobile.waitForTimeout(150);
      assert.equal(await mobile.locator('#touch-title').innerText(), 'Company Book');
      await buttons.getByRole('button', {name: 'Cargo', exact: true}).tap();
      await mobile.locator('#touch-reading-panel summary').tap();
      assert((await mobile.locator('#touch-reading').innerText()).length > 30);
      await mobile.screenshot({path: path.join(output, 'mobile-book.png')});
      await buttons.getByRole('button', {name: 'Back', exact: true}).tap();
      await buttons.getByRole('button', {name: 'Menu', exact: true}).tap();
      await mobile.waitForFunction(() => Module.crownlessScreen === 'paused');
      const mobileRevision = await mobile.evaluate(() => Module.crownlessSaveRevision);
      await buttons.getByRole('button', {name: 'Save world', exact: true}).tap();
      await mobile.waitForFunction(revision => Module.crownlessSaveRevision > revision, mobileRevision);
      await buttons.getByRole('button', {name: 'Resume', exact: true}).tap();
      await mobile.waitForFunction(() => Module.crownlessScreen === 'playing');
      // Dragged and cancelled touches must leave the current route untouched.
      const input = await phone.newCDPSession(mobile);
      canvas = await mobile.locator('#canvas').boundingBox();
      const point = {x: canvas.x + canvas.width / 2, y: canvas.y + canvas.height / 2};
      await input.send('Input.dispatchTouchEvent', {type: 'touchStart', touchPoints: [point]});
      await input.send('Input.dispatchTouchEvent', {type: 'touchMove', touchPoints: [{x: point.x + 30, y: point.y}]});
      await input.send('Input.dispatchTouchEvent', {type: 'touchEnd', touchPoints: []});
      await input.send('Input.dispatchTouchEvent', {type: 'touchStart', touchPoints: [point]});
      await input.send('Input.dispatchTouchEvent', {type: 'touchCancel', touchPoints: []});
      assert.equal(await mobile.evaluate(() => window.touchTaps.length), 1);
      await buttons.getByRole('button', {name: 'Menu', exact: true}).tap();
      await mobile.waitForFunction(() => Module.crownlessScreen === 'paused');
      await buttons.getByRole('button', {name: 'Your traveller', exact: true}).tap();
      await mobile.waitForFunction(() => Module.crownlessScreen === 'avatar');
      await buttons.getByRole('button', {name: /^Coat:/}).tap();
      await buttons.getByRole('button', {name: 'Save appearance', exact: true}).tap();
      await mobile.waitForFunction(() => Module.crownlessScreen === 'paused');
      assert.match(await mobile.evaluate(() => FS.readFile('/crownless-save/crownless_campaign.ccsave.preferences', {encoding:'utf8'})), /avatar 4096/);
      // Exercise the page fallback used when element fullscreen is unavailable.
      await mobile.evaluate(() => { document.querySelector('#stage').requestFullscreen = undefined; });
      await buttons.getByRole('button', {name: 'Full screen', exact: true}).tap();
      assert.equal(await mobile.locator('#stage').evaluate(stage => stage.classList.contains('expanded')), true);
      assert.equal(await mobile.locator('#touch-panel').isVisible(), true);
      await mobile.setViewportSize({width: 844, height: 390});
      await mobile.screenshot({path: path.join(output, 'mobile-expanded-landscape.png')});
      await mobile.locator('#exit-fullscreen').tap();
      assert.equal(await mobile.locator('#stage').evaluate(stage => stage.classList.contains('expanded')), false);
      await mobile.evaluate(() => { document.querySelector('#stage').requestFullscreen = () => Promise.reject(new Error('declined')); });
      await buttons.getByRole('button', {name: 'Full screen', exact: true}).tap();
      assert.equal(await mobile.locator('#stage').evaluate(stage => stage.classList.contains('expanded')), true);
      assert.equal(await mobile.locator('#loading').isVisible(), false);
      await mobile.locator('#exit-fullscreen').tap();
    } finally { await phone.close(); }
    assert.deepEqual(errors, []);
    console.log('Browser desktop and mobile layout, touch input, menus, saves, shaders, fullscreen, and reload checks passed');
  } finally {
    await context.close();
    await browser.close();
    server.close();
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
