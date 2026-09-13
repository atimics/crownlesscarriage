# Archive seat selection

This draft adds the shared placement rule for #240, based on #593 at e2ab591. Schema 76 and generator 25 stay current. It provides candidate inspection, healthy-seat retention, and deterministic replacement advice for the later saved relocation step.

A candidate qualifies when its town is inhabited, has a mill, and holds paper, tools, and two spare wheat after the local food reserve. A viable current seat has priority. Otherwise, the rule selects the highest-scoring viable mill and resolves equal scores by lower town ID.

Scores use these bounded terms:

- Paper: four points per unit, up to 16 units.
- Spare wheat: two points per pair, up to 26 pairs.
- Tools: eight points per unit, up to four units.
- Road access: six points per distinct reachable neighboring town, up to four towns.
- Town security: one point per five security units.
- Monastic patronage: 20 points for the kingdom's living named patron with a home in that kingdom.

Road access uses the physical capacity and road-kind rules available to archive contracts. Closed roads can count when they retain usable capacity. Each neighboring town counts once. Current stocks establish readiness; the later failure-period rule will handle temporary supply gaps.

The archive_seat_plan JSON report exposes current and advised seat IDs, the healthy-seat decision, and every candidate's score inputs. The actual archive continues to use its existing seat rule while saved identity, sustained failure, and relocation are implemented.

## Validation

- Strict release headless build passed.
- All 117 tests passed.
- Static analysis passed with one reviewed baseline item.
- Fixtures cover healthy-seat priority over a richer town, failed supplies, mill eligibility, ruins, missing candidates, stable ties after town reordering, distinct road neighbors, closed and destroyed roads, and living patron identity.
- Every selection fixture checks that the whole simulation stays unchanged.
- Placement reports match after save/load.
- Two 40-year runs preserve every existing JSON field across 82 checkpoints, including simulation hashes. Only archive_seat_plan is removed for comparison.
- Five annual samples keep a viable current seat. The other 77 need further supplies before a mill qualifies. These samples identify supply readiness as a key part of the future relocation work. See measurements.json.

Saved seat identity, a sustained failure clock, named relocation or rival foundation, and physical book journeys remain the next parts of #240.
