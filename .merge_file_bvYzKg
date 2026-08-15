# Blender Asset System

The starter library turns the vertical slice's recurring visual needs into
composable assets instead of one-off scenes. Its first priority is legibility at
an isometric camera distance, so silhouettes, state changes, and module choices
are intentionally broad and readable.

## Generated library

- Source library: `assets/blender/crownless_asset_library.blend`
- Runtime exports: `assets/exports/glb/`
- Machine-readable catalog: `assets/asset_manifest.json`
- Preview renders: `assets/previews/`
- Complete review catalog: `assets/previews/catalog/`
- Rebuild script: `tools/blender/build_asset_library.py`
- Validation script: `tools/blender/validate_asset_library.py`

The `.blend` file is generated rather than edited as an opaque source of truth.
Change the builder, rebuild, and commit the builder plus regenerated outputs.

## Layer model

The library has five authoring bands:

| Band | Purpose |
| --- | --- |
| `00_GUIDES` | Shared socket empties and orientation guides |
| `10_CARRIAGE_CORE` | Stable vehicle body and hitch geometry |
| `20_CARRIAGE_MODULES` | Optional capacity and opportunity modules |
| `30_ENVIRONMENT_KITS` | Neutral route, bridge, mine, and market structures |
| `40_STATE_LAYERS` | Additive visual projections such as shortage or recovery |

`90_PRESENTATION` contains only the camera, lights, and preview ground. It is
never included in a runtime export.

View-layer presets demonstrate supported compositions. For example,
`CC_Market_Shortage` combines the neutral market/granary kit with the food
shortage dressing. `CC_Carriage_Armoured` combines the carriage core with only
the armoured-body module.

## Asset contract

All runtime assets follow these rules:

1. One meter in Blender equals one world meter.
2. `+Z` is up and `+X` is vehicle forward.
3. Exportable collections use a stable `CC_*` name and carry `cc_asset_id`,
   `cc_asset_kind`, `cc_layer_group`, and `cc_library_version` properties.
4. Exportable objects carry the same asset ID plus a semantic `cc_role`.
5. Materials use the shared `MAT_*` palette.
6. Carriage modules are authored in carriage-local coordinates and declare one
   or more compatible socket types.
7. State dressing is additive. It must not duplicate or alter the neutral base
   kit, so strategic state can swap without replacing the whole location.
8. GLB names come from stable asset IDs rather than Blender display names.

## Carriage sockets

The base exports socket empties for `roof`, `rear`, `side_left`, `side_right`,
`underbody`, and `interior`. A new module should be modeled at the compatible
socket's world position in the source library, declare that socket on its
collection, and remain independently exportable.

The starter modules include cargo rack, armoured body, medical bunk, passenger
bench, hidden compartment, scout perch, monster cage, relic containment, and a
diplomatic document safe. The geometry is deliberately low-poly blockout art;
the collection and metadata contract is suitable for later production meshes.

## Environment and state composition

Environment kits are neutral structural shells:

- Straight road segment
- Bridge checkpoint
- Mine entrance
- Market and guarded granary

State layers project simulation conditions without rebuilding those shells:

- Food shortage: empty baskets, ration control, and a barred granary
- Harsh enforcement: barricade, search table, and confiscation crate
- Market recovery: stocked stalls, full sacks, and modest celebration dressing

This directly supports the design rule that strategic variables affect visible
places while keeping the number of authored city grammars bounded.

## Rebuild and validate

From the repository root:

```sh
make blender-assets
make blender-assets-catalog
make blender-assets-check
```

The build starts from Blender factory settings, regenerates every collection,
exports every asset, writes the manifest, renders the previews, and saves the
compressed `.blend` file. Validation reopens that file and checks stable IDs,
sockets, exports, and view-layer presets.

## Adding an asset

1. Add one builder function that creates a leaf collection through
   `new_collection`.
2. Use the shared primitive helpers or replace their meshes with authored mesh
   creation while preserving origin, scale, and metadata.
3. Register the builder under the correct band in `build_assets`.
4. Add a view-layer composition or preview when the asset introduces a new
   state combination.
5. Rebuild and run validation.

When replacing blockout geometry, keep the collection name and `cc_asset_id`
stable. Increment the suffix only for an intentionally incompatible runtime
asset.
