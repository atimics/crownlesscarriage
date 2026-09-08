# Shared freight trip geometry

Advances #391's carrier integration. Base #506 at b461239, schema 64 / generator 25. Save schema stays 64 and SQLite layout stays 31.

CcSimFreightLeg identifies both endpoints on one actual route. Town endpoints occupy 0 and 1000; a road site uses its saved progress_milli on that route. A site's home-settlement attribution is separate from its physical position.

Trip duration is the route's travel days multiplied by the travelled fraction, rounded up to whole days with a minimum of one day. Each leg receives its own day rounding. Whole-route trips preserve their existing duration in either direction. Invalid routes, endpoints, site positions and route durations return an empty result.

Existing shipment dispatch, resumed legs, empty carriage repositioning, and saved shipment/carriage timing validation now use this shared rule. Site dispatch, access checks, pickup and unloading are the next carrier step. Dispatch owns access, border, capacity and custody policy.

Validation covers every route and site in both directions, same-route site pairs, unrelated routes, invalid identities, invalid positions/durations, output reset, and read-only hashing. The current 40-year reports match b461239 for 0x5eed0001 and 0xc0a71a9e, including smithy and site accounting. Historical comparisons match 560 annual hashes across rule versions 26, 27, 38, 41, 46, 63 and 64.

Reproduce current reports with crownless_sim_runner --seed SEED --years 40 --report-every 1 --sites --smithy. Compile parity_probe.c against each checkout's src headers and libcrownless_sim.a for the historical comparison. The probe selects historical rules after current initialization. The SQLite suite separately covers shipped saves and journals. Digests are in parity.json.

The strict Debug build and all 76 headless CTest checks passed.
