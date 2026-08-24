# Runtime Animation System

## Implemented architecture

The humanoid pipeline now has an explicit contract between simulation and
presentation:

1. Gameplay selects an action and supplies movement intent.
2. `CcMotionPlayer` advances a deterministic clip timeline and emits markers.
3. The procedural gait currently supplies the base pose; authored local-space
   clip samples can be supplied through the same clip format.
4. Contact solving owns planted feet and traversal effectors. Combat
   presentation uses bounded two-bone arm IK with transported elbow poles.
5. A single `CcHumanoidPoseOwner` identifies the system allowed to produce the
   base pose: procedural motion, traversal, ragdoll, recovery, or a future
   paired interaction.
6. The fixed tick finalizes consecutive `CcHumanoidPoseSnapshot` values.
7. Rendering interpolates those snapshots once. It does not apply another
   behavioral smoothing loop.

This preserves the deterministic biomechanical prototype while preventing
physics, animation, and rendering from silently chasing different poses.

## Physical authority

Animation does not keep an invisible upright root alive during a fall. Stable
and marginal support use the biomechanical root and contact controller. A brief
support-loss grace remains controlled airborne; continued loss transfers the
live pose and velocity to the ragdoll. While that graph is active, its center of
mass owns the character position and velocity. Recovery drives the same graph,
then hands control back only after supported convergence.

The shared local collision query serves the walking capsule, landmark spheres,
and torso/limb capsule segments. Support is an explicit stable, marginal,
hands, controlled-airborne, or uncontrolled-fall state rather than a renderer
guess from `grounded`.

## Stable idle

Grounded idle uses speed hysteresis and a settle interval. Once stable, both
feet retain persistent world-space anchors, gait phase stops, locomotion
springs settle to zero, and the driven joints lock to their final targets.
Movement intent above the exit threshold releases the lock immediately.

Breathing or personality animation must be added later as an upper-body
additive. It must not modify the pelvis, root, or planted foot transforms.

Combat guard is intent-driven rather than inferred from the previous action.
Releasing a target clears guard intent even if a strike is still finishing, so
the completed strike cannot resurrect a stale pose. Guard and strike hands use
asymmetric three-dimensional targets around the chest, bounded hand speed, and
continuous elbow poles; the arm solver preserves exact segment lengths. The
gameplay swept-weapon contract remains independent of small presentation IK
offsets.

Player-authored walk targets also time out after sustained zero progress. This
prevents a blocked click behind geometry from feeding a walk or guard cycle
forever, without cancelling autonomous navigation waypoints.

## Traversal contact plan

Climbing uses world-anchored effectors. The upward plan acquires both hands,
stages separate low and high wall footholds, plants the lead foot on top, moves
the hands into a supported press, then plants the trailing foot before blending
to standing. Down-climbing reverses that logic with separately timed top, wall,
and ground contacts. No foot target is derived from the moving root, which
prevents the old synchronized wall slide. Per-tick pose displacement, anatomical
segment lengths, hand reach, wall clearance, contact error, and the order of the
two top-foot plants are regression-tested.

## Motion clip contract

`CcMotionClip` contains:

- duration, sample rate, looping policy, and identity;
- sample-major local bone transforms;
- normalized quaternion interpolation;
- synchronized contact, weapon, recovery, and control markers.

The built-in idle, walk, guard, strike, jump, traversal, swim, fall, and get-up
clips currently provide timeline metadata. This is intentional: the exported
hero GLB still contains no authored animation tracks, so the procedural system
continues to provide poses. Importing Blender samples will not require changing
combat event timing or the player API.

## Ragdoll recovery

Ragdoll and recovery have distinct pose owners. A settled body is classified as
supine, prone, left-side, or right-side and selects the matching recovery
timeline. Death disables recovery; knockdown and explicit resurrection enable
it. Angle limits, self-separation, and limb collision volume remain active in
both fall and recovery. The physical pose remains authoritative until recovery
completes.

## Gameplay-scale hero presentation

The engine skin keeps the detailed modular GLB, but its final presentation is
tuned for the low-resolution play camera. The head is enlarged, the hands are
reduced, and the whole figure is made slightly taller and narrower so the face,
torso, and legs remain separate shapes. The runtime material table follows the
exported leather and steel slot order. The model owns the broken crown, while a
bone-driven chest clasp and broken-gold mark reinforce identity without adding
a second crown. The hero shader uses lighter shadow paint and a finer inner ink
edge so garment layers do not collapse into one dark block.

## Diagnostics and regression contract

Each humanoid retains a 64-tick compact animation trace containing pose owner,
clip and time, action, root motion, gait phase, contacts, markers, and idle-lock
state. `CcHumanoidGaitTraceLatest` exposes the newest record for debugging and
future rewind tooling.

`motion_system_tests` verifies marker timing, looping contacts, authored
local-space sampling, exact long-duration idle stability, fixed-tick snapshot
history, foot anchors, trace capture, and ragdoll ownership. Existing locomotion,
combat, climbing, swimming, skinning, and death tests remain required. The
biomechanical contract additionally bounds guard/strike landmark velocity,
checks arm segment lengths and a readable bent guard silhouette, while the
local integration suite verifies target disengagement and full village combat.

## Next asset-backed milestones

1. Export fixed-rate local transforms and metadata from the Blender action
   library into `CcMotionClip` assets.
2. Add inertial offsets when switching between sampled clips and procedural
   fallback poses.
3. Replace the single procedural traversal pose with classified vault, mantle,
   climb, and drop clips plus bounded root warping.
4. Add a paired-interaction owner for wrestling and manhandling; both actors
   must share one phase and cross-character contact plan.
5. Add small pose-search databases only after the authored start, stop, pivot,
   get-up, and traversal coverage is available.
