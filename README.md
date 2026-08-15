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

For ordinary playtesting, use the optimized build with debug symbols. This is
the authoritative visual-review configuration:

```sh
cmake --preset play
cmake --build --preset play
open out/build/play/crownless_carriage.app       # macOS
./out/build/play/crownless_carriage              # Linux
```

Use `cmake --preset development` only for assertion-heavy diagnostics. The
unoptimized development client is not representative of gameplay frame time.

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
  unsupported fall still becomes a ragdoll rather than a canned jump. During a
  raid, click a raider to target them: the hero closes distance, holds
  a sword-length standoff, maintains facing, and exchanges physical attacks
  automatically with an equipped blade and readable guard/cut/recovery poses.
  `1` queues Crushing Blow, `2` queues the posture-breaking Sunder, and `3`
  uses Second Wind; Crushing Blow has a committed two-arm overhead motion,
  while Sunder uses a wide, violet-traced sweep. Nearby defenders peel toward
  other raiders so the selected fight remains readable. Second Wind can
  recover health and posture; the combat bar shows every cooldown. `Space`
  remains a manual basic strike, `X` enters or leaves guard, and clicking the
  ground disengages the target and resumes direct movement.
  Movement, successful jumps, supported climbing, blocks, and landed strikes
  now train three session-level athletic disciplines: Mobility, Grip, and
  Power. Their levels change physical acceleration, jump impulse, reach,
  traversal timing, impact force, and recovery rather than merely speeding up
  an animation. Low obstacles become fast vaults once Mobility is developed.
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
  endpoint. The case carries three maps. The first journey made under an
  accepted charter now departs into a real-time carriage-following road view.
  Travel advances the deterministic world clock at thirty game-minutes per
  real second; every midnight runs the normal living-world update. A
  route-owned crisis interrupts that same journey in progress: `1`
  dismounts into a dedicated road scene and only clears the route after its
  attackers are defeated; `2` enters an unarmed parley in the same physical
  space. Walk to the toll collector and press `F` to buy passage, moving
  traffic immediately while strengthening the collectors who control it. The
  carriage then resumes from its saved route progress and arrives only when
  the full duration has elapsed.
- `.` advances one day; `K` advances one week while the carriage is parked.
- R repairs a selected contested route from its chart.
- `E` launches an expedition when standing beside the local dungeon entrance.
- `F3` toggles the live frame, skin-upload, and character-LOD diagnostic overlay.
- `Q` opens the live situation board. Use the arrow keys to inspect a dated
  charter, `Enter` to accept one explicit company promise, and `Backspace` to
  withdraw while the need remains. Only accepted work earns its sponsor's
  reward, and only a promise the company actually made can damage reputation
  when its deadline is missed. `Tab` opens the causal event ledger.
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
cmake --preset release
cmake --build --preset release
./out/build/release/crownless_benchmark
ctest --preset release
```

The benchmark reports CPU time per simulated day and per biomechanical-agent
step together with a checksum, so optimizations can be compared without
silently removing work.

The client executable also accepts `--benchmark-render 600 90` to time a fixed
number of frames and fail when the current 90 FPS production floor is missed.
Its benchmark window intentionally remains visible because desktop operating
systems may throttle hidden windows. `run_render_benchmark` exposes the same
gate as a build target on machines with a desktop session. The gate also checks
that the scene draws one consolidated high-detail player, uses low-detail NPCs,
and stays within the 32-primitive animated-skin budget.

Use `--capture-action-reel <frame-prefix>` to run the deterministic heroic
demonstration: approach and top-out, down-climb, run and physics jump, buoyant
swim, guarded weapon contacts, clean impacts, and a final physical knockdown.
The checked-in preview is
[`hero_runtime_action_reel_v03.gif`](assets/previews/hero/actions/hero_runtime_action_reel_v03.gif).

Use `--capture-golden <frame.png>` for the fixed street-level art-direction
review: the authored carriage and market, action-figure hero, role-shaped
population, simulation dressing, shared palette, and final color treatment are
framed together under deterministic world conditions.

Use `--capture-travel <frame.png>` for the deterministic real-time travel
proof at twenty percent route progress. The capture frames the moving carriage
with its authoritative clock, progress, speed, condition, and time scale.

This is still an architecture proof rather than the finished RPG. It now
includes one reusable local settlement grammar, an enterable economy-backed
market, an authored action-figure hero, a role-shaped procedural population, and a
physical three-slot map case whose tradeable, persistent charts each describe
one route through an aged and fallible cartographer's projection. Accepted
promises now cross a persistent road encounter that can be fought or bargained
through, with both outcomes submitted to the simulation and projected into
route security, bandit strength, settlement population and markets, and
follow-on caravan traffic. Deeper interiors and dungeon maps remain future
projection layers rather than separate world states.

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
  deadlines, invalidation, and rewards, plus one authoritative player-accepted
  charter that persists through save/load and exposes its next physical action.
  Named sponsors recur on the board, while the named affected person appears
  beside the local notice and follows the same crowd-spacing rules as other
  inhabitants
- Persistent route encounters with live-combat and negotiated resolutions;
  their different effects alter markets, security, population, bandit power,
  route capacity, and visible shipment traffic. Fulfilled promises schedule a
  named delayed echo that is delivered only when the company later returns
- A route-conditioned encounter space shared by direct movement and the combat
  controller: the carriage and horses, stranded travelers, road ruts, trees,
  damage, patrol markers, bridge conditions, crates, and blockade are projected
  from authoritative route state. Road-specific collision removes invisible
  town geometry and gives the carriage and obstacle props physical footprints
- Physical destination aftermath: defended roads leave guard musters, reclaimed
  barricade timber, returning residents, and protected traffic; paid passage
  leaves collectors' colors and toll infrastructure in the street
- Player trade, cargo capacity, direct-route travel, route repair, and dungeon
  intervention expressed as validated simulation commands
- A default street-level isometric view that projects stock, hunger, security,
  prosperity, situations, smugglers, and dungeon pressure into a walkable town
- A real orthographic 3D world with depth-tested buildings and characters,
  screen-relative movement, building/counter/carriage collisions, and an
  enterable market whose shelves and prices use authoritative settlement state.
  Roads, shoulders, curbs, plaza stones, fields, and crop rows occupy explicit
  ordered terrain layers, avoiding coplanar depth flicker at intersections
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
  world-anchored and staggered hand/foot contacts, a supported top-out and
  controlled descent, and no handoff to the robot shell
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
  HUD exposes the current heroic tier. Persistence and balance remain a later
  production pass.
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
