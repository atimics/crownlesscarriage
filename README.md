# Crownless Carriage

A seed-driven carriage-courier fantasy world simulation with a raylib
fixed-pixel, three-quarter 3D client. The core is a deterministic headless
simulation in C17 with SQLite-persisted saves; the desktop and browser
clients, locomotion, and asset pipeline are all built on top of that
simulation.

## Quick start

Prerequisites: a C17 toolchain, CMake >= 3.24, SQLite3 development files,
and (for the desktop client) a macOS setup. raylib is fetched and built
automatically at a pinned commit. Blender is only needed for asset work
(see `docs/ASSET_PIPELINE.md`).

```sh
make test-play        # configure + build + run tests (RelWithDebInfo)
./out/build/play/crownless_carriage              # the client
./out/build/play/crownless_metagame_playtest     # headless REPL (type help)
```

Presets: `development` (Debug), `play` (RelWithDebInfo), `release`
(Release), each with strict warnings (`-Wall -Wextra -Wpedantic -Wshadow
-Wconversion`) and warnings-as-errors. The Makefile wraps them:
`configure-play`, `build-play`, `test-play`, and the same for `release`
and `web` (`configure-web` needs the Emscripten SDK).
Headless builds work anywhere: `-DCC_BUILD_CLIENT=OFF`.

Build the browser version with an installed Emscripten SDK:

```sh
emcmake cmake --preset web
cmake --build --preset web
emrun out/build/web/site/index.html
```

The browser build writes a static site to `out/build/web/site`, servable
by any static host. Campaigns live in the browser's IndexedDB storage;
clearing site data clears those campaigns. The `web.yml` workflow builds
pull requests and publishes `main` via GitHub Pages.

## Runnables

| Binary | Purpose | Flags |
| --- | --- | --- |
| `crownless_carriage` | 3D client (macOS bundle on Apple; WASM site on the web preset) | `--screen-first-hero` / `--old-hero` |
| `crownless_metagame_playtest` | Headless world REPL | `--seed N` |
| `crownless_sim_runner` | Run a world for N years, print summary, optionally save | `--seed N --years N --save PATH --detail` |
| `crownless_sim_metrics` | Aggregate multi-seed, multi-year statistics | `--seeds N --years N` |
| `crownless_benchmark` | Performance benchmarks with hard budgets | `--quick --assert-budget --sim-seeds N --sim-years N --agents N --frames N` |
| `make run_benchmarks` | Build and run the benchmark suite | — |

## Layout

- `src/sim` — the deterministic world simulation (`cc_sim.h` is the data
  model; `world_seed` drives everything)
- `src/story`, `src/metagame` — authored dialogue lines; the REPL layer
- `src/persistence` — save files and the journaled command log (SQLite)
- `src/locomotion` — biomechanics: bones, joints, muscles, gaits, ragdoll
- `src/client` — raylib client, NPC appearance, local places
- `web` — browser shell and IndexedDB persistence shim
- `tools` — headless runners, benchmark, and the Blender asset pipeline
- `tests` — one CTest executable per feature area; tests are the executable
  specification
- `assets` — manifests, shaders, and exported GLB meshes

Further reading — only two more docs, covering what the code cannot say
about itself:

- `docs/ASSET_PIPELINE.md` — the Blender make-target DAG and the
  manifest-to-client contract
- `docs/INVARIANTS.md` — determinism, save compatibility, performance
  budgets, and the art contract

## CI

`.github/workflows/ci.yml` runs four jobs: shipped-asset validation and
shader compilation (Ubuntu), headless build + tests across
Debug/Release x Ubuntu/macOS, a full macOS client build, and an
address/undefined-behavior sanitizer run (which skips the timing-budget
test, since instrumentation makes wall-clock budgets meaningless).
`.github/workflows/web.yml` builds the browser site.
