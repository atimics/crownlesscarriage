const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const { chromium } = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');

async function main() {
  const root = path.resolve(__dirname, '..');
  const output = path.resolve(process.argv[2] || 'browser-results');
  const routes = new Set(['/tests/fixtures/music-playback.html', '/tests/fixtures/music-playback.js',
    '/tests/fixtures/music-meter.js', '/web/music.js']);
  const server = http.createServer(async (request, response) => {
    const route = new URL(request.url, 'http://localhost').pathname;
    if (!routes.has(route)) { response.writeHead(404).end(); return; }
    try {
      response.setHeader('Content-Type', route.endsWith('.html') ? 'text/html' : 'text/javascript');
      response.end(await fs.readFile(path.join(root, route)));
    } catch { response.writeHead(500).end(); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch();
  try {
    const page = await browser.newPage();
    await page.goto(`http://127.0.0.1:${server.address().port}/tests/fixtures/music-playback.html`);
    await page.getByRole('button', { name: 'Run playback check' }).click();
    await page.waitForFunction(() => ['passed', 'failed'].includes(document.querySelector('#result').dataset.state));
    const text = await page.locator('#result').textContent();
    assert.equal(await page.locator('#result').getAttribute('data-state'), 'passed', text);
    const result = JSON.parse(text);
    await fs.mkdir(output, { recursive: true });
    await fs.writeFile(path.join(output, 'music-playback.json'), JSON.stringify(result, null, 2));
    console.log('Browser music stays continuous through a 700 ms page stall:', result);
  } finally {
    await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
