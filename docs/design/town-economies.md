# Town economies

The six towns need each other. Each makes a few things well, and none has
everything. The company's work is to carry what one town makes to the town that
needs it, faster and more reliably than the background trade does.

This note is the spec for the change. It replaces the rule that every town has
the same nine specialist shops (from #931).

## Three kinds of shop

- **Makers** buy their inputs and sell their products. **A maker never buys its
  own product back.** Wood goes into the stationer and paper comes out; you
  cannot sell paper back to it. Goods therefore flow in one direction, and a
  surplus has to travel somewhere else.
- **Merchants** buy and sell the same goods at a spread. They are the only
  place you can always sell cargo, at a loss. **Only Gloamgate and Rosespire
  have a market hall.** Thornford keeps a grain exchange that deals in wheat
  only.
- **Salvagers** buy spoiled or odd goods: the butcher takes rotten meat, the
  grain exchange takes rotten grain, and the Hollowbarrow fence takes salvage
  from the Underroad.

## What each town has

Rules the user set are marked **bold**. The rest is the proposed default.

| Town | Role | Shops |
| --- | --- | --- |
| Thornford | Farming village | **Bakery**, **bowyer**, grain exchange (wheat only), butcher, clothier |
| Gloamgate | Market town | **Market hall**, **bakery**, butcher, stationer |
| Alderwatch | Fortress | **Weaponsmith**, timber merchant, stonecutter |
| Silverwick | Mining town | **Toolsmith**, mine supplier, jeweler |
| Rosespire | Capital | **Market hall**, **bakery**, jeweler, clothier, stationer |
| Hollowbarrow | Dungeon town | Goblin fence only: salvage in, Underroad finds and lamp oil out |

**Bread is baked only in Thornford, Gloamgate and Rosespire.** Bread spoils and
wheat travels, so bread is baked near the people who eat it. The fortress and
the mine town bake nothing and depend on carriages for bread, so a month
without a delivery makes them hungry. That hunger is a real cause, and it is
exactly what a resident tells the company on The Return.

## Six carried items

`CC_GOOD_TOOLS` and `CC_GOOD_WEAPONS` stop being two abstract goods and become
six item kinds. Items exist for the company's own work (travel, haul, mine,
clear, repair, fight, hunt), not for jobs the company never does, so there is
no scythe.

| Item | Company work | As a weapon | Made in |
| --- | --- | --- | --- |
| Pick | Mining yield; digging into the Underroad | Heavy: slow, hits hard, breaks doors and shields | **Silverwick** toolsmith only |
| Axe | Clearing fallen trees; felling timber | Medium: the everyday weapon | Both smiths (Silverwick and Alderwatch) |
| Hammer | Repairing the carriage and wheels; building | Medium: blunt, good against armour | Both smiths |
| Sword | None | The only pure weapon: fast, best in a straight fight | **Alderwatch** weaponsmith only |
| Bow | Hunting on the road | Ranged: the only one that hits from afar | **Thornford** bowyer only |
| Lantern | Light in the Underroad; pushing back fog on the moor | None | Silverwick toolsmith |

Lanterns burn **lamp oil**, a small consumable. Oil is sold at the Hollowbarrow
fence and the market halls.

**Upgrades:**
1. Common: the item above.
2. Fine: the work of a named smith. The smith's reputation travels as gossip.
3. Bane: a relic raised from a slain dragon's hoard-fire, such as "Bane of
   Varkesh the Unappeased, Wyrmsbane". Banes already exist in the simulation
   and are priced by the dragon they killed. A bane is won, never bought.

Axes and hammers are dual-use on purpose. An axe sold to a timber camp is one
fewer axe in the crew when bandits come.

## Prices under the fog

Every price the company knows has an age: "bread 5c at Gloamgate" means "bread
was 5c at Gloamgate on day 88". The map, trade screens and route planning show a
known price together with how old it is, and it fades like the rest of the fog
(`docs/design/the-return.md`). Trade becomes a judgement about how stale your
knowledge is, and fresh news about a shortage becomes valuable cargo.

A shortage also gives a moral choice the simulation already supports: sell into
the famine at a high price, or bring relief (the relief crate and famine relief
systems). The town remembers which the company did, and so does the gossip.

## Background trade

If only the player carries bread, Silverwick starves whenever the company goes
elsewhere. The simulation needs NPC carriers that move staples between towns on
their own, **slowly and unreliably**. The player's edge is speed, fresh
knowledge and nerve, not being the only trade.

## The opening

The game starts in Thornford, which has no market hall. The first run must
still work: the player can buy wheat at the grain exchange and sell it at the
Gloamgate market hall or bakery. Keep the opening journey test green.

## Implementation

An audit of main (17f4ae6b) found:

- **Production already consumes inputs.** `src/sim/cc_production.c` runs
  recipes: wheat to bread (`CcEconomyRunBakery`), iron and wood to tools and
  weapons (`CcEconomyRunSmithy`), and wood to paper (`CcEconomyRunPaperMill`).
  Raw goods come from farms and mines at settlement rates. Goods are conserved
  (`CcSimTrackedGood`). The maker rule is therefore real economics.
- **What a town can make is set by its services**, seeded once at world
  generation by `SeedSettlementServices` in `src/sim/cc_sim.c` (bakery, smithy,
  mill, farm, mine). Shops (`src/client/cc_local_shops.c`) are only a client
  table over the town's shared stock. So this change belongs in the services,
  with the shop table following them.
- **Buyback is built in twice**: `CcLocalShopBuys` lets shops take back what
  they sell, and `CcSimTradeResalePrice` in `src/sim/cc_supply_trade.inc`
  supports selling wheat and raw stone back into the same chain.
- **No NPC carriers exist.** Towns without a service depend on the player.
- **Tools and weapons are two bulk goods.** `CcBelonging` (from #928) already
  names tools "axe", "pick" or "hammer" by occupation and can be repaired with a
  smith's iron.
- **Banes are `CcTreasure`**, a relic you can sell or display, not a weapon.
- **Light is a fixed budget** in the mine (`CcMineState.light`) and the
  Underroad (`light_remaining`), with nothing to refuel it.
- **Hunting does not exist.**

### Pull requests, in order

1. **Town services and makers.** Make `SeedSettlementServices` follow the
   table above: bakeries in Thornford, Gloamgate and Rosespire only; a
   weaponsmith in Alderwatch and a toolsmith in Silverwick; paper mills only
   where there is a stationer; market halls only in Gloamgate and Rosespire; a
   fence in Hollowbarrow. Makers stop buying their own products, in the client
   shop table and in `CcSimTradeResalePrice`. The client shop table follows the
   services. This needs a save schema bump with a migration for existing worlds.
2. **Prices under the fog.** Known prices carry the day they were learned and
   fade with age. This builds on the existing settlement knowledge sources.
3. **Six carried items.** Replace `CC_GOOD_TOOLS` and `CC_GOOD_WEAPONS` with
   pick, axe, hammer, sword, bow and lantern, plus lamp oil. Keep each
   recipe's current uses: tools gate production, weapons drive raids and
   defence. A migration splits the old goods into the new ones.
4. **Lanterns and oil.** Lantern and oil in the company's kit refuel the mine
   and Underroad light budgets, and thin the fog on the moor.
5. **Background carriers.** Slow, unreliable NPC carriers move staples between
   towns, so no town starves only because the player went elsewhere.
6. **Hunting and banes as weapons**, later. A bow brings meat on the road, and
   a bane can be wielded as an upgraded sword or bow.

## Questions the audit answered

- Does settlement production consume input goods today, or is output produced
  from nothing by settlement function? The maker rule is only real if the
  simulation converts inputs into products.
- Are shops only a client view over a town's shared stock, or does the
  simulation know about them?
- Do NPC carriers already move goods between towns?
- Where are the tools and weapons goods used (mine yield, defence, repairs), and
  can the named-belongings system from #928 hold the six item kinds?

The answers are in "Implementation" above.
