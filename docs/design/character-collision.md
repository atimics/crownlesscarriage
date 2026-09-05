# Character collision

Character movement uses continuous capsule sweeps against boxes. Each sweep
finds the first contact, moves to it, and slides the remaining movement along
the surface. The capsule includes the full space between its rounded ends.
A 2 mm clearance keeps walking contacts stable. Ragdoll particles use their
physical radius and the same sweep and overlap recovery routines.

The C implementation lives in `src/locomotion/cc_collision.c`. Rotated boxes
use the town profile's origin, scale, and rotation. Town navigation, character
movement, ragdoll contacts, and interaction visibility read these shapes.
The box capacity includes every building, compound, and landmark in all six
towns. Buildings use authored footprints and wall heights. Sloped roofs and
decorative overhangs are future collision surface types. Terrain support
continues to use the world height sampler.

Walking checks vertical clearance before stepping up by at most 24 cm. It
sweeps across the step and down to a support surface. Climbing keeps its
existing secured-ledge rule. Recovery returns to the supported standing pose.

The fixed simulation step resolves overlapping standing bipeds after movement.
Each correction also checks the world. A free actor takes the remaining
correction when the other actor reaches a wall. Separate scenes and vertically
separated bodies have separate contact spaces. Fallen bodies keep their
particle solver; climbing and swimming keep their existing movement control.

Water entry depends on body depth. Immersion follows depth, and vertical
swimming movement checks the same solid geometry. A character passing above
the pool stays airborne.

Run the focused course with:

```sh
ctest --test-dir out/collision --output-on-failure \
  -R 'continuous_character_collision|character_collision_course|local_collision_space'
```

The tests cover thin walls, rounded corners, rotated towns, full body clearance,
doorways, building tops, ceilings, overlapping crowds, water entry and exit, and the
existing climbing and fall recovery course.

Design references: [PhysX character controllers](https://nvidia-omniverse.github.io/PhysX/physx/5.6.0/docs/CharacterControllers.html)
and [Jolt CharacterVirtual](https://jrouwe.github.io/JoltPhysics/class_character_virtual.html).
