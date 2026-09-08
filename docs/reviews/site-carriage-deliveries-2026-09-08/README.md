# Physical site deliveries

This slice of #391 connects the load plans in #508 to the ordinary royal carriage network. Each kingdom's existing carriage can visit a production site, deliver supplies, collect finished goods and return to town. Goods remain in the actual source store until loading, then in the existing shipment record until unloading or a recorded road loss.

## Dispatch and timing

At each ordinary trade planning opportunity, an eligible idle carriage checks sites in their saved order before town trade. The shared planner protects town reserves and a six-week food buffer. It gives finished goods priority, then working Tools, then declared recipe inputs for two batches. Each trip begins at the site's actual home town and uses its actual route position. Home identity selects the service relationship; separate stores hold the goods.

Pickup visits begin with an empty outward leg. The carriage rechecks output, working Tool floors, town headroom and weekly road capacity at the site before loading. Supply visits load at town, unload at the site and can collect finished output on the return. Each leg takes the shared geometry's rounded-up travel time, with a minimum of one day. Empty legs occupy one road slot. Loaded legs use the existing goods packing rules. Return departures share capacity with ordinary trade.

The existing road danger determines cargo loss at arrival. A surviving carriage reaches the real destination. A closed or politically barred road leaves the carriage and cargo waiting at the leg origin; a resumed leg takes its full travel time and books road capacity again. A full destination accepts the amount that fits and holds the remaining load aboard at that destination. Space and road waits retain their physical cargo for as long as the gate persists. A completed round trip adds one trip and sets the ordinary seven-day dispatch delay.

This is internal service between a town and its attributed worksite. It transfers goods within that service relationship. Town trade continues through its existing purchase and toll rules.

## Accounting and saves

Caller-owned site accounting records town loads (`sent`), site unloads (`received`), site loads (`shipped`), town unloads (`delivered`) and road losses (`lost`). The runner prints these totals with `--sites`. For each good, the tests independently check:

- Site stock = initial stock + production - recipe inputs + received - shipped.
- Sent + shipped = received + delivered + lost + cargo still aboard.

The accounting remains an observation of the simulation. Daily and batched runs produce the same totals and state hash.

World schema 65 enables these journeys. Generator 25 and SQLite 31 continue from the parent. The existing saved carriage and shipment fields hold travelling, waiting and unloading states. Validation checks the real site/town leg, cargo linkage, timing and capacity. Schema 64 journals replay their original rules before upgrade. A comparison with parent f2d1e1d matches all 560 annual hashes across two seeds and schemas 26, 27, 38, 41, 46, 63 and 64.

## Pilot evidence

The same explicitly funded three-site fixture from the production review ran for 40 years with sites open and closed. Closed controls produced and transported zero pilot output.

| Seed | Mill bread delivered | Forge tools delivered / lost | Farm wheat delivered / lost |
| --- | ---: | ---: | ---: |
| 0x5eed0001 | 4 | 2 / 2 | 28 / 8 |
| 0xc0a71a9e | 4 | 5 / 0 | 44 / 8 |

At year 40, population-weighted hunger changed from 4 to 5 in the first seed and from 5 to 3 in the second. These mixed results support further outcome work. The broader starvation and gold-seam work under #391/#394 remains open.

Reproduce each fixture using `docs/reviews/road-site-production-2026-09-08/pilot_fixture.c`, linked against the current persistence and simulation libraries. Pass `SEED OPENED SAVE_PATH`, then run `crownless_sim_runner --load SAVE_PATH --years 40 --report-every 1 --sites --smithy`. The stored measurements include report digests, save digests, final world metrics and exact pilot transfer totals. The historical probe is `docs/reviews/freight-legs-2026-09-08/parity_probe.c`, compiled separately against parent and current libraries.

Validation covers the strict Debug build and 78 headless tests. Focused journey tests cover actual supply and pickup, elapsed travel, working Tools, road and capacity waits, full and partial unloading, loss receipts, save/resume, journal replay and daily/batched parity. Production tests retain the seeded minimum outputs and add complete stock and transport reconciliation. The unfunded control protects source goods in town reserves.
