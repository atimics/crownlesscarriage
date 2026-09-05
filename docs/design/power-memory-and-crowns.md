# Power, memory, and the movement of crowns

Design proposal, 5 September 2026. The measured baseline is linked below. The
experiments and political rules in this document are proposed work.

## The central question

Who gets obeyed today, and how will people explain that obedience tomorrow?

A ruler holds power through people who carry out orders, collect dues, guard
gates, settle disputes, and supply food. A court can recognise a claimant while
a frontier town serves a rival. An army can obey someone whose ancestry its
captain doubts. Each kingdom develops customs for settling these tensions.

The scriptorium records accounts of who **was** king and what happened during
that reign. Later readers use those accounts to understand their world. An
account can become a precedent, a family grievance, an oath, or a reason to
support a claimant. Its influence passes through readers and their choices.

The archive's own standing grows through use: people copy it, teach from it,
cite it in disputes, and trust people who rely on it. A patron can buy scribal
time. A witness can persuade a careful scribe. A surviving copy can outlive both.
Scribes should have loyalties, limits, pride in their work, and reasons to
disagree with patrons.

## Four things to keep track of

| Part of the world | Example | How it changes |
| --- | --- | --- |
| Material events | A captain receives 40 crowns; a bridge burns. | People move or consume things. The engine records exact transfers. |
| Accounts | A traveller says the queen paid the captain. | Witnesses speak, couriers travel, scribes select and copy accounts. |
| Belief and recognition | The captain believes a prince has the better claim but publicly serves the queen. | Evidence, custom, relationships, fear, and expected support affect each person. |
| Remembered history | A later chronicle calls this the queen's first year. | Surviving accounts shape later readers, who select and retell them. |

The engine can record who held a gate, who paid whom, and who publicly recognised
each claimant. The word "king" is a social conclusion drawn from those acts.
During a dispute, show whose court or town uses that title. A historical reign
can have competing dates and names, each attached to an account and its source.

For the proposed toy ledger, useful ideas are linked records, named witnesses,
copied histories, delayed delivery, and disputes over which copy to follow.
People and institutions determine the practical acceptance of a claim. Payment
requires an actual transfer of crowns; accepting a debt creates an obligation
that someone may later try to collect.

## What the code already contains

Reviewed against main at `d048f2a71e2d62ee6efd9a1463e01f2e03b464b9`.

| Existing piece | Design implication |
| --- | --- |
| `CcSimTrackedGold` sums crowns held by markets, treasuries, the player, the reserve, and several hoards. | Build physical custody on the existing conservation rule. |
| `CreateTradeShipment` moves goods into a travelling shipment but credits the seller's market at departure. Debt repayment also directly transfers balances. | Add custody and travel time for distant crown payments. Current totals already conserve crowns, while payment travel is abstracted. |
| `CcKingdom.iron_ledger_debt` stores one amount per kingdom. | Conflicting financial accounts need their own records and readers. |
| Gossip has local versions, carriers, confidence, alarm, and court bias. Scribes preserve a version they heard. | Extend an existing account and transmission system. |
| Archive volumes are physical objects. Scribes need food, paper, and tools. Books can decay or burn. | Preservation, copying, and access have material costs. |
| `AssignHistoryOffices` selects office holders using an internal score. `AdvanceCoronationLaw` grants anointing when the scriptorium is supplied. Its text says the crown becomes lawful. A year of silence can trigger a pretender crisis. | Replace these direct links with claims, recognition, and actions by relevant people. A ceremony records the participants' support. |
| `AdvanceArchives` moves every kingdom's legitimacy toward a target based on scribes and stored lore. | Derive political standing from particular audiences and the accounts that reach them. |
| Characters have knowledge and memories; relationships track trust and obligation. Shops, stocks, routes, credit, and service projects already shape local work. | Give these existing systems clearer social ties. Workshop ownership can emerge from custody, agreements, and recognised rights. |

Code entry points: [simulation](../../src/sim/cc_sim.c),
[state types](../../src/sim/cc_sim.h),
[conservation test](../../tests/sim_tests.c),
[town evolution tests](../../tests/town_evolution_tests.c), and
[metrics runner](../../tools/sim_metrics.c).

Raw gold in `CC_GOOD_GOLD` is a material good. Crowns use `CcMoney`; keep their
accounts separate. The current event parent links often select a recent local
cause. Specific credit for a delivery or a repair needs explicit contribution
links.

## A small political model

Start with one disputed succession and a few named actors in one kingdom:
the current claimant, a rival, a court officer, a guard captain, a merchant,
a town representative, and a scribe. Use existing character and faction records
where they fit. Give each actor resources, obligations, trusted sources, and a
small set of choices.

Track these separately for each actor and claimant:

- **Belief:** what the actor thinks happened and which custom applies.
- **Public recognition:** whom the actor names as ruler.
- **Action:** whose taxes they collect, orders they obey, or soldiers they feed.
- **Expectation:** whom they think other important people will support.

An actor can privately doubt a claimant and still serve them. Changing sides has
costs: an oath, a relative at court, a trade contract, danger on the road, or a
fear of being alone. Existing ties give support some staying power. Reconsider
support when news arrives, promises fall due, conditions worsen, or allies act.

Power has a location and a purpose. A captain can control the bridge. A merchant
can advance food. A court can settle a title dispute. Keep a kingdom-wide
legitimacy summary for display if useful, with its contributing audiences
available for inspection. Resolve concrete acts through the people involved.

For an archive claim, record the subject, claimed event and date, author,
witnesses, source copy, place, writing date, patron if known, and custody.
Record when an audience receives it. Later accounts may cite, dispute, or omit
earlier accounts. Shared ancestry between copies matters: ten copies of one
story represent one line of evidence. Preserve rival accounts when readers
still hold them.

Let practical facts anchor inquiry: a payment's custody, a dated journey,
scorch marks, an occupied gate. Let meaning remain open: whether an act was
rightful, whether obedience was freely given, and which reign a year belongs to.

## Thornford as a playable example

The following is a proposed scenario, with invented names.

Mara's soldiers hold Thornford's bridge. The local court uses her seal. The
miller sells grain to her patrol because they keep the road open. An old
chronicle records her father as a steward; her rival reads this as proof that
the family held the town in trust.

A new account describes the father's years as a reign. The scribe based it on
letters from the court. Mara pays to copy it. Some families accept the account;
others remember dues paid under the former ruler. A captain's children grow up
hearing Mara's version.

The player can carry a witness, recover an older roll, secure grain for the
patrol, negotiate a shared toll, or help someone leave. Each choice reaches
particular people and has a cost. A recovered document gains force when someone
uses it and others accept the result.

Years later, the mill's charter may depend on which reign the court recognises.
Workshop ownership means that the miller, workers, customers, and those who
settle disputes honour particular rights and obligations. The deed, daily work,
physical stock, and local support all matter.

Show this in the town: whose banner guards the bridge, which seal hangs at the
mill, where carts queue, which homes are repaired, and whose name is spoken at
the memorial. Keep Thornford's river, road, and settlement shape recognisable
as these uses change.

## Goals for the whole simulation

1. **Every crown has a place.** Record transfers between holders, including
   carriers and lost caches. Check conservation at each transition. Track any
   future minting or destruction with an explicit source or sink. Save/load and
   replay must preserve custody.
2. **People act on what reaches them.** Every consequential account has a
   source and a delivery path. Private belief, public support, and action can
   differ. Information has a cost in time, access, and trust.
3. **Power rests on working relationships.** Changes in rule emerge through
   support, resources, and action. Different towns can recognise different
   claimants. Historical records influence particular readers over time.
4. **The world can endure, change, and recover.** Track hunger, local trade,
   concentration of crowns, succession, and recovery across many seeds. Aim for
   varied paths with opportunities for repair. Use measured outcomes to set
   balance ranges.
5. **The player can make a difference they understand.** Give each test dispute
   several approaches with distinct costs. Players should identify a useful
   clue and explain a consequence. Explanations can include uncertainty.
6. **People and places become worth returning to.** Remember help, humiliation,
   loss, affection, and promises through named relationships and visible town
   changes. Measure this in human play sessions as well as simulation traces.

Goals 1 and the information rules in 2 are correctness requirements. Goals 3–6
need both simulation evidence and playtesting. Early numeric playtest targets
below are design probes to revise after observation.

## Experiment method

Use seed numbers 1–32 from the existing metrics runner. Preserve the complete
world and random state before each treatment. Apply each arm to a fresh copy.
Follow the intervention for 364 days with daily measurements; use ten 364-day
years for historical effects. The current baseline runner uses 365-day reports,
so align dates before comparing it with a weekly experiment.

Report paired outcomes, medians, ranges, and individual failure traces. A
treatment can change later random draws, so compare the cohort and retain the
trace of each world. Log transfers and political acts as they happen; current
shipment slots and retained event counts measure occupied storage, not total
trade or full historical activity.

| Experiment | Controlled change | Measure and decision |
| --- | --- | --- |
| **A court keeps ruling through archive silence** | At a succession, compare a copied account arriving, a 56-day delay, and no surviving written account. Keep starting food, pay, relationships, and armed strength equal. An oral witness arm provides another route for memory. | Track each actor's knowledge, recognition, obedience, and reasons at days 7, 28, 84, and 364. Check that record changes influence rule through readers' actions. A supplied court should have a viable path to continued rule through oral custom and working agreements. Requires the recognition model. |
| **Two histories, one succession** | Give two courts conflicting accounts of a former reign. Keep initial crowns and forces equal. Compare sealed roads, reconnection, and a recovered independent witness. Cross this with a living witness versus a later generation. | Measure belief differences, time to wider acceptance, changes in support, and surviving rival accounts. Every change must cite received evidence or a local action. Test copied-source dependence and the survival of disagreement. Requires claim records and audience memory. |
| **A rich dragon and hungry markets** | At day 364, move 25% of each town's market crowns into the dragon's hoard. Compare an unchanged control, continued hoarding, and return of the same amounts on day 728. | Check exact crown totals. Measure paid deliveries, unaffordable demand, hunger, defaults, and recovery time over three years. This tests whether concentration contributes to the observed hunger pattern. It can start with present balances plus a small intervention runner and flow counters. |
| **One payment, two accounts** | Transfer 40 crowns from a debtor to a lender. Give different readers a repayment receipt and an older debt copy. Compare safe delivery, theft of the receipt, and theft of the coin shipment before receipt. | Separate custody, claimed debt, and attempted collection. A second collection must take actual crowns from someone. Audit every amount and who believed the debt remained due. Requires travelling coin custody and financial claim records. |
| **Who saved Thornford?** | After the same fire, compare local aid from a merchant with aid from a claimant. Use equal physical cargo and travel costs. Independently vary whether each sponsor's role reaches the archive. Include an unaided control and charge all cargo to its source. | At days 7, 28, 84, and 364 compare repairs, hunger, loyalty, remembered credit, and later toll or title decisions. Local witnesses should preserve a path for gratitude when the written account credits someone else. Current repairs provide the material base; explicit attribution and recognition are additions. |

Start with the crown concentration experiment and the archive-silence
experiment. The first tests a measured economic concern. The second defines the
political rule that the user has clarified. Then build the conflicting-history
scenario around that rule.

For the first human prototype, invite 6–8 players to handle the same Thornford
dispute. Present the relevant sources through a miller, a captain, and a scribe.
Allow travel, bargaining, protection, and evidence recovery. Record which clue
each player used, their chosen approach, their explanation of the result, and
whom they want to revisit. A useful first target is that at least three quarters
can explain a result and name someone they care about. Treat this as a small
qualitative study.

## Lessons from other games

These are design lessons drawn from the developers' descriptions, followed by
proposals for Crownless.

- **Crusader Kings III:** its Legends of the Dead design connects promoted
  family history with later dynastic claims and standing. Give a remembered
  reign consequences for heirs, property, and alliances. Make each audience's
  reception part of that process. [Paradox announcement](https://www.paradoxinteractive.com/media/press-releases/press-release/paradox-announces-third-chapter-of-updates-for-crusader-kings-iii).
- **Victoria 3:** its movement and civil-war design links territorial support
  to the people and military forces behind a movement. Let a Crownless court,
  garrison, and town contribute different forms of power. [Developer diary 130](https://forum.paradoxplaza.com/forum/developer-diary/victoria-3-dev-diary-130-political-movement-radicalism-and-civil-wars.1706965/).
- **Suzerain:** its political choices run through named allies, ambitions,
  family ties, and duties. Put the cost of a settlement in a person the player
  will meet again. [Torpor press kit](https://www.torporgames.com/presskit).
- **Six Ages / King of Dragon Pass:** advisers have cultural knowledge,
  personalities, family ties, and their own concerns; past choices can matter
  generations later. Let a companion explain what a document means to their
  people. [Six Ages manual](https://sixages.com/manual/desktop/Manual-StormAge.html)
  and [developer discussion of advice](https://blog.sixages.com/index.php/2021/05/18/advice/).
- **OSR play:** Chris McDowall's Information, Choice, Impact gives a useful
  test: players need enough information to choose, several approaches, and
  results they can connect to their actions. Give conflicting accounts dates,
  sources, and physical clues they can investigate. [ICI Doctrine](https://www.bastionland.com/2018/09/the-ici-doctrine-information-choice.html).

For gacha inspiration, my proposal is to use anticipation, surprise, distinct
characters, and collection as a way to explore this world. A rare find could be
a surviving witness, an unusual pony, a craft tradition, or a letter connecting
two known families. Value can come from a relationship and a specific use.
Chance can determine leads and encounters; world state determines where the
person or object is and what it wants. A run of poor luck can produce a useful
lead through an existing contact. Test discovery through a random rumour against
discovery through a relationship, with equal expected reward and travel cost.
Observe who players remember and what they choose to pursue.

## The human purpose

The design hypothesis is that people need food, safety, belonging, standing,
and an account of their life they can bear to tell. These needs can pull in
different directions. A scribe may protect a useful peace by preserving a
comforting account. A widow may insist on a costly truth because her husband's
name is all she has left. A captain may keep a promise after its reward is gone.

Give characters several attachments and a few firm commitments. Let experience
change their trust. Let kindness, ordinary work, festivals, and patient repair
have lasting effects alongside fear and ambition.

The player's lasting reward can be very small: a miller trusts them with a
daughter's journey; a town keeps the bridge they helped rebuild; someone still
tells their version of what happened.

## Measured starting point

The [32-world baseline](../experiments/simulation-baseline-2026-09-05/README.md)
contains the raw annual data and limits of the measurement. Across the cohort,
mean reported hunger rises from 9.1 in year one to 37.7 in year ten. The dragon's
mean share of crowns rises from 4.4% to 36.3%. At year ten, 21 of 32 worlds have
a town at hunger 100. This motivates the concentration experiment; the baseline
alone establishes an association.

The baseline has been run. The intervention experiments remain designs, ready
for separate implementation and review.
