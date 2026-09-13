# Silverwick tool production

Base: #496 at `664ef31`; original main `289d55e`.

New worlds give Silverwick a capacity of two tools each week. Each tool uses
two local Iron and one local Wood. The existing reserve gap limits the batch.
Every fourth smithy output wears one available tool under the existing rule.
Alderwatch keeps its weapon specialization and obtains tools through the
existing trade and carriage paths. Fully burned works require fire repairs;
abandoned towns require workers. A zero-capacity line reports its capacity
requirement through the shared smithy plan.

## Save policy

Schema 60, generator 25. The new capacity is assigned after randomized economy
setup, preserving its random draw count. Existing saves keep their stored
capacities, including zero and custom values. A future recommissioning action
can reopen a deliberately idle saved line through an explicit player choice.
The plan preserves older fire-damage behavior while replaying pre-60 journals.
Current worlds stop tool and weapon batches at full fire damage.

The production ledger is caller-owned and starts at zero for each capture.
Its totals measure only the captured interval, including when the runner loads
a save. It holds gross output, inputs, tool wear, and counts of each line state.
Abandoned towns skip weekly work; their unavailable state is available through
the read-only plan. Actors gain knowledge through their ordinary observation
and report paths.

## Reproduce

```sh
cmake -S . -B out/build/foundation -DCC_BUILD_CLIENT=OFF -DCC_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/foundation -j 4
out/build/foundation/crownless_sim_runner --seed 0x5eed0001 --years 40 --report-every 40 --smithy
out/build/foundation/crownless_sim_runner --seed 0xc0a71a9e --years 40 --report-every 40 --smithy
```

The runner emits one cumulative `smithy` row per town at each report. State
indices are 0 ready, 1 service required, 2 capacity required, 3 reserve met,
4 Iron required, 5 Wood required, 6 workers required, 7 fire repairs required.

Both commands were repeated independently with identical output. Removing
`--smithy` preserved every ordinary summary and state hash. Annual structural
validation stayed enabled. Build: AppleClang 17.0.0, strict Debug.

| Seed | Silverwick tools | Iron used | Wood used | Tools worn | Alderwatch weapons |
| --- | ---: | ---: | ---: | ---: | ---: |
| `0x5eed0001` | 2410 | 4820 | 2410 | 602 | 18 |
| `0xc0a71a9e` | 2419 | 4838 | 2419 | 604 | 24 |

Final day is 14601. Final hashes and line-state counts are in
[measurements.json](measurements.json). These are production measurements for
two seeded worlds. Wider health and freight policies remain under #266,
#400/#401 and #434.

The material-economy test also runs a matched zero-capacity control for each
seed. It checks positive local output, exact recipe inputs, zero control
output, Alderwatch's weapon role, annual validation, and capture/hash parity.
The schema-58 journal fixture checks a zero Silverwick capacity and custom
market capacity through replay, upgrade, and another current save round trip.

The changed economy exposed a seed-sensitive letter test: its best-informed
writer became an official. The probe now accepts `--diary` to select an
ordinary writer directly, so the seal test exercises its intended condition.

This delivery advances #461, #392 and #394. The player-facing production panel,
full site production digest, and saved-line recommissioning remain follow-up
work.

## Reserve lookup cost

Linux CI measured 50.34 and 50.52 microseconds per simulated day against a
50-microsecond budget. The reserve helper now returns before scanning war
routes for goods whose war allowance is always zero. Five local Release
benchmark runs per version gave median 22.6702 -> 21.9332 microseconds/day
(3.25% lower). Both 40-year reports matched the prior version byte for byte.
The Linux CI jobs verify the budget on their own hardware.

The next timing check remained above budget. The gossip loop now checks whether
an account is already heard before preparing it, and avoids teller lookup when
the current character rule uses an anonymous town account. Five local Release
runs measured a combined median of 21.3450 microseconds/day, 5.85% below the
original 22.6702. Both 40-year reports still match byte for byte; the gossip and
SQLite suites pass. Linux budget verification remains a CI check.

## Main integration

Main `08b3d13` assigns schema 59 to mine visits. This draft now uses schema 60;
schema-59 saves retain their mine state and saved smithy capacities. The earlier
measurements above describe the pre-integration build. Current measurements
are recorded separately in `measurements-schema60.json`.
