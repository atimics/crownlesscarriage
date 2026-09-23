# Town skies and distant horizons

Town cameras stay at street level. This pass fills the exposed distance with
sky, clouds and settlement-specific horizons; it never moves the camera to
make room for the sky. A tightly composed facade can still fill the screen.
Roads retain the existing side-profile sky/forest and mines remain underground.

## One world, not a backdrop that changes at every cut

The sky is at infinity relative to camera translation. Its gradient uses world
up, and clouds and celestial lights occupy world directions. Near/far horizons
are complete 360-degree rings at radii 184 and 290 around the same town origin.
Each authored scene sees the appropriate sector of that same horizon. Near
layers therefore have geometric parallax; camera cuts do not reshuffle them.
Periodic ridge functions close the azimuth seam. Sparse silhouettes sit beyond
all playable town coordinates and have no collision or interaction entries.

All six settlement profiles have distinct terrain and skyline decoration:
Thornford has low rolling hills, windbreaks and farm gables; Silverwick has
angular quarry ridges and headframes; Gloamgate has roof/belfry silhouettes;
Alderwatch has mountain ridges and watch posts; the capital has domed massing;
the dungeon-town profile has broken piers and bare branches. These are distant
art, not new settlements, services, mine entrances, stock or production events.

## Atmosphere, not a second weather simulation

The existing atmosphere controller supplies continuously blended clear,
rain/overcast, amber dusk, moonlit night and dragon-omen parameters. Clear
weather also receives a dawn tint during the existing clock's first 90 daytime
minutes. Night adds fixed stars and a moon; rain/omen suppress the solar disc.
No weather, clock, economy or save schema is added or changed. Burn damage
adds low haze only. Hunger cannot summon rain; damage is not evidence of a
still-burning fire, so no ungrounded smoke/production plumes are fabricated.

Cloud drift uses bounded periodic cosmetic time, never accumulated draw time
or simulation randomness. The same clock reproduces the same clouds; reduced
motion fixes their positions. Town terrain fog uses the same atmospheric color
family to avoid a dark border between the playable ground and distant art.
Every later lighting setup still publishes its own fog, so the town palette
cannot leak into a road or interior pass.

## Rendering contract

`town_sky.inc` is included in the existing local renderer. The sky is an unlit
triangle pass before the town geometry with depth testing/writing disabled;
both and culling are restored before drawing the horizon and playable world.
Horizon geometry is depth-tested, outside the playable map and inside the
512-unit town far plane. It does not register any picking or collision shapes.
There are fixed geometry budgets, no texture downloads, no new GPU resources,
no per-frame heap allocations and no campaign PRNG consumption. Existing
pixel grading, local scene occlusion and foreground characters are retained.
The superseded one-sided, below-ground backdrop functions are removed; shared
remote-site rock primitives are retained.

## Reproduce

Build the native client and tests with `CC_WARNINGS_AS_ERRORS=ON` and
`CC_BUILD_GRAPHICS_PROBES=ON`, then run from the repository root:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/renderer_regression_tests --sky-graphics unused
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 tools/capture_four_towns.py \
  build/crownless_carriage --output out/towns --conditions
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 tools/capture_town_skies.py \
  build/renderer_regression_tests --output out/skies
```

The first command verifies real framebuffer coverage, reduced motion,
translation invariance of the sky and restored depth testing/writing. Pure
contracts exercise profiles, horizon wrap, bounded palettes, atmosphere
transition continuity and unchanged simulation hashes. The capture tools
remove stale files and record commands, dimensions and SHA-256 receipts.

Review all sixteen principal screens, the nine state variants and the six
settlements in six moods. Inspect visible horizons, not an artificial minimum
sky percentage that would undo the authored cameras. Screenshots and tests
are not a claim of a complete live journey or physical-phone/Safari coverage.
