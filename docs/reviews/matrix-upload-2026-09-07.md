# GPU matrix upload — 7 September 2026

The pinned raylib matrix-array helper uses the C struct's row layout for its
WebGL upload. The single-matrix helper already packs values in OpenGL order.
`cmake/PatchRaylibMatrices.cmake` applies that same packing to every matrix in
an array, for both native OpenGL and WebGL. A 32-matrix stack buffer covers the
game's bone palettes; larger arrays use a checked temporary allocation.

Base: `5cb651dc6c3657b82d5eefedca5a4fbf79064439`. Dependency:
`dbc56a87da87d973a9c5baa4e7438a9d20121d28`. The patch checks the exact source
body and accepts an already-patched copy, so fresh and cached builds agree.

The same C probe calls the actual linked `rlSetUniformMatrices` function on
native OpenGL and WebGL2. A shader transforms `(1,0,0,1)` and writes its result
into a texture. Pixel readback checks X/Y/Z and homogeneous W with one-byte
rounding tolerance. Every case draws a second independently posed actor in
the same frame.

| Case | Original WebGL helper | Patched WebGL | Patched native |
| --- | --- | --- | --- |
| Identity | Passed | Passed | Passed |
| Translation +2 | Failed | Passed | Passed |
| Translation -1 | Failed | Passed | Passed |
| Asymmetric rotation | Failed | Passed | Passed |
| Weighted two-bone transform | Failed | Passed | Passed |
| Larger 33-matrix palette | Failed | Passed | Passed |
| Second actor, all six frames | Failed | Passed | Passed |

[Original WebGL readbacks](matrix-upload-2026-09-07/web-before.json),
[patched WebGL readbacks](matrix-upload-2026-09-07/web-after.json), and
[native results](matrix-upload-2026-09-07/native-after.txt) record the results.
The original-helper control used the pinned original header in the same build
and the same C probe. WebGL ran through Playwright Chromium with software
ANGLE/SwiftShader; native ran on this Mac's OpenGL backend.

The 94-test native suite passed with the patched dependency. The full packaged
browser suite also passed locally. Real-Safari and matched whole-game character
captures remain in #468/#445. The probe verifies the upload operation used by
skinned models; those wider visual checks cover their complete asset paths.

CI enables `CC_BUILD_GRAPHICS_PROBES` for its client and web builds. The native
probe runs under the existing virtual display. The browser probe is uploaded
as a separate test artifact and runs in the single-player browser job. The
published game site retains its regular contents.

```sh
cmake -S . -B build -DCC_BUILD_GRAPHICS_PROBES=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build build --target run_matrix_upload_tests
emcmake cmake --preset web -DCC_BUILD_GRAPHICS_PROBES=ON
cmake --build --preset web
node tests/matrix_upload_browser.cjs out/build/web/matrix-probe browser-results
```
