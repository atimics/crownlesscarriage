# Character Physics Foundation

## Decision

Crownless uses one local character-physics contract for walking, climbing,
jumping, combat obstruction, falling, ragdoll contact, and recovery. It does
not turn the strategic world simulation into a real-time rigid-body simulation.

The strategic layer remains deterministic and advances through explicit time
and commands. Character physics runs only inside an active local scene. Local
outcomes may become strategic commands, but frame-time contacts never write
directly into strategic arrays.

## Authority flow

The visible body has one physical owner at a time:

1. Supported movement uses the force-integrated biomechanical root, planted
   contacts, anatomical joints, and muscle targets.
2. A short loss of support remains a controlled airborne body. This avoids
   turning a small road undulation into an instant collapse.
3. Continued support loss maps the live pose and momentum into the constrained
   particle body. Its center of mass becomes the agent, camera target, shadow,
   navigation position, and velocity; the old walking root no longer continues
   beside it.
4. Recovery drives that same fallen body through brace, kneel, and stand. The
   walking controller receives authority only after the body is supported,
   settled, and close to the standing anatomy.

`control_authority` describes this continuum. Stable walking is `1.0`,
controlled air and partial support retain a bounded fraction, passive ragdoll
is `0.0`, and recovery grows back toward `1.0`.

## Support states

Every biped exposes one of five support states:

- `STABLE`: a broad, load-bearing contact set;
- `MARGINAL`: one contact or a narrow support set;
- `HANDS`: explicit supported traversal;
- `CONTROLLED_AIRBORNE`: a jump or the brief start of support loss;
- `UNCONTROLLED_FALL`: passive body physics owns motion.

The compatibility `grounded` flag is derived from these states. It is not the
source of truth for ragdoll recovery. A fallen body needs several upward
contacts on one landing plane with enough horizontal spread. A hand left on a
higher ledge cannot make the body recover in mid-air.

## Shared collision contract

Walking, climbing, combat, and the fallen body query the same local geometry:

- continuous terrain height and normal;
- tagged platforms and their side walls;
- buildings, castle structures, carriage, dungeon entrance, and grounded art
  props;
- market and road obstacles.

Swept sphere tests prevent a fast landmark from crossing a thin wall between
ticks. The walking root samples a vertical capsule. The fallen body tests both
landmark spheres and limb/torso capsule segments, so a wall cannot pass between
two joint particles. Platform tops support a body only when approached from
above; their sides remain walls.

Climbing resolves its root capsule through this world and validates requested
hand and foot contacts by probing toward the real surface. A missing contact
reduces control instead of leaving a hand pinned to empty space. Combat sweeps
the attacker-to-target line and the moving weapon segments through the same
world, so a shelf or wall can stop a strike.

## Anatomical body rules

The ragdoll preserves bone lengths, shoulder and hip width, bounded shoulder
and hip cones, spine limits, knee and elbow hinge planes, selected
self-separation distances, and collision volume along the torso and limbs.
Impact damping removes solver-created rebound without adding slow-motion drag
during free fall. During active get-up, hinge-plane resistance relaxes while
the authored motor reorients the limbs, then returns to its passive limit.

Recovery checks the full intended standing body, including limb and torso
capsules, against nearby geometry. If the current root would place part of the
body inside a wall or corner, it selects the nearest clear root before brace,
kneel, and stand begin.

Supported walking uses the contact normal as a body frame. Feet, ankles, hips,
and torso follow the ground plane instead of remaining world-up while the legs
stretch underneath them. The balance controller cancels the horizontal part of
the ground reaction before adding travel force. Normal grades are therefore
walkable, while slopes beyond the friction budget still slide or require a
different route.

## Deliberate limits

This foundation is not yet a general physics engine:

- the supported human uses one aggregate dynamic root plus anatomical joint
  dynamics, not a rigid body for every bone;
- most environment objects are static collision geometry;
- characters do not yet exchange full two-way rigid-body impulses;
- moving platforms do not yet transfer their velocity into support contacts;
- cloth, weapons, carriage parts, and debris retain their own bounded systems.

The next useful extensions are moving-support velocity, two-way character
impulses for wrestling and crowd pressure, and more active-ragdoll motors for
large controlled disturbances. They should extend this contract rather than
create a second collision or fall system.

## Regression contract

Automated tests must prove:

- walking up a friction-valid slope makes forward progress and feet follow the
  contact normal;
- loss of support passes through controlled air before passive fall;
- ragdoll activation preserves the live pose and momentum;
- spine/cone limits, knee and elbow hinges, self-separation, and limb capsule
  collision remain active;
- a fall from all four tower edges reaches the street in the gravity timing
  band, does not bounce, recovers, and keeps agent position and velocity equal
  to the fallen body's center-of-mass authority;
- a fallen body scrapes down a wall, hits both faces of a corner, and lands on
  a shoulder without penetration or stretched bones;
- loss of one climbing contact lowers authority and releases that limb without
  a snap; sustained serious contact loss becomes passive physics;
- walking, climbing, combat, and ragdoll collision agree on shared obstacles;
- local combat, climbing, swimming, navigation, and terrain suites still pass.
