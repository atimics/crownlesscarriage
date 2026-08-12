# Crownless Carriage

**Crownless Carriage** is a living-world isometric RPG about operating the
last politically independent carriage line between hostile kingdoms.

The player decides which people, goods, secrets, and contraband are allowed to
move through a simulated region—and then personally handles what happens on the
road, in cities, and beneath them.

The project is in pre-production. This repository contains both its living
design manual and the first executable architecture proof: a deterministic
living-world simulation, a SQLite save, a headless runner, and a raylib
orthographic 3D isometric client. The player occupies the same depth-buffered
world as buildings and inhabitants, walks against solid geometry, enters the
market, trades in person, and approaches the carriage when they need the
physical map case for a journey.

## Play the living-world spine

Requirements: CMake 3.24+, a C17 compiler, Git, and SQLite 3 development
headers. CMake downloads the pinned raylib 6.0 source on first configure.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
open build/crownless_carriage.app       # macOS
./build/crownless_carriage              # Linux
build\\Debug\\crownless_carriage.exe     # Windows multi-config build
```

Controls:

- Left-click any visible ground or platform point to move the biped there.
  The body can cross one 96 x 72 metre exterior linking farms, town streets,
  the carriage yard, dungeon approach, Wayfarer Trials, and Greyward Keep
  without changing local scenes. The isometric camera follows that journey.
  Motion is continuous rather than locked to the displayed paving: the body
  steers at arbitrary angles, collides with solid geometry, climbs reachable
  tagged ledges using fixed hand, wall-foot, and top-foot contacts, turns back
  to climb down lower ledges under hand support, falls under gravity when
  support is actually lost, and lands on physical surfaces. Losing support disengages the gait
  controller and physically collapses the hero before recovery; legs do not
  invent distant footholds beneath a fall. Solid buildings are not climbable.
  The Wayfarer Trials beside town are a physical obstacle course; three
  autonomous biomechanical guards continually climb, cross, descend, and
  choose their next marked contact route there. Press `G` to sound the village
  alarm: training is interrupted and a nearby raid encounter forms around the
  player. Guards and scouts use the same targeting, strike collision, health,
  posture, frontal guard, stagger, knockback, and defeat rules as the player,
  then survivors return to duty. Alarm
  frequency also responds to the simulated region's bandit influence. The
  route crosses a buoyancy trench where the same skeleton releases its ground
  contacts, swims under buoyancy and drag, and reacquires land. Press `J` for
  a controlled physics jump with a live takeoff arc and planted landing; an
  unsupported fall still becomes a ragdoll rather than a canned jump. Press `Space`
  to commit a strike toward a nearby raider and `X` to enter or leave guard;
  left-click movement becomes slower combat-facing strafing while focused.
  Movement, successful jumps, supported climbing, blocks, and landed strikes
  now train three campaign-persistent athletic disciplines: Mobility, Grip, and
  Power. Their levels change physical acceleration, jump impulse, reach,
  traversal timing, impact force, and recovery rather than merely speeding up
  an animation. The character panel shows progress toward each next level, and
  low obstacles become fast vaults once Mobility is developed.
  `F`
  uses a nearby door, notice board, or carriage.
- The humanoid biped is the playable, tuned body plan. Quadruped, hexapod, and
  octopod data remain in automated diagnostic fixtures for later generalization;
  they are not currently exposed as player characters.
- Enter the market and approach its factor; `1`, `2`, `3` buy food, materials,
  or tools, and `Shift` sells.
- Approach the carriage and press F or M to open its physical map case. Each
  carried sheet depicts one route rather than revealing an omniscient kingdom
  screen. Use the arrow keys or click a sheet, B to buy a locally offered
  chart, S to sell one, and Enter to follow an owned chart from the correct
  endpoint. The case carries three maps.
- `.` advances one day; `K` advances one week.
- R repairs a selected contested route from its chart.
- `E` launches an expedition when standing beside the local dungeon entrance.
- `Q` opens the live situation board; `Tab` opens the causal event ledger.
- `F5` saves and `F9` loads SQLite (`Cmd/Ctrl+S` also saves).
- `N` generates the same crisis with a new deterministic seed.

Run the simulation without graphics or execute the verification suite with:

```sh
./build/crownless_sim_runner --seed 42 --years 10 --detail
ctest --test-dir build --output-on-failure
```

Build a Release configuration and run the repeatable headless performance
workloads with:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCC_BUILD_CLIENT=OFF
cmake --build build-release --parallel
./build-release/crownless_benchmark
```

The benchmark reports CPU time per simulated day and per biomechanical-agent
step together with a checksum, so optimizations can be compared without
silently removing work.

The client executable also accepts `--benchmark-render 600` to time a fixed
number of frames. Its benchmark window intentionally remains visible because
desktop operating systems may throttle hidden windows.

Use `--capture-action-reel <frame-prefix>` to run the deterministic heroic
demonstration: approach and top-out, down-climb, run and physics jump, buoyant
swim, guarded weapon contacts, clean impacts, and a final physical knockdown.
The checked-in preview is
[`hero_runtime_action_reel_v03.gif`](assets/previews/hero/actions/hero_runtime_action_reel_v03.gif).

This is still an architecture proof rather than the finished RPG. It now
includes one reusable local settlement grammar, an enterable economy-backed
market, visible animation rigs over the cute prototype characters, and a
physical three-slot map case whose tradeable, persistent charts each describe
one route through an aged and fallible cartographer's projection. Route
encounters, deeper interiors, consequential combat, and dungeon maps remain
the next projection layers; each will submit
validated outcomes to the same core rather than maintaining a second world.

## North star

> Build a causal political RPG in which every important journey originates in
> the world simulation, becomes a character-scale adventure, and leaves visible
> immediate and delayed consequences.

The game joins three kinds of pressure:

- Political economy creates shortages, factions, migration, laws, and human
  conflict.
- Monster ecology creates changing wilderness danger and valuable resources.
- Persistent dungeons create historical and supernatural disruptions that can
  be sealed, exploited, occupied, or transformed.

All three meet on the road through the player's carriage company.

## Start reading

- [Manual contents](docs/README.md)
- [Creative constitution](docs/01-creative-constitution.md)
- [Vertical slice](docs/08-vertical-slice.md)
- [Production roadmap](docs/10-roadmap.md)
- [Blender asset system](docs/production/blender-asset-system.md)
- [Blender hero component system](docs/production/blender-hero-component-system.md)
- [Decision log](docs/decisions/README.md)
- [Current technical architecture](docs/07-technical-architecture.md)

## First proof

The first vertical slice follows one causal crisis:

> A failed harvest creates a food shortage. A contested bridge blocks the
> official convoy. Displaced people join a bandit faction. Miners open an
> ancient tunnel as an alternate route and awaken a subterranean colony.

The player can escort the official convoy, repair or politically reopen the
bridge, smuggle food through the dungeon route, negotiate with the bandits, or
exploit the shortage. Each approach must produce different economic, political,
ecological, and personal consequences.

## Implementation status

The current executable proves:

- A raylib-free deterministic C simulation with typed stable IDs
- Seed-specific settlements, kingdoms, factions, routes, shipments,
  bandits, monster pressure, and a transforming regional dungeon
- Seasonal, input-dependent production and destination-aware multi-leg freight
- Government relief, road maintenance, faction politics, bandit recruitment,
  deep hunts, and a crisis that continues without the player
- Persistent world-generated situations with sponsors, causes, progress,
  deadlines, invalidation, and rewards
- Player trade, cargo capacity, direct-route travel, route repair, and dungeon
  intervention expressed as validated simulation commands
- A default street-level isometric view that projects stock, hunger, security,
  prosperity, situations, smugglers, and dungeon pressure into a walkable town
- A real orthographic 3D world with depth-tested buildings and characters,
  screen-relative movement, building/counter/carriage collisions, and an
  enterable market whose shelves and prices use authoritative settlement state
- Code-native cute 3D inhabitants plus a rig-visible humanoid player shell,
  rendered inside the same depth-tested world pass rather than overlaid afterward
- A camera-independent continuous locomotion agent with exact world targets,
  arbitrary-angle steering, acceleration, solid collisions, vertical velocity,
  gravity, landing contacts, reach-limited contact climbing, and automated
  ascent/drop verification
- Two renderer-independent locomotion libraries: a robotic contact/IK system
  with experimental 2, 4, 6, and 8-limb presets, and a separate generalized
  biomechanical system of bone graphs, rotational joints, anatomical limits,
  passive ligaments, mass, and antagonistic muscle pairs
- A biped controller built on the biomechanical system with heel, flat, toe,
  swing, and airborne phases; live terrain contacts; stance-relative pelvis
  compression; muscle-driven spine and arm response; and visible
  heel–ball–toe rigging
- Biomechanical biped climbing on the same skeleton and hero skin as walking,
  with human-length reach tests, transported knee/elbow planes, progressive
  hand and foot contacts, a supported top-out, and no handoff to the robot shell
- A shared human action layer for locomotion, guard, muscle-driven strike,
  controlled jump, clamber, swim, fall, and recovery. Strikes expose one physical impact window
  that sweeps the hand or held weapon against hostile body capsules and records
  the actual contact point, direction, and relative speed. Blocks additionally
  require the sweep to reach the defender's live hand-to-hand guard segment;
  localized recoil, hits, guard breaks, health, posture, hitstop, stagger,
  knockback, and defeat ragdolls are resolved identically for player and NPC
  combatants;
  swimming removes ground support and applies buoyancy and drag without
  invoking ragdoll
- A five-level physical athletic profile shared by the player controller:
  Mobility grows through travel and jumping, Grip through supported traversal,
  and Power through meaningful combat contacts. Levels expand achievable
  impulse, reach, acceleration, traversal control, and strike effect, while the
  HUD exposes the current heroic tier and progress toward each level. The full
  profile is part of deterministic campaign state and SQLite save/load hashes;
  balance remains a later production pass.
- Force-driven whole-body motion with aggregate bone mass, gravity, damping,
  ground-reaction support, friction-limited propulsion and braking, lateral
  balance, and collision impulses; navigation supplies intent rather than
  assigning velocity
- A generalized particle-and-constraint ragdoll layer that inherits the live
  pose, heel/toe orientation, and momentum when support is lost, preserves bone
  lengths through terrain impact, dissipates impact energy instead of bouncing,
  and keeps the skinned hero live through a brace, kneel, and stand recovery
  before walking control returns
- Swept ledge-top contacts that distinguish landing on a roof from swinging a
  limb beside or beneath it, preventing a tower edge from projecting that limb
  upward; automated falls cover three approach/departure directions on the
  actual climbable street tower
- Low airborne drag with impact-sensitive damping: the 1.65-unit tower drop
  reaches the street in a gravity-consistent timing band, while strong damping
  is reserved for new impacts and lighter damping settles persistent contacts
- The robotic system supports
  up to 16 limbs and four segments per chain, data-driven phase/duty-factor
  gaits, planted contacts, support margins, damage-aware traction, exact
  pole-controlled knees, and iterative arbitrary-chain IK
- A cape-and-pauldron hero silhouette skinned over the diagnostic biomechanical
  rig, with compliant lateral weight transfer, torso counter-rotation, and
  continuous fall/recovery skin and head orientation rather than one-frame
  construction swaps or an authored eight-frame body overlay
- Character-width collision against buildings, props, and visible townspeople,
  with local sidestepping when a direct path meets an occupied space
- Edge-spawned biomechanical travellers whose number follows live shipment
  traffic; they walk completely across town on staggered routes and leave at
  the opposite boundary instead of appearing in place
- A continuous 96 x 72 metre exterior whose roads physically connect fields,
  a full settlement, the carriage yard, dungeon approach, training ground, and
  a gated 25 x 23 metre castle bailey
- A human-scale local-world convention: one world unit is one metre, the player
  is about 1.9 metres tall, doors are 2.15 metres, ordinary buildings are
  5.8–8.8 metres, and castle structures rise to 12.5 metres; traversal geometry
  uses the same units and collision volumes as its rendering
- A carriage-gated physical cartography system: maps are capacity-limited,
  tradeable, persistent objects with one depicted route, provenance, age,
  accuracy, legality, recorded conditions, and no omniscient world view
- A bounded causal event ledger explaining what changed and why
- Normalized, transactional SQLite persistence with exact state-hash round trips
- Deterministic, persistence, and crisis-scenario automated tests

## Design status

The documentation distinguishes between:

- **Contract:** a rule the game must obey.
- **Target:** an intended design subject to playtesting.
- **Hypothesis:** an assumption the vertical slice must prove.
- **Deferred:** deliberately excluded from the first serious prototype.

The manual is authoritative only where it explicitly declares a contract.
Everything else remains revisable as evidence arrives.

## Explicit non-goals for the first slice

- A miniature clone of a grand-strategy game
- Individual simulation of every household or citizen
- A seamless, permanently loaded continent (the current continuous exterior is
  one regional place, while route travel still preserves physical cartography)
- Fully dynamic borders, dynasties, religion, or culture
- Random encounters whose only purpose is to interrupt travel
- Runtime-generated prose
- Unique interiors for every building
- More combat polish before the causal loop is proven
