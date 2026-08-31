# Story, Quests, and Narration

This document adds three layers the manual currently lacks:

1. A **story spine** that names the mythos the simulation already implies.
2. A **quest architecture** — fronts, clocks, evidence-driven completion, and a
   bounded fairy-tale puzzle layer — that organizes situations into arcs.
3. A **narration contract** that defines exactly what an LLM may be fed and
   what it may say, so prose can be added later without touching the kernel.

Sources researched: Sierra's *King's Quest* (1984–1998) for landmark puzzles,
inventory scarcity, and legitimacy fables; OSR play culture (B/X procedures,
gold-for-XP, reaction and morale, hex and dungeon turns, domain play) for
diegetic structure; CosyWorld (`~/develop/cosyworld`) for clocks, fronts, jobs,
evidence-driven completion, sparse beat budgets, and the certified
situation-packet AI boundary; `glm-roguelike` for phase-aware actors;
`zero-grounded-literary-lm` for state-derived text corpora.

## Design diagnosis

The current manual produces excellent *episodes*: a crisis chain compiles into
one situation, the player intervenes, the world changes. What it does not yet
produce:

- **Arcs.** Nothing groups episodes into a season with a beginning, middle,
  and end. Players get weather, not chapters.
- **A mythos.** The systems already describe a post-imperial interregnum in
  which legitimacy is material — but no document says so, so every
  implementation choice has to reinvent it.
- **A prose identity.** Dialogue is "authored from situation templates and
  verified ledger facts." Templates carry facts but no voice, and no contract
  exists for upgrading prose without letting it lie about state.

## Part 1 — The story spine: the Interregnum

The mythos is a naming and projection layer over objects that already exist.
It adds no simulation state.

| Existing system | Mythic reading |
| --- | --- |
| Iron Ledger monasteries with copied accounts | The chancelleries of a dead empire; their deposits are its residual law |
| Named treasures (maker, owner, location, history) | Legitimacy tokens — regalia, foundation charters, oath-gifts |
| Dragon crown strength from physical objects | The region's lost regalia; a Crowned dragon is an uncrowned king wearing the hoard |
| Goblin court (Hoardkeepers, Tongues, Ashkeepers) | The empire's forgotten cult, still keeping forms whose meaning is lost |
| The player's cross-border charter | An imperial carriage warrant — the last paper both crowns still honor |
| Kingdoms with weak legitimacy and treasuries | Successor states ruling by administration, not by right |

Three canon facts, each already supported by the simulation:

1. **Legitimacy is a physical object.** A kingdom's founding regalia is a named
   treasure. While it sits in a dragon hoard, a bandit camp, or a dungeon, the
   kingdom is literally crownless. Recovering it is a physical delivery.
2. **The crowns are failing, not absent.** Kings borrow from the Iron Ledger,
   fight over bottlenecks, and send fallible couriers. The interregnum is a
   process the player can slow, survive, or finish.
3. **The carriage is the last imperial institution.** Roads, accounts, and
   crowns disagree; the charter still moves. This is why everyone tolerates
   the player, and why the player's *choices of what moves* are political acts.

This is the King's Quest inheritance made causal. Graham became king by
carrying three treasures back to the castle; the Crownless player is offered
the same errand in a world where the treasures have owners, debts, and
consequences. The fable becomes a logistics problem, which is what this game
already simulates.

### Mythos rules

- Mythos never creates, moves, or re-prices a good. It names things the
  simulation already tracks.
- Every mythic claim must reduce to a ledger query ("who owns the regalia,
  where is it, who remembers it").
- The mythos is discoverable, not expository: monastic account books, goblin
  liturgy, treasure histories, and route chart marginalia are its sources.

## Part 2 — Fronts: arcs from economic pressure

Ported from CosyWorld's Front entity, adapted to Crownless's situation
compiler and time contract.

A **front** is a season-scale arc that the situation compiler derives from
the causal ledger. The vertical slice's drought-bridge-famine chain is one
front. Fronts are not authored plots; they are groupings the engine proposes
when several situations share causes, actors, or stakes.

### Front record

| Field | Meaning |
| --- | --- |
| `premise` | One sentence a player could retell |
| `cast` | Named characters occupying situation roles |
| `stakes_questions` | 2–4 open questions the world will answer through play |
| `portent_clock` | Region-scale danger clock; fills on committed days |
| `situations` | Active/queued situations this front spawned |
| `resolution` | Validated effects and chronicle entry on completion |
| `invalidation` | What closes the front without resolution |

### Front rules

- At most **two active fronts per region**, plus the existing cap of three
  active situations. Stakes outrank new opportunities (CosyWorld's sparse
  beat budget).
- The portent clock advances only through committed time (travel, rest,
  construction), honoring the time contract. A quiet world is still.
- Front resolution is a **chronicle event**: it writes a permanent history
  entry, fires delayed echoes, and may transform places and characters.
- Fronts must remain invalidatable. If another actor resolves the underlying
  pressure, the front closes or transforms — never freezes.

### Example: the Empty Granary as a front

- Premise: "The bridge is closed and the mining valley is eating its seed
  corn."
- Stakes questions: *Who eats this winter?* / *Does the bridge reopen, or
  does the night road become permanent?* / *Who owns the mine tunnel when the
  crisis ends?*
- Portent clock: famine deaths, mine collapse, or the subterranean colony
  breaching — fills while the food shortfall persists.
- Situations spawned: relief convoy, bridge negotiation, dungeon smuggling,
  bandit recruitment, colony containment (the vertical slice's interventions).
- Resolution outcomes map to the existing slice consequences: structural
  (bridge reopens), criminal (night road legitimized), exploitative (food
  arrives through the black market), or failed (mine abandoned, successor
  bandit group).

## Part 3 — Clocks inside situations

Formalize the existing "deadline or escalation rule" as first-class clocks
(CosyWorld's model):

- Every situation carries a **progress clock** and a **danger clock**.
- Clocks fill from authored contribution strategies bound to real actions —
  a named delivery completed, a shipment protected, a faction paid — never
  from generic proximity or time alone.
- `on_fill` is a validated effect descriptor; `created_by_event_id` and
  `resolved_by_event_id` keep every tick traceable to the ledger.
- Claims are idempotent (once per actor, per target, or per actor-and-target);
  retries cannot duplicate progress.
- Quest status is *derived* from clocks: filled danger clock fails the quest,
  filled progress clock completes it. Explicit terminal status is a legacy
  snapshot convenience only.

## Part 4 — Evidence-driven quest completion

CosyWorld's `world.logistics.completed` derivation is the template. Crownless
already journals every committed action, so delivery, escort, and rescue
quests can be completed only by a **provable causal chain**:

1. The named actor acquired the physical object or person.
2. Continuous movement events carried it toward the destination.
3. A delivery/transfer event completed at the destination with no intervening
  break in possession.

Remove any link from replay and the completion cannot be derived. Generic
"work" or "help" verbs never complete a delivery. This makes quest success
the same class of fact as every other fact in the ledger: replayable,
hashable, and impossible to fake.

## Part 5 — Curiosities: the fairy-tale layer

The one deliberate import from King's Quest. A **curiosity** is a unique,
authored, non-commodity object that unlocks exactly one authored interaction
somewhere in the world.

| Rule | Enforcement |
| --- | --- |
| One carriage slot each | Existing slot rules; a curiosity displaces eight Food |
| No simulation state | Only owner, location, history — the named-treasure schema |
| One authored binding per curiosity | Pack validation rejects unbound or multi-bound curiosities |
| Finite catalog, ≤1 new curiosity per arc | Prevents inventory bloat and lore generation |
| Never combat stats | A curiosity solves a *social, spiritual, or route* problem |
| Clues circulate through the rumor economy | The player learns of it, not from it |

The cargo-slot rule is the point. King's Quest's inventory puzzle was "what
do I carry"; Crownless's is "what do I displace." Carrying the bridle means
four fewer Food units for the famine convoy. The fable layer and the economy
argue with each other, and the player arbitrates.

Worked example — *The Third Bridle*: a river creature is interrupting barge
traffic (monster ecology, already modeled). A curiosity — an old bridle — is
held by a bandit leader who values it as a trophy. The player may negotiate,
steal, fight, or trade for it; presenting it at the correct fording place
converts the creature into a willing ferry. Route capacity rises, the bandit
camp's story changes, and the chronicle records all three. Every step touches
existing systems; the curiosity only binds them.

### Rumor economy

The information-packet system becomes the fable layer's circulation:

- Each settlement's rumor board draws from recent local ledger events with
  accuracy and omission — never omniscient truth.
- Curiosity hints, front stakes questions, and faction intentions arrive as
  rumors, so the player's *knowledge* is also localized and fallible.
- Rumors are the primary LLM text surface (Part 7): short, bounded, validated.

## Part 6 — OSR procedure upgrades

Most of the 1980s procedures are already latent in the manual. Make them
explicit so implementation inherits them:

- **Expedition turns.** Dungeon exploration runs on the sub-day travel clock.
  Light, rations, and water are cargo; every descent is a logistics decision.
  Retreat, resupply, and return are supported states, not failures.
- **Reaction before combat.** Human opponents open with a demand, not an
  attack. Morale is a visible clock: defeated survivors surrender, flee, or
  defect, and each outcome writes back to the camp.
- **Retainers.** Passengers can be hired as guards and specialists. They take
  payment, eat Food, remember treatment, and can be promoted to named
  characters — the existing promotion rules apply unchanged.
- **Treasure as progression.** The carriage is the progression system; its
  upgrades track value *delivered*, not enemies slain. Slaying pays only when
  someone pays for it.
- **Domain endgame.** Front resolution and regalia recovery are the region's
  domain play: the player ends seasons having changed who rules what, without
  ever holding a throne.

## Part 7 — The narration contract

The contract for adding LLM prose later. Ported from CosyWorld's AI-referee
rules and packet discipline, adapted to this ledger.

### Non-negotiable invariants

1. **State first, prose second.** The simulation commits; the model narrates
   committed truth only. LLM output is never simulation input; the only
   feedback loop runs through the player's next decision.
2. **The kernel decides truth.** Prose that claims an item, exit, quantity,
   price, or state change the ledger does not support is rejected or replaced.
3. **Fail closed.** With no inference available, deterministic template prose
   remains complete and playable. A narration action fails visibly and
   without charge rather than inventing speech.
4. **Untrusted text is not instructions.** Names, biographies, treasure
   histories, rumor text, and player input inform fiction; they are never
   model instructions.

### The certified narration packet

Every narration request is a deterministic extraction from the causal ledger
and simulation state — never a raw state dump:

```text
Scene
  visible facts and local projections at this place
  present named characters and their role in the situation
  current front, clocks, and stakes questions (if public)

Knowledge
  facts this audience may know (per-character knowledge filter)
  excluded claim classes (what must not be said)

Offers
  legal interventions with target, cost, and risk
  (only for pre-commitment narration)

Outcome
  committed action, authoritative result, state changes

Style
  voice register (see below), length, sensory frame,
  required names, forbidden claims
```

Omit what the audience cannot know rather than asking the model to hide it.

### Voice registers

Small authored style cards, one per source of speech, so prose has identity
without a general-purpose persona system:

| Register | Used by |
| --- | --- |
| Fable voice | Narration of curiosities, myths, the Crown Cycle |
| Chronicle voice | The history book; kings, wars, ledgers |
| Market voice | Merchants, innkeepers, rumor boards |
| Liturgy voice | Goblins, monasteries, dragon omens |
| Road voice | Drivers, bandits, couriers, retainers |

### The model cast

- **Planner** (structured output): chooses which named character responds,
  which candidate action an NPC takes. Closed candidate sets, kernel-validated.
- **Voice** (prose): writes public speech and scene narration from packets.
  May not state that an uncommitted action has happened.
- **Chronicler** (summarizer): writes the kingdom/season chronicle from
  ledger deltas at front resolutions and week boundaries. Factual claims
  (numbers, owners, places) are validated against the ledger; failures are
  rejected.

### Generated delayed echoes

Delayed echoes — letters from known characters, returning passengers' news —
are generated from the same packets under the *recipient's* knowledge filter
plus their stake register (what they want, fear, and remember about the
player). A letter can be wrong about causes, because characters are, but
never wrong about ledger facts they witnessed.

### Corpus direction

The append-only journal plus deterministic replay is a ground-truth corpus
factory: ledger deltas in, validated text out. This supports hallucination
evals and later fine-tuning (`zero-grounded-literary-lm`'s `state_corpus`
direction) without changing any runtime contract.

## Part 8 — Three example arcs

### Arc 1: The Hollow Crown (region arc)

A kingdom's founding regalia — three named treasures — sit in the dragon
hoard. Legitimacy is weak, the Iron Ledger debt is large, and both theft
chains (Ash-Poor Company and Crown Levy) converge on the same cave. Front
stakes: *Does the king keep his throne?* / *Who pays the dragon's debt?* /
*What replaces a failed crown?* The player may return the regalia (legitimacy
rises, the debt chain begins), sell it to a rival, leave it hoarded, or let
the theft-and-burning cycle run. Every outcome is existing machinery; the arc
is a front that binds them with a mythic name.

### Arc 2: The Third Bridle (fable arc)

Described in Part 5. The curiosity converts a monster-ecology problem into a
route-capacity solution through negotiation, trade, or theft rather than
slaughter — and the bandit camp's story continues either way.

### Arc 3: The Winter Courier (war arc)

Two sealed dispatches enter the courier network at once: a peace offer and a
muster order. Both are physical, fallible, and slow. The player can carry
either as a charter, sell one to a third court, delay both, or deliver the
wrong one into a suppressive court. Front stakes: *Which message arrives
first?* / *Who learns the other existed?* The alliance-versus-war outcome of
the existing courier system becomes a playable tragedy of choices.

## Part 9 — Validation gates

Every addition in this document passes the constitution's scope test:

| Addition | Player decision | Connects to | Local expression | Persistent consequence | Test |
| --- | --- | --- | --- | --- | --- |
| Fronts | Which arc to engage | Situation compiler, ledger | Stakes questions on boards, NPC speech | Chronicle entries, place transformation | Two fronts max; determinism preserved |
| Clocks | How to spend committed time | Journey/dungeon ticks | Visible progress and danger | Validated `on_fill` effects | Idempotent claims; replay-equal |
| Evidence completion | What to carry and protect | Journal, shipments | Board contracts | Quest truth = ledger truth | Chain-removal breaks completion |
| Curiosities | What to displace | Cargo slots, rumor packets | Fable interactions | Route/faction changes | ≤1 per arc; bound; no stats |
| Narration contract | None (surface only) | Ledger extraction | Prose identity | None (one-way) | Packet-only input; fail-closed |

Additional invariants:

- The mythos adds zero simulation state.
- LLM output never mutates state; narration is write-once.
- All generated text is validated against the packet or rejected.
- Deterministic replay never requires inference.
- The attention contract still holds: two fronts, three situations, one
  material world beat per interval, stakes outranking opportunities.
