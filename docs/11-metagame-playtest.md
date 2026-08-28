# Text-First Metagame Playtest

## Purpose

This build tests the Empty Granary from the simulation side while the 3D scene
is still changing. It is not a replacement for the final local presentation.
It answers a narrower question:

> Can a player understand the crisis, make a costly carriage choice, and
> recognize what changed?

The playtest uses the real simulation, situations, markets, roads, encounters,
delayed echoes, and SQLite save format.

## Start

Build the normal play preset, then run:

```sh
cmake --preset play
cmake --build --preset play
./out/build/play/crownless_metagame_playtest
```

Use `--seed NUMBER` to repeat a specific world.

The test begins at the hungry waystation with 75 crowns. The player has enough
to commit to one answer, but not enough to ignore price, time, and road costs.

## Facilitation

Give the player the keyboard. Do not explain the world or recommend a path.
Only say that `help` lists commands.

Watch for:

- Whether the player uses `look`, `causes`, and `people` before choosing
- Which promise they accept
- Which cargo loses space to that promise
- Whether maps feel like useful but old information
- Whether the player sees a real cost in fighting, bargaining, and repair
- Whether the chosen answer clearly closes rival offers
- Whether both delayed outcomes make sense 30 and 60 days later
- Where the player becomes bored, lost, or surprised for the wrong reason

Do not give the player a solution list. The `rumors` command gives local clues:

- The western farms have grain
- Anyone may pay to pass the restricted bridge, but relief papers do not make
  its guards friendly
- An old night road reaches the mine

Offers are local. A player must meet the sponsor to accept or refuse one. A
delivery only counts if the full load first leaves town in the carriage. Maps
are not keys to normal roads: an uncharted trip is slower and more dangerous.
The hidden night road still needs its physical chart, unless its sponsor is
guiding an accepted commission.

The longer campaign also exposes the goblin and dragon cycle. Type `goblins`
to see the Cinder Tithe's members, covenant, cohesion, stores, current
expedition, and public ash-vault work. Goblins raid for whichever physical
stock their lair lacks, return home, then carry portable offerings on a second
journey to the cave. A named target is visible before departure. At that target,
`goblins warn` prepares the town and `goblins intercept` stops the expedition
at a material cost. At the goblin lair, `goblins trade food|tools|weapons COUNT`
supports the community while reducing its dependence on the covenant.

Type `dragon` to inspect the connected dragon state. At the cave, `dragon steal COUNT`
starts 14 days of omens; `dragon return COUNT` repays the exact debt. The dragon
does not attack because a town is rich. It attacks only while stolen hoard
treasure remains unpaid.
Use `dragon steal-treasure NUMBER` and `dragon return-treasure` to test the
stronger wound caused by a named object. Equivalent coins cannot replace it.
While a tribute carrier is on its final approach, `dragon intercept` transfers
its crowns, goods, and named treasure into the carriage. The dragon never owned
that load, so interception creates no hoard wound or omen.

The same `dragon` view shows the Crown Cycle: life stage, current activity,
age, body condition, derived crown strength, memory, territory, regional
shadow, and visible eggs. A hungry dragon hunts physical Food and leaves roofs
alone. Hoard theft damages memory and starts retaliation. A brood consumes
goblin-lair Food for years. A posthumous successor requires either a visible
brood egg or a public dragon-seed project that spends at least twenty years in
rumor and preparation before it can reveal an egg.

The graphical client exposes the same system. Travel to the dragon's lair,
walk to the marked cave, and enter it. The cave panel shows Crown Cycle bars,
battle strength, visible eggs, the current wound and retaliation target, plus
the available theft, restitution, and tribute-interception actions.

Type `inequality` to inspect the social fault lines. The score is not a generic
poverty meter. It rises when hunger and high prices exist beside prosperity,
stored local coin, a large treasury, and weak commoner power. At 75 or more, with real hunger, the
Ash-Poor Company may travel to the cave, steal from the hoard, and bring short
relief home. The resulting omens and fire must trace back through that journey
to the inequality that caused it.

Type `economy` to inspect fields, mountain deposits, rare-seam progress, all six
goods, named treasure ownership, and the copied monastery reserve. The reserve
contains real deposited crowns. It lends only for famine grain and productive
Tools, while each kingdom records and repays its own debt. Type `treasures` for
each named object's materials, maker, location, holder, and appraisal. A
treasure can be moved with `buy-treasure NUMBER` and `sell-treasure NUMBER`; it
fills one carriage slot.

Type `kingdoms` to inspect each realm's material calling, settlements, vital
dependence, current pressure, internal faction balance, and confirmed
relations. The score is a projection of real hunger, roads, garrison supply,
debt, dungeons, monsters, legitimacy, and dragon pressure. It is not a separate
resource.

Type `war` to inspect border pressure,
kingdom treasuries, local markets, war chests, garrison food, tools, and
weapons, legitimacy, current peace/war/alliance relations, and sealed
dispatches moving between courts. A dispatch offered locally can be accepted
like any other charter. Carry it to the named court; the diplomatic change must
not happen before arrival.
The crown moves gold into a war chest; that chest pays wages and named suppliers,
and the supplies travel. A broke and unpopular court with a real supply crisis
may send the Crown Levy to rob the cave. The dragon still responds to the theft,
not to war.

## End

End after the first main intervention and its two delayed echoes. The echoes
arrive 30 and 60 days after the intervention, while the player is in the place
that changed. Use `wait 30` when a short test needs to reach them quickly.

Ask the player to type `debrief`. They should answer the five open questions
without opening `history`.

Record the answers in the player's own words. A correct mechanical action does
not count as understanding if the player cannot explain its cause or human
cost.

Do not count a test as a success because the player found a valid command. Move
forward only when fresh players can explain the crisis without the ledger,
different interventions produce recognizable outcomes, and the journey remains
interesting when repeated.

## Abuse checks

Before each round of human tests, verify these hostile cases:

- Buying and selling the same local food loses money and does not finish a charter.
- A charter cannot be accepted from another settlement.
- Relief cannot be unloaded until the full consignment has travelled.
- An unrelated trip does not create a free charter encounter.
- Fighting costs coins and carriage condition.
- Repairing with cash never consumes tools carried in the wagon.
- Finishing one main answer withdraws the other two.
- Two delayed echoes survive save and load.
- A rich town can be robbed by goblins without being burned by the dragon.
- Prosperity without hunger and weak commoner power does not launch hoard thieves.
- Severe inequality launches a physical expedition rather than instant theft.
- Stolen hoard money returns as bread and debt relief before the fire.
- After one social-theft burning, the wealthy government repays the dragon.
- War by itself never starts omens or dragon fire.
- War spending moves from treasury to war chest, wages, and supplier markets.
- A paid supply convoy removes real wheat, tools, or weapons from its origin and travels.
- Total tracked gold does not change while these transfers happen.
- A Crown Levy requires burden, a material supply crisis, low liquid war funds,
  and low legitimacy.
- Crown Levy gold enters the real local war chest before restitution removes it.
- Restitution uses existing war-chest, treasury, and household coin.
- Goblin loot leaves a real market, returns to the lair, and reaches the hoard
  only after a separate tribute journey.
- A goblin target is visible before departure. Warning reduces the raid;
  interception stops it and records its social and material costs.
- Goblin trade moves real goods and market coin while changing covenant and
  cohesion in opposite directions.
- Intercepted tribute that never reaches the cave does not belong to the dragon.
- Goblins defending the cave lose real members and Weapons. Missing equipment
  drives later goblin raids.
- Goblins may drain exposed working coin. No artificial cash floor or hoard cap
  rescues the economy.
- When the dragon holds enough of the tracked currency, alliance and muster
  messages travel over real roads. The host cannot leave without 32 Food,
  8 Tools, and 12 Weapons.
- A victory moves the real hoard into the returning host and then into allied
  treasuries. A defeat consumes the supplied goods. After a victory, surviving
  goblins stop tribute but still raid when their lair lacks food or gear.
- A declaration, peace offer, alliance, or muster has no effect before its
  courier arrives. Lost messages do nothing; corrupt or damaged messages may
  be suppressed or read backward. The carriage can deliver a sealed dispatch.
- Restricted roads still carry reduced freight. Tolls, sanctions, and bribes
  move existing crowns instead of deleting them.
- Hunger or exhausted monastery credit can open a night road and recruit
  bandits. An empty camp loses members and cannot raid an empty town.
- Farms need fields, mines need deposits, and smith output consumes real Iron.
- Rare Gold and Gems advance through persistent mine work instead of weekly
  random rolls.
- A named treasure keeps its Gold, Gem, maker, owner, location, and one-slot
  cargo identity through save and load.
- Hoard theft brings 14 days of readable omens to a named town.
- Full repayment prevents the attack; partial repayment does not reset time.
- Fire never occurs without a retained theft and omen in its causal history.
- Active dragon omens survive save and load with the same debt and target.
- Crown strength rises from physical coin, Gold, Gems, named treasures,
  continuity, and devotion with diminishing returns.
- A hunger hunt removes Food but creates no omen, retaliation, lost service, or
  burned town.
- Brood eggs, tending time, and goblin Food survive save and load.
- Killing a dragon with no recorded eggs or public dragon-seed project cannot
  create a successor. If a successor hatches, its event points back to the
  visible brood or the long-running ash-vault work.

## Simulation review

Export yearly balance data with:

```sh
./out/build/play/crownless_sim_metrics --seeds 100 --years 10 \
  > out/simulation-shape.csv
```

Review distributions, not only averages. In particular, watch for settlements
staying at zero or one hundred, permanent route closure, repeated identical
situations, bandit dominance, tribute frequency, hoard growth, dragon attacks
without a retained theft, social and war-financed raid frequency, stuck dragon
debts, and universal prosperity.
For long lifecycle runs, also watch dragon stage, body condition, crown
strength, memory, territory, regional influence, eggs, hunts, broods,
dispersed whelps, and Afterdragon duration.
