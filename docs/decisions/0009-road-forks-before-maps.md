# 0009 — Choose roads at forks; let maps advise

**Status:** Accepted

## Context

Physical route maps removed the all-knowing kingdom screen, but they still made
travel feel like selecting a destination from a menu. The paper changed, but
the action stayed abstract. A traveller does not move through a map. A
traveller reaches a fork and takes a road.

## Decision

Travel starts from the roads that leave the player's current place. The choice
shows only that immediate fork. It never shows the whole network.

Maps remain physical, limited, tradeable objects. They add old claims about a
road, can reveal the destination of an unmarked track, and reduce wayfinding
delay and risk. They do not create a road and are not required to travel. A
player may take an uncharted fork and accept the uncertainty.

## Consequences

- The main travel choice is local and embodied.
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
  still a strategic map, even without live data.
