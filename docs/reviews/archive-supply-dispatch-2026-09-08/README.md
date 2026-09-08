# Archive supply dispatch

This draft implements automatic archive supply through the existing royal carriage and shipment pool. Base: #591 at 7844a58. Schema 75 adds the dispatch rule; generator 25 stays current.

When wheat, tools, or paper are short, the archive may buy the quoted load with its actual iron-ledger reserve. The shared freight purchase removes source stock, pays the source market and road dues, allocates the shipment, and sets the carriage's journey. Existing site and funded grain work retain their planning priority.

New empty pickup trips start every four weeks. A carriage already at a supplier can collect within its ordinary seven-day cooldown. The query reports first_dispatch_day and dispatch uses that same date. Arrival at the supplier triggers a fresh quote against current goods, funds, and routes.

Cargo follows the shared freight rules for elapsed travel, route capacity, closed-road dues and danger, border permission, loss, arrival, and delay. The purchase event names the archive, source, cargo, and price. Shipment outcome events link back through the departure record. Current save fields hold all cargo and carriage state.

## Validation

- Strict release headless build passed.
- All 116 tests passed.
- Static analysis passed with one reviewed baseline item.
- Controlled dispatch tests prove exact source debit, seller credit, archive charge, total gold conservation, single payment, delayed arrival, and an unchanged state on repeated dispatch attempts.
- Travel, blocked cargo, and lost cargo survive save/load. A journal suffix replays automatic purchase after empty repositioning.
- Schema 74 save and journal replay retain their old rules before upgrade. All supported schema/generator pair checks pass.
- Two 40-year schema 74 runs match the parent branch at 82 annual checkpoints, including complete simulation hashes.
- Two schema 75 runs book 564 loads: 525 arrival outcomes and 39 losses. See measurements.json and probe.c. These counts come from daily observation of shipment event IDs; arrival includes the shared freight system's fallback unloading outcome.
- The existing 120-year balance sweep passes its collapse, crisis, quiet, scar, war, and peace checks.

The first experiment dispatched empty pickups whenever a quote was available. Its balance sweep had zero war samples. The four-week pickup rule implements the requested rare caravan cadence and passes the existing balance checks.

## Remaining issue work

This advances #248 and supplies the transport foundation for #446. Explicit dangerous passage across hostile borders, dynamic archive relocation (#240), and named recruitment remain further work. Current border access uses the kingdom carriage's existing permissions. The full issues remain open.

## Reproduction

Build the strict headless targets, run CTest, and run tools/static_analysis.py. Compile probe.c with the current simulation library, then run it with a seed and schema (74 or 75). Compile it with the #591 simulation library for the historical comparison. Each probe covers 40 years.
