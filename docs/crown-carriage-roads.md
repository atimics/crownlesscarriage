# Crown carriage roads

Roads belong to the crowns whose territory they cross. When a road fails, it
is the crown's own carriage that mends it — not a road steward, not a repair
crew conjured from nowhere. This document is the design contract for that
system. It replaces the road-steward concept in the simulation: the steward
agent-sweep tool is removed, and route recovery becomes crown business,
visible in the world.

## Claims and dispatch

A route's holders are the kingdoms of its two endpoint settlements. When a
route closes (decay, fire, service collapse, or a sealed mine road), the
holders' crowns are responsible:

- **One crown, both ends.** That crown's royal carriage takes the repair.
- **Two crowns, at peace.** Each crown sends its own carriage. The road is
  mended from both ends.
- **Two crowns, at war.** Each crown still sends a carriage, but the
  carriages travel with paid escorts drawn from the treasury. See
  *Contested roads* below.

Dispatch is opportunistic and subordinate to the markets: trade planning runs
first each day, and only a carriage left idle by planning takes road work.
The realm does not starve its logistics to fix a road. One carriage per crown
means one road at a time per crown; the scan takes the lowest route id.

The crown is a fallback, not a rival to its own towns. A carriage rides only
when the roadside recovery plan for that road is blocked — no supplies or no
hands for the council's public work — and only when the work camp can feed
the repair: two of each material spent, two left for the settlement's own
road maintenance. A camp that cannot supply its work for a month is
abandoned; the carriage returns to the realm's service instead of rusting at
a closed road. A crown whose treasury cannot fund a war escort (soldiers cost
30 crowns to hire, paid into the camp's market) lets a contested road lie
until it can.

The carriage travels to its own end of the broken road through open, usable
roads. If its endpoint settlement is abandoned and the other end belongs
to an enemy, the crown cannot reach the work and the road is left to rot.

## The work is physical

Repair keeps the physical rule from #646: two Tools, two Wood and two Stone
become a way through, worked by a wainwright. The crown's carriage brings
its own materials:

- **Materials** are drawn from the work endpoint settlement's stock at
  completion. If the stores are empty, the carriage camps and waits a week
  for supplies before trying again.
- **Wainwright.** If a living cartwright exists in any settlement of either
  holding crown, the carriage brings one along: 21 days of work. If no
  cartwright serves the crown, unskilled work takes 35 days.
- **Joint work.** When both crowns' carriages work the same road at peace,
  the second arrival halves the remaining time. The road carries both
  crowns' seals when it reopens, and both gain a little legitimacy.

On completion the road reopens at condition 85 with better security, a
`ROUTE_REPAIRED` event is recorded with the crown's name, and any open
route-repair charters on that road resolve.

## Contested roads

When two crowns at war each send an escorted carriage to the same closed
road, the carriages meet at the work site. A skirmish resolves it, once:

- **Escort strength** is read from the crown's books at the moment of the
  skirmish: treasury divided by 40, war chest divided by 25, and legitimacy
  divided by 4.
- **The stronger escort holds the road** and finishes the repair alone, under
  its own flag. The loser's carriage limps home with damage and a week's
  rest; the road loses security from the fighting either way.
- **A stand-off** (equal strength) sees both carriages withdraw. The road
  stays closed and both crowns pay their soldiers for nothing.
- Both sides pay roughly 20 crowns of war costs when steel is drawn, win or
  lose.

The war-party checkpoint rule still applies: a company holding the road
under a control-route order blocks crown traffic, escort or not.

## In the world

Crown carriages on repair work are part of the visible world, the same way
the named ponies are road encounters rather than map furniture:

- A carriage working a road sits on that route with its mode, holder crown,
  and remaining days readable from the world state.
- Travelers crossing the road during the work pass the crown's work camp
  and hear about it; the dispatch, skirmish, and completion events carry the
  crown's name into the record and the gossip pool.
- The visual client should draw the carriage camp on an in-repair route, as
  it draws road sites. See the roadmap issues for the client work.

## What this removes

The road steward — the automated player-company policy that the agent sweep
measured — is retired. The `crownless_agent_sweep` tool, its tests, and its
CI smoke tests are deleted. The historical experiment records that used the
steward stay in `docs/experiments/`; they are records, not commitments.
Route-repair charters offered to the player company remain available: a
crown with no carriage to spare can still hire a traveling company, and the
player's physical repair command is unchanged.

## Measured effect

Healthy worlds are unchanged by the duty: at ten years, closed-route counts
match worlds without it, because local labor still mends what it can and the
crown rides rarely. In declining worlds the carriage cannot stop the
famine-driven endgame — every hundred-year world still ends with its roads
closed, because camps with empty stores cannot feed any repair — but the
mid-run roads it does mend leave the world with more people: across seeds
1-8, year-100 population rose in five of eight worlds (seed 2: 2,486 to
13,610; seed 3: 10,098 to 14,884; mean 9,842 to 12,573) with no world
worse. The duty costs almost nothing when it is useless: a seed-1 decade
spent 24 carriage-days on road work after the fallback guards.

## Rollout

1. Schema 100 introduces dispatch, work, joint repair, and skirmish in the
   simulation core. No saved fields are added: repair missions reuse the
   carriage's `target_id`, `arrival_day`, and `blocked_since_day`, and two
   new carriage modes and two new events are appended, so old saves load and
   upgrade without data migration.
2. The road-steward sweep tool and its CI wiring are removed.
3. Client/visual exposure: carriage camps on in-repair routes, encounter
   flavor when crossing them. Tracked as follow-up issues.
