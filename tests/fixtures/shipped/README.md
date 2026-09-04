# Shipped save fixtures

These SQLite files were written by the listed commit. Each snapshot uses seed
42 and advances the world by one year. The test suite loads every file with
the current reader and checks the migrated state.

| Schema | Generator | Source commit |
|---:|---:|:---|
| 2 | 2 | `9867457cd8b6` |
| 3 | 3 | `64d3ea47b716` |
| 4 | 3 | `22e546c98c6c` |
| 5 | 4 | `da20f62d684a` |
| 9 | 9 | `c3ecfdad77ed` |
| 11 | 11 | `8ef88d55069b` |
| 12 | 12 | `8155b0717133` |
| 13 | 13 | `fcdbd134d32a` |
| 14 | 14 | `282f062494d0` |
| 15 | 15 | `b2fecf49dcac` |
| 16 | 15 | `91ed1b0375a5` |
| 17 | 16 | `e15513228708` |
| 18 | 16 | `89687011682e` |
| 18 | 17 | `5a76dae9c2d2` |
| 19 | 18 | `c403dfd376c0` |
| 20 | 19 | `64ff57f0e0a3` |
| 21 | 20 | `5c92c3001314` |
| 22 | 20 | `81f286e9a981` |
| 23 | 20 | `a3c2070c8b4d` |
| 24 | 20 | `98bb035e4654` |
| 25 | 20 | `42586e17466b` |
| 26 | 21 | `4213ecf3bcd9` |
| 27 | 21 | `e677f0bf0f4a` |
| 27 | 22 | `5062afcf8861` |
| 28 | 22 | `502100425a88` |
| 29 | 23 | `3e5e251bd757` |
| 30 | 23 | `2071283254b0` |
| 31 | 24 | `f060e1467c70` |
| 32 | 25 | `3b55661` |
| 33 | 25 | `e18ce0a` |

The two schema 21 journal files were written by commit `8c34d1b`. One records
`ADVANCE_DAYS` from day 1 through day 7. The other records
`ADVANCE_RUNTIME_TICKS` during travel and crosses from day 6 to day 7.

To create a snapshot, check out the source commit and run:

```sh
cmake -S . -B out/fixture -DCC_BUILD_CLIENT=OFF -DBUILD_TESTING=OFF
cmake --build out/fixture --target crownless_sim_runner
out/fixture/crownless_sim_runner --seed 42 --years 1 \
  --save schema-SCHEMA-generator-GENERATOR.ccsave
sqlite3 schema-SCHEMA-generator-GENERATOR.ccsave \
  'PRAGMA journal_mode=DELETE;'
```

The final command makes read-only test runs sidecar-free. It leaves the saved
simulation data and its stored state hash unchanged.
