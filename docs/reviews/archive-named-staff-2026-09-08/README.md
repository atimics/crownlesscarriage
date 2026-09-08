# Named archive staff and a first task

This draft continues #446 and #470 on top of #613. Schema 81 adds a bounded
staff register. The generator stays at 25 and SQLite at 32. `CcSim` is 184448
bytes.

## Appointment and physical work

The day after paid training, a qualified recruit can join a supplied archive
with a vacant staff place. The first task uses two reserved wheat, one paper,
and the existing tool-wear rule. Remaining supplies return through the existing
refund path. The operation prepares a complete world copy before committing it,
so a failed appointment preserves the original world.

A surviving volume owned and held by the archive town receives an index page.
Its identity and custody stay with that volume. The new page adds one unit of
physical lore and an attributed record of the task.

When the archive has a held account from the recruit, it can instead bind a new
local volume through the existing binding routine. That path also needs one
local gold and one local gem, plus space in the bounded treasure collection.
The account's stored telling supplies the first page, and its event remains the
source of the recording event. The volume belongs to the archive town. The
appointment report gives a records, materials or storage reason while a
required input is pending.

## Continuing staff

The register keeps up to four named people at one archive seat. It retains the
inherited staff count as an explicit legacy count when the first appointment
activates the register. Later appointments use named places. Weekly ledger
changes can reduce the inherited count. Named workers qualify through their
living identity, occupation, activity and presence at the recorded seat.
Death frees the named place and produces an attributed event.

The shared weekly archive plan uses that working count and the trainer's work
reservation from #613. Named scribes select accounts they hold. Inherited staff
keep their existing account rule. Further volumes use the archive town's
binding materials, paper and tools. Food and tool wear follow the existing
archive work rules. Quest casting respects a named staff member's assignment.

## Save and proof

Four person IDs, the archive seat, the inherited count and the active flag have
explicit hash and SQLite fields. Validation checks issued identities, duplicate
places, the seat and the four-person bound. Full-width SQLite integers are
checked before conversion.

The focused tests cover indexing, local binding from a held account, missing
records and materials, full stores, staff capacity, duplicate requests,
identity after retirement, resource use, seven independent saved values and a
journal that completes an appointment. A paired weekly test keeps the world
fixed: a distant globally heard account stays unrecorded until the named scribe
holds that account.

The schema 80 journal fixture drops the new staff table before replay and
compares the historical hash after upgrade. `legacy-probe.c` compares two
40-year runs against parent `c74b796bf514dfac152d60d9a3e62cc4c5143fb8`. All 82
annual schema 80 checkpoints match. `legacy-parity.json` records them.

The strict headless build and all 124 tests passed. Static analysis passed
with one reviewed baseline item. The native play build and eight focused tests
passed: named staff, training, journey, reservation, shared carriage bridge,
bridge scene input, world card input parity and adventure input flow.

## Remaining integration

The explicit recruitment path now reaches named work. Automatic recovery and
player menus remain the next integration work for #446. Existing institutions
retain their earlier staffing behavior until their first named appointment.
