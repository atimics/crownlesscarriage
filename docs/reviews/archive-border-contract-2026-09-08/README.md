# Archive border contracts

This draft extends #592 so archive supply can attempt hostile-border journeys. It supports #248 and the supply foundation for #446. Schema 78 stores the contract flag on the existing royal carriage; generator 25 stays current.

Renumber note: the work was originally measured as schema 76 on its parent (75). Main independently shipped schemas 75 (goblin/dragon faction split) and 76 (rot diet) while the archive series waited, and the dispatch rule renumbered to 77, so the border contract renumbered to 78 and the pre-feature reference schema to 77. The measurements below record the original numbering.

A booked archive carriage carries archive_contract during empty pickup and loaded travel. Route planning and movement share its passage rule. The route must have usable capacity and meet the existing road-kind rule. War borders retain their added path cost, reduced capacity, dues, and shipment loss risk. The shared freight system handles each leg, capacity waits, loss, arrival, and fallback unloading.

Parking clears the contract. Validation restricts it to active ordinary freight modes and wheat, tools, or paper cargo. The flag is included in the simulation hash, SQLite save data, and carriage JSON. Existing saves receive a false default, and earlier schema replay uses its original route permissions.

## Validation

- Strict release headless build passed.
- All 116 tests passed. The dispatch test passed again after extending its hostile empty-pickup journal fixture.
- Static analysis passed with one reviewed baseline item.
- A hostile-border quote includes seven crowns of first-leg dues and half route capacity in the controlled direct-route fixture.
- A real multi-leg hostile load completes through the shared shipment system. Its saved contract survives travel, changes the state hash, and clears after the journey ends.
- An empty pickup across hostile kingdoms survives save/load and journal replay through automatic purchase.
- Direct carriage-array comparisons verify the saved flag independently of the hash comparison. An idle carriage with an active contract fails validation.
- A pre-feature fixture (schema 77 after the renumber) removes the new SQLite column and replays a historical journal suffix before upgrade. All supported version pair checks pass.
- Two 40-year pre-feature runs match #592 at 82 annual checkpoints, including complete simulation hashes.
- Two feature-schema runs record 588 archive bookings, 554 arrival outcomes, and 34 losses. Three distinct archive loads are observed on war-border roads. Arrival includes the shared fallback unloading outcome. See measurements.json and probe.c.
- The existing 120-year world balance sweep passes.

## Issue scope

The archive uses the shared active-seat query for booking and the ordinary shipment pool for transport. The dynamic seat and relocation work in #240, and named recruitment in #446, remain further work. This draft supplies hostile passage for #248.
