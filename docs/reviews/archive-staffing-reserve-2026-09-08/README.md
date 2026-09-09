# Protected archive staffing reserve

This draft tests the explicit funding rule requested by #606, on top of #621. It remains an experiment for review.

Schema 85 protects the weekly threshold for inherited archive staff before a freight quote: 50 crowns for one scribe, 150 for two, and 300 for three or four. Named staff have already received their recruitment wages. Freight may use the remaining reserve. Actual source stock, price, toll, carriage travel, and loss rules apply.

## Controlled results

Each arm uses the same 32 seeds, each advancing daily for 100 years. All 96 worlds completed with annual validation. Main is ce4f6c91473f08977f2ce9a8e4203fa28b1663a4. The parent dispatch stack is bb51f8a9bbae6b1ee42183dcd3edfcaf970ce99b, schema 84. The candidate uses schema 85. Main and the parent differ in several features; the parent/candidate comparison isolates this freight funding policy.

| Measure, summed across worlds | Main | Parent stack | Protected reserve |
| --- | ---: | ---: | ---: |
| Weekly zero-staff observations | 93271 | 104096 | 101813 |
| Weekly reserve below 50 | 93131 | 160839 | 153366 |
| Weekly writing-ready observations | 1585 | 434 | 411 |
| Sum of weekly staff counts | 243405 | 88172 | 97050 |
| Sum of weekly retained lore | 334135 | 1666074 | 1875150 |
| All shipment departure events | 52464 | 50955 | 56281 |
| All shipment arrival events | 50649 | 50611 | 55874 |
| All shipment loss events | 5095 | 4684 | 4908 |
| Endpoint staff total | 26 | 0 | 1 |
| Endpoint lore total | 29 | 0 | 11 |

Each arm contains 166848 weekly observations. Staff count includes inherited and named staff. Shipment counts cover the whole freight network and count events. They measure wider effects rather than archive-only cargo. Annual world hashes and each seed's result are retained in the JSON files.

The policy improves staff continuity and retained lore in this sample. Writing readiness falls from 434 to 411 observations. The ready flag covers staff and materials; actual accounts held by workers also govern writing. Wider policy tuning and archive-specific purchase traces remain further #606 work.

## Save compatibility finding

The first comparison ran schema 84 through the newer validator. Twenty-nine worlds failed with `Situation data is invalid`. Their completed quest records retain lifetime IDs after the people leave the live character array. Historical cast validation had applied only to the current schema. The repair applies that same issued-ID rule to completed situations across supported cast-bearing schemas. Active situations retain the live-person checks.

`compatibility-measurements.json` preserves the original failures. The authoritative parent arm was rerun against the actual parent library, where all 32 worlds passed. A separate run through the repaired validator checks the earlier schema's annual hashes. A century-old schema-84 save and day-journal replay regression covers this failure. This finding informs #614; the live saved world still needs its own reproduction.

## Reproduction

Build `probe.c` against each revision's `libcrownless_sim.a`, with that revision's `src` include directory and `-lm`. Then run `python3 run-study.py BINARY 0 OUTPUT_JSON` to use the binary's current schema. Schema 84 on the candidate binary provides the compatibility comparison. Each seed is `(index * 0x9e3779b9) & 0xffffffff`, indices 1 through 32. The runner preserves failures and partial annual output.

## Validation

Tests cover the protected payment boundary for zero through four inherited scribes, named-only staff, exact failed-dispatch state, coin conservation, delivery, staffing continuity, save/load, and journal replay. Existing disrupted-delivery and recruitment tests remain part of the suite. Final validation results are being collected.
