# sim2 research 06: emergent narrative, storytellers, causal event logs

Date: 2026-09-30. Every source below was opened (WebFetch, or PDF downloaded and run through pdftotext). Claims from web-search snippets only are marked UNVERIFIED.

## Sources read

1. https://rimworldwiki.com/wiki/Storyteller - Storyteller inputs: colony/building wealth, colonist and animal counts, recent deaths or injuries, time since last big event. Has "population intent", an optional "adaptation" score (harder when thriving, easier after damage), and a wealth-independent mode (fixed time curve). Cassandra = rising curve with breathing room, Phoebe = long gaps, Randy = random.
2. https://media.gdcvault.com/gdc2017/Presentations/Sylvester_Tynan_RimWorld_Contrarian_Ridiculous.pdf - Tynan Sylvester, "not a game, a story generator". A story generator must include loss and recovery, not game over. Mechanics should make emotions from the character's POV. Apophenia needs abstracted feedback and long-term relevance.
3. https://www.gamedeveloper.com/design/rimworld-dwarf-fortress-and-procedurally-generated-story-telling - Three pillars: a set story framework, semi-autonomous NPCs, a dynamic event system. One event spirals into chained consequences. Ellipsis (off-screen events) lets players fill gaps.
4. https://steamcdn-a.akamaihd.net/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf - Booth's slides. Per-survivor intensity goes up on damage, incapacitation, ledge falls, and nearby infected deaths (inverse to distance). It decays to zero, but not while enemies are engaged. Phases: Build Up, Sustain Peak (3-5 s), Peak Fade, Relax (30-45 s or until progress). The Director changes frequency (pacing), not amplitude (difficulty). Boss encounters are exempt. "Crude estimation, yet pacing works."
5. https://www.pcgworkshop.com/archive/grinblat2017subverting.pdf - Grinblat and Bucklew, Caves of Qud. "Historical rationalization": pick an event first, then rationalize a cause from entity state. Events are parameterized by and modify entity properties. Each event yields one "gospel" text via a Tracery-like grammar. About 13 events per sultan. History is a diegetic artifact, relayed by NPCs, shrines and journals. Design goal is evocative, not detailed, to leave room for apophenia.
6. https://www.gameaipro.com/GameAIPro3/GameAIPro3_Chapter37_Simulating_Character_Knowledge_Phenomena_in_Talk_of_the_Town.pdf - Ryan and Mateas. Per-character mental models of entities, made of belief facets. Facet fields: owner, subject, type, value, predecessor, parents, evidence, strength, accuracy. Eleven evidence types. Salience drives observation, gossip, forgetting. A hand-authored mutation graph. Knowledge is implanted at the end of worldgen for cost.
7. https://emshort.blog/2019/05/28/curating-simulated-storyworlds-james-ryan-ch-6f/ - Summary of Ryan's dissertation. "Repeating a belief aloud makes the person more committed." Patterns: apophenia hacking, overgenerate and curate. Caveat: simulated facts (hair colour) lack narrative tension.
8. https://dwarffortresswiki.org/index.php/DF2014:Rumor - DF rumors are info about a specific historical event. Spread is faster for important news and slower for distant places. Knowledge fades over weeks and years but leaves a reputation. Timestamps are tracked per individual, site government, culture and civ. No false rumors except secret identities.
9. https://dwarffortresswiki.org/index.php/DF2014:World_History_file - Only civ and leader records, not an event schema. Useful only as a negative: the export is a summary, not a cause log.
10. https://www.gamedeveloper.com/design/designing-i-shadow-of-mordor-i-s-nemesis-system - Nemesis: the game remembers and reflects player actions. Death becomes revenge motivation (loss feeds story). Inspired by sports systems. Designers are "facilitators of improvisation".
11. https://github.com/mkremins/felt - Story sifting: Datalog-style patterns over an event log. Events are facts with eventType, actor, target, tag. Example: a hospitality-violation pattern over three events.
12. https://mkremins.github.io/publications/WAWLT_FDG2020.pdf - Why Are We Like This. A DataScript store of events, relationships and impressions. Each impression points back to its causing event. Everyone knows every event but only cares about relevant ones, so knowledge is replaced by subjective impressions. Up to 3 positive and 3 negative impressions per relationship. Gossip actions copy impressions.
13. https://ck3.paradoxwikis.com/Event_modding - CK3 events: trigger (precondition), weight, immediate block, options with AI weights, cooldowns, hidden events, on_action chaining. Shows the trigger/effect/cooldown pattern for pacing.
14. https://martinfowler.com/eaaDev/EventSourcing.html - Event log is the source of truth. You can rebuild state, query past state, and replay with corrections. Replay must be deterministic, and external side effects and external queries must be captured.
15. https://cjleo.com/blog/the-power-of-wildermyths-modular-storytelling-in-game-design/ - Wildermyth "string of pearls". Modular story pieces between fixed beats. Choices persist via relationships, traits and buffs. (Blog, secondary source. Did not show tag-matching details.)
16. https://arxiv.org/pdf/2109.02564 - Maki-Thompson model: ignorant, spreader, stifler. A spreader stops after meeting someone who already knows. There is a critical parameter where the rumor dies out or survives.
17. https://versu.com/about/how-versu-works/ - Versu: genre, story and character files. NPCs have motivations, emotions, beliefs. Praxis logic language for social practices. (Overview only, no internals.)

Not opened (failed or snippet only), so UNVERIFIED: Bad News project page (ben-samuel.com, certificate error); Prom Week / Comme il Faut papers; Skyrim Radiant Story (only search snippets: simple repeated fetch-style quests, not used for major stories); Facade; Sims memory and gossip; Jason Grinblat GDC talk video; Left 4 Dead emotional model beyond Booth's slides. Sim2 should not cite these until read.

## Recommended event schema and knowledge/rumour model for sim2

### Design stance
- The ground-truth event log is append-only and deterministic (Fowler: same events give same state). Everything the player hears is derived from it, never invented. This is the opposite of Qud's "rationalize after the fact". Qud's trick fits only the deep-past legends (pre-tick-0 history), where we can afford to fabricate.
- Two stores: (a) ground-truth event log (what happened, with cause), (b) per-actor belief store (what each actor thinks happened). Narration and learned-brain observations read only (b). Fitness reads only (a).
- Keep (b) small. Take WAWLT's shortcut where possible: do not track per-fact knowledge for every actor. Track facts only for actors who matter (named actors, faction leaders, player-facing NPCs), and use coarse knowledge for crowds.

### Event record (ground truth), concrete fields
```
Event {
  id:        u64           // monotonic, assigned in tick order
  tick:      u32           // sim tick
  seq:       u16           // order inside the tick (deterministic)
  type:      enum          // ROBBED_CARRIAGE, BANDIT_RECRUITED, STARVED, TUNNEL_DUG, TREASURE_SMUGGLED, TRADE_DONE, GUARD_KILLED, DRAGON_ATE, ...
  actors:    [EntityId]    // who did it (role-tagged: agent, patient, instrument)
  patients:  [EntityId]    // who it happened to
  place:     PlaceId + xy  // where
  amounts:   small fixed array  // gold, food units, hp, counts (integers, no floats)
  cause:     [EventId]     // direct parent events (REQUIRED for non-exogenous events)
  cause_kind: enum         // NEED (hunger), GREED, FEAR, ORDER, OPPORTUNITY, RETALIATION, STORYTELLER_INJECTED, EXOGENOUS(seed/weather)
  reason_tag: enum/str     // short "why" for text: "hungry", "owed_debt", "revenge_for:<event>"
  witnesses: [EntityId]    // who perceived it at the time (derived from radius/LOS rule)
  salience:  u8            // story weight, set by type and amounts (see pacing)
  visibility: flags        // PUBLIC / SECRET / HIDDEN_FROM_FACTION(x)
  outcome_delta: small struct // fitness-relevant deltas (wealth, pop, deaths) so training reads the log without replaying
}
```
Rules:
- Every state change in the sim goes through an event with `cause` set. A check in tests: no event other than `EXOGENOUS`/`STORYTELLER_INJECTED` may have an empty `cause`. This is the "every outcome has a recorded cause" requirement made testable.
- `cause` is a DAG, so a story is a walk back up the graph (e.g. bakery burned -> famine -> baker joined bandits -> carriage robbed -> guards hunt -> goblin caravan sold weapons). Causal walks give the "why" for dialogue and legends for free.
- Also log decisions that did NOT act if a learned brain chose among options (actor, chosen, top-k alternatives, observation hash). This helps debugging and training. Mirrors the L4D "reason" string idea on every transition (Booth: reason strings are invaluable for debug output).
- Log RNG: seed plus a stream id per event, so replay and "what if" forks work.

### Knowledge / rumour model (per actor), concrete fields
Adapted from Ryan and Mateas (belief facet) but keyed on events, not attributes, to match DF rumors.
```
Belief {
  owner:     EntityId
  about:     EventId        // the ground-truth event this is about (or NONE for fabricated)
  claim:     {type, actors, place, amounts, cause_guess}  // what owner thinks, may differ from truth
  strength:  u8             // 0..255, sum of evidence, decays
  evidence:  [ {kind: WITNESSED|TOLD|OVERHEARD|INFERRED|LIE|MUTATED, source: EntityId, tick, place, strength} ]
  parents:   [BeliefId]     // who told me (provenance chain, for debug and for "trace the rumour")
  learned_tick: u32
  accurate:  bool           // derived vs ground truth, stored only for debugging/fitness, NEVER shown to the actor
}
```
Dynamics (all integer/fixed-point and seeded so they stay deterministic):
1. Origination: witnesses get a WITNESSED belief at the event tick (strength high). Non-witnesses get nothing.
2. Propagation: when two actors are co-located (town tavern, camp, caravan stop, carriage meeting), each picks top-n beliefs to share by score = salience x freshness x trust(owner, listener), n from extroversion-like trait (Ryan: choose the n highest-scoring subjects; DF: important news spreads farther and faster). Listener adopts as TOLD with strength = f(source strength, trust).
3. Stifling: a spreader stops repeating a belief after meeting k listeners who already know it (Maki-Thompson stifler). This caps the message load and gives natural rumor extinction. Tune the stifle count so important events (dragon attack) reach far and trivial ones (petty theft) die within one town. The paper shows a critical threshold exists, so expect a phase change and test it.
4. Mutation: each share has a small chance to perturb one field (amounts inflated, actor swapped to a plausible same-faction actor, cause_guess replaced). Use an authored mutation table per field, like Ryan's hair colour graph. Keep it small: amount x1.5-3 (exaggeration), goblin faction confusion, "bandits" vs "guards" confusion.
5. Decay and forgetting: strength falls per day. Below a threshold the belief is forgotten. Salience slows decay. Long-term effect stays as reputation (DF keeps reputation after the detail fades): keep a separate `reputation[actor][subject]` score updated when a belief forms.
6. Conflict: a new claim about the same event replaces the old only if evidence strength is higher, otherwise kept as a candidate (Ryan's belief revision). Repeating a claim aloud adds strength to the speaker (declaration), which makes liars drift toward believing themselves. Optional, cheap.
7. Lies: a LIE evidence kind lets bandits and goblins cover tracks (goblin factions hide smuggling, bandits pin a robbery on goblins). Recipients treat lies as TOLD. The `accurate` flag allows later exposure events.

Scale guard: do not run gossip every tick for everyone. Run it only at encounter events (co-location edges already exist in the sim), and use the WAWLT fallback (everyone knows, only relevant people care) for anonymous crowd NPCs. Ryan himself implants knowledge at the end of worldgen because simulating it during history was too costly.

### Observation for learned brains
- The text/token observation is built from the actor's beliefs plus current percepts, never from the event log directly. Include for each belief: a short token string, age bucket, strength bucket, and source kind (saw / heard / rumour). This teaches brains to weigh hearsay.
- Log, per decision, the belief IDs that were in the observation. Then a wrong decision made from a false rumor is traceable to the mutation event. That gives credit assignment for training.

### Player-facing channels (all derived from the event log and belief store)
- Dialogue and gossip: an NPC says a belief it really holds, with hedging from strength and source ("I heard...", "I saw...").
- News: the town crier or notice board publishes events that reached the town government, with a delay equal to travel time.
- Legends: a batch job scores events by `salience`, keeps the causal roots and leaves, and renders them with a grammar (Qud gospel style). Legends read the ground truth but are told "through a narrator" so they can be partial. Keep the gospel text deterministic from (event id, template id).
- Story sifting: add a few Felt-style pattern queries over the log for narratively good shapes (betrayal, revenge, rags to bandit, hoard theft chain). Matches raise `salience` and feed the news and legends. Start with 5-10 patterns. This lets story quality improve without touching the sim rules.

## Storyteller and pacing ideas that also help ecosystem stability

Both RimWorld and L4D adjust the frequency and amplitude of pressure events from a cheap measured state. Sim2 needs the same for the ecosystem: prevent bandit collapse or explosion, goblin wealth monopoly, dragon starvation or hoard runaway.

1. One small Director system reads aggregate state and only emits exogenous nudge events (logged with `cause_kind = STORYTELLER_INJECTED` and the state that triggered it). It never edits state directly. That keeps causality clean and the training fitness honest (training runs can disable it to see raw behavior).
2. Tension meter per region (L4D intensity): add on carriage robberies, deaths, famine events, dragon sightings weighted by salience. Decay over time, but not while a threat is engaged (Booth's rule). Phases Build, Peak, Fade, Relax drive what the Director may inject. During Relax it suppresses new threats (no new bandit recruitment spikes, no dragon raid) for a bounded time. Booth shows tuning by frequency and not amplitude, which is safer for ecosystem balance.
3. Wealth/adaptation (RimWorld): scale threat to colony wealth and count, with optional "adaptation" that eases after heavy losses. Sim2 analogue: if town population or carriage traffic falls below a floor, the Director delays bandit recruitment and sends a trade caravan or a guard patrol. If bandits are too few, raise hunger shocks (bad harvest event) rather than spawning bandits. This preserves cause: "bandits exist because towns were hungry".
4. Rubber-band by events, not by stats: prefer adding or removing opportunities (a rich carriage route, a harvest failure, a goblin truce) over modifying agent stats. The log shows exactly which nudge happened.
5. Cooldowns and weights (CK3): each injected event type has a cooldown and a weight from state, and "hidden" maintenance events are allowed. Use them for repetition control (no three dragon attacks in a row) and to allow quiet seasons.
6. Loss feeds story (Tynan, Nemesis): a failed carriage or a dead baker should create follow-ups (orphan joins bandits, guard vows revenge) not a dead end. Register "aftermath" hooks per event type that enqueue follow-up events with `cause` set.
7. Story sifting doubles as a health monitor: pattern counts (robberies per 100 days, goblin-dragon tunnels) are also ecosystem metrics, and the same queries can be fitness terms.
8. Exempt anchor events from suppression (Booth exempts bosses): e.g. seasonal harvest and dragon sleep cycles, so the skeleton stays predictable while the rest varies.

## What to avoid
- Fabricating causes after the fact in the live sim (Qud rationalization). It is fine for pre-history legends, but it breaks "every outcome has a recorded cause" for anything the trainer scores.
- Full per-fact knowledge for every actor and every tick (Ryan's own cost warning). Use coarse or derived knowledge for crowds.
- Letting the Director mutate state silently or use hidden hand-tuned rubber-banding. All injections must be events with logged triggers.
- Pure random storytelling (Randy) as the default; it produces unfair spikes. Random is fine as a test mode for robustness.
- Floats in belief strength and mutation rolls if you need bitwise determinism across platforms; use fixed-point and a seeded per-event RNG stream.
- Letting rumors leak ground truth: never put `accurate` or the true cause in an actor observation.
- Generating only "mostly-uninteresting" logs and expecting stories to appear: sifting needs salience tags and a small set of patterns (WAWLT notes logs are mostly uninteresting).
- Over-trusting one sim shape from another. DF never spreads false rumors (verified), so its model cannot show distortion, which Talk of the Town does. Pick mutation deliberately.
- Replay side effects: any training or logging sink called during replay must be disabled (Fowler).

## Open questions
1. How many actors get full belief stores? Proposal: named actors plus faction leaders full, crowds coarse. Needs a profiling budget.
2. Should the player ever get to query the truth (e.g. via a "legend" screen), or only belief-filtered views? Affects whether legends may contradict gossip.
3. Mutation strength: how much distortion before learned brains cannot learn from rumours? Needs an experiment (train with 0%, 5%, 20% mutation).
4. Do we need lies as a first-class action for bandits and goblins in version 1, or only mutation?
5. Director scope: per-region tension or one global? Booth used a per-survivor max; our analogue may be max over regions or per-faction.
6. Fitness from the log: which `outcome_delta` fields does the trainer want, and must the log be replayable from seed alone or snapshot plus tail?
7. Log size: events per day at target population; need retention rules (keep high-salience and their causal ancestors, compact the rest).
8. Unverified sources (Prom Week, Facade, Sims gossip, Bad News details, Radiant Story) may still yield useful ideas for social-action selection and quest-like events. Not read, so no claims made.
