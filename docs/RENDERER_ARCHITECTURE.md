# Local renderer architecture

The `crownless_local_renderer` static library owns the local client renderer.
The game, movement tests, terrain tests, and NPC appearance tests link this one
library. CMake compiles its five source objects once per build.

## Core modules

`src/client/cc_local3d.c` is a small unity root. It includes the internal
modules in source order. This keeps private types and helpers inside one C
translation unit while giving each area a clear file.

| Module | Ownership |
| --- | --- |
| `context_state.inc` | shared definitions, atmosphere, and frame state |
| `terrain_navigation.inc` | terrain generation, collision, and path finding |
| `actor_simulation.inc` | movement, traversal, combat, and actor poses |
| `camera_composition.inc` | street, road, combat, and conversation cameras |
| `asset_loading.inc` | models, shaders, palettes, init, and shutdown |
| `authored_places.inc` | terrain presentation and settlement structures |
| `actor_rendering.inc` | heroes, NPCs, creatures, carriages, and combat cues |
| `road_book.inc` | roads, forks, remote sites, and local scene composition |
| `open_world.inc` | streamed terrain, routes, sites, and settlements |

The unity root is the only include point for these files. New module links use
private functions in source order. A module can become its own C translation
unit when its inputs form a small stable interface.

## Lifecycle boundary

Renderer state follows this order:

1. Set startup options, then call `CcLocalRendererInit`.
2. Set the frame atmosphere, opening step, diagnostic mode, and movement
   preview through the renderer functions.
3. Call `CcLocalRendererBeginFrame`. This advances atmosphere state and resets
   frame metrics.
4. Bind place and world data through `CcLocalBindPlace` and
   `CcLocalBindOpenWorld` before input probes or drawing.
5. Use the `CcLocalDraw*` entry points.
6. Call `CcLocalRendererShutdown` once while the graphics context is active.

Resource caches and shared frame state stay private to the library.
Visible trees share two static GPU batches: one for wood and shadows, and one
for foliage. The renderer rebuilds them when terrain, place style, kingdom
color, or tree visibility changes.
The wood batch uses 16-bit indices so cylinder rings share vertices. Renderer
benchmarks report the total static batch vertex count with the draw count.

## Test interfaces

Tests use `cc_local3d.h` for public terrain and movement behavior.
`cc_local3d_internal.h` exposes narrow collision, camera, face, and renderer
metric seams for focused tests. Internal source modules stay private to the
unity root.

## Clean build measurement

The measurement used AppleClang 17, Unix Makefiles, four build jobs, and the
strict `development` preset. Each value is the median of three runs of
`cmake --build --preset development --clean-first`. Raylib was already present
before every timed build.

| Measure | Before (`81f286e`) | After |
| --- | ---: | ---: |
| Renderer objects for the game and two renderer tests | 15 | 5 |
| Renderer objects in the full client-enabled graph | 16 | 5 |
| Wall time | 4.43 s | 4.50 s |
| User CPU time | 11.63 s | 10.71 s |
| System CPU time | 4.57 s | 4.46 s |

Other parallel targets set the wall time. The renderer work is now shared, and
total CPU time fell by about 6 percent across the whole clean build.
