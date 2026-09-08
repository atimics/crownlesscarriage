# Render benchmark orchestration

Continues #260 after the capture extraction in #538. A single benchmark state
now owns argument parsing, world and scene setup, warmup, measured frames, and
reporting. Main shrinks from 1,117 to 935 lines. The six scene names, report fields,
60-frame warmup, FPS/p95 limits, and model checks keep their existing behavior
for valid runs.

The FPS parser now requires a finite positive value. A live comparison with
parent 629b639 proves that `--benchmark-render 1 nan roadbook-route` previously
returned success (0); this branch rejects the argument (1). Frame-count parsing
also checks overflow. Reporting requires a positive finite elapsed time and a
positive measured frame count. The animation clock adds converted frame values
so the largest accepted count stays within defined arithmetic.

## Evidence

- Strict Release native client and web builds passed.
- All 119 CTest checks passed, including replay and save tests.
- The new benchmark contract verifies option values, exactly 60 warmup frames,
  measured frame count, FPS/p95/model limits, finite inputs, overflow, invalid
  elapsed time, and resetting benchmark state for normal startup.
- All six live benchmark scenes completed 120 measured frames and passed their
  model checks. These scene smoke runs use a 1 FPS / 1,000 ms p95 floor/ceiling;
  `scenes.txt` records their reports.
- The existing `run_roadbook_qa` target also passed. It produced all 19 captures
  and met its real rendering gates: route 335.2 FPS / 5.880 ms p95 against
  25 FPS / 40 ms; network 443.7 FPS / 3.876 ms p95 against 21 FPS / 45 ms.
  `roadbook.txt` records those reports.
- `invalid-fps.json` records the parent/current NaN comparison.

Simulation and persistence implementation files are unchanged. Journey rules
are the next extraction in #260.
