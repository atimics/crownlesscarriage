# Scrivendays and zodiac signs

Status: proposed feature plan. Reviewed against main
`1b5a707cee9ea771c0b139c6f68690d5757b6175`, save schema 111, generator 25.
The numbers below are initial design choices for playtesting.

Scrivendays are annual gatherings where named scribes compare accounts,
commission expeditions, and publish their reckoning of history. Zodiac signs
give everyone a repeating sky calendar. A dragon's first change into a Deep
Wyrm begins an age; people learn of that change through observation and research.
Scribes may establish its date years later.

The first playable journey is: hear competing dates, carry a scribe's question
to a goblin site, bring evidence to Scrivendays, and deliver the resulting
almanac to another town. The town can then use that reckoning in its own records.

## 1. What the code already provides

All source links in this section refer to the reviewed revision above.

| Area | Current code | What the features need |
| --- | --- | --- |
| Calendar | [GrainSeasonFactor](../../src/sim/cc_production.c) uses 52 seven-day weeks, with four 13-week seasons. Sheep work also uses a 364-day cycle. Character ages, dragon thresholds, and annual reports use 365 days. | One named solar calendar with explicit phase and conversion rules; a separate review of old duration rules. |
| Clock | [World clock constants](../../src/sim/cc_sim.h) set idle time to zero game minutes per real second and travel time to 30. | Explicit work and rest actions that advance a meeting through the existing clock. |
| Dragon change | [ChangeDragonStage and AdvanceDragonEcology](../../src/sim/cc_sim.c) produce a dated event. Deepening requires age, crown strength, continuity, territory, and a usable Wyrmheart path. | A lasting private age anchor and locally acquired evidence about the change. |
| Gossip | `GatherGossipEvents` copies the event's text and exact day. `CcCoreParticipantBuild` passes held gossip dates into dialogue. | Reported date ranges and attributed observations for age research. |
| Scribes | [Archive staff](../../src/sim/cc_archive_staff.c) have lifetime character IDs, a work site, and up to four central archive positions. `AdvanceArchives` records accounts those scribes hold. | Meeting and research duties with real departure, attendance, and return. Town scriveners may participate alongside central archive staff. |
| Travel | `AdvanceCharacterTravel` moves travellers and carries news. It keeps appointed archive staff at their seat. [Recruitment](../../src/sim/cc_archive_recruitment.c) already quotes multi-leg travel, food, and payment. | A reserved duty that controls a scribe's journey and shares the existing route rules. |
| Host towns | [Archive seat planning](../../src/sim/cc_archive_seat_plan.c) checks people, paper, tools, grain, roads, and patrons. | A meeting-host quote using those material helpers and a fair rotation rule. |
| Books | [Archive volumes](../../src/sim/cc_archive_volumes.c) are physical treasures. `CcSimTomePassage` derives passages from current town values and current gossip. | Saved text and claims for historical records, plus copies and reading dates. |
| Custody | [Shared custody](../../src/sim/cc_custody.h) has document entries, holders, references, capacity, and transfer revisions. [Archive convoys](../../src/sim/cc_archive_convoy.c) preserve book IDs and travel costs. | Document work storage behind those references, and ordinary transport for evidence and almanacs. |
| Goblins | [Goblin politics](../../src/sim/cc_goblin_politics.inc) tracks three factions, porters, tribute, a founding crown contest, and cult ranks. | Dated local records and testimony about those activities. The founding contest dates a different event from deepening. |
| Observation | [Oven Court](../../src/sim/cc_oven_court.c) separates a local inspection from the action that records a dated note. | The same interaction pattern for sky observations, inscriptions, and witnesses, backed by lasting research records. |
| Sky | [Town sky](../../src/client/local3d/town_sky.inc) draws 96 procedural stars and a celestial disc. | Recognisable constellations and a moving year star, both driven by the shared calendar. |
| Storage | [CcSim](../../src/sim/cc_sim.h) holds 256 recent events, 32 gossip slots, and 24 treasures. | Bounded document storage with explicit preservation rules for evidence that must last across years. |

The existing Guild work supplies the right foundation:
[local appraisal #208](https://github.com/atimics/crownlesscarriage/issues/208),
[Guild #433](https://github.com/atimics/crownlesscarriage/issues/433),
[local holdings #435](https://github.com/atimics/crownlesscarriage/issues/435),
[research #437](https://github.com/atimics/crownlesscarriage/issues/437), and
[review and revision #438](https://github.com/atimics/crownlesscarriage/issues/438).
Scrivendays give that work a place, a date, and a recurring player purpose.
The first delivery can use a small part of those contracts with two schools
and one historical question. Their full institutional scope remains later work.

## 2. Zodiac: two visible cycles

Recommend **13 signs, 28 days per sign, 364 days per solar year**. Keep four
91-day seasons. Sign boundaries and season boundaries have their own positions;
both derive from the same solar phase.

The annual wheel names dates within a year. A slow bright body, provisionally
called **the Wanderer**, passes through one sign per solar year and completes
its circuit in 13 years. Its position at the new year names the year:
"Year of the Lantern." These are proposed rules for Crownless's sky.

This gives an astronomical basis to the names and a useful research problem:
an old inscription naming only the Lantern could belong to several cycles.
Scribes need a second clue to place it in history.

| Order | Proposed sign | Common associations |
| --- | --- | --- |
| 1 | Lantern | Departure, guidance, promises |
| 2 | Hare | New life, haste, escape |
| 3 | Hart | Kinship, pursuit, pride |
| 4 | Broken Crown | Succession, freedom, disputed rule |
| 5 | Hammer | Work, repair, endurance |
| 6 | Cup | Hospitality, bargains, shared meals |
| 7 | Sheaf | Plenty, stores, preparation |
| 8 | Quill | Memory, testimony, learning |
| 9 | Scales | Trade, judgment, debts |
| 10 | Gate | Passage, borders, welcome |
| 11 | Bell | Assembly, warning, remembrance |
| 12 | Ash Tree | Survival, ancestry, renewal |
| 13 | Wyrm | Possession, hidden power, great change |

These names are a proposed first set. Human towns and goblin traditions can
give the same star patterns different names as later content.

### Date rules

Keep the existing absolute simulation day as the saved time reference. Add
pure calendar helpers using wide integer arithmetic and floor division for
dates before the start of the simulated history.

For the first version, use solar phase anchor day 0. This matches existing
`current_day % 364` seasons. A day has:

- solar year index: `floor(day / 364)`;
- day within the year: `floor_mod(day, 364)`, in the range 0 to 363;
- date within a sign: `1 + floor_mod(day, 28)`;
- sign: `floor_mod(day, 364) / 28`;
- year sign: `floor_mod(floor(day / 364) + wanderer_phase, 13)`.

Use a fixed initial Wanderer phase of 0 and store a calendar rule version.
Day 0 is the first day of the Lantern; the current simulation starts on day 1,
the second day of that sign. This is a deliberate phase choice. Tests must
cover it. A later world-origin choice can set another explicit phase.

A public date can read:

> 6 Quill, Year of the Lantern
> Year 3 of Varkesh, by the Gloamgate reckoning

The second line appears once the company has learned that reckoning. Research
records also retain the cycle count where their author knew it. Partial dates
retain their precision: year, season, sign, day, or an interval.

Sky rendering reads the same world day and time as these helpers. Give each
sign a fixed star pattern. The Wanderer's position advances continuously around
that chart over its 13-year circuit. A year's name comes from its position at
the year's start. Cloud and horizon visibility affect what an observer can
record; an almanac can describe the expected sky from earlier observations.

Astrology enters daily life through beliefs and choices: a wedding on a Cup
day, a caravan blessing under the Lantern, or a ruler citing the Broken Crown.
NPC preferences operate within their supplies, duties, relationships, and
knowledge. Birth signs can be derived from known birth dates in a later step.
Keep the first delivery focused on dating, sky reading, and the gathering.

### Time compatibility

The new solar calendar and new feature schedules use 364 days. The first
calendar PR preserves existing absolute birthdays, deadlines, saved events,
dragon thresholds, and the 365-day durations used by older rules. Name those
legacy units explicitly where touched. A later balance PR can unify duration
rules with measured changes to lives, harvests, and dragon transitions.

An adopted dragon date changes a reckoning. The sky cycle and existing promised
deadlines keep their absolute days. A document preserves both its author's date
label and the reckoning edition used to write it.

## 3. Scrivendays: annual, seven days, rotating host

Hold one regional gathering each solar year, during **days 1 through 7 of the
Quill**. That is solar days 197 through 203, in early autumn. In the current
food model this falls within the strongest grain season. The name Scrivendays
refers to the whole gathering; each day has a different purpose.

| Day | Work and player opportunities |
| --- | --- |
| 1 | Arrivals, lodging, registration, public questions, and delivery contracts. |
| 2 | Read submitted records and hear witnesses. |
| 3 | Compare dates, translations, copying history, and sky observations. |
| 4 | Debate disputed claims and question their authors. |
| 5 | Propose a reckoning or a narrower range; fund further expeditions. |
| 6 | Sign findings, record dissent, and select the next host. |
| 7 | Finish the proceedings and almanac copies; book their delivery. |

Ongoing local research continues between gatherings. Scrivendays bring together
people and holdings that are usually separated by travel.

### Hosting and rotation

At the gathering, towns submit bids through present representatives or delivered
letters. A bid names a responsible host, a meeting place, supplies, lodging,
funding, and the routes delegates expect to use.

Choose among eligible bids by the longest time since the town last hosted.
First-time hosts take priority. Break ties by the travel burden described in
the submitted route plans, then stable town ID. This spreads attention across
the map while giving the company a reason to support a smaller town's bid.

Reuse the archive candidate's material helpers. The gathering quote adds the
actual delegate count, copying work, lodging, and travel budgets. A host can
import paper from a mill town. The meeting is a temporary use of a town; its
host selection leaves the archive's permanent seat and staff membership intact.

Announce the chosen host through physical notices. Confirm its local budget
56 days before opening, then send departure notices. Keep a proposed reserve
host in the proceedings. If the host fails before confirmation, issue a new
notice for the reserve. Allow at least 28 days for revised travel plans; a
delayed sitting can open up to 28 days after the ordinary date. Later disruption
can end that year's gathering with a recorded reason and carried-over questions.
The next year returns to the Quill schedule.

People act on notices that reach them. A delegate following an older invitation
may arrive at the original town and learn of the change there. That produces
useful escort, message, and resupply work.

The first generated gathering uses bids prepared by local named scribes and
patrons. Its initial notice gets at least 56 days of lead time. A prepared
campaign fixture may begin shortly before an already announced gathering.

### People, costs, and travel

Start with two small schools of scribes and capacity for three. A school is an
affiliation with named members, holdings, and signed findings. It shares the
existing people and material economy. Up to six delegates can attend a sitting.

Central archive staff and town scribes use the same character identities.
The central archive plans cover for at least one working keeper when it sends
delegates. A lone keeper can send a written submission with a carrier. Travel,
meeting, copying, and return occupy explicit duties; recruitment and other work
consult those reservations.

Use actual routes, travel days, food, fares, tolls, and supplies. Seeded costs
should follow existing recipes and wages. A quote lists the reserved amount and
payer. Food enters the existing nutrition accounting, and copying consumes
paper and work. Returned funds and goods go back to their recorded owner.

Meetings, research orders, and published works retain a deceased contributor's
name and lifetime ID. An unfinished duty can pass to a named successor with a
new assignment. Its earlier work keeps its original attribution.

## 4. How scribes date a dragon age

Keep three distinct records:

1. **The world event:** the dragon's actual first transition into Deep Wyrm form,
   retained by the simulation for replay and evaluation.
2. **The evidence:** what somebody observed, wrote, copied, found, or heard,
   with dates, access, and source history.
3. **The reckoning:** a scribe or school's proposed start date, supported by
   named evidence and adopted by particular readers or towns.

The first transition of a dragon lifetime creates its age anchor. A later
recovery of the same form belongs to that age. Its death becomes a major event
within the age. A successor's first deepening can begin another age. Future
worlds with several dragons can support regional reckonings through the same
dragon IDs and local adoption rules.

The private anchor stores the dragon's lifetime ID and name, the transition
day, and the rule version. Scribe research receives accessible records and
testimony. Information reaches public screens through the company's learned
accounts. Debug chronology remains a separate query.

### Evidence from goblin dynamics

| Expedition target | Finding it can support | Further work |
| --- | --- | --- |
| Porter or treasury tally | Tribute deliveries occurred on recorded dates, under a named court. | Compare an earlier copy, a witness, or a dated change in practice. |
| Former porter or cult member | A remembered change in duties, rites, or movement. | Establish the witness's time reference and whether it was firsthand. |
| Successive court records | The old form was reported at one date and a new form at another. | Examine copying, local dating, gaps, and political motive. |
| An observed Wyrmheart or its record | A witness encountered the heart by a stated date. | Inspect provenance and seek an earlier independently dated account. |
| Sky diary linked to a rite | The sky position helps place a witnessed rite within a cycle. | Establish which cycle and what the rite actually indicates about deepening. |

The present goblin crown contest begins around a dragon's founding succession.
Its `contest_started_day` answers that question. Deepening happens under separate
rules much later. Tribute totals and today's cult ranks describe current or
accumulated activity; they need saved observations to become historical clues.

Add a small set of observable goblin responses and dated practices with their
own actors and timing. Record them where witnesses or records actually exist.
Each inference rule states what a finding supports, how much its timing may lag
the change, and what further evidence would narrow the range. The player can
also encounter a misleading record or a mistaken witness.

First research questions use typed claims: observed form, rite, tribute delivery,
document date, source descent, and proposed deepening interval. A researcher
spends a stated work budget on local holdings. The output names consulted
sources, remaining gaps, and the next useful expedition.

### Agreement and revision

Use a clear first rule: a public reckoning needs present signatories from at
least two schools and support from two thirds of participating school votes.
Each school has one vote. Scribes can publish individual findings at any time.
A smaller gathering can hear evidence and commission work.

Assess source identity, independence as known to the readers, date precision,
contradictions, and the reasons each scribe accepts a claim. Confidence expresses
that assessment. A reported range may remain broad; a school may adopt one
exact day as its preferred estimate and record the supporting range beside it.
An adopted date can later prove mistaken.

Published findings retain signatures, dissent, cited works, composition date,
and available evidence at the sitting's cutoff. A late record can enter the
next hearing. New evidence produces a new edition. Existing copies preserve
their text, and corrections travel to their readers.

Known copies of one account share a source lineage. Hidden copying stays hidden
until a reader discovers it. That allows both careful scholarship and believable
mistakes while research remains grounded in each scribe's available sources.

For an adopted start day `S`, the reckoning labels the following 364 days Year 1,
then Year 2, and so on. Earlier dates can read "three years before Varkesh."
The solar signs continue on their own fixed phase. A later discovery applies
the new age label retrospectively through the new reckoning edition.

## 5. What the player sees and does

Add a Calendar page to the Company Book with four useful views:

- **Today:** sign date, year sign, locally learned age reckoning, and days left
  on accepted promises.
- **Next Scrivendays:** known host, opening date, invitation source, expected
  travel time, and available work.
- **The disputed age:** each learned proposal, its author, supporting sources,
  and the question the next expedition could answer.
- **Sky chart:** recognisable signs, the Wanderer's position, and observations
  the company has actually recorded.

At the host town, use existing approach, conversation, and service interactions.
The company can offer transport, present a carried record, bring a witness,
fund paper, commission a copy, listen to a hearing, and accept almanac delivery.
Each action shows its cost and time before commitment.

Offer explicit actions such as "Spend a watch comparing records" and "Lodge
until tomorrow's hearing." These advance the shared world clock. Looking at
the Calendar and reading an already learned finding are passive views. This
lets the player take part in a seven-day meeting with the current town clock.

A concrete first journey uses two schools debating adjacent candidate years.
One points to a goblin court record. The other holds a sky diary. The company
visits an accessible goblin room or road contact, secures a dated testimony or
tally through an ordinary bargain, and returns to the gathering. Their evidence
narrows the interval or changes a signatory's view. A paid almanac delivery then
changes the recipient town's displayed reckoning after arrival and reading.

The world continues when the player leaves. On return, the Calendar shows the
latest learned proceedings and the age changes reported in them. Shared-world
away advances process travel, sittings, and deliveries in the same order as
daily play. Invitations and unresolved questions can lead into a later sitting.

The shipped Deep Wyrm opening currently says that Varkesh transforms today.
Revise its public introduction and prophecy presentation to an attributed
warning, and let discovery establish the date. Keep the existing historical
snapshot and absolute opening day. Any starting testimony needs an explicit
author, observation, and custody history in the campaign fixture.

## 6. Code shape and saved state

Use small modules alongside the existing simulation:

| Proposed module | Responsibility and existing connections |
| --- | --- |
| `src/sim/cc_calendar.[ch]` | Solar dates, sign cycles, age-label conversion, overflow checks, and pure formatting inputs. Used by simulation, reports, client, and shared snapshots. |
| `src/sim/cc_documents.[ch]` | Saved works, claims, copies, local holdings, source links, and explicit reading. Implements the narrow #435/#437 contract through `cc_custody`. |
| `src/sim/cc_reckoning.[ch]` | Private age anchors, research questions, evidence assessment, proposal editions, signatures, and local adoption. Public queries take a reader ID. |
| `src/sim/cc_scrivendays.[ch]` | Host bids, schedule, attendance, duties, hearings, costs, and proceedings. Calls archive, route, recruitment, and transport helpers. |
| Client Calendar page and sky helpers | Calendar/meeting actions in `cc_adventure.inc` and `main.c`; constellations in `local3d/town_sky.inc`, later shared with road views. |
| Persistence and dialogue | Save tables and journal actions; `cc_sim_hash.c`, validation, legacy migration, shared commands, and held-account dialogue. |

A historical work stores its author name/ID, composition place/day, literal text,
typed claims, cited source editions, and original date labels. A copy references
that work and its actual custody. A reading records who learned which edition
and when. A catalog can hold a title and location while contents require access.

A gathering stores its stable ID, scheduled year/day, host, alternate, notices,
phase, delegates, actual arrivals, duties, reserved supplies, questions, and
publication IDs. A reckoning stores dragon ID, proposed date/range, evidence,
authors, version, and local adoption records.

Begin with one active gathering, up to six delegates, three schools, four
research orders, and a proposed pool of 64 document works. Final claim/citation
limits should follow fixture measurements before the schema change. Copies
consume shared custody capacity. Active cargo, cited works, unread deliveries,
and the company's held editions stay protected. At capacity, copying or intake
returns a clear storage reason. Deliberate summaries preserve their actual
claims and source references. Treat lost contents as lost to their readers.

Record new private age anchors independently of the recent event buffer. Older
saves initialise a known anchor only from a supported retained transition or
an explicit campaign record. Otherwise retain an unknown historical start.
Every lasting field needs save, load, hash, validation, and migration coverage.

## 7. Delivery order

Each row is a focused implementation PR or a small group if save boundaries
require it. Refresh the current base and schema before implementation.

| Order | Delivery | Playable or reviewable result |
| --- | --- | --- |
| 1 | Calendar helpers, 13 signs, date display, and an almanac card. | One consistent solar date across town, road, mine, text, and shared play; existing absolute timing preserved. |
| 2 | Saved local works, claims, copies, reading, and one goblin evidence source. | Present a record to a named local scribe and obtain a sourced date range. Reuse #208/#435/#437. |
| 3 | First-deepening anchor, observer reports, research, and reckoning editions. | Two readers can hold different dates; a delivered source can revise one reader's view. Update gossip and the campaign introduction together. |
| 4 | First prepared Scrivendays, delegate duties, hearing, and paid almanac delivery. | Complete the first journey through ordinary controls, with return and saved consequences. |
| 5 | Annual scheduling, delivered host bids, rotation, reserves, and disruption. | Run several gatherings across different towns while keeping attendance and material costs valid. |
| 6 | Constellation art, Wanderer motion, night observation, and calendar customs. | Recognise a sign in the sky, record an observation, and use it in a historical question. |

The text/date layer can ship before the visible sky work. The completed zodiac
feature includes both. The first full player milestone includes steps 1–4 and
a simple sky observation; later steps broaden recurrence and presentation.

## 8. Acceptance and evidence

Calendar checks cover sign changes at 28 days, seasons at 91 days, years at 364
days, the 13-year return, the day-0 phase, negative historical dates, and the
maximum simulation day. Revising a reckoning preserves absolute deadlines and
original document labels. Passive views preserve the whole-world hash.

Knowledge checks compare identical accessible holdings with different private
dragon truth: the researcher must return the same finding. Other fixtures vary
the delivered evidence while holding private truth fixed. Cover repeated copies,
discovered dependence, uncertain dates, conflicting accounts, an absent original,
a dead witness, and an incorrect adopted date that later changes.

Travel checks cover blocked roads, host failure, stale notices, late arrival,
delegate death, concurrent recruitment, home archive cover, return travel,
insufficient supplies, and receipt exactly once. Reconcile money, food, paper,
tool wear, cargo, and work reservations through existing accounting.

Persistence checks cover every added field independently, source text after town
state changes, evidence after recent-event rollover, full storage, legacy saves,
and the shipped Deep Wyrm snapshot. Compare one-day stepping with long advances
through several gatherings and dragon lifetimes. Compare offline and shared
commands, including repeated requests and save/resume during a hearing.

Extend the existing archive staff/recruitment/convoy, gossip, dragon-cycle,
custody, persistence-fields, journal, and shared-play suites. Add focused calendar,
document, reckoning, and Scrivendays cases through `cmake/tests/`. New event kinds
also need grammar rules and compiled-account coverage under
`tools/dialogue/audit_grammar.py` and `tools/compile_core_accounts.py`.

For the player milestone, capture and inspect the current interface before its
change. Replay the complete evidence journey in native and browser builds,
including narrow screens, ordinary controls, a disputed result, and a restart.
Record which evidence changed a scribe's conclusion and when the second town
learned the resulting calendar.

For recurrence, run a fixed multi-seed set for at least 30 solar years. Report
host distribution, completed/cancelled sittings, travel and food costs, archive
work lost to attendance, research outcomes, delivery delays, and surviving
editions. A separate long-run check covers several dragon successions and
storage turnover. Tune cadence and meeting length from those results and play.

## 9. Decisions to carry into implementation

Use the following defaults for the first fixture: 13 signs, a 13-year Wanderer
cycle, annual seven-day Scrivendays in the Quill, delivered bids with host
rotation, two schools, one disputed deepening, and a physically delivered
almanac. Sign names, meeting duration, delegate budgets, and vote thresholds
remain tuning choices. Keep the user's central rule throughout: scribes discover
and debate the date through expeditions and evidence.
