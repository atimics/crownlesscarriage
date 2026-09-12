# Archive convoy completion

PR #679 advances issue #240 on top of first-leg PR #635.

A reserved convoy can now continue from an intermediate town. Each later leg checks the road, capacity, sponsor, local crew wheat, and toll funds. Books retain their original owner during travel. A viable final destination receives the books and becomes the saved archive seat. The prior scribe roster clears; the abbot keeps the office and characters keep their locations. The daily world step records completion and refunds the remaining escrow.

## Evidence

- 127 core CTest checks passed.
- Native build and 17 focused checks passed.
- Static analysis passed with one reviewed baseline item.
- The convoy module compiled with strict warnings for WebAssembly.
- The final expanded convoy test passed in both builds. It covers two roads, a day at the stop, food shortage, a blocked later road, conservation, original ownership, final viability, saved completion, and a journal restart at the stop.
- 32 worlds at schema 91 passed 100 annual validity checks each.
- 32 worlds at schema 90 matched all 3,200 annual hashes from the first-leg study.

The ordinary-world study exercises default world evolution. Active convoy behavior is covered by the constructed journey tests. Automatic reservation, rescue after permanent road failure, and persistent rival institutions are follow-up work. Native window rendering remains a separate device check.

## Reproduce

Build the strict headless target, then run:

```sh
cc -std=c17 -O2 -Isrc docs/reviews/archive-convoy-completion-2026-09-12/probe.c out/build/headless/libcrownless_sim.a -lm -o /tmp/completion-hash-probe
python3 docs/reviews/archive-convoy-completion-2026-09-12/run-study.py /tmp/completion-hash-probe
ctest --test-dir out/build/headless --output-on-failure -j4
```

`manifest.json` records the implementation, parent, compiler, and executable/library hashes. `results.json` retains each run and exit code. `summary.json` records the aggregate and legacy comparison.
