# The Return

Status: milestone 2 (the scene shows the changes), building on milestone 1
(design, last-seen record, change digest).

Crownless is a small life inside a big living history. The player sits on a
carriage company's bench. The simulation is deep, but today the player mostly
sees it in menus and panels. The Return is one core loop that we will build
until it feels great, in the Silverwick region: Thornford, Gloamgate, and
Silverwick.

## The loop

1. **Leave.** The company leaves a town. The game remembers how the company
   saw it.
2. **Ride.** The company rides the road. News travels at carriage speed,
   through travellers and roadside signs (milestone 4).
3. **Arrive.** The town has visibly changed, because the real simulation
   changed it: fire damage, a new ruler's banner, empty stalls, a hungry crowd
   (milestone 2).
4. **Hear.** A resident at the gate says what changed, in their own words,
   with how sure they are and who told them (milestone 3).
5. **Fewer panels.** The world shows what it can (milestone 5).

Art direction: a Sierra/King's Quest look. Each screen has a still, composed
camera. Pans happen only as transitions. The painted look is rendered live,
with style packs (`assets/stylepacks`, #922), never as pre-rendered plates.

## What already exists

| Need | Existing system | Where |
| --- | --- | --- |
| Town condition | `CcSimTownConditions`: burnt, rebuilding, lawless, peaceful, thriving, hungry, abandoned | `src/sim/cc_sim.c`, `CcTownCondition` in `src/sim/cc_sim.h` |
| Condition in the scene | fire scars, hungry crowd, tuft colour, sky scar, lawless figures | `src/client/local3d/authored_places.inc`, `road_book.inc`, `town_sky.inc`, `actor_rendering.inc` |
| Condition as text | the header line in town (`CcLocalTownConditionText`) | `src/client/cc_local_place.c`, `src/client/main.c` |
| Captures | `--capture-town-state INDEX X Z PNG burnt\|rebuilding\|lawless\|peaceful\|thriving\|hungry` (the #880 variants), `--capture-town-arrival`, `--capture-road-arrival`, `--capture-storybook-arrival`, `--capture-return SEED DAYS TOWN PNG` (milestone 2) | `src/client/cc_capture_request.inc`, `cc_capture_scenes.inc` |
| Rulers | `CcKingdom.ruler_character_id`; succession pushes `CC_EVENT_ROYAL_SUCCESSION` | `src/sim/cc_sim.c` (succession, contested claims) |
| Fires | dragon retaliation sets `fire_damage` and `last_fire_day`; masons repair it | `AdvanceDragonRetaliation`, `RepairSettlementFire` |
| Food, prices, stock | `CcSettlement.hunger`, `stock[]`, `price[]`; shortage and famine events | `src/sim/cc_food_economy.c`, `cc_goods.c` |
| Deaths | `CC_EVENT_CHARACTER_DIED`; the dead keep a bounded historic record | `ReplaceDeadCharacter`, `RecordCharacterLifetime` |
| Bandits, dragon, cults | bandit camps (`camp_settlement_id`), raids, dragon omens, goblin cult | `CcBanditGroup`, `CcDragon`, `CcGoblinPolitics` |
| Census | every resident has a row and a home district | `src/sim/cc_census.c` |
| Gossip | the ledger of 32 stories; each carrier holds its own telling (`CcGossipVersion`: source, retellings, confidence); `told_player` marks stories told to the company | `GatherGossipEvents`, `ExchangeGatheredGossip`, `CcSimStoryTold` in `cc_sim.c`; `docs/npc-society.md`; `tools/gossip_flow.c`, `tools/gossip_corpus.c` |
| Speech | held-account grammar and the core model; typed fact selection picks a fact, the renderer owns the words | `docs/core-v2-accounts.md`, `docs/core-language-corpus.md`, `src/story/cc_core_*.c`, `tools/dialogue/FACT-SELECTION.md` (#866, #868) |
| Learned names | `CcCharacter.introduced_day` is set when the company meets someone | `cc_sim.c` (introductions) |
| Arrival | `FinishJourneyArrival` parks the carriage, swaps gossip, and logs the ride | `src/sim/cc_journey_runtime.c` |
| Departure | `CcJourneyDepart` | `src/sim/cc_journey_departure.c` |
| Personal requests | #928 (open): needs and requests from residents | `docs/personal-requests.md` on that branch |

## Data model

### The last-seen record (saved)

`CcReturnMemory` holds one `CcTownSeen` per settlement slot
(`src/sim/cc_sim.h`). `CcReturnRecordSeen` fills it when the company leaves a
town. It keeps only what a returning traveller notices and what later
milestones need:

- the day the company left;
- the kingdom and ruler, with the ruler's name;
- up to four faces: residents the company had met, with their names;
- condition flags, services (market, inn, and so on), and threats (a bandit
  camp, a planned raid, the dragon's anger);
- population, security, prosperity, hunger, and fire damage;
- stock and price for each good.

It is 376 bytes per town, 2.3 KB in all.

**Why this must be saved.** A town's past state is gone once the simulation
moves on. The event ring holds only 256 events, often a few days' worth. The
gossip ledger holds 32 stories. Neither can rebuild the price of wheat, the
number of residents, or who lived by the market on the day the company left.
Replaying the save journal could rebuild it, but that is slow, needs I/O, and
older saves do not have a full journal. So the record is new saved state.

It is saved as one blob in the `return_memory` table, like the census. It is
part of the state hash from schema 122, and `CcSimValidate` checks it. Empty
slots add nothing to its hash. The record is only written by a travel
command, so journal replay rebuilds it exactly.

**Older saves** load with no memory. We cannot know what the player saw, so
every town counts as a first visit until the company leaves it again.

### The change digest (pure)

`CcReturnDigestBuild(sim, town, &digest)` captures the town now and compares
it with the record. It reads the simulation and writes only the digest. Each
`CcReturnChange` has:

- `kind`: abandoned, fire, rebuilt, new ruler, new kingdom, death, gone,
  hunger, fed, lawless, safer, thriving, poorer, population, bandit camp,
  bandits gone, dragon omen, service lost or opened, empty market, restocked,
  price;
- `before` and `after` values, names for people and crowns;
- `magnitude` (0 to 100): how large the change is;
- `surprise` (0 to 100): how unexpected it is from the last view. A long
  absence lowers surprise, because change is then expected;
- `evidence_event_id`: the world event behind it. We look in the event ring
  first, then in the gossip ledger, which keeps stories longer;
- `knowledge`: new, told, or witnessed, with who told the company and how sure
  they were.

`score = weight(kind) × (100 + magnitude) × (100 + surprise) / 40000`.
News the company was already told scores a third; news it witnessed scores a
quarter. The sort has a total order (score, kind, detail, subject), so the same
world always gives the same list. Small price moves are dropped, and the digest
keeps at most three market lines.

## How news reaches the player

- **Witnessed:** the company saw it (`CcEvent.witness_id` is the company).
- **Told:** a person told the company the story (`told_player` on that
  person's gossip carrier). Their telling carries a confidence and a source
  (`CcGossipVersion.source_character_id`).
- **Read:** a notice or roadside sign. Notices exist (`CcNoticeBoard`), but the
  digest does not mark them yet. Milestone 4 adds this.

Passive story swaps on arrival do not count as told, because nobody said
anything to the player.

## Milestones

1. **Record and digest (this change).** `src/sim/cc_return.[ch]`, schema 122,
   `crownless_return_digest` for review, and `tests/return_tests.c`.
2. **The scene shows the changes (this change).** Drive the town scene from
   the digest's top unknown changes (up to three).
   `CcReturnSceneCuesBuild` (`src/client/cc_local_place.[ch]`) maps them to
   cues; `CcLocalReturnStagingPlace` stands each cue's props in the town
   (positions are written into the cues on the update path, when the
   carriage rolls through the gate and again when it parks; draw code only
   reads them, `DrawReturnArrivalStaging` in
   `src/client/local3d/authored_places.inc`):
   - **Fire** shows on the town itself, in every view, not only on
     arrival. `fire_damage` sets how much burns; plots burn in a fixed
     per-town order, nearest the gate first (the fire came in over the
     approach road). A caught building is charred, soot-streaked, with a
     hole burned through its roof; a building past its threshold by 75% is
     a gutted shell: broken wall stumps with black tops, empty windows,
     fallen rafters, rubble and embers, no roof. Landmarks and the east
     windmill burn in the same order; compound stone scorches. Ash and
     scorch lie on the ground round every burned plot. When the fire is new
     news, dark smoke columns rise from the burned plots the arrival lens
     sees.
   - **Hunger**: a knot of thin, hunched people at the gate, some kneeling,
     some lying on mats, with empty bowls.
   - **The market**: a gate market (two or three striped stalls) stands on
     every visit and carries the town's own stock. An empty market leaves
     the tables bare with an upturned basket; a lost market boards the
     nearest booth up (planks and an X). A lost service also boards the
     real shop's front door and windows when that building still stands.
   - A fresh banner for a new ruler; a bandit camp on the rise beyond.
   - `cues.speaker` is a spot inside the gate kept clear of every prop, for
     the gate voice's speaker (milestone 3).

   The arrival opens on an **establishing shot**: a still, composed view
   from the approach road down the carriage path into town, one authored
   station per town profile (`TownEstablishingStationFor`,
   `src/client/local3d/camera_composition.inc`). It holds while the
   carriage rolls through the gate, then pans to the wide shot that watches
   it park. `--capture-return SEED DAYS TOWN PNG` renders it after a real
   ride (`src/sim/cc_return_ride.[ch]`); `--capture-town-arrival INDEX
   establishing PNG` renders any town's first-visit establishing shot. The
   header keeps one short journal line in place of the condition list once
   the scene is already carrying the news (step 4 of the "fewer panels"
   direction). Review frames: `docs/reviews/the-return-scene-2026-09-25/`.
3. **The gate voice.** A resident at the gate speaks the top unknown change in
   their own words, using their own telling of the evidence story.
4. **News on the road.** Travellers and roadside signs tell stories while the
   carriage moves, so some changes arrive already told or read.
5. **Fewer panels.** Replace the condition text and panel lines that the scene
   and voice now carry.
6. **Polish.**

## Review tool

```sh
out/build/play/crownless_return_digest --seed 4 --days 365
```

It starts a new world in Thornford, rides to Silverwick by the real roads,
waits, and rides back. Each arrival prints the digest. Seed 4 after a year:

```text
Gloamgate: last seen day 2, back on day 382 (380 days away); 15 changes, 8 shown.
 1. [fire] Fire has burned the town: 60% damage, was 0%. (score 558, ...; DRAGON FIRE day 381)
 2. [hunger] The town is hungrier: hunger 58, was 0. (score 471, ...; SHORTAGE day 371)
 3. [service lost] The Market is gone. (score 234, ...; DRAGON FIRE day 381)
```

## Out of scope for milestone 1

- Any change to the scene, the camera, or the panels.
- Speech: no new grammar rules or model changes.
- Road news, notices as knowledge, and letters.
- Memory of places other than the six settlements (road sites, the mine).
- More than four remembered faces per town.
- Rebuilding memory for older saves.
