# Creative Constitution

## The game in one sentence

**Crownless Carriage** is a causal political action-RPG in which the player
operates an independent carriage line and decides which people, goods, and
information move between kingdoms threatened by human conflict, monsters, and
persistent dungeons.

## Player fantasy

The player possesses an old cross-border charter recognized—sometimes
grudgingly—by several crowns. It grants passage but not allegiance. This makes
the player useful to rulers, guilds, rebels, smugglers, refugees, scholars, and
hunters without making them a sovereign.

The player is not a universal errand-runner. They are the operator of a scarce
connection. Their power comes from deciding:

- Who receives a seat
- What receives cargo space
- Which message arrives in time
- Which border law is obeyed or broken
- Which route and danger are accepted
- Who bears the cost of leaving something behind

The carriage is simultaneously transportation, home, party camp, inventory
limit, progression system, and visible record of the player's values.

## Design pillars

### 1. Causality before randomness

Important encounters originate in traceable world conditions. Randomness may
select details or introduce uncertainty, but it may not be the sole explanation
for a major crisis.

### 2. People and places before statistics

No major state is surfaced without:

- A recurring person affected by it
- A recognizable place expressing it
- A visible symptom the player can witness

The detailed ledger is optional evidence, never the primary storytelling
interface.

The default view is character-scale and local. Travel begins on a cart track.
At a junction the carriage can see every road that physically leaves it, but
nothing beyond the nearby terrain. Physical route notes are secondary evidence
kept in the carriage. They can warn, mislead, or save time, but they never
replace the act of taking a branch in front of the player.

### 3. Every journey is a commitment

A departure consumes time and limited capacity. Passengers, cargo, guards,
medical equipment, expedition supplies, and contraband compete for space.
The player cannot solve every problem in one trip.

### 4. Adventure changes the world

Combat, negotiation, exploration, and dungeon outcomes must write meaningful
results back to the strategic simulation. Clearing a bandit camp, relocating a
monster, or reopening a mine changes routes, factions, production, safety, and
future situations.

### 5. Immediate consequences and delayed echoes

Every important decision produces:

- At least one immediate visible consequence
- At least one delayed consequence delivered through a return visit, letter,
  passenger, changed route, or recurring character

### 6. Persistence over disposable content

Settlements, routes, camps, named characters, and dungeons retain meaningful
changes. Completed locations may be occupied, exploited, restored, abandoned,
or transformed rather than disappearing from the game.

## Definition of a living world

A world is living when the player can:

1. Encounter a visible condition.
2. Discover a concise explanation for why it exists.
3. Identify people who benefit and suffer.
4. Change one of its causes through play.
5. Later observe a materially different outcome.

Activity alone is insufficient. Moving prices, caravans, and borders that the
player cannot understand or influence create a busy world, not a living one.

## Causal legibility contract

- A player-facing explanation should contain no more than three causal links.
- The engine may retain deeper provenance for debugging and investigation.
- Every surfaced claim must be supported by the causal event ledger.
- The game may simplify emphasis but may not knowingly attribute a crisis to an
  unrelated cause.
- Every surfaced crisis needs at least two interested actors with incompatible
  goals and three materially different interventions.
- A single player action has a bounded effect. One wagon cannot feed an entire
  kingdom unless the situation establishes why that wagon is pivotal.

## Presentation hierarchy contract

- The game opens and returns from journeys at character scale.
- Local movement, people, buildings, interiors, and physical symptoms are the
  primary interface to the world.
- The current roadside junction answers where the player can turn now. Every
  branch there is visible at once, while later roads remain out of sight. A
  carried route chart only adds what its maker claimed at survey time. No live
  interface answers for the entire world.
- Travel may pull the camera back only to a view plausible from the carriage.
  A whole-world view exists only on a hand-drawn map object.
- Settlements, dungeons, goblin caves, and dragon caves are separate local
  maps joined by carriage roads, not rooms placed together on one town stage.
- Strategic facts that matter to the player must project into at least one
  local sensory or social expression before requiring a statistics panel.
- Choosing a destination is only the beginning of an adventure. Arrival,
  interruption, negotiation, fighting, trade, and consequence occur in local
  scenes.

## Time contract

- The strategic world does not advance during menus, dialogue, ordinary local
  exploration, or real-time combat.
- Time advances through explicit commitments: travel, rest, recovery,
  construction, and long contracts.
- Every commitment previews its duration and known deadlines.
- Routine travel may be accelerated or skipped once a route is stable.
- Slow processes may resolve weekly, but all systems share one authoritative
  daily calendar.

## Attention contract

- No region presents more than three active crises demanding player attention.
- The situation engine selects the most playable and consequential conditions;
  lesser events remain background texture.
- Resolved situations receive recovery time before producing a closely similar
  crisis.
- Routine route interruptions are suppressed unless they introduce a changed
  context or a meaningful decision.

## Scope test for new features

A proposed feature enters production only if it answers all of the following:

1. Which player decision does it create or constrain?
2. Which existing system does it connect to?
3. How is it expressed locally without opening a ledger?
4. What persistent consequence can it produce?
5. How will automated or player testing prove its value?

If those answers are weak, the feature is lore generation rather than game
design and remains deferred.
