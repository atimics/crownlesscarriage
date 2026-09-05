const assert = require('node:assert/strict');

function gameControls(page, touch = false) {
  async function buttons() {
    return page.evaluate(() => Module.crownlessTouchFrame.buttons.map((button, index) => ({
      ...button, index, revision: Module.crownlessTouchFrame.revision,
      label: button.label.replace(/\s*\[(?:F7|Enter|Backspace)\]$/, '').replace(/^\[\d\]\s*/, '').replace(/\s+(?:F5|Esc|Enter|Up|Down|[BQTMH])$/, '').trim()
    })));
  }
  function button(name) {
    const matches = label => name instanceof RegExp ? name.test(label) : label === name;
    async function read() { return (await buttons()).find(item => matches(item.label)); }
    async function waitFor() {
      const deadline = Date.now() + 30000;
      while (!(await read())) {
        assert(Date.now() < deadline, `Game button ${name} must be visible: ${JSON.stringify(await buttons())}`);
        await page.waitForTimeout(100);
      }
    }
    async function click() {
      await waitFor();
      const item = await read();
      assert(item.enabled, `Game button ${name} must be enabled`);
      const canvas = page.locator('#canvas');
      const bounds = await canvas.boundingBox();
      const size = await canvas.evaluate(node => [node.width, node.height]);
      const x = bounds.x + (item.x + item.width / 2) * bounds.width / size[0];
      const y = bounds.y + (item.y + item.height / 2) * bounds.height / size[1];
      if (touch) await page.touchscreen.tap(x, y);
      else await page.mouse.click(x, y, {delay:80});
      await page.waitForTimeout(120);
    }
    return {click, tap: click, waitFor, read, isDisabled: async () => !(await read()).enabled};
  }
  return {button, buttons, reading: () => page.evaluate(() => Module.crownlessTouchFrame.reading)};
}
module.exports = {gameControls};
