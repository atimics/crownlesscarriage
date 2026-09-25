# Style packs

Status: steps 1-3 shipped 2026-09-24 (this PR); step 4 designed, not built.

## Why

Crownless had one compiled-in look: shader paths, palette, render target
size and post-process constants were C macros in
`src/client/local3d/asset_loading.inc` and `src/client/cc_visual_style.h`.
Sierra's AGI/SCI engines shipped one interpreter and let each game's data
files decide what it looked like. This PR gives Crownless the same seam: a
style pack is a small resource pack under `assets/stylepacks/<id>/` that a
CLI flag or environment variable selects at startup, and the compiled binary
never has to change to add a new look.

Later passes -- a live painterly pass, a toy/tilt-shift pass, a cel-shaded
pass -- are designed below but not implemented. This PR (steps 1-3) is:
route the seam, move a first slice of inline colors through it, and prove the
seam with a second pack (`sierra_pixel`) that turns on a dither the shaders
already had, unused.

## The manifest

`assets/stylepacks/<id>/style.json`. Loaded and validated once, by
`CcStylePackLoad()` in `src/client/cc_style_pack.c`, at the very top of
`main()` -- before `InitWindow()`, since it only parses JSON and fills plain
C structs. See that file for the authoritative parser and
`tools/validate_style_packs.py` for a fast, Python-side structural check CI
runs without building the client.

```jsonc
{
  "schema_version": 1,
  "id": "classic",
  "version": "1.0.0",
  "shaders": {
    "world_vertex": "../../shaders/world_lit.vs",
    "world_fragment": "../../shaders/world_lit.fs",
    "skinned_vertex": "../../shaders/world_lit_skinned.vs",
    "painted_environment_fragment": "../../shaders/painted_environment.fs",
    "tree_foliage_fragment": "../../shaders/tree_foliage.fs",
    "hero_fragment": "../../shaders/hero_pixel.fs",
    "npc_fragment": "../../shaders/npc_indexed.fs",
    "grade_fragment": "../../shaders/style_grade.fs"
  },
  "constants": {
    "hero_ink_strength": 0.52,
    "material_ink": [0.52, 0.88, 0.62, 0.58, 0.68, 0.76, 0.46, 0.60, 0.96],
    "dither_strength": 0.0
  },
  "render_target": { "width": 630, "height": 320, "upscale_filter": "point" },
  "palette": { "...": "the full CcVisualPalette shape; see cc_visual_style.h" },
  "post_chain": [
    {
      "name": "grade",
      "shader": "../../shaders/style_grade.fs",
      "inputs": ["scene_color"],
      "cache": "per_frame"
    }
  ]
}
```

Fields:

- `schema_version` -- must equal `CC_STYLE_SCHEMA_VERSION` (1). Anything else
  rejects the pack.
- `id` -- must equal the manifest's own directory name.
- `version` -- the pack's own free-form version string.
- `shaders` -- required, all eight roles required. A path is written
  relative to the pack's own directory (`assets/stylepacks/<id>/`). A pack
  that does not change a given shader can reuse the shared original with a
  relative path that walks back out, e.g. `../../shaders/world_lit.vs`
  reaches `assets/shaders/world_lit.vs` from either shipped pack's
  directory; neither shipped pack keeps its own copy of a shader it did not
  change. `LoadVisualStyle()` in `asset_loading.inc` resolves and loads
  exactly these eight, replacing what used to be eight `CC_*_SHADER` macros.
- `constants` -- optional; a missing field keeps classic's compiled-in
  value. `hero_ink_strength` and `material_ink` (nine floats, one per NPC
  archetype material) are what the audit named; `dither_strength` is this
  PR's addition, read by `world_lit.fs` and `painted_environment.fs` as the
  uniform `ditherStrength` (see "Step 3" below).
- `render_target` -- required. `width`/`height` replace the compile-time
  `CC_LOCAL_ART_WIDTH`/`CC_LOCAL_ART_HEIGHT` (`cc_local_viewport.h`) at
  runtime; `upscale_filter` is `"point"` or `"bilinear"`, replacing a
  hard-coded `TEXTURE_FILTER_POINT`. Both shipped packs use 630x320/point,
  i.e. today's values -- classic must, and sierra_pixel has no reason to
  differ. See "Render target size" below for how the runtime value reaches
  every place that used to read the macro.
- `palette` -- optional; a missing field keeps classic's compiled-in
  `CcVisualPalette` (`cc_visual_style.h`). This is both the "final palette
  table" the post-process LUT is built from and the UI/hero palette
  (`palette.crownless`), since they are the same struct. Both shipped packs
  include the full palette; `classic`'s was generated mechanically from the
  compiled-in initializer (never hand-retyped) and is checked byte-for-byte
  against it by `tests/style_pack_manifest_tests.c`, specifically so a
  transcription mistake cannot silently break the pixel-identity claim this
  PR makes for classic.
- `post_chain` -- required, 1-4 passes. Each pass has a `name`, a `shader`
  (pack-relative, like the shader roles above), a non-empty `inputs` array
  (`scene_color`, `scene_depth`, `scene_normal`), and an optional `cache`
  (`per_frame`, the default, or `per_shot`). Exactly one pass, named
  `"grade"`, is actually executed in this PR
  (`PresentTarget`, `actor_rendering.inc`); its `shader` must match
  `shaders.grade_fragment`. Further passes are validated for shape and
  logged (`STYLE: pack '<id>' declares N post passes; only "grade" runs in
  this build`) but not run -- this is the seam "Future passes" below is
  designed against, not a shipped feature.

Unknown fields, anywhere in the document, reject the pack. So does a missing
shader role. On any rejection the pack falls back to the values compiled
into `CcStylePackResetToClassic()` in `cc_style_pack.c` -- not to another
pack's manifest, and not to a partially-applied mix of old and new values --
so a broken `style.json` can never produce a black screen, including if
`classic`'s own manifest is what broke.

### What is not yet pack-driven

The atmosphere and lighting tables (`ART_LIGHT_PROFILES`, `ART_ATMOSPHERES`
in `local3d/context_state.inc`) stay compiled-in constants in this PR. They
are not part of `style.json`. Neither shipped pack needs to touch atmosphere
or lighting, and hand-transcribing roughly 300 floating-point fields into
JSON for zero visible change was worse, precision-wise, than not doing it: a
single mistyped digit there would not be caught by the palette-equality test
above (there is no compiled-in table left to compare against once one is
duplicated by hand) and might not be visible in a screenshot either. If a
future pack needs its own atmosphere, add it to the schema then, generated
from the compiled tables the same mechanical way `classic`'s palette was, not
retyped.

## Step 2: named palette entries

`src/client/local3d/actor_rendering.inc` had roughly 150 inline
`(Color){r, g, b, a}` literals across its rendering paths (`grep -c
"(Color){"` on the pre-PR file). This PR moves eight of them -- the ones
that recur verbatim across multiple draw calls, or that already had an
obvious real-world name -- into `CcVisualPalette` fields, added right after
`crownless` in `cc_visual_style.h`:

| Field | Was | Used for |
| --- | --- | --- |
| `contact_shadow_soft` | `{2, 7, 10, 98}` | small/character contact shadow |
| `contact_shadow_strong` | `{2, 7, 10, 115}` | larger creature contact shadow |
| `road_dust` | `{150, 125, 86, 255}` | footstep dust smear (`Fade(..., dust * 0.42f)`) |
| `footstep_print` | `{67, 59, 48, 116}` | footprint ink mark |
| `robot_chassis_teal` | `{42, 128, 136, 255}` | automaton chassis plate (4 call sites) |
| `robot_chassis_gold` | `{223, 173, 67, 255}` | automaton trim accent (3 call sites) |
| `robot_limb_dark` | `{54, 66, 71, 255}` | automaton limb segment (3 call sites) |
| `robot_skin_bronze` | `{221, 174, 118, 255}` | automaton hand/extremity (3 call sites) |

Every value above is copied verbatim from the literal it replaced -- no
rounding, no new `Fade()`/blend call introduced -- so classic's rendered
output is unchanged; the pixel-identity capture in the PR description
confirms this for six representative scenes.

**Left inline, and why:**

- The `HumanoidContactColor()` table (`{116, 224, 197, 255}` heel,
  `{245, 151, 66, 255}` toe, plus two that already used named ramps) feeds
  only `DrawHeroSkinRigOverlay()`, the developer skeleton overlay gated by
  `draw_hero_rig_debug`. Developer-only, per the task's own carve-out.
  `Fade(HumanoidContactColor(...), ...)` and rig-debug wireframe colors
  elsewhere in `actor_rendering.inc` are the same case.
  - UI chrome in `main.c` (map paper colors, HUD chart ink) already routes
  through `CC_STYLE_*` macros (`CC_STYLE_PARCHMENT`, `CC_STYLE_CONTRABAND`,
  ...); it was already pack-driven before this PR, just not listed as new
  work here.
- Computed/animated colors (e.g. the ember pulse at
  `(Color){255, (unsigned char)(235 + 12 * pulse), ...}`) are math, not an
  art pick, so there is no fixed literal to name.
- The remaining ~140-odd literals (creature/NPC/goods/carriage detail
  colors, most appearing exactly once) are real candidates for the same
  treatment, deferred here to keep this PR's diff reviewable and its
  pixel-identity claim easy to verify by inspection. Repeat the mechanical
  process above -- find an exact, byte-identical literal, name it for what
  it depicts, add the field, add the macro, verify a pixel-diff -- in a
  follow-up.

## Step 3: `sierra_pixel`

`assets/shaders/world_lit.fs` and `assets/shaders/painted_environment.fs`
have carried an `orderedDither4x4()` 4x4 Bayer function since before this PR,
unused by either shader. `sierra_pixel` ships its own copies of exactly
those two files (`assets/stylepacks/sierra_pixel/shaders/`) that:

1. declare `uniform float ditherStrength;`
2. add `artDitherOffset()`, which centers `orderedDither4x4(gl_FragCoord.xy)`
   to -0.5..0.5, and
3. add `color += artDitherOffset() * ditherStrength;` right before each
   shader's final `finalColor = ...` line (`world_lit.fs` has two exit
   points -- the terrain/travel fast path and the general path -- both get
   it).

`gl_FragCoord` is in the *render target's own pixel space*: the 3D scene
draws directly into the low-resolution `local_target`
(`BeginTextureMode(target)` in `actor_rendering.inc`), and the camera is
already snapped to that pixel grid by `SnapCameraToArtPixels`
(`camera_composition.inc`). So the dither pattern is anchored to a fixed
art-texel grid and holds still frame to frame for static geometry, instead
of swimming as the camera moves subpixel amounts on screen -- the effect the
task asked to avoid.

`LoadVisualStyle()` always calls `SetShaderValue(shader, GetShaderLocation
(shader, "ditherStrength"), &pack.dither_strength, ...)` on the world and
painted-environment shaders, for every pack. classic's copies of these two
files are untouched originals with no `ditherStrength` uniform declared, so
`GetShaderLocation` returns -1 and the `SetShaderValue` call is a
documented raylib no-op; there is no per-pack branch in C for this.
`sierra_pixel`'s manifest sets `constants.dither_strength = 0.045` -- roughly
1/22 of full range, applied through a 16-step threshold table -- modest
enough to read as grain/stipple breaking up the palette LUT's quantization
bands (a flatter, more saturated King's Quest feel), not as visible noise.
Every other constant, and the full palette, is identical to classic; only
the two shader files and `dither_strength` differ.

Optional per the task ("Optionally use a flatter, more saturated palette")
and deliberately skipped here: `sierra_pixel`'s palette is byte-identical to
classic's. Recoloring for a King's Quest feel is real work best done by
someone looking at it, not invented here; this PR's job was to prove the
mechanism (a second pack, a real uniform, a real visual change) without
taking on unreviewed art direction.

## Render target size

`CC_LOCAL_ART_WIDTH`/`CC_LOCAL_ART_HEIGHT` (`cc_local_viewport.h`) stay
literal `#define`s -- `tools/art/run_art_check.py` parses them with a regex
expecting `#define NAME <digits>`, and they now double as
`CcStylePackResetToClassic()`'s hard fallback. Everywhere that used to read
those macros for anything other than that Python tool now reads
`CcArtWidth()`/`CcArtHeight()`/`CcArtUpscaleFilter()` instead: two `extern
int32_t`s (`cc_active_art_width`, `cc_active_art_height`, plus the upscale
filter) defined once in `cc_style_pack.c` and set by `CcSetActiveArtViewport
()` from inside `CcStylePackLoad()`. They are `extern`, not a per-translation
-unit `static`, because `main.c` and the local3d renderer's own unity build
(`cc_local3d.c` and everything it `#include`s) are two different
translation units that both need to see the pack's chosen size --
`CcLocalViewportBounds()` (screen-space letterboxing, used for every pointer
/touch/UI hit test) lives in the shared header both include.

What already worked before this change, and needed no edit: the actual
screen<->render-target coordinate math (picking, `GetScreenToWorldRayEx`,
the pony/portrait/mine-scene input paths) already reads `target.texture.
width`/`height` off the real `RenderTexture2D` at each call site, not the
macro -- so it already tracked `MineRenderTargetSize()`'s frame-to-frame
resize correctly. The two things that still read the macro directly, and
needed the switch, were the render target's own allocation in `main.c`
(`LoadRenderTexture`, and the per-frame default size before a mine scene's
`MineRenderTargetSize()` override) and `CcLocalViewportBounds()`'s own
letterboxing math. `MineRenderTargetSize()` itself is untouched and remains
independent of style packs, since it derives its size from the actual window
and UI layout, not the art canvas.

One compile-time-only spot could not simply switch to a function call:
`actor_simulation.inc`'s `StreetPortalProximityScore()` aliased the macros
through `enum { ART_WIDTH = CC_LOCAL_ART_WIDTH, ... }`, which needs an
integer constant expression. Rewritten as two `const int32_t` locals
initialized from `CcArtWidth()`/`CcArtHeight()` -- legal at block scope,
still resolves to the runtime value, and the only place in this codebase
that needed it.

## CLI flag and environment variable

`--style <id>` (highest priority), `CROWNLESS_STYLE` (environment variable),
`classic` (default). Resolved and stripped out of `argv`/`argc` at the very
top of `main()`, before anything else looks at them: most of `main()`'s own
capture/test/benchmark flags read `argv` by fixed position (some branch on
the exact `argc`), not by scanning for a flag name, so `--style` has to be
gone before any of that code runs, or it silently shifts every later
positional argument by two. `--style` can be typed anywhere on the real
command line; after the strip, the rest of `main()` behaves exactly as if it
had never been there.

## Web build

`CMakeLists.txt`'s emscripten path builds a "startup" data package (embedded
in `index.data`, available synchronously when the wasm boots) and a "lazy"
one (individual files fetched over HTTP at runtime, e.g. the language
model). `CcStylePackLoad()` reads `style.json` synchronously, at the top of
`main()`, the same way `LoadVisualStyle()` synchronously loads
`assets/shaders/*.vs`/`*.fs` -- so style packs are a startup asset, not a
lazy one; a lazily-fetched manifest would not have arrived yet when
`CcStylePackLoad()` looks for it. Both `assets/stylepacks/*/style.json` and
`assets/stylepacks/*/shaders/*.vs`/`*.fs` were added to
`CC_WEB_STARTUP_ASSET_SOURCES`, and `cmake/PrepareWebAssets.cmake` (the
script that actually populates the packaged directory, rather than the
dependency list) gained a generic loop -- globbing pack directories instead
of naming them, so a new pack needs no CMake change -- that copies each
pack's `style.json` as-is and rewrites each pack's own shaders from
`#version 330` to `#version 300 es` (plus precision qualifiers), exactly like
the existing loop already did for `assets/shaders/*.vs`/`*.fs`. A pack that
reuses a shared shader via a relative `../../shaders/x` path needs no extra
web handling: that shared file is already staged and rewritten by the
existing loop.

## Future passes (design only -- not built in this PR)

The user's direction for the eventual painted, hand-drawn look: it has to be
part of the *live* render pipeline -- never a pre-rendered plate -- so the
existing 3D motion, parallax and lighting survive. The camera composition in
this game already suits that: most screens hold the camera still and only
pan it as a deliberate transition (the character reaches the screen edge,
the camera moves, then settles) -- see `SnapCameraToArtPixels` and the fixed
`STREET_CAMERA_SHOTS`/room-shot tables in `camera_composition.inc`. That
means the expensive per-pixel filters below do not have to run every frame.

### Per-shot caching

This is why `post_chain` entries already carry a `cache` field
(`per_frame`, implemented; `per_shot`, declared, not implemented). The
design once a pass wants `per_shot`:

- The scene target needs a second attachment for depth (`scene_depth` is
  already a declarable `post_chain` input, unused). Vanilla raylib's
  `RenderTexture2D` only exposes a depth *renderbuffer* for the 3D pass's
  own depth test, not a sampled depth *texture*; giving the shot cache a
  depth buffer it can later read means either an `rlgl`-level custom
  framebuffer with a depth texture attachment, or a second draw of scene
  depth into its own texture. `art_grade_target`
  (`local3d/asset_loading.inc`) is the obvious place to grow a sibling depth
  texture -- it already exists, is already sized to the art canvas, and is
  already where the current single grade pass reads from.
- "Settled" needs a signal: the camera composition functions already know
  whether they are mid-transition (`SnapCameraToArtPixels`,
  `FixedCameraRig`/`CombatCameraRig` state) or holding a fixed shot. A
  per-shot pass reads that same state to decide "paint now" vs. "reuse the
  cache."
- When the camera settles: render the static layer (terrain, buildings,
  parked carriage, anything not currently animating) once, run the
  expensive filters on it, and keep the painted color plus depth.
- Every frame after that: draw only the dynamic layer (walking actors, the
  moving carriage, animated props) and depth-composite it over the cached
  painted background, instead of re-painting the whole frame.
- While the camera is moving (the transition pans above): repaint every
  frame. Some stroke swimming during a pan is the accepted tradeoff per the
  user's direction, not a bug to chase.
- Repaint the cache on a lighting, weather or town-state change (the same
  `ArtAtmosphereState`/atmosphere-blend transitions that already drive
  `context_state.inc` today), since those change the static layer's colors
  even with the camera not moving.

### Techniques for the painterly pass itself

Once caching makes the cost affordable, the pass chain a future pack (e.g.
a "storybook" or "hand-drawn" pack) would declare:

- **World-anchored brush and paper textures.** Sample a paper/canvas grain
  and directional brush strokes in world space (or the art-texel grid, like
  `sierra_pixel`'s dither), not screen space, so texture does not swim as
  the camera pans during a transition.
- **A depth- and normal-aware Kuwahara or oil filter.** Needs `scene_depth`
  (and eventually a normal buffer -- `scene_normal` is already a declarable
  `post_chain` input, also unused) to avoid painting across silhouette
  edges; a plain screen-space Kuwahara would smear foreground/background
  together.
- **An edge-detect outline with a controlled line-boil wobble.** A
  depth/normal edge pass, redrawn on its own cadence (8-12 fps, not every
  frame) so the outline "boils" the way hand-drawn animation does, rather
  than sitting perfectly still (which reads as sterile) or updating at full
  60 fps (which reads as a flat, non-hand-drawn outline shader).
- **Palette quantization plus dithering anchored to texels.** The
  post-chain's existing `grade` pass already quantizes through a perceptual
  LUT (`LoadSharedPaletteLookup`, `asset_loading.inc`); `sierra_pixel`'s
  `ditherStrength` is the first, smallest version of "dithering anchored to
  texels" the schema already supports. A heavier painterly pack would likely
  want a coarser, more visible dither than sierra_pixel's modest 0.045.
- **A camera snapped to the texel grid with a sub-pixel upscale offset**
  (the t3ssel8r approach): the camera is already snapped to whole art
  pixels (`SnapCameraToArtPixels`); the missing half is rendering at the art
  resolution while nudging the upscaled presentation by a sub-pixel offset
  so panning reads as smooth on screen without reintroducing shimmer at the
  art-texel level. This interacts with `render_target.upscale_filter` and
  `PresentTarget`'s draw of `local_target` to the screen
  (`actor_rendering.inc`).

### Other future packs

- **Toy / tilt-shift.** Needs the `scene_depth` input (already declarable)
  and a circle-of-confusion blur pass keyed to it -- a straightforward
  addition to `post_chain` once a depth texture exists, no new schema
  fields required.
- **Cel-shaded cartoon.** Needs an outline pass (the same edge-detect
  building block as the painterly pass above, without the boil or the paint
  filter) plus a harder-stepped `world_fragment`/`painted_environment_
  fragment` shader pair, the same kind of pack-owned shader swap
  `sierra_pixel` already demonstrates.
