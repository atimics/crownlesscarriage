# Style pack pixel-identity and sierra_pixel review

Reviews the style-pack seam PR (`client/style-packs`), steps 1-3: classic
must reproduce today's look exactly; `sierra_pixel` (classic plus an ordered
dither) is a genuinely different, second pack. See
`docs/design/style-packs.md` for the manifest schema and design notes.

Base commit compared against: `5efffe2d` (`origin/main`, the commit this
branch was cut from). Binary: `crownless_carriage` from `cmake --preset play`
+ `cmake --build --preset play`, native macOS/arm64.

## Method

Six scenes, captured on both `5efffe2d` and this branch with `--style
classic` (the default), then compared with Pillow (`ImageChops.difference`,
full-image `getbbox()`):

```
crownless_carriage --capture-travel <png>
crownless_carriage --capture-mine-yard <png>
crownless_carriage --capture-character <png>
crownless_carriage --capture-town 0 <png>
crownless_carriage --capture-creatures goblins <png>
crownless_carriage --capture-road <png>
```

## Result: classic is pixel-identical

All six scenes: **zero differing pixels** (`ImageChops.difference(...).
getbbox() is None`) between `5efffe2d` and this branch's classic pack.

The PNG files themselves are not all byte-identical, though the decoded
pixels are: `character.png` and `town0.png` differ in their compressed
`IDAT` chunk length (410,272 vs 410,480 bytes, and similar) between the two
builds, while `mine-yard.png`, `road.png`, `goblins.png` and `travel.png`
match byte-for-byte. This is the PNG encoder producing a different deflate
stream for identical pixels -- not a rendering difference -- and is the same
kind of nondeterminism the task description already calls out for
`--capture-road-arrival`'s ~132 pixels of jitter, just at the compression
layer instead of the render layer here, with the actual pixels unaffected in
every one of the six scenes. Pixel comparison, not file-hash comparison, is
the reliable signal for this claim.

These classic frames are not committed to this directory: they are pixel-
identical to what a checkout of `5efffe2d` already produces, so committing
them would spend this PR's research-artifact budget
(`tools/check_research_artifacts.py`, 2 MiB of new bytes under
`docs/reviews/`+`docs/experiments/`) on bytes that add no information. Their
SHA-256 (of this branch's classic capture, for the record) is:

```
73463d3e91ea3de977f954a6596e9a6ee8b55c9e138f8231fab0d405814b7556  character.png
ca30e6ae9838ae65bf829facf12b29139a6c4ba42ee6b7bb103f79c4c78ae2be  goblins.png
af3da8250d57883637e36e743dd62d7c5e26fa7d0b59f9213a77b2a05d6ee3af  mine-yard.png
d1e1cdc466804605dafeff24f666c39141586a1cddca79fb993b23f04a2c4537  road.png
ce071d1bdbe514dfbea6f69f629e544f4d88ef5d29a43cf765349ab5b5861017  town0.png
1a6f2d78cc172f0d38589c45a08aad3374024c0dec75a0144f70cd98e2b56107  travel.png
```

`tests/style_pack_manifest_tests.c` gives this claim a second, independent
guard: it loads the real `assets/stylepacks/classic/style.json` and asserts
its palette, `hero_ink_strength`, `material_ink` and render target size are
byte-identical to `CC_VISUAL_PALETTE_CLASSIC_INIT` (`cc_visual_style.h`)'s
compiled-in values -- so a transcription mistake in the manifest would fail
CI before anyone captures a frame.

## sierra_pixel

The same six scenes, captured with `--style sierra_pixel`, viewed frame by
frame. Committed here at half resolution (640x380, from 1280x760) to stay
inside the research-artifact budget; the dithering is still clearly visible
at this size.

| File | SHA-256 |
| --- | --- |
| `sierra_pixel-travel.png` | `fba4f221eb375f674453b23a8cc81c2d90ba6674aee87ee9e6f47dbf3c17bb67` |
| `sierra_pixel-mine-yard.png` | `0ff0264b61e609ca1bc1170de562d4788023bbff8707decc06f56dd32719d04e` |
| `sierra_pixel-character.png` | `330ab5ecc10e5ad190fca42e5563ebc9ef1eab0da039cc475162657eddbde3fa` |
| `sierra_pixel-town0.png` | `66354d6c5a6dd28443e0f40be2367014de94e547e19e6ecae6af7b876698c9b2` |
| `sierra_pixel-goblins.png` | `e41cb2702ddb8c497f332fa96830c256b890c02950e1789812544f701fe1664a` |
| `sierra_pixel-road.png` | `29314522e540ac79c6632a7a8a8b6b20729d4bab517f91d46fcd4580321d5fa0` |

All six were viewed with the Read tool before this review was written.
Findings:

- `sierra_pixel-travel.png` shows the effect most clearly: the open-sky
  gradient and grass field, previously flat-banded, now carry a visible
  fine stipple that breaks the palette LUT's quantization steps into
  grain, in the spirit of a King's Quest-era dithered palette.
- `sierra_pixel-town0.png` (the same shot used for the classic comparison
  above) shows the same sky treatment plus a faint stipple on the granary
  wall's flat color fields.
- `sierra_pixel-character.png` and `sierra_pixel-goblins.png` (both
  ground-heavy, close third-person shots) show dithering on the terrain and
  rock materials; character/creature models read the same as classic --
  expected, since `hero_pixel.fs` and `npc_indexed.fs` are unmodified,
  reused originals in this pack (see the manifest in
  `assets/stylepacks/sierra_pixel/style.json`).
- `sierra_pixel-mine-yard.png` (a flatter-lit, higher-resolution UI scene --
  mine scenes use `MineRenderTargetSize()`, not the art canvas) shows the
  effect only faintly, as expected: less gradient banding exists there to
  begin with.
- `sierra_pixel-road.png` (a combat encounter) shows dithering on the road
  and stonework; no artifacts on the UI panels or character sprites, which
  do not go through the two modified shaders.
- No black screens, no shader compile failures (`SHADER: ... compiled
  successfully` for every role in the run log), no visible seams between
  dithered and non-dithered surfaces.

`dither_strength` is `0.045` (`assets/stylepacks/sierra_pixel/style.json`),
applied through a 16-step ordered-dither table -- modest by design; see
"Step 3" in `docs/design/style-packs.md` for why that value and why
`gl_FragCoord`-anchored dithering does not shimmer under camera movement.

## Verification commands

```
cmake --preset play && cmake --build --preset play
ulimit -s 65520 && ctest --test-dir out/build/play --output-on-failure
python3 tools/validate_style_packs.py
python3 tools/check_research_artifacts.py --base origin/main --head HEAD
```

238/238 tests passed, including the new `style_pack_manifest_loading` test
(`tests/style_pack_manifest_tests.c`: a valid pack, an unknown field, a
missing shader role, an unsupported schema version, a pack id with no
`style.json` at all, and the classic-equals-compiled-in-defaults check
above).
