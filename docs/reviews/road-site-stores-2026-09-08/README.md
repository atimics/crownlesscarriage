# Local road-site stores

Advances #391's physical input/output storage foundation. Base #504 at 0202ec6, schema 62 / generator 25. This PR uses schema 63 / generator 25 and SQLite layout 31.

Each road site holds a bounded goods array at its own location. Capacity is 24 cargo slots, using the player's existing goods packing rules. This equals two standard carriage loads. Existing worlds start with empty site stores during migration.

At an open roadside stop, players can unload or load one bundle through an action card or world click. The text interface accepts `road unload GOOD` and `road load GOOD`. The shared-world action `transfer_road_site` takes a good and signed amount: positive unloads from carriage to site, negative loads into the carriage. The engine bounds the amount and checks both inventories and capacities before changing either one.

A transfer preserves total goods and leaves the home town's store intact. The carriage must reach the site through the ordinary journey path. Loading uses the current stop and leaves journey position and time unchanged, like local cargo handling. Each successful act records its amount, good, site and direction in the event chain. The journal and shared-world request sequence handle replay and retries.

The new saved goods enter the schema-63 hash. SQLite requires a complete site/good table and checks row order and bounded quantities. Validation checks each amount and the combined cargo slots. CcSim grows from 181752 to 183096 bytes, an increase of 1344 bytes.

Validation:

- Strict Debug build; all 74 headless tests passed.
- Every good at every site: independent hash mutation, unload, journal replay, load, direct saved amounts, total goods, and unchanged town stocks.
- Full store/carriage, blocked site, distance, invalid good, zero amount and extreme negative amount preserve state on refusal.
- Schema-62 migration preserves the historical hash and creates empty stores. A missing stock row fails loading.
- A runtime journey reaches the first site before clearing and delivering Wheat.
- Strict client build; card and world-click load/unload tests and mine input tests passed.
- Shared-world server tests cover local stock visibility, a two-player transfer, duplicate request handling and reload.

The next #391 delivery connects these stores to the common recipe path and schedules ordinary carrier deliveries for the mill, forge and farm pilots.
