# Emergency nutrition trade credit

## Policy

Issues #400 and #401 describe hungry buyers beside grain suppliers. Ordinary
market trade already accepts wheat and protects supplier reserves. Its emergency
credit predicate still tests the bread alias. Schema 98 extends that predicate
to goods with civilian nutrition: bread, wheat, and meat.

The buyer must have hunger at least 65. Its kingdom takes the debt. Payment
comes from the existing iron ledger reserve, within the existing credit limit
of 480 plus four times legitimacy, less outstanding debt. The market spends
its own coins first. The supplier receives the goods price; road dues go through
the ordinary toll path. Current need includes stock and incoming loads. Supplier
reserve, path capacity, carriage availability, and route permissions govern the
physical shipment. Wheat retains its one-point civilian nutrition value and its
shared bakery and animal-feed pool. The normal wheat reserve remains protected.
The existing bread survival-floor rule remains specific to bread.

Eligibility is evaluated when planning and again when dispatching. Each purchase
uses current hunger, stock, funds, and routes. Grain organiser and archive orders
continue to spend their dedicated funds. The rule uses existing price, shipment,
debt, and repayment records. Schema 97 and earlier retain their replay rules;
loading a completed old save upgrades it to schema 98. Generator 25 continues.

## Evidence

The nine pinned trade cases cover old rules, funded wheat, low hunger, an empty
ledger, full debt, a closed road, a busy carriage, stock at reserve, and funded
meat. Wheat purchases 28 units for 28 crowns; meat purchases 20 units for 80.
Both arrive through ordinary shipments. Gold is conserved across dispatch.
The supplier retains its reserve, and delivery occurs after departure. Save
reload and 14-day continuation hashes match. The revised test fails against
the parent at the funded wheat case.

The new rule changes the 30-year traveller fixture's world history. The old
assertion expected every home role to remain present after all journeys and
appointments. It now tests the actual ordinary-departure guard over seven days
in 16 seeds. Removing that guard makes the revised test fail. The existing
30-year movement, gossip, and save tests continue.

`run-study.py` runs 32 seeds for 40 years, with the dragon natural or slain at
day one. Each case runs the parent at schema 97, the changed engine at schema
97, and the changed engine at schema 98. All 7,680 annual states validate;
all 2,560 old-schema rows match the parent exactly. `results.json.gz` retains
every row. `summary.json` gives arithmetic means of the final world snapshots.
Hunger is population-weighted within each world, then averaged across worlds.
Wheat overflow is cumulative over the 40-year run.

| Final measure | Natural parent | Natural changed | Slain parent | Slain changed |
| --- | ---: | ---: | ---: | ---: |
| Population | 16,391.28 | 16,652.47 | 17,582.28 | 17,240.25 |
| Hunger | 10.25 | 8.09 | 11.13 | 12.22 |
| Wheat overflow | 27,236.03 | 27,252.50 | 20,718.38 | 19,250.91 |
| Abandoned towns | 0 | 0 | 0 | 0 |
| Ledger reserve | 0.22 | 0 | 27.91 | 17.50 |

The controls show mixed welfare outcomes. This is a whole-world policy
comparison: changed trade also changes later history. The ledger is nearly
exhausted in the natural-dragon group, so road recovery, funding concentration,
and supplier reserve policy remain part of the parent issues. This draft
establishes a bounded paid relief path for modern staples.

## Reproduction

Build the parent and changed Release simulation libraries in separate directories.
Compile `probe.c` against each library with `-O2 -I src -lm`. Run:

```
python3 run-study.py PARENT_PROBE CHANGED_PROBE results.json.gz
```

The probe uses seed ordinal times `0x9e3779b9`, with ordinal 1 through 32.
The parent source is commit `7e022e70`, which combines #685 and #687.
