# Crownless Carriage

A seed-driven carriage-courier fantasy world simulation with a raylib
fixed-pixel, three-quarter 3D client. Authored storybook towns are joined by
a finite carriage road book whose generated corridors and fog reflect what
the player has learned. The core is a deterministic headless simulation in
C17 with SQLite-persisted saves; the desktop and browser
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
| `crownless_agent_courier` | Bounded, journaled interface for an external agent | `--journal PATH [--seed N \| --resume] [--counterfactual]` |
| `crownless_sim_runner` | Run a world for N years, print summary, optionally save | `--seed N --years N --save PATH --detail` |
| `crownless_sim_metrics` | Aggregate multi-seed, multi-year statistics | `--seeds N --years N` |
| `crownless_benchmark` | Performance benchmarks with hard budgets | `--quick --assert-budget --sim-seeds N --sim-years N --agents N --frames N` |
| `make run_benchmarks` | Build and run the benchmark suite | — |

## Agent courier protocol

`crownless_agent_courier` lets an LLM or another process occupy the same
courier role as a human REPL player. It does not expose the simulation
struct. Each turn it writes one JSON object containing the sequence number,
game day, simulation hash, and a bounded observation: the current place,
local talk and rumors, the carried promise and cargo, reachable roads, and
consequences witnessed there. The controller replies with one normal REPL
command on standard input.

Every accepted world action and every time advance goes through the existing
append-only SQLite journal. The journal path must be new unless `--resume` is
used, so a controller cannot silently replace an earlier run.

```sh
printf 'talk 1\naccept 1\ntravel 1\nquit\n' |
  ./out/build/play/crownless_agent_courier \
    --seed 42 --journal /tmp/courier-42.ccsave --counterfactual

./out/build/play/crownless_agent_courier \
  --journal /tmp/courier-42.ccsave --resume
```

Output uses JSON Lines with alternating `observation` and `result` records.
Each result says whether the command was accepted and carries the resulting
state hash. Global economy, kingdom, war, faction, history, and save-control
commands are rejected at this boundary; the courier must act from what could
be known in the fiction. With `--counterfactual`, the process emits one final
record after the session closes. It advances the same seed to the same day
with the company taking no actions, then reports settlement, commitment,
faction, and event-ledger differences between the two branches.

The interactive REPL exposes the same comparison through the `mark` command:
it runs the no-action control on demand and prints the branch differences
(settlements, commitments, factions, and events present in only one branch)
alongside the actual and control state hashes. The agent-courier protocol
remains the automation boundary; `mark` is the player-facing view of the
identical report.

## Automated releases

Versioned releases are built from tags. Update the version in the first
`project(...)` line of `CMakeLists.txt`, merge that change to `main`, then tag
the merged commit:

```sh
git tag v0.1.0
git push origin v0.1.0
```

The tag must use `vMAJOR.MINOR.PATCH`, match the CMake project version, and
point to a commit on `main`. The release workflow then:

1. builds and tests the macOS app;
2. builds the browser version;
3. packages both builds and writes SHA-256 checksums; and
4. publishes a GitHub Release with generated release notes.

Rerunning a release replaces its downloadable files instead of creating a
second release. GitHub Pages continues to publish every merge to `main`.

## Layout

- `src/sim` — the deterministic world simulation (`cc_sim.h` is the data
  model; `world_seed` drives everything)
- `src/story`, `src/metagame` — authored dialogue lines; the REPL layer
- `src/persistence` — save files and the journaled command log (SQLite)
- `src/locomotion` — biomechanics: bones, joints, muscles, gaits, ragdoll
- `src/client` — raylib client, NPC appearance, local places
- `src/world` — finite world manifest, procedural terrain, and bounded chunks
- `web` — browser shell and IndexedDB persistence shim
- `tools` — headless runners, benchmark, and the Blender asset pipeline
- `tests` — one CTest executable per feature area; tests are the executable
  specification
- `assets` — manifests, shaders, and exported GLB meshes

Further technical reading:

- `docs/ASSET_PIPELINE.md` — the Blender make-target DAG and the
  manifest-to-client contract
- `docs/ART_GALLERY.md` — a small gallery of the current visual direction
- `docs/INVARIANTS.md` — determinism, save compatibility, performance
  budgets, and the art contract

## Licensing

Original Crownless Carriage code and creative material are copyright © 2026
the Crownless Carriage contributors. All rights are reserved. Public access
allows source review; other use needs permission from the relevant rights
holder, apart from rights already provided by law.

Third-party components keep their own permissive terms. See `LICENSE` for the
project terms and `THIRD_PARTY_NOTICES.md` for the component inventory, license
texts, and the supplied-media record. Browser and macOS packages include
these notices.

## CI

`.github/workflows/ci.yml` runs four jobs: shipped-asset validation and
shader compilation (Ubuntu), headless build + tests across
Debug/Release x Ubuntu/macOS, a full macOS client build, and an
address/undefined-behavior sanitizer run (which skips the timing-budget
test, since instrumentation makes wall-clock budgets meaningless).
`.github/workflows/web.yml` builds the browser site.
