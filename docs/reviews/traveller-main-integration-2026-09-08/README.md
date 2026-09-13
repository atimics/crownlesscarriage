# Traveller draft integration

Code revision 52314d7 integrates main 08b3d13. Simulation schema 60 adds traveller purses and hardship; SQLite version 30 adds their storage contract. Main schemas 58 and 59 retain their name and mine behavior.

Release build with CC_BUILD_CLIENT=OFF, CC_BUILD_BENCHMARKS=ON, strict warnings as errors. The full suite passed 72 tests and caught an old database-version expectation. After updating that expectation, sqlite_round_trip passed its focused rerun; all 73 checks passed across these runs. The CSV test now checks the exact twelve optional fields, complete rows, and nonnegative traveller values.

The attached probe compares main and this revision at schemas 26, 27, 33, 34, 36, 37, 58, 59 with two fixed seeds and 40 annual checkpoints each. All 640 hashes matched. Traveller tests also load and upgrade schemas 57 through 59 with empty purses.

`crownless_sim_metrics --seeds 8 --years 40 --final-only --campaign-metrics` completed with 320 annual validations. Its current-schema endpoint output is current-40.csv. Seed indexes map to uint32(index * 0x9e3779b9). The earlier 128,000-year report remains evidence of its recorded historical revision.
