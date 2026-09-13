# Archive volume rules extraction

Issue #260. Draft #627, based on #626.

Parent: `99a25b69f6ea01bf586447c41753dfbf00dc4a26`.
Implementation: `dad6d7d34c40edc5695b326be109e63812bc72f7`.

`cc_archive_volumes.c` owns archive title classification, live book checks,
physical lore totals, save-upgrade lore counting, age ordering, and four-book
selection for binding. Archive work, decay, custody changes, and validation
share its internal interface. The public lore queries remain in `cc_sim.h`.

The moved function bodies match the parent exactly after interface renames.
Ordering stays creation day followed by array slot. Schema 86, generation 25,
and the saved fields stay the same.

## Local validation

- Strict headless build and all 125 tests passed. These include saved-world,
  journal replay, custody, archive work, and performance checks.
- Strict desktop build and 15 focused archive, quest cast, persistence, and
  royal carriage tests passed.
- Emscripten 5.0.4 browser build with graphics probes passed.
- Browser incremental startup and lazy asset refresh passed.
- Static analysis passed with one reviewed baseline item.
- Thirty-two seeds ran for 100 years in each source version. All 3,200 annual
  hashes match. Every annual state validates in both versions.

Desktop checks here cover compilation and automated tests. A fresh native
window capture remains part of the wider #260 acceptance review.

## Reproduce the comparison

Build the headless simulation library in the parent and extracted worktrees.
Compile `probe.c` against each source tree and its matching library:

```sh
cc -O2 -I<PARENT>/src probe.c <PARENT>/out/build/headless/libcrownless_sim.a -lm -o /tmp/volume-parent-probe
cc -O2 -I<EXTRACTED>/src probe.c <EXTRACTED>/out/build/headless/libcrownless_sim.a -lm -o /tmp/volume-fixed-probe
python3 check-parity.py /tmp/volume-parent-probe /tmp/volume-fixed-probe
```

`parity.json` records the shared annual hashes. The script requires successful
runs and equal full result records before writing the report.
