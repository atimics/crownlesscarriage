# 0009 — Meet one branch at a time; let maps advise

**Status:** Accepted

## Context

Physical route maps removed the all-knowing kingdom screen, but they still made
travel feel like selecting a destination from a menu. The paper changed, but
the action stayed abstract. A carriage follows a track, reaches a side branch,
and either turns or keeps going.

## Decision

Travel starts with the carriage on a track leaving the player's current place.
The presentation orders outgoing routes into a sequence of local junctions.
Each junction set has one continuous cart track and no more than one side
branch. Keeping on the track advances to the next branch. Taking the branch
commits to that route. The set never shows the whole network or turns several
graph edges into one crossroads.

Maps remain physical, limited, tradeable objects. They add old claims about a
road, can reveal the destination of an unmarked track, and reduce wayfinding
delay and risk. They do not create a road and are not required to travel. A
player may take an uncharted branch and accept the uncertainty.

## Consequences

- The main travel choice is local and embodied.
- Carriages stay on visible tracks instead of radiating from a hub.
- A place with many outgoing routes becomes several small encounters.
- The player can leave without first buying permission in the form of a map.
- Maps remain useful because they change knowledge and preparation.
- Hidden roads can be entered as unmarked tracks before their destination is
  known.
- The interface cannot quietly grow back into a complete topology screen.

## Rejected alternatives

- Selecting a carried chart to start travel was rejected because the chart
  remained the real navigation interface.
- Making maps cosmetic collectibles was rejected because knowledge should
  still affect time, risk, trade, and carriage capacity.
- Showing all known roads at once was rejected because discovered topology is
  still a strategic map, even without live data, and it produces implausible
  hub-shaped crossroads.
