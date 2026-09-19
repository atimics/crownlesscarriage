const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const {gameControls} = require('./game_controls.cjs');
const bufferContracts = require('./webgl_buffer_contracts.cjs');
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
  async function assertSaveStatusLane(target, width, height) {
    await target.setViewportSize({width, height});
    await target.waitForTimeout(100);
    const layout = await target.evaluate(() => {
      const canvas = document.querySelector('#canvas').getBoundingClientRect();
      const status = document.querySelector('#save-status').getBoundingClientRect();
      const scaleX = canvas.width / document.querySelector('#canvas').width;
      const scaleY = canvas.height / document.querySelector('#canvas').height;
      const buttons = (Module.crownlessTouchFrame?.buttons || []).map(button => ({
        left: canvas.left + button.x * scaleX,
        top: canvas.top + button.y * scaleY,
        right: canvas.left + (button.x + button.width) * scaleX,
        bottom: canvas.top + (button.y + button.height) * scaleY
      }));
      return {
        canvas: {left: canvas.left, top: canvas.top, right: canvas.right, bottom: canvas.bottom},
        status: {left: status.left, top: status.top, right: status.right, bottom: status.bottom},
        buttons
      };
    });
    assert(layout.status.top >= layout.canvas.bottom - 1, JSON.stringify(layout));
    assert(layout.status.left >= -1 && layout.status.right <= width + 1 &&
      layout.status.top >= -1 && layout.status.bottom <= height + 1, JSON.stringify(layout));
    for (const button of layout.buttons) {
      const overlap = button.left < layout.status.right && button.right > layout.status.left &&
        button.top < layout.status.bottom && button.bottom > layout.status.top;
      assert(!overlap, JSON.stringify({layout, button}));
    }
  }
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
    /* Per-frame graphics work, counted where the browser pays for it. The heap
       the game can see stays small; what reloads a tab is the traffic through
       these calls every frame. */
    const budget = window.frameBudget = {frames: 0, uploadCalls: 0, uploadBytes: 0,
      draws: 0, vertices: 0, textureBinds: 0, programBinds: 0};
    window.frameDurations = [];
    let lastFrameTime;
    const frame = window.requestAnimationFrame;
    window.requestAnimationFrame = function(callback) {
      return frame.call(window, time => {
        budget.frames++;
        if (lastFrameTime !== undefined) window.frameDurations.push(time - lastFrameTime);
        lastFrameTime = time;
        return callback(time);
      });
    };
    const upload = prototype.bufferSubData;
    prototype.bufferSubData = function(target, offset, source, sourceOffset, length) {
      budget.uploadCalls++;
      const elementSize = source?.BYTES_PER_ELEMENT || 1;
      const start = sourceOffset || 0;
      const count = arguments.length >= 5 && length !== 0
        ? length : Math.max(0, (source?.length || 0) - start);
      budget.uploadBytes += count * elementSize;
      return upload.apply(this, arguments);
    };
    const elements = prototype.drawElements;
    prototype.drawElements = function(mode, count) {
      budget.draws++; budget.vertices += count;
      return elements.apply(this, arguments);
    };
    const arrays = prototype.drawArrays;
    prototype.drawArrays = function(mode, first, count) {
      budget.draws++; budget.vertices += count;
      return arrays.apply(this, arguments);
    };
    const texture = prototype.bindTexture;
    prototype.bindTexture = function() { budget.textureBinds++; return texture.apply(this, arguments); };
    const program = prototype.useProgram;
    prototype.useProgram = function() { budget.programBinds++; return program.apply(this, arguments); };
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
    assert.match(await page.locator('#save-status').innerText(), /browser can save this campaign/i);
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
    await page.waitForFunction(() => [...document.querySelectorAll('[data-crownless-music]')]
      .some(media => !media.paused && media.readyState >= 3), {timeout: 60000});
    assert(await page.locator('[data-crownless-music]').count() <= 4,
      'The game uses at most three playing songs and one prepared song');
    /* A playing frame's graphics traffic. Safari holds the memory its allocator
       reaches streaming this, so the ceilings guard the browser's cost, not the
       game's own heap, which stays around fifty megabytes either way. */
    const reading = () => page.evaluate(() => ({...window.frameBudget,
      skippedUploads: window.crownlessUploads?.skipped || 0,
      skippedBytes: window.crownlessUploads?.skippedBytes || 0}));
    await page.evaluate(() => { window.frameDurations = []; });
    const opening = await reading();
    await page.waitForTimeout(4000);
    const closing = await reading();
    const drawn = closing.frames - opening.frames;
    assert(drawn >= 20, `A playing game should draw frames; it drew ${drawn}`);
    const perFrame = Object.fromEntries(Object.keys(opening)
      .filter(key => key !== 'frames')
      .map(key => [key, (closing[key] - opening[key]) / drawn]));
    const frameTimes = await page.evaluate(() => window.frameDurations.slice().sort((a, b) => a - b));
    const frameMs = Object.fromEntries([['median', 0.5], ['p95', 0.95], ['p99', 0.99]]
      .map(([name, fraction]) => [name, frameTimes[Math.min(frameTimes.length - 1,
        Math.floor(frameTimes.length * fraction))]]));
    await fs.writeFile(path.join(output, 'frame-budget.json'),
      JSON.stringify({frames: drawn, perFrame, frameMs}, null, 2));
    const ceilings = {uploadCalls: 190, uploadBytes: 4 * 1024 * 1024, draws: 450,
      vertices: 720000, textureBinds: 700, programBinds: 900};
    for (const [name, ceiling] of Object.entries(ceilings)) {
      assert(perFrame[name] <= ceiling,
        `Each frame should stay under ${ceiling} ${name}, not ${perFrame[name].toFixed(1)}`);
    }
    /* The browser owns buffer contents. #468 removes the shadow cache after
       GPU readback exposed stale writes; traffic budgets use the direct path. */
    const buffers = await bufferContracts(page);
    await fs.writeFile(path.join(output, 'buffer-contracts.json'), JSON.stringify(buffers, null, 2));
    for (const result of buffers) {
      assert.equal(result.error, 0, `${result.name} must be a valid WebGL operation`);
      assert.deepEqual(result.actual, result.expected, `${result.name} must preserve uploaded bytes`);
    }
    const uploadOverloads = await page.evaluate(() => {
      const gl = document.createElement('canvas').getContext('webgl2');
      const buffer = gl.createBuffer();
      gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
      const source = new Uint16Array([10, 20, 30, 40]);
      gl.bufferData(gl.ARRAY_BUFFER, source.byteLength, gl.DYNAMIC_DRAW);
      const before = window.frameBudget.uploadBytes;
      gl.bufferSubData(gl.ARRAY_BUFFER, 0, source, 1, 0);
      const zeroLength = window.frameBudget.uploadBytes - before;
      const afterZero = window.frameBudget.uploadBytes;
      gl.bufferSubData(gl.ARRAY_BUFFER, 0, source, 1);
      const omittedLength = window.frameBudget.uploadBytes - afterZero;
      gl.deleteBuffer(buffer);
      return {zeroLength, omittedLength};
    });
    assert.deepEqual(uploadOverloads, {zeroLength: 6, omittedLength: 6});
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
      const frame = await page.locator('#game-frame').boundingBox();
      assert(Math.abs(bounds.width / bounds.height - 16 / 9) < 0.01);
      assert(Math.abs((bounds.x - frame.x) * 2 + bounds.width - frame.width) < 2);
      if (height > width && width <= 600) {
        assert(Math.abs(bounds.y - frame.y) < 2);
      } else {
        assert(Math.abs((bounds.y - frame.y) * 2 + bounds.height - frame.height) < 2);
      }
      await page.evaluate(() => {
        window.lastCanvasPointer = null;
        document.querySelector('#canvas').addEventListener('pointerdown', event => {
          const canvas = event.currentTarget;
          const bounds = canvas.getBoundingClientRect();
          window.lastCanvasPointer = [(event.clientX - bounds.x) * canvas.width / bounds.width, (event.clientY - bounds.y) * canvas.height / bounds.height];
        }, {once: true});
      });
      const hit = await page.evaluate(({x, y}) => {
        const element = document.elementFromPoint(x, y);
        const panel = document.querySelector('#touch-actions').getBoundingClientRect();
        return {element: element?.id || element?.className || element?.tagName,
          panel: {x: panel.x, y: panel.y, width: panel.width, height: panel.height}};
      }, {x: bounds.x + bounds.width / 2, y: bounds.y + bounds.height / 2});
      assert.equal(hit.element, 'canvas', JSON.stringify({bounds, frame, hit}));
      await page.locator('#canvas').click({position: {
        x: bounds.width / 2, y: bounds.height / 2
      }});
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
    assert.match(await page.locator('#save-status').innerText(), /could not save/i);
    assert.equal(await page.locator('#save-status').getAttribute('data-state'), 'failed');
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
    /* Starting a fresh campaign writes the journal and the first save before
       the screen flips to playing; loaded CI runners sometimes exceed 30s. */
    try {
        await page.waitForFunction(() => Module.crownlessScreen === 'playing',
            undefined, {timeout: 120000});
    } catch {
        const screen = await page.evaluate(() => Module.crownlessScreen);
        throw new Error(
            `A fresh campaign stalled on screen '${screen}' after Enter at title.`);
    }
    assert.equal(await page.evaluate(() => Module.crownlessSaveRevision), revision + 2);
    await assertSaveStatusLane(page, 1280, 720);
    await page.screenshot({path: path.join(output, 'save-lane-desktop.png')});
    const recovery = await page.evaluate(() => {
      window.dispatchEvent(new ErrorEvent('error', {message: 'Injected runtime failure'}));
      const loading = document.querySelector('#loading');
      const runtime = {
        visible: !document.querySelector('#loading').hidden,
        text: document.querySelector('#status').textContent,
        progressHidden: document.querySelector('#progress').hidden,
        progressValue: document.querySelector('#progress').value,
        panelRole: loading.getAttribute('role'),
        statusRole: document.querySelector('#status').getAttribute('role'),
        panelTabIndex: loading.tabIndex,
        focused: document.activeElement === loading
      };
      const event = new Event('webglcontextlost', {cancelable: true});
      document.querySelector('#canvas').dispatchEvent(event);
      return {runtime, graphics: {
        visible: !document.querySelector('#loading').hidden,
        text: document.querySelector('#status').textContent,
        prevented: event.defaultPrevented
      }};
    });
    assert.deepEqual(recovery, {
      runtime: {
        visible: true,
        text: 'The game stopped after startup. Your browser state remains open. Check the browser console.',
        progressHidden: true,
        progressValue: 0,
        panelRole: 'alert',
        statusRole: 'status',
        panelTabIndex: -1,
        focused: true
      },
      graphics: {visible: true, text: 'The graphics context was lost. Reload the page to continue.', prevented: true}
    });
    await page.screenshot({path: path.join(output, 'graphics-recovery.png')});
    /* Desktop checks are complete. Release its running game before mobile
       startup so the phone fixture has its own browser resource budget. */
    await context.close();
    const phone = await browser.newContext({viewport: {width: 390, height: 844},
      hasTouch: true, isMobile: true, deviceScaleFactor: 3});
    const mobile = await phone.newPage();
    mobile.on('pageerror', error => errors.push(error.message));
    try {
      await mobile.goto(`http://127.0.0.1:${server.address().port}/`);
      await mobile.waitForFunction(() => window.Module?.crownlessScreen === 'title' && Module.crownlessTouchFrame?.buttons.length);
      await mobile.evaluate(() => Module.setCrownlessSaveStatus(
        'could not save. Your journal and scene remain in this tab. Reload after checking browser storage.', 'failed'));
      await assertSaveStatusLane(mobile, 390, 844);
      await mobile.screenshot({path: path.join(output, 'save-lane-portrait.png')});
      const controls = gameControls(mobile, true);
      assert.equal(await mobile.locator('#touch-actions').count(), 1);
      const semantics = await mobile.evaluate(() => {
        const panel = document.querySelector('#touch-actions');
        const actions = panel.querySelector('div');
        const loading = document.querySelector('#loading');
        const stage = document.querySelector('#stage');
        const frame = {
          title: 'Mine yard', detail: 'Carriage beside the mine road.',
          reading: 'Pack 2 of 8. Cart 4 of 12.', revision: 701,
          buttons: [
            {label: 'Inspect Bread x8', enabled: true, active: false},
            {label: 'Unload 1', enabled: false, active: false}
          ]
        };
        const activate = Module._CrownlessTouchActivate;
        window.semanticActivations = [];
        Module._CrownlessTouchActivate = (index, revision) =>
          window.semanticActivations.push([index, revision]);
        Module.renderCrownlessTouch(frame);
        const first = actions.querySelector('button');
        first.focus();
        const normal = first.getBoundingClientRect();
        first.click();
        Module.renderCrownlessTouch({...frame, revision: 702});
        const retained = document.activeElement === first;
        Module.renderCrownlessTouch({...frame, revision: 703,
          buttons: [
            {label: 'Inspect Bread x7', enabled: true, active: false},
            {label: 'Unload 1', enabled: false, active: false}
          ]});
        const removalFocus = document.activeElement === panel.querySelector('h2');
        const changed = actions.querySelector('button');
        changed.focus();
        const content = {
          title: panel.querySelector('h2').textContent,
          detail: panel.querySelectorAll('p')[0].textContent,
          reading: panel.querySelectorAll('p')[1].textContent,
          detailsOpen: panel.querySelector('details').open,
          actionCount: actions.querySelectorAll('button').length,
          actionsNested: [...actions.querySelectorAll('button')].every(button => button.parentElement === actions),
          disabled: actions.querySelectorAll('button')[1].disabled
        };
        stage.classList.add('expanded');
        const expanded = changed.getBoundingClientRect();
        const expandedVisible = expanded.width > 20 && expanded.height > 20 &&
          stage.contains(panel) && expanded.top >= stage.getBoundingClientRect().top &&
          expanded.bottom <= stage.getBoundingClientRect().bottom;
        stage.classList.remove('expanded');
        loading.hidden = false;
        Module.renderCrownlessTouch(frame);
        loading.focus();
        const recovery = panel.hidden && document.activeElement === loading;
        loading.hidden = true;
        Module.renderCrownlessTouch(Module.crownlessTouchFrame);
        Module._CrownlessTouchActivate = activate;
        return {...content,
          activation: window.semanticActivations,
          normalVisible: normal.width > 20 && normal.height > 20,
          normalInView: normal.top >= panel.getBoundingClientRect().top &&
            normal.bottom <= panel.getBoundingClientRect().bottom,
          retained, removalFocus, expandedVisible, recovery
        };
      });
      assert.deepEqual(semantics, {
        title: 'Mine yard', detail: 'Carriage beside the mine road.',
        reading: 'Pack 2 of 8. Cart 4 of 12.', actionCount: 2, actionsNested: true,
        detailsOpen: false, disabled: true, activation: [[0, 701]], normalVisible: true,
        normalInView: true,
        retained: true, removalFocus: true, expandedVisible: true, recovery: true
      });
      for (const [width, height] of [[320, 740], [390, 844], [667, 375], [844, 390], [1024, 768]]) {
        await mobile.setViewportSize({width, height});
        await mobile.waitForTimeout(100);
        assert(await mobile.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1));
        const canvas = await mobile.locator('#canvas').boundingBox();
        assert(Math.abs(canvas.width / canvas.height - 16 / 9) < 0.01);
        assert(canvas.y + canvas.height <= height + 1);
        const readable = await mobile.evaluate(() => {
          const portrait = innerHeight > innerWidth && innerWidth <= 600;
          const panel = document.querySelector('#touch-actions');
          const bounds = panel.getBoundingClientRect();
          const buttons = [...panel.querySelectorAll('button')].map(button => {
            const box = button.getBoundingClientRect();
            return {width: box.width, height: box.height};
          });
          return {portrait, visible: bounds.width > 100 && bounds.height > 100,
            fontSize: parseFloat(getComputedStyle(panel).fontSize), buttons};
        });
        if (readable.portrait) {
          assert(readable.visible, JSON.stringify(readable));
          assert(readable.fontSize >= 16, JSON.stringify(readable));
          assert(readable.buttons.every(button => button.width >= 44 && button.height >= 44),
            JSON.stringify(readable));
        }
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
      const firstTownAction = await mobile.evaluate(() => {
        const panel = document.querySelector('#touch-actions');
        const action = panel.querySelector('.touch-buttons button');
        const details = panel.querySelector('.touch-scene-details');
        const panelBounds = panel.getBoundingClientRect();
        const actionBounds = action.getBoundingClientRect();
        return {panelBottom: panelBounds.bottom, actionTop: actionBounds.top,
          actionBottom: actionBounds.bottom,
          beforeDetails: actionBounds.bottom <= details.getBoundingClientRect().top};
      });
      assert(firstTownAction.actionTop >= 0 &&
        firstTownAction.actionBottom <= firstTownAction.panelBottom &&
        firstTownAction.beforeDetails, JSON.stringify(firstTownAction));
      await mobile.screenshot({path: path.join(output, 'mobile-nearby-cards.png')});
      await controls.button('Talk Mara Venn').tap();
      await controls.button(/^1 What do they need\?/).waitFor();
      assert.equal(await mobile.locator('#touch-actions details').getAttribute('open'), '');
      let visibleChoices = (await controls.buttons()).map(button => button.label);
      assert(visibleChoices.some(label => /^1 What do they need\?/.test(label)), JSON.stringify(visibleChoices));
      assert(visibleChoices.some(label => /^2 Not now\./.test(label)), JSON.stringify(visibleChoices));
      let firstChoice = mobile.locator('#touch-actions button').filter({hasText: /^1 What do they need\?/});
      await firstChoice.focus();
      await mobile.keyboard.press('Enter');
      await controls.button(/^1 I'll take the job\./).waitFor();
      visibleChoices = (await controls.buttons()).map(button => button.label);
      assert(visibleChoices.some(label => /^1 I'll take the job\./.test(label)), JSON.stringify(visibleChoices));
      assert(visibleChoices.some(label => /^2 Not now\./.test(label)), JSON.stringify(visibleChoices));
      firstChoice = mobile.locator('#touch-actions button').filter({hasText: /^1 I'll take the job\./});
      await firstChoice.focus();
      await mobile.keyboard.press('Enter');
      await controls.button(/^1 Not now\./).waitFor();
      await mobile.locator('#touch-actions button').filter({hasText: /^1 Not now\./}).focus();
      await mobile.keyboard.press('Enter');
      await controls.button('Talk Mara Venn').waitFor();
      const oldControl = await controls.button('Menu').read();
      const bookButton = mobile.locator('#touch-actions button').filter({hasText: /^Book/});
      await bookButton.focus();
      await mobile.keyboard.press('Enter');
      await mobile.waitForFunction(() => Module.crownlessTouchFrame.title === 'Company Book');
      await mobile.evaluate(({index, revision}) => Module._CrownlessTouchActivate(index, revision), oldControl);
      await mobile.waitForTimeout(150);
      assert.equal(await mobile.locator('#canvas').getAttribute('aria-label'), 'Company Book');
      const bookReading = await controls.reading();
      assert(bookReading.length > 30);
      assert.match(bookReading, /food boxes are aboard/i);
      await mobile.screenshot({path: path.join(output, 'mobile-book.png')});

      const backButton = mobile.locator('#touch-actions button').filter({hasText: /^Back/});
      await backButton.focus();
      await mobile.keyboard.press('Enter');
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
