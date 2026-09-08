# Character occupations and craft observations

Base: archive seat selection `4a23a1821affb5a39dafae38a31ea768bffbcefd`
(PR #595), plus the shared topic rules from #599. Related issue: #470.

Schema 77 adds a saved occupation alongside each character's role. The ten
trades are woodcutter, shepherd, miller, smith, quarryman, farmer, baker,
innkeeper, cartwright and scribe. A separate none value supports an unassigned
skill. Role changes preserve occupation. Descendants inherit the family trade.

Initial allocation uses the town's services and production. In stable ID order,
residents cycle through its available trades: shepherd for a farm or stable,
farmer for a farm, miller for a mill, smith for a smithy, baker for a bakery,
innkeeper for an inn, cartwright for a stable, woodcutter for wood production,
quarryman for stone production, and scribe as the final general trade. This
keeps the 24-character identity budget. A fresh world and an upgraded save use
the same allocation. Existing schema-77 saves keep their saved occupations.

## Observation rules

Herd events, paper milling, baking, woodlot harvests, quarry output and masonry
repairs enter the gossip ring as craft news. A local adult with the matching
occupation topic receives a direct account when the event enters the ledger.
The production act gathers its event immediately and captures the eligible
adults who are present. The direct version records that person's ID with full
confidence and zero retellings. Later arrivals learn through received accounts.

Shepherds use the herd topic; millers, farmers and bakers use wheat; woodcutters,
quarrymen, innkeepers and cartwrights use road; smiths use war; scribes use
throne. Shared public news retains its existing rules. Received accounts retain
their normal source, retelling and lifetime rules, including when they cover
another trade. Named craft tellers must hold the account.

The research mission selects the resident with the most held accounts matching
its topic. Pages name both role and occupation. The probe's `--sealed` option
selects a writer with a composing seal for the existing seal contract test.

## Evidence

- Strict headless build and all 119 headless tests passed. The expanded
  occupation check also passed with a real spring shearing fixture.
- Static analysis passed with one reviewed baseline item.
- All 616 independent field mutations passed, including each of the 24 saved
  occupations. The separate save-only run also passed 616 mutations.
- Tests cover a Thornford shepherd, a refugee's independent role and skill,
  stable allocation after slot reordering, source-bearing direct observation,
  an actual carried account reaching a smith in another town, retained source
  after the shepherd's death, the descendant's trade, direct save comparisons,
  invalid enum values and a corrupt SQLite integer wider than 32 bits.
- A late-arrival regression first reproduced the earlier direct-account error.
  The shepherd now learns through received accounts after arrival. A separate
  spring shearing fixture proves that a present shepherd holds the direct
  account before the next daily gossip refresh.
- A schema-76 fixture drops the occupation column, replays a day journal, then
  upgrades. The legacy hash matches before the schema is raised.
- Two 40-year runs retain all 82 schema-76 checkpoint hashes from the parent.
  Schema 77, generator 25, SQLite format 32; `sizeof(CcSim)` is 184176 bytes.
  The occupation field uses existing character alignment space.
- `economy-probe.c` compares two schema-77 worlds with identical initial state,
  then clears all occupations in one. Across each seed's 14,600 daily steps,
  the nutrition, smithy, town production, site production and site freight
  ledgers match byte for byte. Every town's complete stock array also matches.
- `herd-mission.txt` shows four real pages, including direct herd testimony.
  The probe's displayed world seed is derived from its requested seed 42.

Weekly samples over 40 years give these held-account results. A pair is two
characters in the same current town with active gossip carriers. These are
repeated snapshots, so one retained account can appear in several samples.

| Requested simulation seed | Resident pairs | Pairs with distinct sets | Herd account samples | Direct herd samples |
| --- | ---: | ---: | ---: | ---: |
| 42 | 51,719 | 29,740 (57.5%) | 16,969 | 3,601 |
| 1592590337 | 52,371 | 34,127 (65.2%) | 25,224 | 7,111 |

Both schema-76 controls had zero distinct pairs and zero held herd accounts.
`measurements.json` contains counts and all yearly hashes. Compile the probes
against the headless simulation and locomotion libraries. `probe.c` takes seed
and schema; `economy-probe.c` takes seed. The parent comparison uses the parent's
header and library plus the original probe topic switch.

## Further issue work

The shared gossip ring keeps its existing 32 slots. Cast allocation and topic
budgets can now be reviewed against measured held sets. Named work at sites,
the innkeeper's bread input and funded scribe recruitment remain the later
trade effects in #470 and #446. The full issue remains open.

## Seed 42 cast

| Character | Home | Role | Occupation |
| --- | --- | --- | --- |
| Mara Venn | Thornford | official | shepherd |
| Jory Fen | Silverwick | traveller | shepherd |
| Ilyra Senn | Gloamgate | official | shepherd |
| Cera Mott | Alderwatch | laborer | shepherd |
| Bren Alder | Silverwick | laborer | farmer |
| Tomas Rill | Gloamgate | official | miller |
| Roswyn Sheafbinder | Thornford | laborer | farmer |
| Tallis Shallowford | Thornford | scout | innkeeper |
| Hartha Longreckon | Thornford | traveller | cartwright |
| Silwyn Sealwright | Gloamgate | scout | smith |
| Reidwen Wheelwright | Gloamgate | traveller | baker |
| Chena Highwall | Alderwatch | laborer | farmer |
| Thora Shieldwright | Alderwatch | scout | smith |
| Alda Ashfinder | Alderwatch | traveller | innkeeper |
| Silven Cinderhand | Silverwick | scout | smith |
| Forga Bellows | Silverwick | traveller | innkeeper |
| Flinta Sealkeeper | Rosespire | official | shepherd |
| Warda Coffer | Rosespire | laborer | farmer |
| Rosel Rosethorn | Rosespire | scout | miller |
| Barwen Shaftward | Rosespire | traveller | smith |
| Hartha Oldstone | Hollowbarrow | official | innkeeper |
| Warrin Windlass | Hollowbarrow | laborer | woodcutter |
| Cindren Salvager | Hollowbarrow | scout | scribe |
| Waryn Stonehewer | Hollowbarrow | traveller | innkeeper |
