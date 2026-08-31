# MicroDuck Simulator Review

Reviewed on 2026-08-30 from the public
[launch article](https://pollen-robotics.com/microduck/blog/introducing-microduck/),
[browser simulator](https://huggingface.co/spaces/pollen-robotics/microduck-simulator),
[simulator source](https://huggingface.co/spaces/pollen-robotics/microduck-simulator/tree/main),
[robot runtime](https://github.com/pollen-robotics/microduck), and
[training environments](https://github.com/pollen-robotics/microduck_rl).

## What is useful

The simulator's strongest idea is its control boundary. Keyboard, gamepad, and
touch input all produce one continuous movement command plus named one-shot
actions. Walking, sitting, rolling, kicking, picking up, skating, and fall
recovery use different learned policies, but each policy keeps the same input
and joint-output shape. A behavior can own the robot for a short time and then
return control to ordinary movement.

Mode changes are requests rather than pose changes. The app may wait for a
one-shot action or entrance sequence to finish, then reconcile the requested
mode. Models and policies stay loaded after their first use, so changing mode
does not rebuild the visible character. The result feels immediate while
keeping a clear physical owner.

Other good product ideas are less technical:

- The first screen teaches six verbs and then starts play.
- A ball turns movement quality into something a player can feel.
- Falling is ordinary play because recovery is part of the behavior set.
- Color and voice give mechanically identical robots individual identity.
- The debug readout exposes speed, distance, render rate, and control rate
  without making them the main interface.

## Applied to Crownless

Journey pace now requests a physical gait from the existing horse controller:

| Journey pace | Visible gait | Physical contract |
| --- | --- | --- |
| Careful | Walk | One hoof may swing; three support the body |
| Steady | Trot | Diagonal pairs share a two-beat phase |
| Push | Canter | A faster four-beat sequence may lift two hooves |

All three modes produce the same creature pose type. The carriage renderer and
horse skin therefore do not care which gait owns the body. A faster gait can
take control immediately. Returning to a walk waits while two hooves are
airborne and stops scheduling a replacement pair. The current contacts and
joint positions remain live during the handoff.

This connects an existing player decision to a local physical symptom. Careful,
steady, and push pace already change time, fatigue, and wear in the strategic
simulation. The road scene now shows that decision in the team before the
numbers arrive, and the road panel names the active gait beside the pace and
arrival estimate.

Automated fixtures require each gait to keep its support and swing budget,
require the trot to lift a diagonal pair, and require a return to walking to
wait for a safe support set.

## Already present in Crownless

Crownless does not need a second recovery system. Its humanoid already moves
through supported control, marginal support, controlled air, ragdoll, brace,
kneel, and stand under one body-authority contract. That is the relevant
equivalent of MicroDuck's automatic fall recovery, with deterministic contacts
instead of a learned stand policy.

Named carriage horses already carry persistent identity through name, coat,
health, fatigue, training, temperament, strength, and lineage. More cosmetic
variants would be useful later, but they are not needed to prove the gait
change.

## Not copied

- ONNX policy inference and MuJoCo are not needed. They would add a second
  physics runtime and make exact replay harder without improving this authored
  carriage problem.
- Browser delivery is not relevant to the native C17 client.
- Online translucent ghosts do not serve the single-player causal world. A
  future road-memory or caravan-trace system should come from saved world
  events, not anonymous network presence.
- The ball sandbox is excellent for a robot demo, but adding a loose toy to
  Crownless would not yet connect to a journey, situation, or persistent
  consequence.
- No MicroDuck code, policy, model, sound, or art asset is included. Crownless
  implements the control and handoff ideas in its existing deterministic rig.

