# Capture harness acceptance

Issue #389 is ready for closure on the audited main commit 4f7635cc3a115492470a60ef9223dab0d3c859db. This audit checks the existing extraction and its build artifacts.

The request parser (38d8752), frame hooks and reel helpers (4cd1281), scene hooks (a097f58), and final capture boundaries (629b639) are all included in this commit. The harness lives in the cc_capture files under src/client. Test builds enable the capture paths; the web preset selects the play defaults.

## Acceptance evidence

| Requirement | Verified evidence |
| --- | --- |
| Play tests pass | The exact play preset is built and tested in this audit. See play-tests.txt. |
| CI capture steps produce artifacts | The completed Linux client and sanitizer jobs in run 34270839992 produced the required UX, pony, road-width, and opening captures. Nine downloaded artifact groups contain 283 PNGs. Every PNG passes its chunk checksums and image-stream check. artifacts.json records dimensions, byte sizes, and hashes. |
| Web capture code is removed | The completed web run 34270839978 artifact has zero occurrences of --capture, capture_, and CcCapture. Its 77 defined symbols include zero capture symbols. The current index.wasm is 5,652,708 bytes. |
| Extraction reduces bundle size | The retained builds for 610e16a and 629b639 both used Emscripten 5.0.4. Direct file checks confirm 5,638,751 bytes before and 5,584,051 after: 54,700 bytes saved before compression. Later game changes account for the separate current artifact size. extraction.json records hashes and compiler paths. |
| Main is shorter | Direct source counts show 2,253 lines before extraction, 1,117 after extraction, and 944 in the audited commit. |

The CI head 540dd3f and audited main have the identical source tree cc7b3644ceb8838b2f4f5f9321e4e33092608bb0. The completed native jobs pass 149 Linux tests and 151 macOS tests. Their capture artifacts therefore cover the exact source files audited here. The current web and browser jobs also pass.

A visual check of the downloaded trade capture confirms the rendered shop, goods list, quantity controls, and footer. The image checks cover the full artifact set.

- [Native tests and capture artifacts](https://github.com/atimics/crownlesscarriage/actions/runs/34270839992)
- [Web build and browser checks](https://github.com/atimics/crownlesscarriage/actions/runs/34270839978)
- [Original extraction evidence](../capture-boundaries-2026-09-08/README.md)

## Local reproduction

The local build uses the play preset with the existing raylib source cache:

```sh
cmake --preset play -DFETCHCONTENT_SOURCE_DIR_RAYLIB=/Users/ratimics/develop/crownless/out/build/development/_deps/raylib-src
cmake --build --preset play
ctest --preset play
```

Symbol inspection uses emnm --defined-only on the downloaded index.wasm. artifacts.json covers all downloaded PNGs and the current linked WebAssembly. extraction.json covers the matched historical build pair.
