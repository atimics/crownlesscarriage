# Automatic archive convoy decisions

PR #680 advances issue #240 on top of #679.

Schema 92 adds a weekly world decision. The existing relocation quote checks the saved failure period, viable destination, living sponsor, book custody, carriage at the origin, road capacity, funds, and crew wheat. A successful decision reserves real resources and records the sponsor. The daily convoy step then runs the saved journey. Ended orders return unused resources through the existing refund limits.

The focused convoy test covers healthy-seat stability, weekly timing, the failure period, legacy behavior, duplicate prevention, named events, conservation, sponsor death, road loss, and journal restart on the automatic departure day. Other convoy tests cover later roads, stop supplies, arrival viability, and saved seat completion.

The ordinary-world study checks 32 generated worlds over 100 years at schema 92 and compares 3,200 annual schema 91 hashes against the completion study. Active automatic departure is exercised by the constructed journal fixture. The ordinary-world study measures validity and replay parity; relocation frequency is a separate measure.

Persistent rival institutions, carriage travel to the collection town, and rescue after permanent road failure remain follow-up work. Native window rendering remains a separate device check.

## Results

- All 127 core checks passed.
- Native build and 17 focused checks passed.
- Static analysis passed with one reviewed baseline item.
- The changed simulation module compiled for WebAssembly with strict warnings.
- All 32 current worlds passed 100 annual validity checks.
- All 3,200 schema 91 annual hashes matched the parent study.

## Reproduce

Build the strict headless target, then run:

```sh
cc -std=c17 -O2 -Isrc docs/reviews/archive-convoy-initiation-2026-09-12/probe.c out/build/headless/libcrownless_sim.a -lm -o /tmp/initiation-hash-probe
python3 docs/reviews/archive-convoy-initiation-2026-09-12/run-study.py /tmp/initiation-hash-probe
ctest --test-dir out/build/headless --output-on-failure -j4
```

`manifest.json` records the implementation, parent, compiler, and executable/library hashes. `results.json` retains each run and exit code. `summary.json` records the aggregate and legacy comparison.
