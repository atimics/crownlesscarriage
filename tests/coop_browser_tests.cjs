const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const os = require('node:os');
const {spawn, execFileSync} = require('node:child_process');
const {gameControls} = require('./game_controls.cjs');
const {chromium} = require(process.env.CC_PLAYWRIGHT_MODULE || 'playwright');

async function main() {
  const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'crownless-coop-browser-'));
  const deployed = Boolean(process.env.CC_COOP_ORIGIN);
  const origin = (process.env.CC_COOP_ORIGIN || 'http://127.0.0.1:8788').replace(/\/$/, '');
  const host = deployed ? null : spawn(process.env.CC_COOP_PYTHON || 'python3', [
    'tools/coop/server.py', '--library', path.resolve(process.argv[2]),
    '--database', path.join(temp, 'worlds.sqlite3'), '--port', '8788',
    '--game-dir', path.resolve(process.argv[3])
  ], {stdio: ['ignore', 'inherit', 'inherit']});
  let browser, owner, crew, testWorldId = null;
  try {
    let health;
    for (let i = 0; ; i++) {
      try {
        const response = await fetch(origin + '/healthz');
        if (response.ok) {health = await response.json(); break;}
      } catch {}
      assert(i < 100 && (deployed || host.exitCode === null),
        'Shared host must become ready');
      await new Promise(resolve => setTimeout(resolve, 100));
    }
    browser = await chromium.launch({args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader']});
    const a = await browser.newContext(), b = await browser.newContext({
      viewport: {width: 390, height: 844}, hasTouch: true, isMobile: true});
    owner = await a.newPage(); crew = await b.newPage();
    const ownerControls = gameControls(owner);
    let crewControls = gameControls(crew, true);
    /* The host hands the invitation link to the share sheet. Headless Chromium has none, so record it. */
    await owner.addInitScript(() => {
      Object.defineProperty(navigator, 'share', {configurable: true, value: async data => { window.sharedInvitation = data.url; }});
    });
    await owner.goto(origin);
    assert.equal(await owner.getByRole('link').count(), 1);
    await owner.getByRole('link', {name:'Start', exact:true}).click();
    await owner.waitForFunction(() => window.Module?.crownlessScreen === 'title', undefined, {timeout:120000});
    await ownerControls.button('Online').click();
    await owner.waitForFunction(() => Module.crownlessTouchFrame.buttons.some(button => button.label === 'Online' && button.active));
    await ownerControls.button('Play').click();
    await owner.waitForFunction(() => Module.crownlessScreen === 'worlds');
    await ownerControls.button('Create world').click();
    await owner.getByRole('textbox', {name:'Your name', exact:true}).fill('Mara');
    await owner.getByRole('textbox', {name:'World name', exact:true}).fill('Shared test road');
    await owner.getByLabel('World pass', {exact:true}).fill('0'.repeat(64));
    await ownerControls.button('Create world').click();
    await owner.waitForFunction(() => Module.crownlessTouchFrame.detail.includes('fresh world pass'));
    const worldPass = deployed ? process.env.CC_COOP_WORLD_PASS :
      execFileSync(process.env.CC_COOP_PYTHON || 'python3', [
      'tools/coop/server.py', '--database', path.join(temp, 'worlds.sqlite3'), '--issue-world-pass'
    ], {encoding:'utf8'}).trim();
    assert.match(worldPass || '', /^[a-f0-9]{64}$/,
      'A fresh single-use world pass is required for this test');
    await owner.getByLabel('World pass', {exact:true}).fill(worldPass);
    await ownerControls.button('Create world').click();
    await owner.waitForFunction(() => Module.crownlessScreen === 'company');
    assert.equal(await owner.locator('input').count(), 0);
    await ownerControls.button('Invite crew').click();
    await owner.waitForFunction(() => typeof window.sharedInvitation === 'string');
    const invitation = await owner.evaluate(() => window.sharedInvitation);
    assert.equal(await owner.evaluate(() => Module.crownlessScreen), 'company');
    await owner.waitForFunction(() => Module.crownlessTouchFrame.detail.includes('Invitation shared'));
    const worldId = new URL(invitation).hash.slice('#join='.length).split('.')[0];
    testWorldId = worldId;
    assert.equal(new URL(invitation).origin, origin);
    assert.match(new URL(invitation).hash, /^#join=[a-f0-9]{32}\.[a-f0-9]{64}$/);
    await fs.mkdir('browser-results', {recursive:true});
    await owner.screenshot({path:'browser-results/in-game-invitation.png'});
    /* Without a share sheet or clipboard the link is still shown in full for hand copying. */
    await owner.evaluate(() => {
      Object.defineProperty(navigator, 'share', {configurable: true, value: undefined});
      Object.defineProperty(navigator, 'clipboard', {configurable: true, value: {writeText: async () => { throw new Error('blocked'); }}});
    });
    await ownerControls.button('Invite crew').click();
    await owner.waitForFunction(() => Module.crownlessScreen === 'invitation');
    assert.equal(await owner.getByRole('textbox', {name:'Invitation', exact:true}).inputValue(), invitation);
    await owner.screenshot({path:'browser-results/in-game-invitation-link.png'});
    await crew.goto(invitation);
    await crew.getByRole('link', {name:'Start', exact:true}).click();
    await crew.waitForFunction(() => window.Module?.crownlessScreen === 'join', undefined, {timeout:120000});
    await crew.getByRole('textbox', {name:'Your name', exact:true}).fill('Bren');
    await crewControls.button('Join company').tap();
    await crew.waitForFunction(() => Module.crownlessScreen === 'company');
    await ownerControls.button('Back').click();
    await ownerControls.button('Refresh crew').click();
    await owner.waitForFunction(() => Module.crownlessTouchFrame.buttons.some(button => button.label.includes('Bren')));
    await owner.screenshot({path:'browser-results/in-game-company.png'});
    const token = await crew.evaluate(() => localStorage.getItem('cc-coop-token'));
    async function state() {
      const response = await fetch(`${origin}/api/worlds/${worldId}/state?campaign=1`, {headers:{Authorization:`Bearer ${token}`}});
      assert(response.ok, `Shared state returned ${response.status}`);
      return response.json();
    }
    let game = crew;
    const errors = [];
    game.on('pageerror', error => errors.push(error.message));
    const initial = await state();
    const receipt = {
      environment: deployed ? 'deployed-service' : 'local-built-site',
      commit: execFileSync('git', ['rev-parse', 'HEAD'], {encoding:'utf8'}).trim(),
      deployment_revision: deployed ? health.revision : null,
      world_id: worldId,
      seed: initial.state.seed,
      players: initial.crew.map(player => ({id:player.id, name:player.name})),
      checkpoints: [], road_choices: []
    };
    assert.equal(receipt.players.length, 2);
    assert.notEqual(receipt.players[0].id, receipt.players[1].id);
    const journeySignature = world => ({
      route: world.journey.route,
      direction: world.road_position?.direction,
      progress: world.journey.progress,
      choices: (world.road_position?.next_legs || []).map(leg => leg.token),
      company_hash: world.hash,
      day: world.day,
      minute: world.minute
    });
    async function checkpoint(label) {
      let authoritative;
      const syncSamples = [];
      for (let attempt = 0; attempt < 30; ++attempt) {
        authoritative = await state();
        const browsers = await Promise.all([owner, game].map(page =>
          page.evaluate(() => ({hash:document.body.dataset.companyHash,
            ready:document.body.dataset.companyReady,
            sync:Module.ccCoop.syncStatus()}))));
        const sample = {host:{revision:authoritative.revision,
          action_revision:authoritative.action_revision,
          hash:authoritative.state.hash,
          travel_stopped:authoritative.travel_stopped}, browsers};
        if (attempt === 0 || JSON.stringify(sample) !== JSON.stringify(syncSamples.at(-1)))
          syncSamples.push(sample);
        if (browsers.every(browser => browser.hash === authoritative.state.hash)) break;
        await new Promise(resolve => setTimeout(resolve, 1000));
      }
      const synced = syncSamples.at(-1);
      assert(synced.browsers.every(browser => browser.hash === synced.host.hash),
        `Shared ${label} did not converge: ${JSON.stringify(syncSamples)}`);
      const signature = journeySignature(authoritative.state);
      for (const page of [owner, game]) {
        const peer = await page.evaluate(async id => {
          const token = localStorage.getItem('cc-coop-token');
          const response = await fetch(`/api/worlds/${id}/state?campaign=1`,
            {headers:{Authorization:`Bearer ${token}`}});
          if (!response.ok) throw new Error(`State request returned ${response.status}`);
          return response.json();
        }, worldId);
        assert.deepEqual(journeySignature(peer.state), signature);
      }
      receipt.checkpoints.push({label, revision:authoritative.revision,
        action_revision:authoritative.action_revision, ...signature,
        browser_sync:synced.browsers.map(browser => ({
          revision:browser.sync.revision, hash:browser.hash,
          ready:browser.ready}))});
      return authoritative;
    }
    await ownerControls.button('Enter world').click();
    await crewControls.button('Enter world').tap();
    for (const page of [owner, game]) {
      await page.waitForFunction(() => document.body.dataset.companyReady === 'ready' && document.body.dataset.playerPosition, undefined, {timeout:120000});
    }
    async function selectMenuItem(page, index) {
      await page.locator('#canvas').focus();
      for (let step = 0; await page.evaluate(() => Module.crownlessMenuFocus) !== index; ++step) {
        assert(step < 16, 'Menu item must be reachable');
        const previous = await page.evaluate(() => Module.crownlessMenuFocus);
        await page.keyboard.press('ArrowDown');
        await page.waitForFunction(value => Module.crownlessMenuFocus !== value, previous);
      }
      const draft = await page.evaluate(() => Module.crownlessScreen === 'avatar' ? Module.crownlessAvatarDraft : null);
      await page.keyboard.press('Enter');
      if (draft !== null && index < 5) await page.waitForFunction(value => Module.crownlessAvatarDraft !== value, draft);
    }
    await game.locator('#canvas').focus();
    await game.keyboard.press('Escape');
    await game.waitForFunction(() => Module.crownlessScreen === 'paused');
    await selectMenuItem(game, 9);
    await game.waitForFunction(() => Module.crownlessScreen === 'avatar');
    await selectMenuItem(game, 4);
    await selectMenuItem(game, 4);
    await selectMenuItem(game, 4);
    await selectMenuItem(game, 1);
    await selectMenuItem(game, 1);
    await selectMenuItem(game, 1);
    await selectMenuItem(game, 1);
    await selectMenuItem(game, 2);
    await selectMenuItem(game, 2);
    await selectMenuItem(game, 2);
    await selectMenuItem(game, 2);
    await selectMenuItem(game, 5);
    await game.waitForFunction(() => Module.crownlessScreen === 'paused');
    assert.deepEqual((await state()).appearance, {skin:0, hair:4, style:4, face:0, coat:3});
    await selectMenuItem(game, 0);
    await game.waitForFunction(() => Module.crownlessScreen === 'playing');
    /* The playing screen is published before the next frame refreshes the avatar. */
    await game.waitForFunction(() => document.body.dataset.avatar === '12576');
    assert.equal(await game.locator('body').getAttribute('data-avatar'), '12576');
    assert.equal(await game.locator('#touch-panel').count(), 0);
    assert(await game.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
    assert.equal(await owner.locator('body').getAttribute('data-avatar'), '0');
    for (const [page, name, appearance] of [[owner, 'Bren', 12576], [game, 'Mara', 0]]) {
      await page.waitForFunction(({name, appearance}) => {
        const drawn = JSON.parse(document.body.dataset.crewDrawn || '[]');
        return drawn.length === 1 && drawn[0].name === name && drawn[0].appearance === appearance;
      }, {name, appearance}, {timeout:30000});
    }
    await fs.mkdir('browser-results', {recursive:true});
    await owner.screenshot({path:'browser-results/visible-crew.png'});
    const remoteStart = JSON.parse(await owner.locator('body').getAttribute('data-crew-drawn'))[0].position;
    const purchased = await game.evaluate(() => Module.ccCoop.apply('trade', '0', 0, 1));
    assert.equal(purchased.accepted, true);
    assert.equal(purchased.world.state.company.cargo[0], 1);
    const atTown = await state();
    for (const page of [owner, game]) await page.waitForFunction(hash => document.body.dataset.companyHash === hash, atTown.state.hash);
    const start = await game.locator('body').getAttribute('data-player-position');
    const canvas = await game.locator('#canvas').boundingBox();
    // The road crosses the middle of this view; the lower edge is a cliff.
    await game.touchscreen.tap(canvas.x + canvas.width * 0.68, canvas.y + canvas.height * 0.50);
    await game.waitForFunction(position => document.body.dataset.playerPosition !== position, start);
    await owner.waitForFunction(position => {
      const peer = JSON.parse(document.body.dataset.crewDrawn || '[]')[0];
      return peer && Math.hypot(peer.position[0] - position[0], peer.position[2] - position[2]) > 0.1;
    }, remoteStart);
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready' && document.body.dataset.playerPosition, undefined, {timeout:120000});
    assert.notEqual(await game.locator('body').getAttribute('data-player-position'), start);
    /* The playing screen is published before the next frame refreshes the avatar. */
    await game.waitForFunction(() => document.body.dataset.avatar === '12576');
    assert.equal(await game.locator('body').getAttribute('data-avatar'), '12576');
    await owner.waitForFunction(() => JSON.parse(document.body.dataset.crewDrawn || '[]')[0]?.name === 'Bren');
    await game.locator('#canvas').focus();
    await game.keyboard.press('Escape');
    await game.waitForFunction(() => Module.crownlessScreen === 'paused');
    await selectMenuItem(game, 11);
    await game.waitForFunction(() => Module.crownlessScreen === 'company');
    assert(await crewControls.button('Pause shared world').isDisabled());
    await crewControls.button('Return to the road').tap();
    await game.waitForFunction(() => Module.crownlessScreen === 'playing');
    await owner.waitForFunction(() => JSON.parse(document.body.dataset.crewDrawn || '[]')[0]?.name === 'Bren');
    const destination = (await state()).state.travel[0].id;
    const departed = await game.evaluate(target => Module.ccCoop.apply('travel', target, 0, 0), destination);
    assert.equal(departed.accepted, true);
    await owner.locator('#canvas').focus();
    await owner.keyboard.press('Escape');
    await owner.waitForFunction(() => Module.crownlessScreen === 'paused');
    await selectMenuItem(owner, 12);
    await owner.waitForFunction(() => Module.ccCoop.paused());
    const travelling = await state();
    assert.equal(travelling.state.journey.active, true);
    for (const page of [owner, game]) await page.waitForFunction(hash => document.body.dataset.companyHash === hash, travelling.state.hash);
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready', undefined, {timeout:120000});
    await game.waitForFunction(hash => document.body.dataset.companyHash === hash, travelling.state.hash);
    await owner.evaluate(() => Module.ccCoop.togglePause());
    for (const page of [owner, game]) await page.waitForFunction(() => !Module.ccCoop.paused());
    await ownerControls.button('Resume').click();
    await owner.waitForFunction(() => Module.crownlessScreen === 'playing');
    await ownerControls.button('Drive to Gloamgate').waitFor();
    const beforeChoice = await checkpoint('road-choice-before-reload');
    assert(beforeChoice.state.road_position.next_legs.length > 0);
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready',
      undefined, {timeout:120000});
    const afterChoice = await checkpoint('road-choice-after-reload');
    assert.deepEqual(journeySignature(afterChoice.state),
      journeySignature(beforeChoice.state));
    await ownerControls.button('Drive to Gloamgate').click();
    await ownerControls.button('Stop').click();
    await ownerControls.button('Road options').click();
    const goalDirection = beforeChoice.state.road_position.direction;
    // These fixed pilot-road IDs match cc_road_position.h.
    const junctionId = BigInt('0x7200032300000001').toString();
    const millSegmentId = BigInt('0x7300032300000003').toString();
    const onwardLeg = road => road?.next_legs?.find(leg =>
      leg.direction === goalDirection && leg.segment !== millSegmentId);
    const returnFromMill = road => road?.next_legs?.length === 1 &&
      road.next_legs[0].kind === 2 &&
      road.next_legs[0].segment === millSegmentId &&
      road.next_legs[0].destination === junctionId &&
      road.next_legs[0].direction === -goalDirection ?
      road.next_legs[0] : null;
    function recordRoadChoice(label, current, leg, result) {
      const road = current.road_position;
      const candidates = road.next_legs.map(candidate => ({
        kind:candidate.kind, direction:candidate.direction,
        segment:candidate.segment,
        destination:candidate.destination, token:candidate.token}));
      const detail = {label, route:current.journey.route, goal:road.goal,
        anchor:road.anchor, candidates, selected:leg.token};
      console.log('shared road choice', JSON.stringify(detail));
      receipt.road_choices.push(detail);
      assert(candidates.some(candidate => candidate.token === leg.token));
      assert.equal(road.goal, current.journey.destination);
      assert.equal(result.world.state.journey.route, current.journey.route);
      assert.equal(result.world.state.road_position.goal, road.goal,
        'The chosen leg keeps the route connected to Gloamgate');
    }
    function assertHostAdvancedAfterRejectedSkip(result, sampled) {
      const latest = result.world?.state;
      assert(latest, `Rejected Skip watch needs the latest host state: ${result.message}`);
      const signature = world => ({active:world.journey.active,
        phase:world.journey.phase, stop:world.journey.stop,
        watch:world.journey.watch, progress:world.journey.progress,
        road_revision:world.road_position?.revision});
      assert(!latest.journey.active || latest.journey.phase !== 1,
        `Rejected Skip watch must have left the travelling phase: ` +
          JSON.stringify({message:result.message, sampled:signature(sampled),
            latest:signature(latest)}));
      assert.notDeepEqual(signature(latest), signature(sampled),
        `Rejected Skip watch left the same road state: ` +
          JSON.stringify({message:result.message, sampled:signature(sampled),
            latest:signature(latest), choices:latest.road_position?.next_legs}));
    }
    let firstWorld = null;
    for (let step = 0; step < 32 && firstWorld === null; ++step) {
      const current = (await state()).state;
      if (current.journey.road_site) {
        firstWorld = current;
        break;
      }
      const leg = current.journey.phase === 4 ?
        onwardLeg(current.road_position) : null;
      assert(current.journey.phase !== 4 || leg,
        `The first road choice must lead toward Gloamgate: ` +
          JSON.stringify({goalDirection, journey:current.journey,
            road:current.road_position}));
      if (leg) {
        const previousRevision = current.road_position.revision;
        const chosen = await owner.evaluate(token =>
          Module.ccCoop.apply('road_leg', token, 0, 0), leg.token);
        assert.equal(chosen.accepted, true, chosen.message);
        recordRoadChoice('first-site-onward', current, leg, chosen);
        assert.notEqual(chosen.world.state.road_position.revision, previousRevision,
          'The exact current road token advances the shared revision');
        continue;
      }
      const advanced = await owner.evaluate(() =>
        Module.ccCoop.apply('skip_watch', '0', 0, 0));
      if (!advanced.accepted) {
        assertHostAdvancedAfterRejectedSkip(advanced, current);
        continue;
      }
      if (advanced.world.state.journey.road_site) firstWorld = advanced.world.state;
    }
    assert(firstWorld?.journey.road_site, 'A nearby stop is offered');
    const firstJourney = firstWorld.journey;
    const siteLeg = onwardLeg(firstWorld.road_position);
    assert(siteLeg, 'The physical road site offers a current onward token');
    const siteRevision = firstWorld.road_position.revision;
    const leftSite = await owner.evaluate(token =>
      Module.ccCoop.apply('road_leg', token, 0, 0), siteLeg.token);
    assert.equal(leftSite.accepted, true, leftSite.message);
    recordRoadChoice('leave-first-site', firstWorld, siteLeg, leftSite);
    assert.notEqual(leftSite.world.state.road_position.revision, siteRevision,
      'The exact site token resolves the physical road choice');
    // Resolving the physical site keeps the shared carriage moving.
    await game.waitForFunction(progress => {
      const reading = Module.crownlessTouchFrame?.reading || '';
      return !reading.includes('Press on') && !reading.includes('Fast forward') &&
        document.body.dataset.companyHash;
    }, firstJourney.progress);
    await game.waitForTimeout(1800);
    const moving = (await state()).state.journey;
    assert(moving.progress > firstJourney.progress || !moving.active, JSON.stringify({firstJourney,moving}));
    assert((await crewControls.buttons()).every(button => !/Press on|Fast forward|Continue on the road/.test(button.label)));
    let afternoon = null;
    let millVisits = 0;
    for (let step = 0; step < 80; ++step) {
      const current = (await state()).state;
      if (current.journey.active && current.journey.phase === 1 &&
          current.journey.stop === 0 && current.journey.watch % 2 === 0 &&
          !current.journey.road_site) {
        const held = await owner.evaluate(route =>
          Module.ccCoop.apply('stop_travel', String(route), 0, 0),
        current.journey.route);
        assert(held.accepted, held.message);
        assert(held.world.travel_stopped,
          'The host holds the carriage before the afternoon camp choice');
        afternoon = held.world.state;
        break;
      }
      assert(current.journey.active, 'The company must reach the afternoon road watch');
      if (current.journey.stop === 1) {
        const result = await owner.evaluate(() => Module.ccCoop.apply('press_on', '0', 0, 0));
        if (!result.accepted) {
          const latest = result.world?.state?.journey || (await state()).state.journey;
          assert.equal(result.message, 'The carriage is not waiting at a travel stop.',
            JSON.stringify({message:result.message, sampled:current.journey, latest}));
          assert(latest.stop !== current.journey.stop ||
            latest.phase !== current.journey.phase ||
            latest.watch !== current.journey.watch ||
            latest.progress !== current.journey.progress,
          `A rejected Press on must have crossed the sampled watch: ` +
            JSON.stringify({sampled:current.journey, latest}));
        }
        continue;
      }
      const road = current.road_position;
      const mill = road?.anchor === junctionId && onwardLeg(road) &&
        road.next_legs.find(leg => leg.kind === 3 &&
          leg.segment === millSegmentId &&
          leg.direction === goalDirection);
      const onward = onwardLeg(road);
      if (current.journey.phase === 4 || current.journey.road_site) {
        const choice = mill && millVisits < 1 ? mill :
          onward || returnFromMill(road);
        assert(choice, JSON.stringify({journey:current.journey, road}));
        if (choice === mill) millVisits++;
        const result = await owner.evaluate(token =>
          Module.ccCoop.apply('road_leg', token, 0, 0), choice.token);
        assert(result.accepted, result.message);
        recordRoadChoice(choice === mill ? 'mill-visit' :
          choice === onward ? 'toward-goal' : 'return-from-mill',
        current, choice, result);
        continue;
      }
      const result = await owner.evaluate(() =>
        Module.ccCoop.apply('skip_watch', '0', 0, 0));
      if (!result.accepted)
        assertHostAdvancedAfterRejectedSkip(result, current);
    }
    assert(afternoon, 'The road offers an afternoon camp watch');
    if (await ownerControls.button('Back to road').read())
      await ownerControls.button('Back to road').click();
    const stoppedForCamp = await checkpoint('afternoon-stop');
    assert.equal(stoppedForCamp.travel_stopped, true);
    assert.equal(stoppedForCamp.state.journey.route, afternoon.journey.route);
    assert.equal(stoppedForCamp.state.journey.progress, afternoon.journey.progress);
    assert.equal(stoppedForCamp.state.journey.watch, afternoon.journey.watch,
      'Camp remains at the stopped afternoon watch');
    await ownerControls.button('Save').click();
    await owner.screenshot({path:'browser-results/shared-afternoon-stop.png'});
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready',
      undefined, {timeout:120000});
    const reloadedStop = await checkpoint('afternoon-stop-reloaded');
    assert.deepEqual(journeySignature(reloadedStop.state),
      journeySignature(stoppedForCamp.state));
    await crewControls.button('Road options').waitFor();
    await crewControls.button('Road options').tap();
    await crewControls.button('Camp until morning').waitFor();
    assert((await crewControls.button('Camp until morning').read()).enabled,
      'The second player sees the same camp choice');
    if (await ownerControls.button('Road options').read())
      await ownerControls.button('Road options').click();
    await ownerControls.button('Camp until morning').waitFor();
    const campResponse = owner.waitForResponse(response =>
      response.url().includes(`/api/worlds/${worldId}/command`) &&
      response.request().postDataJSON()?.action === 'camp');
    await ownerControls.button('Camp until morning').click();
    const campReceipt = await (await campResponse).json();
    assert.equal(campReceipt.accepted, true, campReceipt.message);
    const camped = await checkpoint('camped');
    assert.equal(camped.state.journey.progress,
      stoppedForCamp.state.journey.progress);
    assert.equal((camped.state.day - stoppedForCamp.state.day) * 1440 +
      camped.state.minute - stoppedForCamp.state.minute, 480);
    assert.equal(camped.travel_stopped, true);
    await owner.screenshot({path:'browser-results/shared-road-camp.png'});
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready',
      undefined, {timeout:120000});
    const reloadedCamp = await checkpoint('camped-reloaded');
    assert.deepEqual(journeySignature(reloadedCamp.state),
      journeySignature(camped.state));
    const crewUrl = game.url();
    await game.close();
    game = await b.newPage();
    crew = game;
    crewControls = gameControls(game, true);
    game.on('pageerror', error => errors.push(error.message));
    await game.goto(crewUrl);
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready',
      undefined, {timeout:120000});
    const reconnectedCamp = await checkpoint('second-client-reconnected');
    assert.deepEqual(journeySignature(reconnectedCamp.state),
      journeySignature(camped.state));
    await crewControls.button('Travel').waitFor();
    assert((await crewControls.button('Travel').read()).enabled,
      'The reconnected second player sees the same continue choice');
    await game.screenshot({path:'browser-results/shared-reconnected-camp.png'});
    await ownerControls.button('Back to road').click();
    const continuationResponse = owner.waitForResponse(response => {
      if (!response.url().includes(`/api/worlds/${worldId}/command`)) return false;
      return response.request().postDataJSON()?.action === 'resume_travel';
    });
    await ownerControls.button('Travel').click();
    const firstContinue = await continuationResponse;
    const continueCommand = firstContinue.request().postDataJSON();
    const continueReceipt = await firstContinue.json();
    assert.equal(continueReceipt.accepted, true);
    assert.equal(continueReceipt.duplicate, false);
    await crewControls.button('Stop').waitFor();
    await ownerControls.button('Stop').click();
    const beforeRetry = await state();
    const retry = await owner.evaluate(async ({worldId, body}) => {
      const token = localStorage.getItem('cc-coop-token');
      const response = await fetch(`/api/worlds/${worldId}/command`, {
        method:'POST', headers:{'Content-Type':'application/json',
          Authorization:`Bearer ${token}`}, body:JSON.stringify(body)});
      if (!response.ok) throw new Error(`Retry returned ${response.status}`);
      return response.json();
    }, {worldId, body:continueCommand});
    const afterRetry = await state();
    assert.equal(retry.accepted, true);
    assert.equal(retry.duplicate, true);
    assert.equal(retry.sequence, continueReceipt.sequence);
    assert.deepEqual(journeySignature(afterRetry.state),
      journeySignature(beforeRetry.state));
    assert.equal(afterRetry.revision, beforeRetry.revision);
    assert.equal(afterRetry.action_revision, beforeRetry.action_revision);
    receipt.continuation = {command:continueCommand,
      first:{accepted:continueReceipt.accepted, duplicate:continueReceipt.duplicate,
        sequence:continueReceipt.sequence},
      retry:{accepted:retry.accepted, duplicate:retry.duplicate,
        sequence:retry.sequence}, revision:afterRetry.revision};
    await checkpoint('continue-retry-once');
    await ownerControls.button('Travel').click();
    await ownerControls.button('Drive to Gloamgate').click();
    let arrived = null;
    for (let step = 0; step < 120; ++step) {
      const current = (await state()).state;
      if (!current.journey.active) {arrived = current; break;}
      if (await ownerControls.button('Travel on').read())
        await ownerControls.button('Travel on').click();
      else if (await ownerControls.button('Travel').read())
        await ownerControls.button('Travel').click();
      else await owner.waitForTimeout(500);
    }
    assert(arrived, 'Both players reach the route destination');
    assert.equal(arrived.company.location, camped.state.journey.destination);
    await checkpoint('arrived');
    await owner.screenshot({path:'browser-results/shared-arrival.png'});
    // Pause for a stable reload check; the company keeps the same route and pace.
    await owner.evaluate(() => Module.ccCoop.togglePause());
    for (const page of [owner, game]) await page.waitForFunction(() => Module.ccCoop.paused());
    const savedRoad = await state();
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready', undefined, {timeout:120000});
    await game.waitForFunction(hash => document.body.dataset.companyHash === hash, savedRoad.state.hash);
    await owner.evaluate(() => Module.ccCoop.togglePause());
    for (const page of [owner, game]) await page.waitForFunction(() => !Module.ccCoop.paused());
    // The C client reports each death and resets both players after one jump.
    await owner.evaluate(() => Module.ccCoop.togglePause());
    for (const page of [owner, game]) await page.waitForFunction(() => Module.ccCoop.paused());
    const beforeDeath = await state();
    await owner.evaluate(() => Module.ccCoop.life(true));
    await owner.waitForFunction(() => Module.ccCoop.dead());
    await game.waitForTimeout(1000);
    assert.equal((await state()).state.day, beforeDeath.state.day);
    await game.evaluate(() => Module.ccCoop.life(true));
    await owner.evaluate(() => Module.ccCoop.togglePause());
    for (const page of [owner, game]) await page.waitForFunction(() => !Module.ccCoop.paused());
    for (const page of [owner, game]) {
      await page.waitForFunction(() => Module.ccCoop.partyWipes() === 1 &&
        !Module.ccCoop.dead(), undefined, {timeout:30000});
    }
    const nextCompany = await state();
    assert.equal(nextCompany.state.day, beforeDeath.state.day + 7300);
    assert.equal(nextCompany.state.journey.active, false);
    for (const page of [owner, game]) {
      await page.waitForFunction(hash => document.body.dataset.companyHash === hash,
        nextCompany.state.hash);
    }
    await game.reload();
    await game.waitForFunction(() => document.body.dataset.companyReady === 'ready' &&
      Module.ccCoop.partyWipes() === 1 && !Module.ccCoop.dead(), undefined, {timeout:120000});
    assert.equal((await state()).state.day, nextCompany.state.day);
    const deleted = await owner.evaluate(async id => {
      const token = localStorage.getItem('cc-coop-token');
      const response = await fetch(`/api/worlds/${id}/host`, {
        method:'POST', headers:{'Content-Type':'application/json',
          Authorization:`Bearer ${token}`},
        body:JSON.stringify({action:'delete'})});
      if (!response.ok) throw new Error(`World cleanup returned ${response.status}`);
      return response.json();
    }, worldId);
    assert.equal(deleted.deleted, true);
    receipt.cleanup = {deleted:true};
    await fs.writeFile('browser-results/shared-road-receipt.json',
      JSON.stringify(receipt, null, 2));
    assert.deepEqual(errors, []);
    console.log('Desktop and phone players draw each other with their chosen appearance and moving poses; touch walking, leaving, rejoining, reload, continuous shared travel, optional stops, and the twenty-year party death jump pass.');
  } catch (error) {
    await fs.mkdir('browser-results', {recursive:true});
    if (deployed && testWorldId) {
      let deleted = false;
      if (owner && !owner.isClosed()) {
        try {
          deleted = await owner.evaluate(async id => {
            const token = localStorage.getItem('cc-coop-token');
            const response = await fetch(`/api/worlds/${id}/host`, {
              method:'POST', headers:{'Content-Type':'application/json',
                Authorization:`Bearer ${token}`},
              body:JSON.stringify({action:'delete'})});
            return response.ok && (await response.json()).deleted === true;
          }, testWorldId);
        } catch {}
      }
      await fs.writeFile('browser-results/shared-world-cleanup.json',
        JSON.stringify({world_id:testWorldId, deleted,
          needs_cleanup:!deleted}, null, 2));
    }
    for (const [name, page] of [['owner', owner], ['crew', crew]]) {
      if (page && !page.isClosed()) {
        await page.screenshot({path:`browser-results/crew-failure-${name}.png`}).catch(() => {});
        console.error(name, await page.locator('body').getAttribute('data-crew-drawn'));
      }
    }
    throw error;
  } finally {
    if (browser) await browser.close();
    if (host) {
      host.kill('SIGTERM');
      if (host.exitCode === null) await new Promise(resolve => host.once('exit', resolve));
    }
    await fs.rm(temp, {recursive:true, force:true});
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
