# Route outage observations

Code 13f2e92, Release, CC_BUILD_CLIENT=OFF, CC_BUILD_BENCHMARKS=ON, strict warnings as errors. Three focused checks passed: daily route classification, CSV interval/cumulative output and observation parity, and nutrition CSV.

`crownless_sim_metrics --seeds 8 --years 40 --final-only --route-csv routes.csv` completed with 320 annual validations. Numeric seeds and rules versions are recorded in each row. Endpoint state and last-year intervals are separate from cumulative totals. Summary totals are route-days across eight worlds.

The fixture covers physical closure, official war borders, abandoned endpoints, smuggler passage, and outage continuity. The CSV test confirms that observation preserves ordinary output, final-only rows match the last annual interval, and cumulative fields equal the sum of intervals.

Unavailable means physically closed or an official road crossing a war border. Player-specific access, repairs attempted/completed, and whole-network isolation remain further diagnostics under #266.
