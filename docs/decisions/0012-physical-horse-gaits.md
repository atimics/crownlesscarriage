# 0012 — Let journey pace request a physical horse gait

**Status:** Accepted

## Context

Careful, steady, and push pace already change journey time, fatigue, wear, and
risk. The road renderer previously changed only the team's speed. Both horses
used the same one-hoof walk scheduler at every pace, so an important player
decision was much clearer in the panel than in the world.

Pollen Robotics' MicroDuck simulator demonstrates a useful boundary: several
behaviors can share one controller interface while each owns its own contact
and timing rules. A mode request is reconciled when the physical body can hand
over safely.

## Decision

Keep one persistent horse controller and one creature-pose output, but give the
controller walk, trot, and canter gait policies. Journey pace selects the
requested policy. Switching preserves current joints and planted contacts.
When two hooves are airborne, a request to walk first stops new pair swings and
waits until the support set is safe.

## Consequences

- The road scene visibly expresses the strategic pace decision.
- Every gait retains explicit minimum-support and maximum-swing budgets.
- The renderer and asset pipeline remain independent of the active gait.
- Other animal profiles fail closed when asked for an unimplemented gait.
- Adding a future horse behavior requires a policy behind the same pose
  contract and a tested handoff; it must not replace or teleport the body.

## Rejected alternatives

- Separate horse models or held animation clips were rejected because a mode
  change would replace the body instead of changing how it moves.
- A learned policy runtime was rejected because deterministic contact
  scheduling already solves the visible road problem.
- Changing only animation speed was rejected because steady and push pace
  would still have the walk's one-hoof support pattern.

