# Journey runtime and arrival

Continues #260 on top of #548 (parent 9d4414a).

`cc_journey_runtime.c` owns travel ticks, watch stops, ambush warnings and
resolution, and arrival. The public runtime function forwards to one internal
entry point. The simulation supplies its existing event, gossip, discovery,
bandit lookup, courier delivery, and delayed-echo services through an immutable
table. The journey module controls their call order.

All six moved function bodies match the parent after service-call substitution.
Watch strain, daily simulation, mine branches, pony encounters, and carried
courier/treasure updates keep their existing order. Departure, choices,
encounters, queries, and runtime now each have a journey owner.

## Validation

- Strict Release native and browser builds passed.
- All 119 CTest checks passed, including travel, loot, replay, and shipped-save
  fixtures.
- Local Cppcheck 2.20.0 passed the repository gate.
- `runtime_probe.c` produced 8,640 identical state rows from separately linked
  parent and current libraries. It covers two seeds, nine schema modes from
  26 through 60, every initial route in both directions, all three paces, and
  ten prepared variants after real departure commands.
- Boundary cases include the next watch, warning/resolution thresholds,
  arrival, day rollover, stopped travel, tick overflow, and pony encounters.
  The comparison includes 1,728 arrivals and 864 ambush resolutions. Arrival
  fixtures carry a courier bound for the destination and a player-owned
  treasure. Each comparison uses the full simulation hash.
- `parity_probe.c` produced 720 matching annual state hashes across two seeds,
  nine schema modes, and 40 years per run. `parity.json` records both digests.
- All 19 native road-book captures passed. Both rendering gates passed:
  route 436.4 FPS / 4.867 ms p95; network 308.4 FPS / 5.888 ms p95.
  `roadbook.txt` records capture names and performance reports.

## Reproduce the comparisons

Compile the runtime probe with `-std=c11 -Isrc -Itests` and the annual probe
with `-std=c11 -Isrc`. Link each against the corresponding build's
`libcrownless_sim.a` and `-lm`. Run parent and current binaries and compare their
text output byte for byte.
