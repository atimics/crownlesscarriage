# Capture frame hooks

Continues #389 on top of the request parser in #531.

`cc_capture_frames.inc` owns frame input, reel clocks, overlay drawing,
screenshot timing, and completion reports. `CcCaptureState` holds the walk
poses and reel counters. Normal builds compile the hook calls to play defaults.
`cc_capture_reels.inc` holds 14 helpers behind `CC_CLIENT_SELF_TESTS`.
All 14 extracted helper definitions match their parent definitions exactly.
The helper file contains 671 lines. Capture scene setup is the next extraction.

## Validation

- Strict Release native client build passed.
- All 116 CTest checks passed after the final helper extraction.
- The frame contract checks warmup, still-image delays, output filenames,
  eight-frame walk cycles, 45-frame creature reels, completed action/gameplay
  reels, and idle capture state during normal play.
- Thirteen native review capture commands produced PNG files with the expected
  sizes. `captures.json` records their dimensions and file sizes. The set covers
  the seven CI UX cases, three road widths, and three pony scenes.
- Visual inspection covered the conversation and 1040-pixel road captures.
- A native walk-cycle capture produced exactly eight images, ending at `07`.
- The web preset completed with Emscripten 5.0.4 and the same raylib source as
  the parent. Final `index.wasm` sizes: parent 5,585,950 bytes; this branch
  5,583,303 bytes. This step saves 2,647 bytes before compression.
- The final WebAssembly contains zero `--capture` flags. Compared with the
  pre-extraction parent of #531 (5,638,751 bytes), the two steps save 55,448 bytes.

The measurements use completed linked files. Emscripten writes an earlier
intermediate WebAssembly file during linking; the completed artifact provides
the release size.
