# Technical Architecture

## Accepted technology

- **Language:** portable C17 for simulation and client code
- **Client:** raylib 6.0, pinned by the build
- **Persistence:** embedded SQLite with normalized authoritative tables and
  transactional snapshots
- **Build:** CMake and CTest
- **Networking:** none in the first proof; commands and deterministic snapshots
  form the future server boundary

This keeps the simulation small and inspectable without requiring the renderer,
save format, or eventual network protocol to share memory layouts. Native C
structs are not serialized as blobs and are not wire formats.

## Architecture principle

The strategic simulation is a pure C library with no dependency on raylib,
rendering frame time, input state, or active local maps. The existing isometric
prototype can become a presentation and character-play client after the core
contracts are established.

```text
Player input
    |
    v
Presentation / local world
    |
    | validated commands
    v
Deterministic simulation core
    |
    | snapshots + causal events
    v
Projection systems and UI
```

## Stable identity

Array positions are not persistent identities. Use typed stable IDs for:

- Province
- Settlement
- Kingdom
- Faction
- Character
- Route
- Shipment
- Bandit group
- Monster population
- Dungeon
- Situation
- Causal event

Dense arrays may store entities internally, but references use IDs resolved
through validated lookup tables. Deletion uses tombstones or generations so a
stale ID cannot silently refer to a different entity.

## Simulation core responsibilities

- World records and stable identity
- Authoritative integer calendar
- Fixed daily and weekly pipeline
- Production and accounting
- Shipments and route resolution
- Government and faction decisions
- Bandit and monster strategic state
- Dungeon regional states
- Causal event emission
- Situation detection
- Command validation
- Save/load and state hashing

## Presentation responsibilities

- Physical map-case visualization of carried or locally offered route charts
- Active city or route segment
- Local navigation, animation, and combat
- Instantiating strategic entities near the player
- Projecting strategic conditions into visuals and behaviour
- Collecting local outcomes into commands
- Communicating forecasts and causal explanations

Presentation may interpolate carriage and shipment icons, but interpolation is
never authoritative.

## Local navigation boundary

The local character controller operates in world space, not in screen or
isometric projection space. A screen click raycasts the actual ground and
platform geometry and produces an exact floating-point world target. No paving
cell, projected diamond, or pre-authored isometric arc participates in motion.
After target selection, the locomotion agent owns:

- Continuous world-space position, velocity, acceleration, and facing
- Static solid footprints shared with rendered geometry
- Character-width capsules for the player and visible local inhabitants, with
  deterministic sidestepping around occupied space
- Surface heights, ledge contacts, staged climbing, gravity, and landing
- Walk, climb, and drop states derived from physical motion
- Procedural limb contacts and rig state consumed directly by the renderer

The orthographic camera is therefore replaceable. Rotating it, changing its
projection, or presenting the same local state through another renderer must
not alter locomotion results. The renderer reads agent state; it does not infer
movement from projected screen coordinates.

## Two generalized locomotion families

The prototype deliberately keeps two independent, renderer-free C systems.
They share world-space terrain probes and contact data, but they do not pretend
that a machine and an animal produce motion in the same way.

### Robotic contact locomotion

The limb solver is a standalone C library with no dependency on raylib, the
town renderer, or the world simulation. It describes rigid chains, target
contacts, support geometry, and available drive. A morphology supplies:

- Any limb count up to the current fixed capacity of 16
- Up to four rigid segments per limb, with independent lengths
- Body-local socket, neutral contact, and bend-bias vectors
- Per-limb phase offsets
- Global duty factor, swing budget, minimum support count, stride threshold,
  velocity lead, foot clearance, and swing duration
- A declaration that the morphology requires dynamic rather than static balance

Quadruped, hexapod, and octopod fixtures exercise this system alongside the
two-chain climbing rig. It remains the appropriate baseline for the Crownless
Carriage and other constructed bodies: their actuators, failure, support hull,
and contact schedule are explicit. The playable human does not hand off to this
solver: its walking, climbing, falling, and recovery modes now remain on the
separate biomechanical skeleton. This preserves the distinction between a
servo-driven constructed morphology and a bone-, ligament-, and muscle-driven
body.

Each update follows a shared pipeline:

1. Transform every socket and neutral contact from morphology space to world
   space, adding a short velocity lead to anticipate motion.
2. Probe along gravity for a surface point and normal, then low-pass filter the
   contact data.
3. Keep stance contacts fixed in world space to prevent foot sliding.
4. Use gait phase and duty factor to identify eligible swing limbs, but allow an
   overstretched limb to request an emergency step.
5. Refuse a lift that violates the morphology's remaining-support budget. For
   three or more contacts, calculate the signed distance from the projected body
   center to the convex support hull.
6. Move accepted contacts along a smoothed arc. Solve two-bone legs exactly
   around a stable pole direction; use iterative forward/backward reaching for
   experimental chains with three or more segments.
7. Feed support margin, support center, planted count, and healthy-limb ratio
   back into body acceleration and available traction.

### Biomechanical locomotion

The second library represents an anatomical morphology as a bone graph plus
rotational joints. Every joint declares inertia, a neutral angle, anatomical
limits, viscous damping, passive tissue stiffness, and a nonlinear ligament
response near its limits. Any joint can have arbitrary muscle entries. The
current biped uses antagonistic flexor/extensor pairs with finite activation and
relaxation time, moment arms, maximum force, and a force-length curve. A local
stretch/velocity reflex converts the controller's intended angle into muscle
excitation; the joint is never assigned an unconstrained servo velocity.

The same rig now owns a force-integrated whole-body root. Bone masses aggregate
into body mass; gravity, damping, accumulated external forces, acceleration,
velocity, and world position are integrated in ordinary SI-like units. A world
collision constrains that state by producing an impulse instead of silently
replacing the requested motion. This root is still an aggregate body rather
than a sum of independently simulated rigid bones.

The humanoid gait controller is the first consumer, not part of the generic
tissue layer. It schedules heel strike, flat support, toe-off, swing, and
airborne contacts for each leg; plans a landing from the remaining swing time
plus half of the coming stance; and may revise that plan while accelerating.
Feet remain planted in world space. The pelvis compresses down when necessary
to preserve thigh and shin length instead of allowing the torso to outrun its
support, while an unplanted leg may fold within reach. Pelvis weight transfer is
coupled to the stance phase. A planted foot generates a spring-damped vertical
ground reaction that counters gravity, plus a friction-limited horizontal force
that accelerates or brakes the body toward navigation intent. A bounded lateral
reaction pulls the body toward the current support side. Cadence therefore
follows measured body speed; navigation no longer assigns root velocity. Spine
counter-rotation and arm swing pass through
the muscle/ligament dynamics, so they lag and settle as compliant tissue rather
than following a sine value as a servo command. Below the locomotion threshold,
the cyclic drive is removed and shoulder/elbow damping lets the arms return to a
quiet resting pose.

Loss of support is a controller boundary, not another gait phase. The walking
planner immediately releases its planted contacts and maps the current and
previous visible poses into a generalized Verlet particle graph. Each anatomical
landmark has mass and collision radius; fixed-distance constraints join pelvis,
spine, chest, head, arms, and legs, while cross-braces keep shoulder and hip
widths coherent. The inherited two-frame displacement carries real approach
momentum into the fall. Gravity then moves every landmark, terrain projections
resolve body contacts, and iterative constraints prevent the legs or arms from
lengthening toward a distant surface. Navigation intent cannot schedule a step
while this graph is active. Contact resolution removes inward velocity and
solver-created outward center-of-mass velocity, retains only a very small
restitution component, and damps tangential/internal motion according to the
number of simultaneous contacts. Positional correction therefore cannot become
a conspicuous whole-body bounce on the next frame. Elevated top surfaces use a
swept contact test: a particle may land on or remain supported by a top only if
its previous lower extent was at that elevation or above it. A hand or ankle
that approaches the same footprint from beside or below cannot be projected to
the roof. Recovery contact counts are also filtered to the agent's actual
landing elevation, so limbs left draped on a higher ledge cannot prematurely
start the get-up sequence. Once the agent's physical base has committed below
an elevated ledge, stale contacts above it are released; a trailing foot can no
longer suspend or slow the whole falling graph.

Airborne and contact damping are separate regimes. Free-fall Verlet velocity
retains 99.6 percent per frame, avoiding the slow-motion terminal speed caused
by the earlier 3.5-percent per-frame loss. A newly colliding landmark applies a
larger one-frame whole-body energy loss; a landmark that remains supported uses
only light resting damping plus local tangential friction. This preserves normal
gravity timing off the tower without allowing the post-impact graph to chatter.

Recovery is permitted only after multiple body contacts and whole-body mean
speed indicate a settled fall, with a bounded timeout for residual solver
jitter. It does not replace the graph with a standing pose. The live ragdoll
first blends muscle-like landmark targets from its arbitrary fallen pose toward
a hands-and-knees brace, then a kneel, then an upright target. These motors
include gravity compensation, velocity damping, acceleration limits, terrain
contacts, and the same fixed bone constraints as the fall. Walking receives
control only after the final stage has run and every landmark is within a small
world-space tolerance of the standing rig and the graph's mean speed is low.
Heel and toe are live, constrained ragdoll particles rather than points rebuilt
from the old facing yaw, and their lightweight contact markers preserve foot
pitch without adding fictional distal ballast. The standing and fallen torso
skins cross-fade while the same underlying joints continue moving; hair, face,
and crown use a head-relative frame in every state. Together these changes
prevent one-frame foot, torso, and upright snaps without damping the fall into
slow motion.

Terrain contacts are hard constraints. After the analytic two-bone contact
solve, its actual hip, knee, and ankle angles are observed back into the
biomechanical state and produce a bounded reaction torque. This is a deliberate
hybrid: it keeps exact non-sliding feet and fixed bone lengths while making the
pose controller tissue-aware. A later rigid-body layer can use the same muscle,
joint-limit, mass, and reaction state without changing the morphology format.
The player skin retains the cape-and-pauldron silhouette, while the diagnostic
overlay now exposes heel, ball, toe, joint, and contact state.

Traversal does not convert a ledge into a ballistic jump. Only explicitly tagged
traversal geometry can start a climb; houses remain solid at every height. The
controller finds the nearest face and two world-space hand contacts, rejects the
attempt if either anatomical arm cannot reach, and validates a body-sized
landing footprint. A grounded flag alone is insufficient: a biped still in
ragdoll or recovery cannot acquire a climb, preventing a half-fallen pose from
snapping into supported traversal.
The hands remain fixed while a damped body motor pulls against them. The wall
plane projects out penetration. The same 46.5 cm thighs, 47.5 cm shins, 34 cm
upper arms, and 35 cm forearms used by walking solve against the climb contacts.
Knee and elbow planes are transported around the limb axis with a bounded
angular rate, so a nearly straight limb cannot flip to the other valid IK
solution. Both hands ease from their live pose into the ledge contacts; both
feet progressively plant on attainable wall contacts and independently travel
to the top. Once the body arrives, it retains all supports during a settle phase
and converges within a landmark tolerance of ordinary standing before the
traversal flag changes. Completion
requires the simulated body to converge on the landing rather than merely
finishing an animation timer. A downward departure still uses gravity and
becomes a drop.

Local geometry shares a human-scale convention. The visible biped is roughly
1.9 world units tall, doors are 1.84 units, ordinary buildings are 3.2–4.2 units,
and the current ruined-road climbing proof has a 1.65-unit lip. Render meshes,
ray-picking occluders, collision footprints, reach tests, and surface heights all
use those same values.

Neither family is yet a full articulated rigid-body dynamics simulation. The
robotic family uses explicit support feedback and damage-aware traction. The
supported biomechanical gait integrates aggregate body forces and gravity plus
joint inertia, muscles, passive tissue, and contact reactions. Its unsupported
state is now a generalized particle ragdoll, but individual bones still do not
own oriented six-degree-of-freedom rigid bodies, and angular joint limits during
a fall remain approximated by the braced constraint graph. Terrain projection
and final foot constraints still resolve penetration. This gives us inspectable
physical consequences without claiming that position constraints are a complete
articulated-body, friction, or soft-tissue model.

The implementation follows several useful findings from existing work:

- FABRIK solves joint positions through forward/backward point reaching, is
  inexpensive, and extends to constrained and multi-end-effector chains
  ([Aristidou and Lasenby](https://www.andreasaristidou.com/FABRIK)).
- Practical terrain placement must probe in world space, smooth targets and
  normals, adjust body height, and only then solve IK
  ([Khronos procedural-animation guidance](https://github.khronos.org/Vulkan-Site/tutorial/latest/Advanced_glTF/Procedural_Animation_IK/04_foot_placement.html)).
- A support polygon is a useful safety approximation for slow locomotion but
  becomes insufficient as acceleration increases
  ([Byl, MIT](https://groups.csail.mit.edu/robotics-center/public_papers/Byl08e.pdf)).
- Duty factor and relative phase compactly describe contact patterns across
  machines with different limb counts
  ([Chong et al.](https://arxiv.org/abs/2112.00662)).
- Contact-responsive, per-segment state machines can scale across rough-terrain
  machines with many legs and provide a deterministic baseline for later
  learning systems
  ([Chen, Wang, and Revzen](https://arxiv.org/abs/2603.09147)).

The renderer exposes which family is active. Robotic bodies show chain state,
support count, margin, and traction. The biped shows contact phase and mean
muscle activation, and colors its visible rig by heel, flat, toe, swing, or air
state. Skins bind to the same joint positions without becoming locomotion
authority.

A neural network is deliberately not in the control loop. Contact sampling,
gait scheduling, inverse kinematics, constraints, and gravity are predictable,
cheap, and testable in ordinary C. If authored rules later fail on irregular
terrain, a small learned policy may propose footholds, gait timing, or residual
body corrections. It must first run in shadow mode beside the deterministic
controller, record disagreements and outcomes, and earn replacement through
replay tests. It must never be required for basic balance, collision safety, or
save determinism.

## Command boundary

Local play submits explicit commands such as:

```text
BEGIN_TRAVEL
SHIPMENT_DELIVERED
SHIPMENT_DESTROYED
PASSENGER_ARRIVED
ROUTE_REPAIRED
BANDIT_LEADER_CAPTURED
BANDIT_AGREEMENT_CREATED
MONSTER_NEST_REDUCED
MONSTER_POPULATION_RELOCATED
DUNGEON_STATE_CHANGED
RELIEF_ALLOCATED
```

The core validates entity identity, preconditions, date, quantities, and
authority before applying a command and emitting follow-up events.

## Deterministic randomness

Each subsystem owns an independent seeded random stream. Adding a cosmetic draw
or changing monster iteration order must not alter market history.

Streams should be derived from stable identifiers and explicit counters rather
than ambient global random state. Debug output records the stream and draw that
caused a significant event.

## Numeric representation

Strategic quantities use integers or documented fixed-point units. Floating
point may be used for presentation, but not where platform or iteration
differences could change long-term authoritative outcomes.

Every quantity declares:

- Unit
- Valid range
- Source and sink operations
- Overflow behaviour
- Serialization representation

## Save structure

A save contains:

- Format version
- Generator version
- World seed
- Immutable-generation identifiers
- Current calendar date
- Complete mutable strategic snapshot
- Stable ID tables and generations
- Active situations and contracts
- Causal event history required by play
- Route and dungeon mutations
- Discovered information
- Player company, carriage, crew, cargo, and reputation
- Physical map objects, ownership, map-case capacity, survey claims, and prices
- Deterministic stream states or counters

Procedural decoration is regenerated. Meaningful mutation is stored.

The architecture proof stores authoritative records in SQLite tables for world
metadata, kingdoms, settlements, routes, factions, shipments, bandit groups,
monster populations, dungeons, situations, causal events, and the player
company. Physical route charts are stored as stable map-object rows rather
than reconstructed presentation state. Shipment intent is stored separately
from its current route leg. A
save is one atomic transaction. Loading validates its schema, references, and
exact state hash before accepting it.

## Networking boundary

Single-player remains the product assumption. If cooperative or hosted play is
later justified, the server owns the simulation and accepts the same validated,
dated commands used by the local client. Clients receive snapshots and causal
events and may interpolate presentation only. SQLite remains the single-player
save and developer inspection format; it is not used as a network protocol or
as a shared multi-writer database.

## Generator versioning

Changing generation code can move cities, remove camps, or invalidate dungeon
references. Every save records a generator version. Supported approaches are:

- Preserve the old generator for existing saves
- Migrate the immutable generated base explicitly
- Snapshot generated authoritative records in the save

The project must choose one before public save compatibility is promised.

## Causal event retention

Not every event remains forever. Events are classified as:

- Permanent historical milestone
- Active causal dependency
- Player-known history
- Debug-only trace
- Expirable operational event

Compaction may remove debug-only events after their children have materialized,
but cannot break explanations, active situations, or character memory.

## Headless tools

Required developer tools include:

- Multi-year simulation runner
- Seed batch runner
- State inspector by entity ID
- Route and shipment inspector
- Causal-chain viewer
- Pause-on-condition breakpoints
- Command/event replay
- State-hash comparison
- Save/load equivalence runner
- CSV or JSON metric export

The simulation inspector is core infrastructure, not optional polish.

## Test layers

1. Unit tests for accounting, IDs, routes, and event creation.
2. Property and invariant tests over randomized commands.
3. Deterministic replay tests.
4. Save/load equivalence tests at arbitrary dates.
5. Multi-seed long-run stability tests.
6. Projection tests proving strategic states select correct local changes.
7. Vertical-slice scenario tests for each intervention.
