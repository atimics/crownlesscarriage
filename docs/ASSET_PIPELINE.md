# Asset pipeline

The art pipeline turns `assets/blender/*.blend` sources into exported GLB
meshes plus JSON manifests, and the client is compiled against the
manifests. This doc covers the parts that are not visible from any single
file: the make-target ordering and the manifest contract.

See `docs/production/blender-asset-system.md` for the detailed asset library,
layer, socket, and authoring contracts.

## Target ordering (the part that matters)

The `blender-*` targets in `Makefile` have real dependency ordering.
Building things in the wrong order silently re-exports stale meshes.

Authoring chain for the hero:

```
blender-hero-assets                 build crownless_hero_components.blend
        + compose preview
blender-hero-assets-check           validate the .blend
blender-hero-animation / blender-hero-actions
        render + encode action animations into crownless_hero_actions.blend
blender-character-engine            export the engine rig GLB (v01)
blender-hero-paint-channels         depends on blender-character-engine;
        re-export + validate paint channels
blender-character-hair-v08          depends on blender-character-engine;
        renders hair + character sheet v08
```

Everything "screen-first" (`render_screen_first_*` / `compose_screen_first_*`
/ `export_screen_first_engine_hero`) works toward the v08 screen-first rig
(`crownless_screen_first_engine_rig_v08.glb`), which is the current hero
export the client uses.

Environment and population chains (independent of the hero chain):

- `blender-assets` / `blender-assets-catalog` / `blender-assets-check` —
  the general asset library (`crownless_asset_library.blend`)
- `blender-economic-assets` — the focused economic GLBs, icon atlas, and
  review sheet
- `blender-painted-market-pilot` — painted market/granary environment
- `blender-npc-assets` + `blender-npc-assets-check` — NPC archetype and
  dynamic-module libraries
- `blender-creature-assets` + `blender-creature-assets-check` — creature
  library; this **also regenerates
  `src/client/cc_creature_catalog.generated.inc`** via
  `tools/blender/generate_creature_catalog.py`. Never hand-edit that file;
  run `make blender-creature-assets` and commit the result.
- `blender-world-kit` + `blender-world-kit-check` — world kit pieces and
  connections. Run `blender-world-kit-review-check` after generation when you
  also want to validate every review image.

`make art-check` is the full validation gate: it builds and tests the
game (`test-play`), runs the pure-Python validators, and then
`tools/art/run_art_check.py`, which launches the client to capture
screenshots and enforces the painterly art thresholds documented in that
script (and in `docs/INVARIANTS.md`).

Generated contact sheets, comparison images, and reels live under the ignored
`assets/previews` directory. They are review output rather than runtime input.
Attach useful outputs to the related pull request or release. Keep only a
small, current gallery under `docs/images`, with every retained file linked
from `docs/ART_GALLERY.md`.

`make art-assets-check` reports the size of tracked art, rejects tracked review
output, and finds binary assets with no reference from the project. The main
tree has a 64 MiB tracked-art budget and an 8 MiB per-file budget.

## Which targets need what

- Need the `blender` binary (override with `BLENDER=...`): `blender-assets`,
  `blender-assets-check`, `blender-hero-assets`, `blender-hero-assets-check`,
  all `render_*`-backed targets, and the `blender-*-engine` exporters.
- Pure Python (fine on any machine): `blender-exports-check`,
  `blender-character-animations-check`, `blender-npc-assets-check`,
  `blender-creature-assets-check`, `blender-world-kit-check`, and
  `blender-world-kit-review-check` after its preview images are generated.
- CI runs the pure-Python checks plus shader compilation with `glslc`
  (`assets/shaders/*.vs` / `*.fs`); it does not run Blender.

## The manifest-to-client contract

The JSON manifests in `assets/` are the contract between the pipeline and
the C client:

| Manifest | Describes |
| --- | --- |
| `asset_manifest.json` | general asset library exports |
| `hero_component_manifest.json` | hero component library |
| `npc_archetype_manifest.json`, `npc_dynamic_module_manifest.json` | NPC libraries |
| `creature_manifest.json` | creature exports + catalog variants/poses |
| `world_kit_manifest.json`, `world_kit_connections.json` | world kit pieces and how they connect |

Two rules to know:

1. **CMake validates manifests at configure time.** The
   `cc_manifest_export_paths` helper in `CMakeLists.txt` fails the configure
   step if a manifest references a missing file. A stale manifest is a
   build error, not a runtime surprise.
2. **Exported paths are compiled in.** On Apple targets the manifests' GLB
   paths are baked into the client as `CC_*_ASSET` defines (see the
   `CC_BUILD_CLIENT` block in `CMakeLists.txt`). Adding an asset means
   manifest + export + reconfigure.

`tools/blender/inspect_glb.py` validates exported GLB files. Two profiles:
`generic` (structure checks) and `library` (every mesh node must carry a
`cc_asset_id` matching the manifest, plus naming and metadata rules). The
library profile is what CI and the pipeline use for shipped assets.
