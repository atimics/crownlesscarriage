# 0015 — Make the carriage journey the open world

**Status:** Accepted

## Context

The first finite-world pass put every town inside one square terrain mesh.
That made the strategic map consistent, but it weakened the authored town
composition and made the carriage feel like a marker moving over a generic
landscape.

The game is named for the Crownless carriage. Travel must be a main play
space with preparation, incomplete knowledge, road choices, changing
conditions, and consequences for horses, wagon, cargo, time, and promises.
It cannot be a menu between towns or an automatic scenic cutscene.

Research in `signal` showed that routes, hauling, infrastructure, and gaps in
safe coverage should be physical parts of play. Research in `cosyworld`
showed that route existence, route knowledge, scouting, travel, and return
anchors are different states. Its useful rhythm is sanctuary, sign, venture,
challenge, discovery, and return.

The Cairn wilderness procedure divides travel into meaningful watches. Each
watch joins a player action to weather, signs, environment, resource use, or
an encounter. Old-School Essentials makes maps partial player knowledge,
changes speed by road and terrain, and treats reaction, parley, evasion, and
retreat as normal encounter outcomes. Pointcrawl design makes the path
between known places the real exploration structure.

Sources:

- https://cairnrpg.com/second-edition/players-guide/procedures/
- https://cairnrpg.com/second-edition/players-guide/overview-and-principles/
- https://cairnrpg.com/second-edition/players-guide/core-rules/
- https://cairnrpg.com/second-edition/players-guide/marketplace/
- https://cairnrpg.com/second-edition/wardens-guide/setting-seeds/
- https://cairnrpg.com/second-edition/wardens-guide/wilderness-exploration/
- https://cairnrpg.com/second-edition/wardens-guide/pointcrawls/
- https://osesrd.opengamingnetwork.com/adventuring/
- https://www.bastionland.com/2018/09/the-ici-doctrine-information-choice.html
- https://thealexandrian.net/wordpress/48666/roleplaying-games/pointcrawls

## Decision

Authored towns remain separate storybook stages. Walking, conversations,
markets, fights, and local research use the existing town profiles, camera
shots, landmarks, and arrival compositions.

The open world exists only in carriage mode. Leaving a town follows the
carriage through the authored yard and gate, then raises the camera into the
road book. Arrival reverses the move: the road book closes on the destination
and the existing town-arrival camera carries the carriage through its gate.

The road book has no square ground plane. It draws finite generated corridors
along strategic routes. A corridor contains the road, verge, trees, landmarks,
and room for road events. The generated route pose remains the authority for
the visible carriage position and heading.

Fog represents missing knowledge, not merely distance. A carried or archived
chart reveals its route and both anchors. At an uncharted junction, only the
first visible stretch is shown. During uncharted travel, the revealed return
chain grows behind the carriage and fog stays ahead. The camera frames known
roads and expands as the player's knowledge expands.

Important events leave the wide road-book view and use close authored road
scenes. The carriage remains the physical centre of the event. After scouting,
parley, evasion, repair, camp, or combat, travel returns to the same route and
progress.

The complete carriage loop is:

1. Prepare in town: choose a promise and route, load cargo and fodder, inspect
   charts, horses, wagon condition, weather, and known danger.
2. Depart: board the carriage and drive through the authored town gate.
3. Take a road watch: choose careful, steady, or push pace; later watches add
   scout, repair, forage, camp, and press-on choices.
4. Read a sign: show evidence before danger, including tracks, smoke, weather,
   traffic, road damage, and faction control.
5. Resolve a road event: quiet progress, environment, social encounter,
   hazard, discovery, or attack. Reaction, parley, evasion, and retreat remain
   valid.
6. Stop at an anchor: inn, bridge, toll, cairn, camp, junction, or discovered
   site. An anchor gives a safe return chain and can change later journeys.
7. Arrive and settle: enter the authored destination, unload, fulfil promises,
   update road knowledge, and let consequences enter world memory.

The road uses this daily procedure:

1. The opening journey is one eight-hour watch. It teaches the road book and
   reaches Gloamgate in half a day.
2. Later journeys contain at least three travel watches. This guarantees one
   night stop, even on a short road.
   A journey chosen after midday is prepared overnight and leaves at first
   light, which keeps the watch names tied to the world clock.
3. The morning watch ends at a midday halt. Watering the team and reading the
   signs lowers risk. Pressing on adds fatigue and danger.
4. The afternoon watch ends at an overnight halt. Camp uses the reserved
   fodder and raises road risk. A road house gives stronger rest, stable feed,
   and a small wagon repair for a visible price.
5. Every route has a named road house. Its mile mark, place in the route, and
   price are stable for that world. Longer roads may place it at a later night
   stop.
6. Travel strain is paid when a watch ends. This keeps the pace choice for the
   whole watch authoritative. An eight-hour night stop advances the world
   clock and all deadline clocks.

The interface shows the current day, time, watch number, next stop, team
readiness, wagon condition, and arrival estimate. Route choice shows the full
watch plan and the known road house before departure. These details turn time
and supplies into planning information.

The procedure follows these OSR play principles:

- Information comes before choice. The road book shows distance, danger,
  lodging, price, fodder, team readiness, and wagon condition before the
  player commits. Road signs warn of many threats before they become attacks.
- Choices create clear trade-offs. A break exchanges momentum for recovery
  and safer reading of the road. Pressing on protects momentum while adding
  fatigue and danger. Camp saves crowns while accepting exposure. Lodging
  spends crowns for safety, feed, and repair.
- Time is a material cost. Travel, overnight rest, promise deadlines, markets,
  faction plans, horse needs, and world events share one clock. The world can
  change while the company rests.
- Logistics create play. Fodder, money, cargo weight, horse traits, road
  condition, pace, and wagon wear shape the journey. These values stay visible
  enough for the player to make plans.
- Encounters allow several answers. Warnings, reaction, parley, bargaining,
  evasion, fighting, and retreat keep danger open to judgement.
- The world remembers action. Route knowledge, road events, spent supplies,
  animal strain, wagon damage, faction changes, and missed deadlines persist
  after arrival.
- The procedure stays easy to run. One travel choice and one event belong to
  each watch. The system handles the small measures while the player makes
  the decisions that change risk and resources.

## Consequences

- Town art keeps the King's Quest-like authored framing.
- The kingdom becomes legible through journeys rather than being exposed at
  the start.
- Map ownership, scouting, and travel history have visible value.
- A generated route can support many reusable storybook road compositions
  without requiring a handcrafted scene for every metre.
- Terrain streaming remains bounded derived data, but it supports road
  corridors and close road events rather than replacing authored towns.
- Future exploration can extend into fog from road anchors without changing
  the town contract.

## Rejected alternatives

- One square walkable terrain containing miniature versions of every town
- A static kingdom map used only to choose destinations
- Random travel events with no warning, location, or persistent consequence
- Showing every route and destination before the player learns them
- Replacing authored towns with procedural building layouts
