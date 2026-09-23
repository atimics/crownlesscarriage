# Ground-level authored town views

The user-facing correction is to the viewpoint, not another building family.
The towns are fixed, composed third-person adventure scenes: see people and
facades from the street, not roofs from a map. Roads retain their side profile;
the mine retains its first-person view. The town camera is not a first-person
or over-the-shoulder player-follow camera.

## Camera contract

Every town scene keeps its named trigger and physical geography. `target_y` is
look-at height over target terrain; `camera_offset_y` is eye height over terrain
at the actual camera station. `fovy` remains the authored vertical framing span
at the target, converted to a perspective angle. Do not reinterpret it as
perspective degrees or apply the exterior helper's 2.5x telephoto pullback.

All 36 camera definitions across the six existing profiles use street-level
stations. The sixteen principal four-town views were composed individually,
including looking along Workers' Lane, facing the archive facade and viewing
Alderwatch's crossing from its town-side approach. Gates, streets, building
footprints, collisions, service doors, parking, save positions and economies
are unchanged.

The old `horizontal_span * 0.55` minimum dive and roof-height camera lifting
are removed from town view construction. A blocked camera station seeks clear
horizontal ground rather than rising above the roof. Scene-boundary hysteresis
is retained, but changes between authored town screens now cut rather than
fly the lens through intervening buildings. Arrival hands off its displayed
frame exactly once, then cuts to the appropriate authored street station.

Within a screen, the actor can move independently in a quiet area. Necessary
reframing aims the lens from that station instead of translating it through
neighboring houses. An actor approaching the eye plane causes a horizontal
retreat, retained separately from aim. Neither operation is a vertical crane.
The projection-aware guard solves yaw and pitch together and retains the
same presented camera for input queries. The southern yard also owns its
approach (22-unit activation radius); its previous close-up-only radius could
select a heart camera behind a south-road walker.

## Regression changes

The previous tests required low *authored numbers* but an actual elevated
camera, including `ray.direction.y <= -0.25` and multi-storey roof clearance.
Those assertions contradicted the requested style. They now check the actual
eye height, perspective lens, low angle, terrain-clear sightlines and matching
input rays. No navigation or collision checks were disabled.

`TestGroundTownCameras` checks all 36 screens on two seeds after settling:
ground eye, no aerial dive, valid perspective, named screen identity, actor
bounds, paused-camera identity and projection/input-ray agreement.
The route matrix retains safe-frame, paused-query and bounded within-screen
motion checks across six towns, two seeds, 30/60/144 Hz. Deliberate cuts are
identified by the authored screen ID, not exempted by a jump-size heuristic.
Miller's Bend still checks real pointer preview, exact command agreement and
completed walking; its camera-motion check allows at most two named cuts while
retaining the original within-screen movement bound. Idle lens/actor motion,
arrival handoff, conversation, pony and building-cutaway contracts remain.

## Rendered review

Reproduce the actual native screenshots (staged positions, not a playthrough):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCC_WARNINGS_AS_ERRORS=ON -DCC_BUILD_GRAPHICS_PROBES=ON
cmake --build build --parallel 4
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 tools/capture_four_towns.py \
  build/crownless_carriage --output out/ground-town --conditions
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build --output-on-failure \
  -R 'distinct_local_places|local_collision_space|renderer|carriage|creature|travel|footing|town_arrival|town_departure|roadbook_arrival|oven_court|silverwick_mine'
```

Review actual output, including foreground clipping, actor and gate legibility,
quiet-area motion and cut orientation. This is not a new sky/lighting system or
a complete ordinary-control playthrough; existing asset warnings remain. Full
player journeys, live combat/conversation framing, touch/Safari and device GPU
variation need play review. A changing camera must never relocate the company,
change its cargo or replace its ponies.
