# 01 - Dwarf Fortress and relatives (research for sim2)

Method note: WebFetch returns a small-model summary, not raw text. Only the Qud paper (PDF converted locally) was read in full. Other claims are as summarized by the fetch tool. Items I did not open are marked UNVERIFIED.

## Sources read (opened)
1. https://www.freeholdgames.com/papers/Generation_of_mythic_biographies_in_Cavesofqud.pdf - Grinblat & Bucklew FDG'17. Read in full. Picks events at random, then rationalizes causes from entity state.
2. https://dwarffortresswiki.org/index.php/Maximizing_framerate - FPS death causes: unit turns >60% of frame time, line-of-sight O(n^2) ("slowest part by a wide margin", 26-tile cutoff), pathfinding <10%, item count, temperature.
3. https://stackoverflow.blog/2021/12/31/700000-lines-of-code-20-years-and-one-developer-how-dwarf-fortress-is-built/ - Tarn on architecture: A* plus connected-component flood fill, polymorphic items "a mistake", no multithreading, cat bug.
4. https://dwarffortresswiki.org/index.php/World_generation - Phases: terrain, then a "giant zero-player strategy game... thousands of agents"; history length up to 2000 years; later years cost more than earlier ones.
5. https://www.bay12games.com/media/df_talk_8_transcript.html - Toady transcript: worldgen combat is abstracted; armies lumped into groups; detail is still readable in Legends; thousands of fights a year would "grind to a halt".
6. https://dwarffortresswiki.org/index.php/XML_dump - Legends export: historical_event (timestamp, type, location, links to figures/sites/entities), historical_event_collection (e.g. wars); export incomplete, DFHack adds legends_plus.xml.
7. https://dwarffortresswiki.org/index.php/Labor - Job assignment: jobs are created by designations, zones, workshops, work orders; an idle dwarf with the labor enabled takes the job; work details and specialization.
8. https://dwarffortresswiki.org/index.php/Personality_trait - Facets are 0-100 with 7 report bands; 78% of dwarves fall in neutral 40-60; wiki admits many effects unknown.
9. https://dwarffortresswiki.org/index.php/Thought - Thoughts feed stress; memories are relived; numerics are not exposed.
10. https://dwarffortresswiki.org/index.php/Cat - catsplosion hurts FPS; killing pets causes unhappy thoughts (page had nothing on the alcohol bug).
11. https://kotaku.com/dwarf-fortress-creators-favorite-bug-questions-the-natu-1833375249 - Cat alcohol chain: cat walks in bar, contaminant on fur, grooming ingests it, poisoning, death.
12. https://www.gamedeveloper.com/design/q-a-dissecting-the-development-of-i-dwarf-fortress-i-with-creator-tarn-adams - "Numbers usually make for poor stories"; "50000+ boulders will cause trouble"; logging a simulation is a valid story approach "though it has drawbacks".
13. https://www.gamedeveloper.com/design/-i-dwarf-fortress-i-figuring-out-how-to-simulate-the-universe-one-step-at-a-time - Features "kicked down the road" when foundations (law, property, economics) are missing; systems share a common framework.
14. https://sidequest.zone/2020/07/23/it-was-inevitable-tarn-adams-on-dwarf-fortress/ - Broader stories need broader systems; political subgroups need a restructure to allow evolving situations.
15. https://www.bay12games.com/dwarves/dev.html - Dev roadmap (Dec 2025): magic, armies, diplomacy; smaller releases; no perf notes.
16. https://dwarffortresswiki.org/index.php/List_of_Dwarf_Fortress_developer_interviews - Index of talks (GDC 2016/17, "Long-term Simulation Design in DF", "Emergent Narrative in DF"). Index only; I did not open the talks.
17. https://rimworldwiki.com/wiki/AI_Storytellers - Storyteller incident scaling (wealth, colonist count, recent deaths, time since last event), adaptation score, population intent.
18. https://zaydqazi.substack.com/p/the-story-generator-a-game-design - RimWorld analysis (thin on mechanics).
19. https://steamcommunity.com/app/1162750/discussions/3/3419934814481362952/ - Songs of Syx dev: hierarchical path grids 8x8, 16x16, 32x32, whole map; cached cluster routes; "a bit of a mess" to keep updated; odd paths.
20. https://github.com/CleverRaven/Cataclysm-DDA/issues/27226 - CDDA proposal: Gillespie algorithm for off-map NPC/monster/resource activity. P5, unimplemented.
21. https://problemchild1500.itch.io/kenshi-virtual-simulation-engine/devlog/1461553/devblog-6-process - Kenshi community mod; only says off-screen combat should track limb damage and scars. Thin.

Search results only (NOT opened, treat as UNVERIFIED): se7en.ws cat article (claims beer on paw "inherited all variables of a full mug"; consistent with source 3 and 11 but I did not read the original Tarn quote); Steam threads on FPS death; O'Reilly Game AI Pro ch.41 (403); PC Gamer interview (paywalled fetch).

## What to steal for sim2
1. **History is an event log of typed records with entity links.** DF: event = timestamp + type + location + refs to figures/sites/entities, grouped into collections (wars) (src 6). For sim2: every state change emits an event {tick, type, actor ids, site, cause_event_id}. Add the cause pointer DF lacks in the export; "every outcome has a recorded cause" follows. Fitness (survival, reproduction) reads straight from this log.
2. **Two-tier fidelity.** Worldgen uses abstracted combat and lumps able-bodied people into one army group; detail is still logged (src 5). sim2 should run caravans, goblin camps and bandit raids as group-level resolutions with a recorded outcome, not per-blow combat. This is how thousands of world-years per hour is reachable.
3. **Cost rises with history length and population** (src 4). Budget per-year cost and cap entity counts per site; benchmark year N vs year 10N.
4. **Event-driven not tick-scanned.** DF's agents "take turns" each tick, and that is >60% of cost (src 2). The CDDA Gillespie idea (src 20) is the model: schedule the next interaction and jump time. Use an event queue with seeded tie-break for bandit ambush, caravan arrival, tunnel completion. Use per-tick only for things that truly need it.
5. **Reachability via connected components** (src 3): maintain component ids, update by flood fill on map change, so impossible path requests are rejected in O(1). Applies to carriage routes, tunnel networks and camp-to-camp roads. Songs of Syx adds hierarchical grids (src 19); for a road/tunnel graph use cluster-level route caching.
6. **Cap perception.** The O(n^2) unit-checks-other-units cost is the worst line item (src 2). sim2 observation for decide() should come from spatial-hash buckets with a hard radius and a hard max-neighbour count, not all-pairs.
7. **Data-driven raws** (src "Raw file" search result, UNVERIFIED beyond search snippet): token lists per creature/entity. Put trades, goblin faction traits, goods and dragon stats in data files loaded at start; mods add/remove tokens. Keeps the C core small.
8. **Needs and personality as small integer vectors.** DF facets 0-100 (src 8). Store as u8 per facet; feed that plus needs to decide(). Tarn warns numbers make poor stories (src 12), so also emit a text/enum reason in the log ("stole because starving").
9. **Job pool with idle-claim.** Jobs are posted to a pool; an idle actor with a permitted labor claims one (src 7). Fits sim2: a job is an event entry, decide(role, obs, options) picks from the options list. Keep the options list bounded.
10. **Rationalize after the fact for flavour only** (src 1). Qud picks an event at random, then writes a cause from entity state, and may mutate state to create a cause. Do NOT use this for the causal log (you need real causes), but it is a cheap way to generate legend text and treasure names from the log. Its 40,000-word grammar and 19 events / 10 domains show a small content set is enough for flavour.
11. **Named artifacts as first-class entities.** Qud and DF make items named in events historical entities that are later placed in the world (src 1). For goblin treasure (crowns, tomes) give each a persistent id and a provenance chain of events; the dragon hoard becomes a readable ledger.
12. **Storyteller as a director, optional.** RimWorld scales raids by wealth and time since last event, with adaptation (src 17). A world-level pacing knob could keep bandit recruitment from running away or dying out, but it breaks "no hidden hand" purity. If added, log its choices as events.
13. **Regression tests for interacting systems** (see cat bug below): property tests on the log, e.g. "no actor dies of an undocumented cause".

## What to avoid
1. **FPS death from entity and item counts** (src 2, 10, 12). Every pet, ghost, corpse and item stack costs per tick. Cats multiply ("catsplosion"); DF's own fix is population caps. Give sim2 hard caps and death/decay for all entity types including items and corpses.
2. **All-pairs awareness** (src 2). See steal 6.
3. **Cross-system leakage bugs** (cat alcohol, src 3, 11). A contaminant reused the full-dose ingestion path; one number was off. Lesson: a system that reuses another system's effect path needs explicit scaling, and emergent deaths need a cause in the log so they are detectable. Add invariant checks for implausible mass deaths.
4. **Deep polymorphic class hierarchy for items** (src 3). Tarn calls it a mistake. ECS components avoid this.
5. **Single thread, tangled 700k-line C++ with pre-2006 crust** (src 3). Keep sim2 systems small, named, and order-fixed.
6. **Features blocked on missing foundations** (src 13). DF cannot do politics without law/property/economics. Build sim2's property/ownership of treasure early, since theft, trade and the hoard all depend on it.
7. **Opaque numeric mood/stress** (src 9). Wiki says effects of facets are unknown even to players. For learned brains this is fine, but debugging needs the reasons logged.
8. **Incomplete export** (src 6). DF's legends dump lacks data; third-party DFHack patches it. Design the log schema complete from day one, versioned.
9. **Assuming world history can be interrupted anywhere** (src 1 makes this point about DF: it must stay agnostic past vs present). sim2 need not allow player-start mid-history; don't pay that cost unless required.
10. **Per-blow worldgen combat** (src 5): would "grind to a halt".
11. **Tick-scanning idle actors every tick** (src 2, 20): use the event queue instead.

## Determinism notes
No source I opened documents DF's RNG seeding or cross-platform determinism. UNVERIFIED. DF is single-threaded (src 3), which helps reproducibility, but I found no claim that DF is bit-identical across platforms. Qud's paper does not discuss seeds either (src 1). For sim2, use integer or fixed-point math, one PRNG stream per system keyed by seed plus entity id plus tick (so iteration order cannot change outcomes), and a stable sort on event ties. This is my design advice, not a finding from these sources.

## Open questions
- How does DF order agent turns and break ties, and is worldgen reproducible from a seed? Not found. Tarn's "Long-term Simulation Design in DF" (NYU PRACTICE 2016) and "Emergent Narrative" talks are the likely sources; not opened.
- Real numbers for DF worldgen cost per year vs number of sites: wiki gives only qualitative scaling.
- How DF's hungry people turn to crime or how villains/conspiracies are modeled ("Villains" talk, Roguelike Celebration 2018): not opened. Relevant to bandit recruitment from hungry townsfolk.
- RimWorld and Kenshi: no primary source read on point budgets or off-screen simulation. Kenshi background simulation is UNVERIFIED (only a community-mod devblog, thin). Mount & Blade caravans: not researched.
- CDDA never implemented the Gillespie proposal (issue is P5); no evidence it works in a game at scale. Worth a prototype for caravan and raid scheduling.
- Does a learned-brain fitness of survival and reproduction produce degenerate strategies (e.g. hide forever)? DF offers no evidence either way.
