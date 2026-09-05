const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const {gameControls} = require('./game_controls.cjs');
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
    await page.waitForFunction(() => window.Module && Module.crownlessCampaignAccess === 0 && document.querySelector('#loading').hidden && window.shaderLinks.some(link => link.skinned), undefined, {timeout: 120000});
    await page.waitForFunction(() => Module.crownlessScreen === 'title');
    const startupMemory = await page.evaluate(() => {
      const buffers = new Set();
      const files = [];
      function visit(directory) {
        for (const name of FS.readdir(directory)) {
          if (name === '.' || name === '..') continue;
          const filename = directory + '/' + name;
          const stat = FS.stat(filename);
          if (FS.isDir(stat.mode)) visit(filename);
          else {
            const contents = FS.lookupPath(filename).node.contents;
            buffers.add(contents.buffer);
            files.push({filename, size: stat.size, bufferBytes: contents.buffer.byteLength});
          }
        }
      }
      visit('/assets');
      return {
        files,
        retainedFileBytes: files.reduce((sum, file) => sum + file.size, 0),
        retainedBufferBytes: [...buffers].reduce((sum, buffer) => sum + buffer.byteLength, 0),
        wasmBytes: HEAPU8.byteLength
      };
    });
    await fs.writeFile(path.join(output, 'startup-memory.json'), JSON.stringify(startupMemory, null, 2));
    assert(startupMemory.files.length > 0, 'Voice files remain available after startup');
    assert.equal(startupMemory.retainedBufferBytes, startupMemory.retainedFileBytes,
      'Retained voice files should own only their audio bytes');
    for (const file of startupMemory.files) {
      assert(file.filename.startsWith('/assets/audio/'));
      const actual = await page.evaluate(filename => Array.from(FS.readFile(filename)), file.filename);
      const expected = await fs.readFile(path.join(__dirname, '..', file.filename.slice(1)));
      assert.deepEqual(Buffer.from(actual), expected, file.filename);
    }
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
    const beforeSave = await page.evaluate(() => Module.crownlessSaveRevision);
    await page.keyboard.press('Control+s');
    await page.waitForFunction(previous => Module.crownlessSaveRevision > previous, beforeSave);
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
      await mobile.waitForFunction(() => window.Module?.crownlessScreen === 'title' && Module.crownlessTouchFrame?.buttons.length);
      const controls = gameControls(mobile, true);
      assert.equal(await mobile.locator('#touch-panel, #touch-actions, #exit-fullscreen').count(), 0);
      for (const [width, height] of [[320, 740], [390, 844], [667, 375], [844, 390], [1024, 768]]) {
        await mobile.setViewportSize({width, height});
        await mobile.waitForTimeout(100);
        assert(await mobile.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1));
        const canvas = await mobile.locator('#canvas').boundingBox();
        assert(Math.abs(canvas.width / canvas.height - 16 / 9) < 0.01);
        assert(canvas.y + canvas.height <= height + 1);
        await mobile.screenshot({path: path.join(output, `mobile-${width}x${height}.png`)});
      }
      await mobile.setViewportSize({width: 390, height: 844});
      await mobile.evaluate(() => {
        window.touchTaps = [];
        const tap = Module._CrownlessTouchTap;
        Module._CrownlessTouchTap = (x, y) => { window.touchTaps.push([x, y]); tap(x, y); };
      });
      await controls.button('Play').tap();
      await mobile.waitForFunction(() => Module.crownlessScreen === 'playing');
      assert.equal((await mobile.evaluate(() => window.touchTaps)).length, 1);
      const nearby = (await controls.buttons()).filter(button => button.y >= 590);
      assert.equal(nearby.length, 4, JSON.stringify(await controls.buttons()));
      assert((await controls.buttons()).every(button => !/More objects|Previous objects|Fast forward|Press on/.test(button.label)));
      await mobile.screenshot({path: path.join(output, 'mobile-nearby-cards.png')});
      const oldControl = await controls.button('Menu').read();
      await controls.button('Book').tap();
      await mobile.waitForFunction(() => Module.crownlessTouchFrame.title === 'Company Book');
      await mobile.evaluate(({index, revision}) => Module._CrownlessTouchActivate(index, revision), oldControl);
      await mobile.waitForTimeout(150);
      assert.equal(await mobile.locator('#canvas').getAttribute('aria-label'), 'Company Book');
      await controls.button('Cargo').tap();
      assert((await controls.reading()).length > 30);
      await mobile.screenshot({path: path.join(output, 'mobile-book.png')});
      await controls.button('PONIES').tap();
      await mobile.waitForFunction(() => Module.crownlessTouchFrame.reading.includes('Ponies 1-2 of 7'));
      assert((await controls.reading()).includes('With you'));
      await mobile.screenshot({path: path.join(output, 'mobile-ponies.png')});
      for (const range of ['3-4', '5-6', '7-7']) {
        await controls.button('Next').tap();
        await mobile.waitForFunction(range => Module.crownlessTouchFrame.reading.includes(`Ponies ${range} of 7`), range);
      }
      assert(await controls.button('Next').isDisabled());
      await controls.button('Previous').tap();
      await mobile.waitForFunction(() => Module.crownlessTouchFrame.reading.includes('Ponies 5-6 of 7'));
      await controls.button('Back').tap();
      await controls.button('Menu').tap();
      await mobile.waitForFunction(() => Module.crownlessScreen === 'paused');
      const revision = await mobile.evaluate(() => Module.crownlessSaveRevision);
      await controls.button('Save world').tap();
      await mobile.waitForFunction(before => Module.crownlessSaveRevision > before, revision);
      await controls.button('Resume').tap();
      await mobile.waitForFunction(() => Module.crownlessScreen === 'playing');
      const tapsBefore = await mobile.evaluate(() => window.touchTaps.length);
      const input = await phone.newCDPSession(mobile);
      const canvas = await mobile.locator('#canvas').boundingBox();
      const point = {x: canvas.x + canvas.width / 2, y: canvas.y + canvas.height / 2};
      await input.send('Input.dispatchTouchEvent', {type:'touchStart', touchPoints:[point]});
      await input.send('Input.dispatchTouchEvent', {type:'touchMove', touchPoints:[{x:point.x+30, y:point.y}]});
      await input.send('Input.dispatchTouchEvent', {type:'touchEnd', touchPoints:[]});
      await input.send('Input.dispatchTouchEvent', {type:'touchStart', touchPoints:[point]});
      await input.send('Input.dispatchTouchEvent', {type:'touchCancel', touchPoints:[]});
      assert.equal(await mobile.evaluate(() => window.touchTaps.length), tapsBefore);
      await controls.button('Menu').tap();
      await controls.button('Your traveller').tap();
      await mobile.waitForFunction(() => Module.crownlessScreen === 'avatar');
      await controls.button(/^Coat:/).tap();
      await controls.button('Save appearance').tap();
      await mobile.waitForFunction(() => Module.crownlessScreen === 'paused');
      assert.match(await mobile.evaluate(() => FS.readFile('/crownless-save/crownless_campaign.ccsave.preferences', {encoding:'utf8'})), /avatar 4096/);
      for (const fail of [false, true]) {
        await mobile.evaluate(fail => { document.querySelector('#stage').requestFullscreen = fail ? () => Promise.reject(new Error('declined')) : undefined; }, fail);
        await controls.button('Full screen').tap();
        await mobile.waitForFunction(() => document.querySelector('#stage').classList.contains('expanded'));
        await mobile.setViewportSize({width:844, height:390});
        await mobile.screenshot({path:path.join(output, 'mobile-expanded-landscape.png')});
        await controls.button('Full screen').tap();
        await mobile.waitForFunction(() => !document.querySelector('#stage').classList.contains('expanded'));
      }
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
