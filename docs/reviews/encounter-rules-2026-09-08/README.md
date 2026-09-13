# Journey encounter rules

Continues #260 on top of #547 (parent 1bf0856).

`cc_journey_encounter.c` owns combat, negotiated passage, provision offers,
withdrawal, and loot rolls. The public simulation dispatcher calls one internal
entry point for the four encounter commands. An immutable service table supplies
existing random draws, event recording, entity lookup, treasure allocation, and
journey traffic creation. Those shared services keep their current owners.

All four moved function bodies match the parent after service-call substitution.
Loot random draws, event order, payments, and the return to travel keep their
existing order. Runtime transitions are the next journey boundary.

## Validation

- Strict Release native and browser builds passed.
- All 119 CTest checks passed, including loot, simulation, journal replay,
  shipped-save fixtures, and travel tests.
- Local Cppcheck 2.20.0 passed the repository gate.
- `encounter_probe.c` produces 23,976 matching rows: 16,128 command results
  and 7,848 runtime transitions after accepted choices. Both separately linked
  libraries agree on error text and full state hashes. Fixtures start with an
  accepted relief delivery and a real departure that reaches a blocked road.
- The probe covers 32 seeds, nine schema modes (26 through 60), seven command
  choices, and eight prepared variants. Cases include empty funds, full cargo,
  absent bandits, alternate journey phase, mismatched origin, stocked provision
  offers, and the treasure limit. A prepared double-six random state exercises
  288 trophy awards. The remaining cases retain their generated random state.
- `parity_probe.c` produces 720 matching annual state hashes across two seeds,
  nine schema modes, and 40 years per run. `parity.json` records both digests.
- All 19 native road-book captures passed. Both rendering gates passed:
  route 510.0 FPS / 4.463 ms p95; network 434.4 FPS / 4.211 ms p95.
  `roadbook.txt` records capture names and rendering reports.

## Reproduce the comparisons

Compile the encounter probe with `-std=c11 -Isrc -Itests` and the annual probe
with `-std=c11 -Isrc`. Link each against the relevant build's
`libcrownless_sim.a` and `-lm`. Run parent and current binaries and compare their
text output byte for byte.
