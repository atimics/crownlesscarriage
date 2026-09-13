# Capture scene extraction

Continues #389 on top of #532. Capture fixtures now run through named hooks
at the existing startup boundaries: world placement, dungeon setup, world
entry, departure, local scene, UX, and opening message. Saved-session recovery
keeps its place between departure setup and local scene setup.

`main()` shrinks from 1,822 to 1,250 lines. The scene file also holds the
capture-only action/gameplay setup and pony-interface review helper. The shared
reel selector moves into the guarded reel file. Failed capture setup now shares
renderer, texture, window, and instance-lock cleanup at the call site.

The shared journey setup used by capture and rendering benchmarks, window
options, and render-selection flags remain for the next pass.

## Evidence

- Strict Release native client build passed.
- All 117 CTest checks passed after the final helper extraction.
- The new scene contract checks normal simulation state, requested town
  placement, and entry into the underroad fixture.
- Thirteen native review captures produced PNGs at their expected sizes.
  Eight match the parent image bytes exactly. Conversation and trade images
  received visual checks. The other five captures differ in bytes; the evidence
  records those differences rather than treating all images as identical.
- `run_roadbook_qa` passed and produced all 19 PNGs. Both rendering budgets passed:
  route 520.0 FPS / 3.827 ms p95; network 389.8 FPS / 4.887 ms p95 on this Mac.
- Final web preset build passed with Emscripten 5.0.4. Completed `index.wasm`:
  5,583,365 bytes, 62 bytes above the parent. This step improves code structure.
  Across the three capture changes the completed file is 55,386 bytes smaller
  than the pre-extraction file (5,638,751 bytes).
- The final WebAssembly byte scan found zero `--capture` flags.

`captures.json` records the review image sizes and parent comparisons.
`roadbook.json` records the 19 road-book image sizes and hashes.
