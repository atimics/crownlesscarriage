# Capture request extraction

This first step for #389 moves 363 lines of capture argument parsing into
`src/client/cc_capture_request.inc`. A typed request holds the 74 scene options.
The native test build keeps the existing flags and validation. Builds with
`BUILD_TESTING=OFF` use a zeroed request, so normal startup selects play mode.
Scene setup and frame capture hooks remain in `main()` for a later extraction.

## Evidence

- Release client build with strict warnings passed.
- All 115 CTest checks passed, including the new capture request contract.
- A native `--capture-title out/capture-request-title.png` produced a 1280 by 760
  image. Visual review confirmed the title menu and world background.
- Both parent 610e16a and this branch used the web preset, Emscripten 5.0.4,
  and the same cached raylib source.
- Parent `index.wasm`: 4,201,041 bytes; extracted version: 4,166,951 bytes.
  Reduction: 34,090 bytes (0.81%), before compression.
- A byte scan found 54 distinct `--capture` flags in the parent WebAssembly
  and zero in this version. This measures flag removal; it does not establish
  removal of every capture helper.

The request test covers normal startup, plain and UX output paths, UX range
validation, storybook settings and finite values, the road-arrival alias,
and resetting a reused request. Existing client tests cover startup and input.
The CI capture jobs provide the broader scene artifact check.
