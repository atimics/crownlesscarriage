# Character steps and turns

Each boot keeps its heading while it supports the character. During a step, it
turns toward the new facing direction along the shortest angle. A standing turn
uses small steps and settles into the existing idle pose.

The foot follows an eased curve through lift and landing. At 0.70 m/s on flat
ground, a test sampled at 120 Hz measures a maximum landing speed of 0.0052 m/s,
compared with 1.0346 m/s for the previous controller. Both have about 12 cm of
mid-step clearance. These figures describe the test fixture.

The upper body keeps the game's eight-pose walk rhythm. Each change eases over
55% of the pose interval, followed by a hold. The held pose blends with the
physical pose as speed rises from 0.10 to 0.35 m/s. This gives starts and stops
a soft transition. Body lean is captured with each pose. Feet blend from their
walking pose into the climb preparation.

Pose history advances on each fixed simulation step. Rendering blends the
previous and current poses. A regression test compares matched poses at 30,
60, and 120 presentation samples per second, plus uneven sampling. It covers
a turn, a walk, and a stop, and requires exact matches.

Arm swings grow with walking speed. Forearm bend follows the shoulder with a
small delay. In the flat-ground test, the shoulder's swing spans 0.622 radians
at 0.35 m/s and 1.181 radians at 1.30 m/s. Both arm bones keep their lengths.

Resting characters breathe on a 3.6-second cycle. Their appearance seed sets
the cycle offset. The chest moves through 3.2 cm vertically, and the arms and
head follow it. The pelvis and feet keep their physical positions. Breathing
fades as walking speed rises and respects the reduced-motion setting. It uses
the interpolated simulation clock. The 120 Hz test measures a largest chest
step of 0.267 mm.

[View the matched before and after animation](character-animation-comparison.gif).

![Walk, walking turn, and standing turn](character-animation.gif)

This eight-second review uses fixed simulation ticks, the game's pose blending,
and its character models. Each scene finishes at rest. Generate 120 frames at
15 frames per second with:

```sh
cmake --preset play
cmake --build --preset play --target run_humanoid_animation_captures
```

Frames are saved in `out/build/play/humanoid-animation`. The full Linux client release job runs the same capture and publishes the
`humanoid-animation` artifact.

The motion tests cover planted headings, left and right turns, the angle wrap,
standing half-turns, settling, step clearance, and takeoff and landing speed.
The local movement suite checks climbing, foot contacts, and held upper-body
poses in the character's facing frame. Renderer tests also cover breathing,
repeated rendering, reduced motion, guard poses, and the walk-to-idle threshold.
