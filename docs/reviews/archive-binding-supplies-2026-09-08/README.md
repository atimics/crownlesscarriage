# Archive binding supplies

This change connects the local book recipe to shared archive freight. It addresses part of #248 and #446, on top of #618.

Schema 83 adds one gold and one gem to supply demand when a completed recruit has an account to bind, or when the local archive has an active staff record. An appointment that can index an existing local volume uses its reserved kit. Wheat, tools, and paper retain priority. Incoming shipments cover demand. Source reserve floors, actual ledger funds, prices, road capacity, tolls, carriage travel, and loss rules apply to binding goods.

The existing archive cargo validator accepts these two goods from schema 83. Schemas through 82 retain their earlier behavior. Generator 25, SQLite schema 32, and the simulation size remain unchanged.

## Evidence

- Quote tests cover both goods, source reserve floors, price, insufficient funds, incoming cargo, and older schema behavior.
- Dispatch tests cover actual payment, source stock removal, tracked money and goods, delivery, save loading, and equal continued simulation hashes for both goods.
- A daily seed-42 fixture reaches the observed appointment shortage on day 19. It identifies gold, then gems after gold becomes available. An available carriage reveals the source-stock blocker.
- `legacy-parity.json` compares this library with parent 226206d. All 82 annual schema-82 hashes match across two 40-year worlds. Build `legacy-probe.c` against each library and run seeds 42 and 24301.
- `world-measurements.json` uses `world-probe.c`, seeds 42 and 24301, schemas 82 and 83, with daily advances for 40 years. Each trial has one commission and zero appointments. Gate-day totals also match the parent behavior. Periodic world validation passes.

The ordinary worlds still need viable seats and source production or surviving local records. This freight change provides a route for existing binding goods when a supplier has stock to spare. #446 remains open.

Full validation is running; final results will be added before this draft is handed over.
