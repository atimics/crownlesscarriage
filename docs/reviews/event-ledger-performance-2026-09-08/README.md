# Event ledger performance

The full event ledger is usually in time order. Compact it in place and use scratch space for rotated ledgers. Parent links and pinned records keep their existing rules.

Release builds on the same local Mac, nine alternating pairs of `crownless_benchmark --quick`: median CPU time fell from 21,915.6 to 17,324.7 ns/day (20.9%). Every benchmark checksum matched. Raw measurements are in timings.json. Linux CI measures its own 50,000 ns/day budget.

All 72 headless tests passed on main-based commit 7d86821. The same code passed all 84 tests on backlog base 236d3bc. Rotated ledger tests cover three offsets and schemas 26, 44, and current. The attached probe compared 640 annual state hashes across eight schemas, two seeds, and 40 years; all matched.

A three-second sample of the backlog baseline placed PushEventRecord first among sampled top-of-stack functions (1,011 samples). Its ledger compaction copied the full ledger twice on each append. The backlog quick benchmark median fell from 20,963.2 to 17,253.6 ns/day over nine alternating pairs, with matching checksums. These local measurements describe these runs.
