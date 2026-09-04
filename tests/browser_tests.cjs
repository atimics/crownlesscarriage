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
    await page.screenshot({path: path.join(output, 'opening.png')});
    const shaders = await page.evaluate(() => window.shaderLinks);
    assert(shaders.every(shader => shader.linked), JSON.stringify(shaders));
    assert(shaders.every(shader => shader.vectors <= 256), JSON.stringify(shaders));
    assert(shaders.some(shader => shader.skinned && shader.vectors === 140), JSON.stringify(shaders));
    await fs.writeFile(path.join(output, 'shaders.json'), JSON.stringify(shaders, null, 2));
    for (const [width, height] of [[800, 600], [1280, 720], [600, 900]]) {
      await page.setViewportSize({width, height});
      await page.locator('#fullscreen').click();
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
      assert(Math.abs(pointer[0] - 640) < 2 && Math.abs(pointer[1] - 360) < 2);
      await page.screenshot({path: path.join(output, `fullscreen-${width}x${height}.png`)});
      await page.evaluate(() => document.exitFullscreen());
      await page.waitForFunction(() => document.fullscreenElement === null);
    }
    await page.setViewportSize({width: 1280, height: 900});
    await page.keyboard.press('Control+s');
    await page.waitForFunction(() => Module.crownlessSaveRevision > 0);
    const revision = await page.evaluate(() => Module.crownlessSaveRevision);
    await page.reload();
    await page.waitForFunction(() => window.Module && Module.crownlessCampaignRestored && document.querySelector('#loading').hidden);
    assert.equal(await page.evaluate(() => Module.crownlessSaveRevision), revision);
    rejectedWrite = true;
    await page.evaluate(() => {
      const transaction = IDBDatabase.prototype.transaction;
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
    assert.deepEqual(errors, []);
    console.log('Browser startup, shader budget, fullscreen, reload, and rejected save checks passed');
  } finally {
    await context.close();
    await browser.close();
    server.close();
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
