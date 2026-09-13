# Supplied road repair crews

Code 5cc1bee, parent 32e6b65. Release with CC_BUILD_CLIENT=OFF, CC_BUILD_BENCHMARKS=ON, strict warnings as errors. All 81 headless tests passed.

The regression fixture puts 1,000 Wood at the preferred endpoint and a complete smaller kit at the opposite endpoint. Schema 60 uses the complete kit, spends actual materials, and reopens the road through the real daily loop. Schema 59 keeps the old choice and the road stays closed. The supplier and execution use the same material gates.

All 640 annual hashes in the attached legacy probe matched the parent, including schemas 58 and 59. No new saved fields were added; current schema is 60 and SQLite remains 29.

`crownless_sim_metrics --seeds 8 --years 40 --final-only --route-csv routes.csv` completed 320 annual validations. The parent capture is ../route-outages-2026-09-08/routes.csv. Across the eight worlds, final closed roads fell from 62 to 59 of 64. Cumulative physical closures fell from 582,748 to 580,074 route-days; unavailable inhabited connections fell from 586,009 to 583,335 route-days. These are paired whole-policy observations for these seeds.

The remaining closures still require material supply and other recovery work. This change retains the original ranking whenever both endpoints are supplied or both lack a complete kit.

## Funded repair follow-up

Revision 57a2cb2 extends the same schema-60 preference to an owned, inhabited
repair base that can pay the existing crown or local kit cost. The shared funding
check drives selection and execution. A daily-loop fixture uses an owned endpoint's
Wood and Stone, reopens the road, and preserves total coin while schema 59 stays
closed. All 81 tests and the same 640 historical hashes passed again.

The eight-seed, forty-year command above produced the same aggregate outcomes as
the communal change alone. The focused funded fixture proves its conditional
response; this sample showed no additional long-run gain. funded-comparison.json
records all three comparisons.

The complete route CSV is identical to routes.csv.
