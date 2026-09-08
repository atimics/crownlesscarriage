# Archive recruit travel

This draft continues #446 and #470 on top of #602. Schema 79 gives a reserved
recruit a real road journey. The generator stays at 25 and SQLite at 32.
`CcSim` is 184376 bytes.

## Daily progress

The first daily update rechecks the named recruit and the archive town. Local
recruits arrive for induction. Remote recruits follow `CcTradeFindPath` with the
same border policy and one-slot requirement as royal couriers. Each departure
checks the current roads and held food. The character's town changes on the
saved arrival day. Each intermediate town provides a stop before the next
road check.

Travel food pays for the next leg in advance: two wheat per seven total road
days, rounded up. The cumulative day counter makes rounding shared across
legs. A longer replacement road can leave the order waiting for food. A
broken route leaves it waiting for a route. The report includes that reason.
The town meal routine respects the provisions of a recruit on the road.
Quest casting respects the recruit's existing commitment.

Arrival uses the courier road-risk threshold, `danger / 5` out of 100. A road
attack leaves the recruit recovering at the next town and loses the remaining
travel food. Death ends the order under the original person ID. Travel food
held on the road is then lost. Departure, stops, arrival and failure produce
named interaction events with a place and cause.

Cancellation is available while waiting, after arrival, or after failure.
Unused travel food goes to the recruit's last town. Training supplies stay at
the archive, and their refund goes there. Money returns to the original
funders. Capacity checks precede every refund. An active road leg finishes
before cancellation becomes available.

## Save contract

Six additional values hold the last town, current route, next town, road arrival
day, provisioned road days and actual archive arrival day. They have explicit
hash and SQLite fields. The reader checks full-width integers before narrowing.
Validation checks connected road endpoints, dates and state-specific fields.
Older schema 78 journals replay their original reservation rules before upgrade.

The former quoted arrival and ready dates remain estimates. Actual progress has
its own fields. Reserved wages and training supplies remain available for the
next work: induction, trainer labor, named staff appointment and a first archive
task. Those stages complete the recruitment outcome in #446.

## Proof

`archive_journey_tests` exercises local arrival, several road legs, food use,
duplicate daily calls, closed-path waiting, insufficient food, refunds at the
current town, road loss, death, saved fields, wide SQLite integers and a journal
that crosses departure and arrival. `archive_order_tests` continues the original
reservation, refund and donor-ledger proof. The schema 78 journal fixture removes
the new table before replay and checks the historical hash.

`legacy-probe.c` compares 41 annual checkpoints for each of two seeds against
parent `77cde2a11d9b71a6e1462d0708df711a56515934`. All 82 schema 78 hashes match;
`legacy-parity.json` records them.
