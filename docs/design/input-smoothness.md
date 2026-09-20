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
3. **60 FPS cap against a 60 Hz fixed step.** `SetTargetFPS(60)` with no vsync
   (`src/client/main.c:11284`) beats: some frames run zero steps, some two.
   With (2) in place, rendering at vsync using the interpolation alpha would
   remove it. Next.
4. **Hover preview pathfinds on an 80 ms timer.** The movement reticle runs a
   full pick, including a 27,648-node A* with a full array reset
   (`src/client/main.c:5523-5546`, `src/client/local3d/terrain_navigation.inc:1762`).
   A terrain-only hit for hover, and pathfinding only on click, would make the
   reticle cheaper and let it update more often. It also changes the preview UX,
   so it needs a decision first.
5. **World streaming and scenery rebuild on the render thread.**
   `CcWorldStreamFollowRoute` runs per frame during travel
   (`src/client/main.c:2024`) and storybook cells upload meshes when they enter
   view (`src/client/local3d/open_world.inc:1100-1138`). Amortizing mesh builds
   and pre-generating the strip ahead of the camera would smooth it.

## What already exists

Fixed-step accumulator with interpolation alpha (`src/client/cc_local_runtime.c`),
humanoid pose blending (`BlendHumanoidPose`), exponential camera easing for
combat and conversation (`src/client/local3d/camera_composition.inc:1584`), rate
limited pace and travel blend (`src/client/cc_client_policy.c:180-296`), and
pixel-snapped orthographic cameras (a deliberate pixel-art choice).

## Next

1. Interpolate the carriage and camera from the tick pair (removes the largest
   visible stepping).
2. Move hover to a terrain-only probe and pathfind on click.
3. Then revisit vsync once (1) lands.
