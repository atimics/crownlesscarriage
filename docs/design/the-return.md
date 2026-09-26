# The Return

Status: milestones 1 (design, last-seen record, change digest), 2 (the scene
shows the changes), 3 (the gate voice) and 4 (news on the road).

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

It is 376 bytes per town, 2.3 KB in all; with the road news of milestone 4,
it is 568 bytes per town, 3.4 KB in all.

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

### Known prices (pure)

Town economies uses the record for prices under the fog
(`docs/design/town-economies.md`). `CcKnownPricesFor(sim, town, &known)` in
`src/sim/cc_known_prices.[ch]` gives:

- **here:** today's prices, only for the town the company stands in (not on
  the road);
- **seen:** the record's prices, with `known_day`, `age_days`, and a
  confidence that fades from 100 to 10 over 120 days;
- **unknown:** nothing, for a town the company has never left.

A shortage or relief story told to the company after the record (by a
resident or a traveller on the road) adds news beside the old price, with the
teller's confidence. It never changes the number. The map case lists the
known prices of the two towns a chart depicts, and the gate's road choice
shows the far town's bread price with its age. No new saved state.

## How news reaches the player

- **Witnessed:** the company saw it (`CcEvent.witness_id` is the company).
- **Told:** a person told the company the story (`told_player` on that
  person's gossip carrier). Their telling carries a confidence and a source
  (`CcGossipVersion.source_character_id`).
- **Read:** a notice at a milestone on the road (milestone 4). The digest
  marks the change `read`, at full confidence.
- **On the road:** a traveller's story, a notice, or smoke on the horizon,
  met while the carriage moves (milestone 4, below). The digest finds these in
  the town's `road_news` first.

Passive story swaps on arrival do not count as told, because nobody said
anything to the player.

## The gate voice (milestone 3)

`src/story/cc_gate_voice.[ch]` builds the line; `src/client/cc_gate_voice_ui.inc`
shows it. Building is pure. It reads the simulation and writes only the voice.

1. **Who speaks.** A face from `CcTownSeen.face_ids` who is alive, grown, and
   in town. The dead leave the character table, so a dead face is skipped. With
   no face, a local speaks: one who holds the evidence story first, then a
   resident, then an official, innkeeper, or courier, then the lowest id.
2. **What they say.** The top change in the digest that is still new and not
   yet spoken on this visit. "Tell me more" gives the next one, from the same
   speaker.
3. **Gate order.** A line is spoken in a fixed order: what happened, then
   why, then who said so. Short sentences. A resident says "here" or "the
   town", never their own town's name.
4. **Heard.** If the speaker holds the evidence story, the line comes from
   their own telling (`versions[slot]`), parsed by the account grammar
   (`CcCoreAccountPrepare`). Typed fact selection asks the role the change is
   about (who did it, or where) and chooses the matching fact, most certain
   first. A doubtful answer is withheld.
   - **The story is the change** (a burning, a succession, a death): the event
     comes first. For a dragon fire the gate render mode says who burned how
     much of the town now, then the rule's reason: "Varkesh burned most of the
     town. It was over missing hoard money, I hear." With a doubtful actor:
     "Fire took most of the town. Some say it was over missing hoard money ..."
   - **The story is the reason** (an empty market after a raid): what anyone
     can see comes first, then the story: "There's no bread in the market. The
     Cinder Tithe raided the town, I hear."
   - **The story only names this town** ("Hunger was reported in
     Gloamgate."): it adds nothing, so the resident says what they see: "Bread's
     gone. People are going hungry here." It still counts as told.
   - **Who said so:** their own eyes, "I saw it myself."; a named source,
     "— Thora at the inn told me." (70 or more), "— I heard it from Thora at the
     inn." (40 to 69), "— Thora at the inn said so, but I'm not sure of it."
     (below 40); no named source, ", I hear.", ", so people say.", ", if the
     story's true." A reason that already hedges ("Folk say it was ...") does
     not hedge twice.
5. **Seen.** If the speaker does not hold the story, or the grammar cannot
   read it, they say only what is plain to see, at full confidence: "Fire took
   most of the town."
6. **Evidence.** Every clause records its part (greeting, event, cause,
   source) and what it traces to: the story's event, the version's source, the
   town against the company's record (and the town now, for "most of the town"
   or "Bread's gone"), or the remembered face. `tests/gate_voice_tests.c`
   checks each clause and the order.
7. **Told.** When a heard line is spoken, the client applies
   `CC_COMMAND_HEARD_STORY` (speaker, slot) through the journal. That sets
   `told_player`, so the digest ranks the change as told and the next voice
   moves on. No new saved state.

The client opens the voice at the gate, in a town the company has seen
before. The speaker waits at the staged scene's `cues.speaker` spot, inside
the gate among the milestone 2 props. When the arriving carriage comes within
a few steps of them, the carriage stops and the voice opens, once per arrival
(`OfferGateVoiceAtGate`). If the player parks early, before the carriage
reaches the spot, the voice opens after the park instead. It shows the
speaker's name and trade, the line, and "Tell me more" or "Thank you". While
it is open it holds the input, replaces the local panel, the arrival note and
the context actions. The line is also a `CcSpeech` turn (`return.gate`), so
it goes through the existing speech and audio path.

The speaker stands in the scene. The update path stages them
(`CcLocalCourseStageGateSpeaker`): at the `cues.speaker` spot when the
company is by the gate, otherwise a few steps ahead of the player, a little
to the side. Either way they face the player, with their own appearance seed.
The renderer only draws the staged agent, and the conversation camera frames
the two of them. The staging is one small function, apart from the town crowd.
The combined review (all three milestones together) is in
`docs/reviews/the-return-combined-2026-09-26/`.

Review: `crownless_return_digest --seed 4 --days 365 --voice` and
`--capture-gate-voice SEED DAYS TOWN PNG [TURNS]`; frames are in
`docs/reviews/the-return-gate-voice-2026-09-25/`.

## News on the road (milestone 4)

While the company rides a leg, it can meet news about a town it has seen
before, about a change it does not know yet. There are three channels:

| Channel | Where on the leg | What | Digest |
| --- | --- | --- | --- |
| Told | 35% of the way | A traveller on the same road (a character travelling between the leg's two towns, either way) who holds the evidence story and has not told it to the company. The best-ranked such change wins. | `told`, with the traveller and their confidence |
| Read | 70%, the milestone | A notice for a public fact about the destination: a new ruler (a proclamation), a new crown, a famine, or a bandit camp (a bounty). | `read`, confidence 100 |
| Witnessed | 70%, 80% and 90% | Smoke over the destination when it burned in the last 10 days. At the milestone, smoke in view takes the notice's place. | `witnessed` |

So a leg meets at most one traveller and one notice, plus the smoke of a
fire. It stays sparse: 12 review rides (six seeds, 60 and 365 days) meet seven
pieces in all, and many rides meet none. Only unknown changes with evidence
count, never prices or restocked stalls.

**The sim decides; the client shows.** `CcRoadNewsAdvance`
(`src/sim/cc_road_news.c`) runs from the journey tick with the route progress
before and after each step. When the carriage crosses a point, the pure
finder picks the news and `CcRoadNewsRecord` writes it into the town's
`CcTownSeen.road_news` (up to three a town). A told story also sets
`told_player` on the traveller's carrier, as `CC_COMMAND_HEARD_STORY` would.
Journey ticks are journalled, so replay meets the same news. Rendering only
reads it.

**The digest.** `FindKnowledge` looks at the town's road news first and marks
the change told, read, or witnessed, with `on_road` set. Read news scores a
third, like told. The gate voice skips every known change, so it moves on. The
arrival scene still stages road news (a notice is not the town, and smoke on
the horizon is not the burned street): `CcReturnSceneCuesBuild` ranks it by
its score before the discount.

**The words.** `src/story/cc_road_voice.[ch]` builds one line:

- Told: the gate voice builder in road mode (`CcGateVoiceSayOnRoad`). The
  traveller names the town, never "here", and never says "You're back". The
  event comes first from their own telling, then the hedge or who told them,
  then, when they set out from that town, a word about their own road. When
  the grammar cannot read the telling, the sim's own words are used, without
  the tally.
- Read: the notice's words, in quotes, from the change.
- Witnessed: what the company sees from the bench.

**The travel view.** `UpdateRoadNews` (`src/client/cc_road_news_ui.inc`)
builds the line on the update path when a new piece appears, and says a
traveller's line through the speech path (`return.road`). `DrawRoadNews`
shows it in a card for 12 seconds of travel. The storybook travel view
(`open_world.inc`) draws the traveller on the verge where they were met,
facing the carriage; a milestone with a post and a notice; and the town's
smoke columns (the M2 prop, `DrawSmokeColumnAt`, scaled up) over the
destination.

**Saved state (schema 123).** `CcTownSeen` gains `road_news[3]`
(`CcRoadNews`: channel, kind, detail, subject, evidence event, traveller,
route, story slot, confidence, day, tick). It resets when the company leaves
the town again. The `return_memory` blob is version 2; version 1 (schema 122)
still loads, with no road news. Empty entries add nothing to the hash. Schema
122 journals replay without road news.

Samples (`crownless_return_digest`):

- Told, seed 2, 365 days: *Willet Sheafbinder, scribe:* "The Cinder Tithe
  raided Thornford, I hear. I left while there was still bread for the road."
- Told, seed 7, 365 days: *Ferwen Longreckon, smith:* "The Cinder Tithe
  raided Thornford — Aldwyn at the inn told me."
- Read, seed 4, 365 days: *A notice at the milestone:* "Famine in Gloamgate.
  Grain and bread are wanted at the gate."
- Witnessed, seed 4, 365 days: *From the bench:* "Black smoke hangs over
  Gloamgate. Something there has burned."

**The payoff.** Seed 4, 365 days: on the way back to Gloamgate the company
hears a traveller's omen story, reads the famine notice, and sees the smoke.
At the gate the resident no longer leads with the fire ("Varkesh the
Unappeased burned most of the town ..."). They say: "The market is closed.
Varkesh the Unappeased burned the town over money missing from the hoard, I
hear."

Review: `crownless_return_digest --seed 4 --days 365 --voice` prints each
piece of road news when it is met, and `--capture-road-news SEED DAYS
told|read|witnessed PNG` renders the travel view at that moment. Frames are in
`docs/reviews/the-return-road-news-2026-09-25/`. Tests:
`tests/road_news_tests.c` and `crownless_carriage --test-road-news`.

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
4. **News on the road (this change).** News travels at carriage speed, and the
   ride changes what the gate says. See "News on the road" below.
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
