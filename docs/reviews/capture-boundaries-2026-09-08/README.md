# Capture boundaries

Completes the capture extraction implementation for #389, on top of #531,
#532 and #535. Also advances the client-orchestration step in #260.

Window sizing, initial view, atmosphere, menus, frame presentation and special
review scenes now live behind capture hooks. Main uses one capture-mode decision
for startup and persistence. Each frame gets its presentation options from the
harness. The individual capture flags and reel state stay inside the harness.

Journey setup has a shared options interface for benchmarks and captures. The
capture wrapper supplies its fixture options behind the test-build guard. The
benchmark supplies its existing 420/1000 journey position and clock step.

Main shrinks from 1,250 to 1,117 lines in this step. Before the four capture
changes it had 2,253 lines. Benchmark orchestration is the next extraction in
#260.

## Validation

- Strict Release native and web builds passed.
- All 118 Release CTest checks passed, including replay and save checks.
- The exact `ctest --preset play` acceptance command passed all 118 checks.
- The presentation contract covers normal play controls, UX window size/range,
  pause-screen selection and completed-reel presentation.
- Thirteen native UX, road-width and pony captures produced their expected
  dimensions. The three road-width and three pony-scene files match the parent
  bytes exactly.
- `run_roadbook_qa` passed with all 19 PNGs. Route rendering measured 337.3 FPS
  and 5.578 ms p95; network rendering measured 236.1 FPS and 5.728 ms p95.
  Both exceeded their FPS requirements and met their frame-time budgets.
- Heraldry and NPC-front captures produced images and received visual checks.
- `captures.json` records the sizes and hashes of all 34 images.
- Completed web preset `index.wasm`: 5,584,051 bytes with Emscripten 5.0.4.
  The pre-extraction file was 5,638,751 bytes: 54,700 bytes saved before compression.
  This final step adds 686 bytes relative to #535 while simplifying ownership.
- Byte scans found zero `--capture`, `capture_`, or `CcCapture` strings in the
  final WebAssembly. `emnm --defined-only` also found zero capture symbols.

The GitHub checks on this final draft provide the CI-platform capture artifact
verification. Local captures establish the native paths above.
