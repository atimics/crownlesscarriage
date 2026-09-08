# Archive supply booking

This draft adds the booking query for #248, based on #590 at 540dd3f. It also supports the supply eligibility work in #446 and reports in #266. Schema 74 and generator 25 remain current.

The query uses the active archive seat and quotes the kingdom's existing carriage. Wheat has first priority, followed by tools and paper. Wheat covers one weekly archive task after the town's two-week food reserve. Incoming food and goods count toward that requirement. Paper and tools each need one available unit for a bounded task.

A quote requires an idle, serviceable carriage, elapsed dispatch cooldown, source surplus, a path from the carriage to the supplier, a loaded path to the archive, available freight capacity, and actual iron-ledger funds. Wheat suppliers retain six weeks of grain, matching the existing grain delivery rule. Other goods retain their source reserve target. Quantities respect shortage, source stock, carriage room, route capacity, and affordability.

Quotes share the ordinary royal freight path and toll rules. Closed roads reduce capacity and add dues. Allied border passage adds its existing dues. War access follows the ordinary royal carriage permissions. Candidate scores use unit price, first-leg toll, loaded path cost, and repositioning path cost. Equal scores select the lower source ID.

The JSON archive_supply report labels each quote as a held booking snapshot. Its price covers goods and the first loaded leg. Path cost is the route search score. Dispatch must recheck the quote when the carriage reaches the supplier.

## Remaining delivery work

Actual booking payment, repositioning dispatch, loaded shipment creation, dangerous war passage, and arrival/loss events remain the next implementation step in #248. They will use the shared carriage and shipment pool. Named staffing recovery remains in #446. This draft delivers the shared booking query and visible quotes.

## Validation

- Strict release headless build passed.
- All 115 tests passed after adding the booking fixtures.
- The three affected booking, production JSON, and resume tests passed again after adding report output. Quotes match after save/load.
- Static analysis passed with one reviewed baseline item.
- Every booking fixture compares the whole simulation before and after the query.
- Fixtures cover funding boundaries, incoming stock, completed shipments, source reserves, stale weekly route usage, closed roads, allied tolls, war access, carriage condition and cooldown, wheat nutrition reserves, partial loads, and stable supplier choice after town reordering.
- Two 40-year runs match all existing JSON fields across 82 checkpoints, including simulation hashes. The comparison removes only archive_supply. See parity.json.
