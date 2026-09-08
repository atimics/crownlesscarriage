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
    const name = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
    const file = path.resolve(root, '.' + name);
    if (!file.startsWith(root + path.sep)) { response.writeHead(403).end(); return; }
    try {
      const types = {'.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm'};
      response.setHeader('Content-Type', types[path.extname(file)] || 'application/octet-stream');
      response.end(await fs.readFile(file));
    } catch { response.writeHead(404).end(); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  let browser;
  try {
    browser = await chromium.launch({args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader']});
    const page = await browser.newPage();
    const log = [], errors = [];
    page.on('console', message => log.push(message.text()));
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(`http://127.0.0.1:${server.address().port}/matrix-upload.html`);
    await page.waitForFunction(() => globalThis.crownlessMatrixUpload, null, {timeout: 30000});
    const result = await page.evaluate(() => globalThis.crownlessMatrixUpload);
    await fs.writeFile(path.join(output, 'matrix-upload.json'), JSON.stringify({
      browser: browser.version(), ...result, errors, log,
    }, null, 2));
    assert.equal(result.cases, 12);
    assert.equal(result.failures, 0, JSON.stringify({result, log}));
    assert.deepEqual(errors, []);
    console.log('GPU matrix upload: all 12 WebGL readbacks passed.');
  } finally {
    if (browser) await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
