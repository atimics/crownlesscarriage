# sim2 research 05: economy, trade, caravans, banditry, ecology

Rule: only claims from pages I opened (WebFetch returned content). Search-snippet-only items are marked UNVERIFIED.

## Sources read (opened)
1. https://www.raphkoster.com/2006/06/03/uos-resource-system/ - UO resource objects: PRODUCTION = tag, current, max, regrowth rate; creatures eat at "bite size"; eaten-out objects deleted.
2. https://www.raphkoster.com/2006/06/04/uos-resource-system-part-2/ - hunger-driven AI (wolf eats MEAT from rabbits, rabbits eat GRASS), spawn driven by resource surplus, chunk eggs; hoarded FUR by players stopped spawning (closed loop failed); radial searches too slow, creatures had to sleep when no players near.
3. https://www.raphkoster.com/2012/08/17/gdconline-ultima-online-postmortem/ - comments: spawn regions scanned top-left to bottom-right (bias); after a rewrite "spawns were too fast"; dragons eating deer needed intervention.
4. https://en.wikipedia.org/wiki/Gold_sink - passive vs active sinks; UO luxury items sink; Alter Aeon feedback controller that tracks total money and adjusts drop rates and shop prices.
5. https://conceptofprogress.wordpress.com/2012/03/08/eve-a-day-in-the-life-of-the-eve-economy/ - EVE faucets ~1.96T ISK/day (bounties 56%) vs sinks ~1T/day; mild inflation only because of hoarding and dead accounts.
6. https://www.gamewatcher.com/victoria-3-dev-diary-9-national-markets-open-economic-model - Vic3: price from buy vs sell orders around a base price; shortage slows all buildings; no numbers given. (Paradox forum pages blocked by bot check; diary #37 had no formulas.)
7. https://www.paradoxinteractive.com/games/victoria-3/news/dev-diary-37-market-expansion - checked; no formulas, only qualitative. Low value.
8. https://steamcommunity.com/app/33570/discussions/0/2995423424882139318/ - Patrician III player data: beer buy 33-40/sell 50-75; bulky goods 10 cargo slots; per-city demand (e.g. 12 skins/week).
9. https://rimworldwiki.com/wiki/Trade - buy at 140%, sell at 60% of market value; traders camp 10.8-18 hours; base restock 30 days.
10. https://rimworldwiki.com/wiki/Raid_points - raid points = (wealth pts + pawn pts) x difficulty x start x adaptation; wealth 0 pts below 14,000; min 35, max 10,000; adaptation factor 0.4-1.47 falls after deaths (anti-snowball).
11. https://lofigames.com/patch-notes/ - Kenshi: spawn tuned so killing a squad does not spawn endless replacements; town population slowly regenerates when residents die; trade profit raised 50% so it competes with theft; squads wandered off map and got lost.
12. https://www.playthepast.org/?p=5805 - Banished: deaths come from labor shortage; shortage of food/firewood/tools creates death spirals; early-game failure most common.
13. https://ccl.northwestern.edu/netlogo/models/WolfSheepPredation - NetLogo: without grass regrowth the predator-prey model is unstable; with regrowing grass it is generally stable.
14. https://mesa.readthedocs.io/stable/examples/advanced/wolf_sheep.html - Mesa defaults: 100 sheep, 50 wolves, repro 0.04/0.05, wolf food 20, sheep food 4, grass regrow 30 steps, 1 energy/step, 20x20 torus, offspring splits parent energy.
15. https://en.wikipedia.org/wiki/Lotka%E2%80%93Volterra_equations - dx/dt=ax-bxy, dy/dt=-gy+dxy; neutral cycles; carrying capacity and functional response (Rosenzweig-MacArthur) damp them.
16. https://subversion.american.edu/aisaac/notes/sugarscape.html - Sugarscape: growback +1/step capped at max 0-4; metabolism U[1,4]; vision U[1,6]; death at wealth<=0 and replaced by a random newborn (initial wealth U[5,25], max age U[60,100]).
17. https://mesa.readthedocs.io/stable/examples/advanced/epstein_civil_violence.html - Epstein rule: grievance = hardship x (1-legitimacy); rebel if grievance - risk_aversion x P(arrest) > 0.1; P(arrest)=1-exp(-2.3 x floor(cops/actives in vision)); cop density 0.074, citizens 0.7, jail max 30.
18. https://www.jasss.org/19/4/8.html - extortion racket ABM: pay if pP x D >= E; punishment costs capital; rackets collapse when punishment cost is high vs revenue; observed punishments deter neighbours (cheap deterrence); fakers free-ride.
19. https://en.wikipedia.org/wiki/Stationary_bandit_theory - long time horizon -> stationary bandit taxes below total plunder and protects the base; short horizon -> roving plunder.

Opened but unreadable (binary PDF): arxiv 1505.06012 (Sugarscape spec), thinkmind Olson ABM. Seen only in search snippets (UNVERIFIED): Bannerlord "-2 village production within 40 units of a hideout", Bannerlord food/prosperity recession, Kenshi hungry bandits spawn with low hunger and steal food mainly.

## Mechanisms to steal

### Town economy
- Each trade is a production chain with stock per good per town: stock, target stock, max. Price = base x f(stock/target), clamped (for example 0.5x to 3x). Patrician: towns want ~20 days of supply (snippet, UNVERIFIED); Vic3: price from buy vs sell orders. Use stock-ratio pricing, it is O(1) and deterministic. Suggested: price = base * clamp((target/max(stock,1))^0.5, 0.5, 3).
- Regrowth model from UO/Sugarscape/Mesa: every resource node has (amount, max, regrow). Sugarscape: +1 per tick capped at max. Logistic regrowth (r*x*(1-x/K)) beats linear at low stock.
- Town buy/sell spread like RimWorld (buy 140%, sell 60%) makes caravan arbitrage non-infinite and gives a sink for carried money.
- Food is the master good. Consumption per head per day is fixed; if stock < 0 pop loses health. Banished: starvation is a labor spiral, so a baker who dies stops bread. Add a minimum headcount per trade and let town regenerate residents slowly (Kenshi 0.75: population regen when residents die), limited by food stock.
- Hunger drives both bandit recruitment and migration. Use a per-agent energy counter (Mesa, Sugarscape): burn 1/tick, eat to refill, die at 0. Reproduce only above a threshold, parent gives half energy to child (Mesa). This gives a natural carrying capacity without a hard cap.
- Closed loops fail. UO stopped spawning when players hoarded FUR. Every hoard needs a decay or sink and every faucet a cap. Keep an external faucet (seed grain from fields) so the system is not fully closed.
- Anti-snowball damping: RimWorld adaptation factor (0.4-1.47) scales threat by recent losses. Use it on bandit recruitment and goblin raids so one famine does not produce a death spiral.

### Carriage ambush
- Bandit decision (Epstein-style): hungry agent joins bandits if hunger_pressure - risk_aversion * P(caught) > threshold, with P(caught)=1-exp(-k * guards_near/bandits_near). k near 2.3 is the published default; tune.
- Bandit target choice (extortion ABM): attack if P(win) * loot >= expected loss. Loot is from carried crowns/goods, so heavier and poorer-escorted carriages are attacked more, which gives route danger as an emergent map value.
- Record cause on each ambush: {hunger value, loot estimate, guard count seen, roll}. Matches the "every outcome has a cause" requirement.
- Bandits are a flow from towns, not a separate spawner. Kenshi spawn tuning lesson: do not respawn instantly when a band is killed; refill only via the hunger pipeline.
- Short horizon of bandits = roving plunder (Olson). If bandits over-rob, trade drops and hunger rises, which makes more bandits (positive loop). Break it with guard spending and price rise so merchants still profit; Kenshi bumped trade profit +50% to keep trade worth the risk.

### Guard patrols
- Guard count funded by town tax on trade volume (stationary bandit logic: long horizon, protect the base). Patrol cost: upkeep per guard per day from town treasury; if treasury low, fewer guards, more banditry (negative feedback via cost).
- Deterrence by observation (extortion ABM): a visible punishment lowers nearby recruits, so a hanged bandit should be an event that raises P(caught) for neighbours for N days. Cheaper than patrolling everywhere.
- Guard effort should have diminishing returns (cops/actives ratio saturating exp form above).

### Goblin smuggling chain and dragon hoard
- Hoard is a sink with leakage: dragon hoard takes items in, nothing out except theft or dragon death. Per gold-sink survey, use passive decay (tribute paid, tunnel collapse, upkeep) plus an active controller (Alter Aeon: adjust drop rates by total money). Suggested: track world crowns total, raise bandit loot or lower hoard intake when circulating money falls below a target band.
- EVE shows that faucets exceeding sinks by 2x is tolerable only with hoarding and attrition; size the dragon sink to the faucet (mint/loot rate) so inflation/deflation stays bounded. Simple rule: hoard intake rate proportional to (circulating - target).
- Black market: goblin camps pay a spread (like the 140/60 rule, say 120%/50%) for stolen goods so theft is worse than trade unless risk is low; Kenshi raised trade profit specifically to compete with theft.
- Dragon as top predator (UO): big "bite size", leaves small prey; when low-tier food vanishes it attacks settlements. Model dragon appetite as treasure plus cattle; if hoard stalls, raids rise.

### Stability numbers/loops to test
- Lotka-Volterra alone cycles without damping; need prey carrying capacity (grass regrowth) and capped predation. Use Mesa defaults as a starting sanity set: prey repro 0.04, predator 0.05, food value ratio 4:20, regrow 30 steps.
- Run a headless sweep of 1000 seeded worlds and assert: no town at zero pop by day 365, bandit share under 15% of adults (design choice), circulating crowns within 0.5x-2x target. Numbers are proposals, not sourced.

## What to avoid
- Fully closed resource loops with player-side hoards (UO failure).
- Expensive radial searches per agent each tick (UO AI too slow); use per-town tables and event wakeups; sleep agents out of view.
- Scan-order bias in spawn/update loops (UO top-left bias); shuffle with seeded RNG.
- Unbounded predation/offspring without carrying capacity (NetLogo: unstable).
- Instant replacement of killed bandit/monster squads (Kenshi).
- Pure order-book pricing with no numeric guards; Vic3 gave no public formula, so do not copy, use stock-ratio pricing.
- Rebalancing spawn by rewriting the whole system; UO then got "too fast" spawns.

## Open questions
- What is the tick length and trade cycle (day)? Affects all rates above.
- Should prices be per town only or propagate via caravan arbitrage only? (I recommend the second.)
- Crown supply faucet: who mints crowns (mines, tribute)? Needed to size the dragon sink.
- Are bandits allowed to return to town life (recovery), as Sugarscape replaces dead with newborns?
- Not verified: actual Bannerlord/Kenshi bandit spawn code, Sicilian mafia Palermo paper (paywalled), Olson ABM paper (PDF unreadable), EVE official reports (only a 2012 blog read).
