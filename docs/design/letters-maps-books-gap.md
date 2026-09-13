# Letters, Maps and Books — gap analysis against the code

Status: analysis, 7 September 2026. Companion to the letter probe
(`tools/letter_probe.c`), the three probe experiment captures, and the
schema-51 silent-dragon fix. This doc says, for each piece of the design,
what the code already has, what is missing, and what ships first.

## The model under analysis

Each character has **one notable topic**. That topic governs the small set of
gossip facts they hold (a **page** of at most 3 facts per person, the same
page a scout's letter cites). A research book is **up to 10 pages** (at most
30 facts). A scriptorium research mission sends a scout out with a book
pre-filled from the archive's current holdings on the topic; the scout
collects facts **newer than the baseline** or **contradicting it**, fills
pages, and returns the book so intake hears what the archive did not know.

This is the simplest form of the model that still gives the core game loop:
*the scout is the only character whose knowledge advances by physically
going to look, and their book is how prior knowledge survives dead towns.*

## What the code already has

| Design piece | Code today | Notes |
|---|---|---|
| Held-account boundary | `CcSimPersonalGossip` (carrier → story + version); `CcSimNextUntoldStory` | A character only cites accounts they hold. Freeze-tested in `gossip_tests` (isolation, boundary, no global read). |
| Fact = event + version | `CcGossip` (event_id, text, per-town `local[]` versions, court bias, alarm, confidence, retellings, recorded) | The account/version spine the design needs; gossip, documents, and carriage are three delivery channels over it. |
| Accounts propagate on roads | `ExchangeGossip` between carriers and towns; `CcSimRefreshCharacterGossip` daily | Inter-town travel works; this is the #278/#433 spine. |
| Scriptorium as intake | `CcArchives` (scribes, lore_stored, recorded, paper/tools), `HearGossip`, `AdvanceArchives` | A delivered fact reaches intake (`heard_day`); the weekly archive step writes tomes. |
| Physical material gating | paper/tools ladder, `CcMaterialChainSnapshot`, scriptorium town | Copies and tomes have real costs; scarcity shapes the economy. |
| Map sentiment | `CcMap` = 12 collectible route-charts, catalogue/archive masks, atlas | Maps are already "documents about roads," just fixed-slot and buy-instant. |
| Courier / dispatch | `CcCourier` (war/peace/dragon), `CcSituationKind` includes courier delivery | Sealed documents physically travel; kinds are narrow. |
| Dragon accounts | dragon lifecycle events + `IsNotableGossip` (schema 51) | The silent-dragon fix: dead-town dragon news can reach intake now. |

## What the fixes just changed (schema 51)

1. **Dead towns no longer silence their own stories.** `GatherGossip` drops
   nothing on abandoned origin; `ExchangeGossip` lets a carrier at a ruin
   pick the story up. A whelp hatching in a hollow lair can now reach the
   Scriptorium **if someone goes to look** — which is the scout's whole
   reason to exist, and now provable (new regression test carries the whelp
   fact lair → scout → intake).
2. **Dragon succession is notable regardless of magnitude.** Whelp dispersal,
   successor hatching, afterdeath, un-crowning, crowning, slaying and hoard
   recovery gossip like omens do — a future chronicler can find them.

The 1000-year runs at seed 9: before, zero dragon facts after year 3; after,
the dragon's death and hoard-return narrative circulates, and the mechanics
test carries the whelp fact end-to-end.

## The gap: what the model needs that the code does not have

| Gap | Current code | Model needs | Where it docks |
|---|---|---|---|
| **Per-character notable topic** | `CcCharacter` has role/goal but no topic; every resident of a town holds the same ~30 accounts (in-town flattening) | One `notable_topic` per character; the gossip they *hold* is filtered to it and bounded to a page (≤3) | Sim change (schema bump): add field, promote/hold by topic, bound holdings |
| **Page = 3 facts** | `CC_MAX_GOSSIP` 32 per carrier, all kept | Page bound: when a character hears a new on-topic fact, it replaces the least-valuable on-topic fact | Sim change within the same schema bump |
| **Book = 10 pages** | no book object; `CcTreasure`/`CcMap` are the piecewise carriers | A carried book with bounded pages, each page holding ≤3 facts with provenance | #434 custody object OR reuse `CcTreasure` as the book entity |
| **Baseline + contradiction** | no record of "what the archive knew on day D" beyond gossip `recorded` | Book pre-filled from archive holdings as of mission day; novelty = after baseline; contradiction = same kind/origin but different claim | Probe `--scan` coverage line is the stand-in; real rule lives in `#438` |
| **Mission = situation kind** | `CcSituationKind` has no research mission | `CC_SITUATION_RESEARCH` (patron, topic, region, baseline, reward, deadline) | New situation kind + quest objective |
| **Role-topic mapping** | roles exist, goals produce discourse | one topic per role, e.g. herdsman→herds, guard→bandit/road, official→throne, traveller→road, refugee→wheat | `#277` scorer input; probe `RoleNotableTopic` |
| **Scout NPC travel** | characters are static at towns; `TRAVELLING` activity exists | a scout travels on the road network, exchanges at each stop, fills pages | `CcCourier`/`#278` + `#204` NPC caravans |
| **Book return + intake** | `HearGossip` handles a single story | returning a book delivers N pages; intake records per-page facts as heard | Extend `HearGossip` to a batch delivered-by-book |

## The topic table (probe already implements it)

`dragon, goblin, war, throne, wheat, herds, ponies (WIP), road, bandit, treasure`.
Scan on seed 9 year 10 showed throne/wheat/bandit researchable, war/herds/
road/treasure empty because their event kinds are not `IsNotableGossip`.
Two options: widen notability for livestock/courier/ledger events, or leave
those subjects to be researched only through books, ledgers and the
cartographer's archive — a design decision, now measurable per topic.

## Ship order (each is a focused PR with its own schema discipline)

1. **Probe: mission mode** — `--mission herds --baseline 730` pre-fills a
   book from pre-baseline holdings, collects only newer/contradictory
   on-topic facts, fills pages, prints the returned page + contradiction
   flags. No sim change; makes the mechanic tangible with real data.
2. **Sim schema: one notable topic per character + page bound** — add the
   field, gate on schema 52, update hash/save/validate; bounds holdings to
   ≤3 on-topic facts, breaking the in-town flattening (the observable fix).
3. **Research mission situation** — `CC_SITUATION_RESEARCH` + scout travel
   + book custody (#434), reusing the schema-51 pickup so missions can
   recover dead-town lore.
4. **Book/intake batch** — a returned book delivers several facts at once.

## Outcome of this analysis

The probe proved the design has two halves: **the telling network** (gossip,
in-town, role-filtered) and **the looking network** (scout missions, books,
baseline/contradiction). The code has the telling network fully but with the
in-town flattening and the dead-town silence — the silence is fixed (schema
51); the flattening needs the per-character topic (ship order #2). The
looking network is entirely greenfield: it is the research book, the
mission, and the scout, and it is where the new letters actually come from.