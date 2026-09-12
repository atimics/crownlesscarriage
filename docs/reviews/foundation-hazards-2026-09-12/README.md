# Foundation hazards from issue #661

PR #682 addresses four remaining findings on main e27996ae:

- Event pin insertion and lookup use bounded probes. A full set switches compaction to direct reference queries, preserving the existing pin decisions.
- `CC_EVENT_KIND_COUNT` sizes the gossip coverage histogram through the newest event kinds. Existing event values stay fixed.
- Town road color uses `profile->road[0].surface`.
- Local label capacity and the four reserved priority slots are explicit. Later journey, witness, and lair additions check capacity too.

Current main already counts the recruitment purse for current-schema worlds. The save schema stays 95.

## Evidence

All 135 core checks passed. Native build and four focused checks passed, including causal history, gossip corpus integrity, and renderer skin rotation. Static analysis passed with one reviewed baseline item. The simulation module compiled for WebAssembly with strict warnings.

The added regression fills all 4,096 event-pin slots, checks every present ID and an absent ID, retries a duplicate, then exercises overflow and direct-query membership. The existing history tests cover retained causal references and save restart.

The parent and changed binaries each ran 32 worlds for 100 years. All 6,400 annual validity checks passed. All 3,200 paired annual hashes matched. `results.json` retains every hash and exit code; `summary.json` records the comparison.

Three paired local benchmark samples produced the same checksum on both binaries. All six exceeded the 90,000 ns/day budget: parent 90,348–93,217 and changed 90,261–92,430. The median difference was about 0.6%. These samples remain in `benchmarks.json`; Linux CI is the budget gate. The default macOS CTest benchmark runs without the budget assertion.

Native runtime checks cover the existing renderer contracts. A town-label screenshot review remains separate visual evidence.

## Reproduce

Build the parent and changed revisions in separate Release worktrees. Compile `probe.c` against each worktree's simulation library, then run:

```sh
python3 docs/reviews/foundation-hazards-2026-09-12/run-study.py /tmp/parent-probe /tmp/current-probe
out/build/headless/crownless_benchmark --quick --assert-budget
ctest --test-dir out/build/headless --output-on-failure -j4
```

The manifest records revisions, compiler, schema, and probe hashes.
