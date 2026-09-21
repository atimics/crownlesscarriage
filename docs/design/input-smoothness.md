# Input and rendering smoothness review

Status: reviewed 2026-09-20

The native client is raylib, capped at 60 FPS with a fixed 60 Hz simulation
step. The controls are click-to-walk (WASD only cancels an approach), so "not
smooth" here means presentation latency and hitching, not missing steering.

## Findings, by impact

1. **Journal flushes stall the render thread (fixed here).** Every six runtime
   ticks the journal hashed the whole simulation twice and committed with
   `synchronous=FULL` (`src/persistence/cc_save.c:16`, `:6676`, `:650`). During
   travel the client advances up to eight ticks per frame
   (`src/client/cc_client_policy.c:189`), so fast-forward flushed, fsynced and
   periodically `VACUUM`ed on every frame. Fixed by batching runtime ticks to
   one second (`CC_JOURNAL_RUNTIME_FLUSH_TICKS 6 -> 60`); journey transitions
   and close still flush immediately, so completion stays durable.
2. **Travel presentation is tick-quantized (fixed here).** The carriage position
   was copied from integer journey progress each tick
   (`src/client/main.c:2015-2032`) and the storybook camera derived from it
   (`src/client/local3d/open_world.inc:963`). The carriage now keeps a
   presentation-only `render_position` interpolated between the last two ticks
   (`PositionOpenWorldJourneyAt`), and the storybook camera, ground, carriage,
   crew and label draw from it. Simulation logic still reads the exact
   `position`, so arrival and departure checks are untouched. Runtime-untested
   here (the client regression modes need a window server); compile-verified
   against raylib with warnings as errors.
3. **60 FPS cap against a 60 Hz fixed step (fixed here).** `SetTargetFPS(60)`
   with no vsync (`src/client/main.c:11284`) beat. Normal play now also sets
   `FLAG_VSYNC_HINT`, so frame times are stable and tear-free while the 60 FPS
   target still caps high-refresh displays. Captures and benchmarks are
   unchanged.
4. **Hover preview pathfound on an 80 ms timer (fixed here).** The movement
   reticle ran a full pick including the 27,648-node A* with a full array reset
   (`src/client/local3d/terrain_navigation.inc:1762`). Hover now resolves the
   point under the cursor without pathfinding (`PickAgentTargetInternal` with
   `pathfind == false`) and the path is built on click; the reticle cooldown
   drops from 80 ms to 20 ms. The hover preview no longer draws the route line.
5. **Storybook scenery built every visible mesh in one frame (fixed here).**
   `DrawStorybookScenery` now builds at most one new cell mesh per frame, so
   entering a forest fills in over the next few frames instead of hitching.
   World chunk streaming now also runs under a CPU-time budget: the client uses
   `CcWorldStreamFollowRouteTimed` with 1.5 ms per frame, and generation stops
   once that is spent. The untimed entry points are unchanged, so tests stay
   deterministic.

## What already exists

Fixed-step accumulator with interpolation alpha (`src/client/cc_local_runtime.c`),
humanoid pose blending (`BlendHumanoidPose`), exponential camera easing for
combat and conversation (`src/client/local3d/camera_composition.inc:1584`), rate
limited pace and travel blend (`src/client/cc_client_policy.c:180-296`), and
pixel-snapped orthographic cameras (a deliberate pixel-art choice).

## Next

1. Runtime verification of (2)-(5) on a machine with a window server; the
   client regression modes run on the release tag build.
2. If chunk generation still shows up in profiles, move it to a worker thread;
   the current budget bounds the per-frame cost but does not overlap it with
   rendering.
