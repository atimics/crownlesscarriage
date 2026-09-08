# Roadside recovery inspection

Release code 2fa9c5f, parent a04e820. Build options: CC_BUILD_CLIENT=OFF, CC_BUILD_BENCHMARKS=ON, strict warnings as errors. All 75 headless tests passed. Expanded supplier-choice coverage passed its focused rerun.

The attached parity probe compares eight schemas (26, 27, 33, 34, 36, 37, 58, 59), two seeds, and 40 years per seed/schema. All 640 annual hashes matched. Link the probe against each build's libcrownless_sim.a with its src include directory and -lm.

`crownless_sim_runner --seed 0x5eed0001 --years 1 --detail` produced detail.txt. Its roadside rows describe the actual endpoint snapshot and shared execution gates. They show simultaneous material shortages and calendar gates with stable route and settlement IDs.

Focused tests check resource thresholds, labor and supplier selection, tie order, calendar, abandoned endpoints, open roads, war access, unavailable records, and read-only state. A real daily-loop pair reopens the supplied road with an allied border while the war-border control remains closed.

The plan covers communal roadside recovery. Kingdom repair selection, routine upkeep, recolonization, campaign gates, and ritual gates remain separate diagnostic work under #266.
