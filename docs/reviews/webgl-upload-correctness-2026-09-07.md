# WebGL buffer correctness — 7 September 2026

The game now uses WebGL2's buffer operations directly. GPU readback found five
incorrect results with the previous upload cache. All six cases pass with the
direct path. This resolves the cache portion of #468; #419 owns later measured
traffic improvements.

Base: `5cb651dc6c3657b82d5eefedca5a4fbf79064439`, raylib
`dbc56a87da87d973a9c5baa4e7438a9d20121d28`, local Emscripten 5.0.4.
Both packaged builds used the same base and opening scene at 1280x900,
Playwright 1.62.1 Chromium, software ANGLE/SwiftShader on macOS. Audio reached
playing state before each four-second frame sample. Each run used a fresh
browser context. The direct build removes the cache; it retains the same C,
assets, shaders and other JavaScript.

| GPU readback case | Cache | Direct |
| --- | --- | --- |
| Explicit zero length means the rest of the source | Failed | Passed |
| Initial data plus partial writes and an unwritten prefix | Failed | Passed |
| Write through COPY_WRITE_BUFFER | Failed | Passed |
| Copy through COPY_WRITE_BUFFER | Failed | Passed |
| Typed-array view and source offset | Passed | Passed |
| Reallocation followed by partial writes | Failed | Passed |

[Cached readbacks](webgl-upload-correctness-2026-09-07/wrapped-buffers.json)
and [direct readbacks](webgl-upload-correctness-2026-09-07/direct-buffers.json)
record actual and expected bytes and GL errors. The same cases now run after
the packaged game starts in `tests/browser_tests.cjs`, so CI exercises the
prototype methods installed by that build.

| Opening-frame measurement | Cache | Direct |
| --- | ---: | ---: |
| Frames sampled | 145 | 138 |
| Upload calls / frame | 99.93 | 157.94 |
| Upload bytes / frame | 2,609,064 | 3,326,930 |
| Draw calls / frame | 341.21 | 343.71 |
| Median frame interval, ms | 25.10 | 25.80 |
| p95 frame interval, ms | 33.40 | 34.10 |
| p99 frame interval, ms | 33.90 | 34.30 |

[Cached frame report](webgl-upload-correctness-2026-09-07/cached-frame-budget.json)
and [direct frame report](webgl-upload-correctness-2026-09-07/direct-frame-budget.json)
also contain vertex and binding counts. These are single short software-renderer
samples. Device speed, GPU memory and real-Safari reload behavior require their
own measurements under #445.

The browser upload ceilings move from 130 calls / 3 MiB to 190 calls / 4 MiB
per frame, with headroom over the measured direct path. Draw, vertex, texture
and program ceilings retain their previous values. Frame intervals are reported
for comparison. Correct GPU bytes are the acceptance rule for upload behavior.

Validation: both complete packaged-browser runs passed the desktop/mobile,
menus, saves, reload, shaders and layout checks. The direct run also passed all
six GPU buffer cases. Commands:

```sh
emcmake cmake -S . -B out/build/web -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DCC_BUILD_CLIENT=ON -DCC_BUILD_BENCHMARKS=OFF \
  -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/web -j 4
node tests/browser_tests.cjs out/build/web/site browser-results
```
