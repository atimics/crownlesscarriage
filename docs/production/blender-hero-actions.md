# Blender Hero Actions

The self-contained Blender action scene demonstrates five gameplay-readable
motions on the modular Crownless hero: walking, jumping, climbing, swimming,
and sword-and-shield fighting. These authored clips are visual references, not
the runtime motion source. See `engine-hero-rig.md` for the simulation binding.

```sh
blender --background --factory-startup \
  --python tools/blender/build_hero_action_reel.py
python3 tools/blender/encode_hero_actions.py
```

Add `-- --preview` to the Blender command to render one diagnostic key pose per
action. The complete build creates `assets/blender/crownless_hero_actions.blend`,
five individual GIF loops, and a combined action reel. The Blender source keeps
the full 24 fps, 240-frame timeline; review GIFs are sampled at 12 fps.
Use `-- --save-only` to rebuild the `.blend` without rendering the reference
timeline before exporting the engine GLB.

The climbing wall, water plane, sword, and shield are presentation-only props.
The cape is stowed during climbing and swimming to keep those silhouettes clear.
