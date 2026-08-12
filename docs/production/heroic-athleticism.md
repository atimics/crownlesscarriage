# Heroic Athleticism and Contact Motion

## Decision

Crownless heroes grow by gaining physical capability, not by multiplying clip
playback speed. Locomotion, traversal, and combat remain simulations of one
continuous body. Progression expands what that controller can reliably reach,
support, accelerate, redirect, and survive.

This is a prototype contract for the vertical slice:

- Feet, hands, weapons, ledges, and opponents are explicit contacts.
- An action is an ordered contact plan with flexible timing, not a pose name.
- Authored or learned motion may propose a trajectory and pose, but live
  contacts and momentum own the result.
- Athletic levels change bounded controller capabilities. They never teleport
  the root, invent a remote foothold, or bypass collision.

## Research synthesis

Ubisoft's DReCon work combines a physics-driven character with motion matching
to retain responsiveness, future trajectory, facing, and foot placement. Its
Learned Motion Matching work reinforces the useful split for Crownless: pose
and trajectory selection can be data-driven while inertialization, foot
locking, and IK preserve contact quality.

PartwiseMPC represents contact-rich actions as ordered, underconstrained
body/environment contact keyframes whose timing remains flexible. WoCoCo uses
sequential contacts to stage whole-body parkour and climbing tasks. These are a
particularly close fit for Crownless because its runtime already owns planted
feet, climb hands, wall feet, weapon sweeps, and body capsules.

PlaMo separates scene-aware path planning from a robust physics locomotion
controller, including a path speed profile and head-height constraints. That
supports a future navigation contract in which the planner asks for a low
vault, high mantle, duck, jump, or wall contact while the same biomechanical
body executes it.

DeepMimic and MaskedMimic show a useful longer-term direction for heroic motion:
motion priors can improve style and recovery without replacing the environment
contacts or control objectives that make the action playable.

## Runtime implementation

### Athletic profile

The player agent now carries three disciplines, each with five levels:

| Discipline | Earned from | Physical effect |
| --- | --- | --- |
| Mobility | Ground travel and landed jumps | Acceleration, speed, jump takeoff, and low-obstacle vault selection |
| Grip | Completed supported ascents and descents | Reach allowance and faster controlled traversal |
| Power | Blocks, guard breaks, landed hits, and defeats | Strike damage and contact impulse |

Experience thresholds rise with level. The HUD presents both the three levels
and their averaged heroic tier, with a progress bar for every discipline. The
profile is authoritative campaign state: it survives local transitions, is
included in deterministic state hashes, and round-trips through SQLite saves.
Final tuning is deliberately deferred.

### Traversal contacts

The existing climb solver still validates reachable tagged surfaces and owns
the hand, wall-foot, and top-foot sequence. Developed Mobility recognizes a low
rise as a vault and shortens the contact sequence, while Grip modestly expands
human-scale reach and controller authority. Unsupported motion still falls.

The next traversal layer should express every move with a small contact plan:

1. Approach contacts and reachable takeoff region.
2. Required hand, foot, or body support contacts.
3. Clearance constraints for pelvis, head, and equipment.
4. Flexible completion timing and a valid recovery support polygon.

This representation can cover mantles, vaults, cat leaps, ladders, wall runs,
and down-climbs without building disconnected animation state machines.

### Combat contacts

A strike now records its swept contact point, incoming direction, and relative
speed against the defender's body capsule. A guard succeeds only when it is
frontal and the weapon sweep reaches the live guard segment between the hands.
The measured contact drives additive whole-body recoil, posture loss,
knockback, and localized impact presentation. Defeat transfers the live pose
and momentum into the generalized ragdoll.

The current strike is still a broad single attack family. Production combat
needs weapon-specific contact shapes, anticipation, active/parry windows,
push-off from the rear foot, hit-location response, and anti-stunlock recovery
transitions. Synced reactions should be exceptional finishers; ordinary combat
should remain unsynced so both bodies preserve agency and environmental
collision.

## Progression rules

- Levels increase reachable and controllable envelopes gradually; collision
  and contact validity remain absolute.
- A stronger hero produces more impulse but also needs planted support to use
  it. Airborne or poorly balanced attacks should lose effective force.
- Better Mobility improves acceleration, redirection, and landing control before
  it increases top speed.
- Better Grip improves time under supported control and recovery options before
  it expands maximum reach.
- Repeating meaningless input is not training. Experience is awarded only for
  distance travelled or resolved physical events.
- Enemy athletic profiles use the same parameters and rules as the hero.

## Production sequence

The campaign-persistence and character-progress pass is complete. Next:

1. Add trajectory history plus explicit start, stop, and pivot intentions;
   distance-match step phase while preserving planted contacts.
2. Add a traversal scanner that emits ordered contact plans and clearance
   constraints for vault, mantle, cat leap, ladder, and wall-run candidates.
3. Replace the single strike envelope with weapon data, per-limb contact shapes,
   foot-support contribution, parries, and guarded recovery variants.
4. Add stamina and fatigue as temporary reductions in control authority, not
   hard animation locks.
5. Build authored motion-prior tests for heroic style, then compare them against
   contact error, responsiveness, recovery time, and player readability.

## Primary references

- Ubisoft La Forge, *DReCon: Data-driven Responsive Control of Physics-based
  Characters*: https://www.ubisoft.com/en-us/studio/laforge/news/VjEIwquaIyEZZSw5RZI0V/drecon-datadriven-responsive-control-of-physicsbased-characters
- Ubisoft La Forge, *Introducing Learned Motion Matching*:
  https://www.ubisoft.com/en-us/studio/laforge/news/6xXL85Q3bF2vEj76xmnmIu/introducing-learned-motion-matching
- UBC, *PartwiseMPC: Interactive Control of Contact-Guided Motions*:
  https://www.cs.ubc.ca/~van/papers/2024-partwiseMPC/index.html
- Zhang et al., *WoCoCo: Learning Whole-Body Humanoid Control with Sequential
  Contacts*: https://arxiv.org/abs/2406.06005
- NVIDIA Research, *PlaMo: Plan and Move in Rich 3D Physical Environments*:
  https://research.nvidia.com/labs/par/plamo/
- NVIDIA Research, *MaskedMimic*:
  https://research.nvidia.com/labs/par/maskedmimic/
- UBC, *DeepMimic: Example-Guided Deep Reinforcement Learning of
  Physics-Based Character Skills*:
  https://www.cs.ubc.ca/~van/papers/2018-TOG-deepMimic/index.html
- Ubisoft, *For Honor v1.11 Patch Notes* (generic versus synchronized reactions
  and anti-stunlock transitions):
  https://www.ubisoft.com/en-us/game/for-honor/news-updates/6BLnxNeyg4hUICdMjgLjbm/for-honor-v111-patch-notes
