const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const {chromium} = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');

async function main() {
  const root = path.resolve(process.argv[2]);
  const output = path.resolve(process.argv[3]);
  await fs.mkdir(output, {recursive: true});
  const server = http.createServer(async (request, response) => {
    try {
      const pathname = new URL(request.url, 'http://localhost').pathname;
      const file = path.resolve(root, '.' + pathname);
      if (!file.startsWith(root + path.sep)) throw new Error('Invalid path');
      const types = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.html': 'text/html'};
      response.setHeader('Content-Type', types[path.extname(file)] || 'application/octet-stream');
      response.end(await fs.readFile(file));
    } catch {
      response.writeHead(404).end();
    }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  let browser;
  try {
    browser = await chromium.launch({args: [
      '--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
    ]});
    const page = await browser.newPage({viewport: {width: 800, height: 600}});
    const errors = [];
    const warnings = [];
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', message => {
      const value = message.text();
      if (value.includes('PASS') || value.includes('Travel leaf')) console.log(value);
      if (message.type() === 'error') errors.push(value);
      if (value.includes('WARNING')) warnings.push(value);
    });
    await page.goto(`http://127.0.0.1:${server.address().port}/capture.html`);
    await page.waitForFunction(() => typeof Module !== 'undefined' && Module.captureReady,
                              null, {timeout: 90000});
    await page.evaluate(() => Module.ccall('CaptureTests', null, [], [], {async: true}));
    for (let town = 0; town < 6; town++) {
      for (const frame of [0, 136, 137, 173, 181, 240]) {
        await page.evaluate(({town, frame}) => Module.ccall('CaptureTown', null,
          ['number', 'number'], [town, frame], {async: true}), {town, frame});
        await page.locator('#canvas').screenshot({
          path: path.join(output, `town-${town}-frame-${frame}.png`),
        });
      }
      console.log(`Captured town ${town}`);
    }
    await fs.writeFile(path.join(output, 'diagnostics.json'), JSON.stringify({errors, warnings}, null, 2));
    if (errors.length) throw new Error(errors.join('\n'));
  } finally {
    if (browser) await browser.close();
    server.close();
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
