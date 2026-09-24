# Scrivendays simulation and play checks

## World sweeps

These runs start from ordinary generated worlds with literal seeds 1–32 and
1–4. Each solar year contains 364 daily simulation steps. State is checked every
week and at the end. Each final world is saved, loaded, and checked for an
identical full-state hash. The CSV files retain every result.

| Run | Worlds | Total world years | Meetings | Books compared | Returns | Failed trips | Published almanacs | True age changes | Valid worlds and reloads |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [30 years](worlds-30-years.csv) | 32 | 960 | 53 | 118 | 83 | 23 | 0 | 0 | 32 |
| [1,000 years](worlds-1000-years.csv) | 4 | 4,000 | 4 | 10 | 9 | 4 | 0 | 4 | 4 |

The 30-year worlds had 32 delegates still away or waiting at the final day.
Meetings depended on the existing world economy and travel conditions. In the
1,000-year worlds, a true dragon-age change occurred in every world, while
surviving evidence and attending schools fell short of a publishable date.
Scribe survival and continuing research across centuries are the next balance
question. The supplied-world test below covers successful agreement and delivery.

Reproduce from the native build:

```sh
out/build/play/crownless_scrivendays_sweep 32 30
out/build/play/crownless_scrivendays_sweep 4 1000
```

The sweep used simulation rules from `74b22da8`, with the final-save checks in
`73b8de1b`. That later commit also fixes sky reading, displayed citations, and a
Linux test helper. Those changes leave the daily autonomous sweep rules intact.

## Supplied-world scenario

`tests/scrivendays_tests.c` sets up a world with stocked towns, open roads, and
six named scribes. It advances the scribe system through two annual gatherings.
The first expedition records a Crowned Dragon. The fixture changes the dragon
to Deep Wyrm on day 300. The next expedition records that sighting in the carried
tomes. The hearing publishes an interval containing day 300 and an adopted date
that differs from the private date. This checks inference from the notes.

The checks require a rotating host, two meetings, at least four completed
returns, one published edition, adoption in at least two towns, a save during
travel, and an identical final save/load hash. This is a supplied subsystem
scenario; the CSV runs above exercise the full world economy.

## Other checks

- All 236 native tests passed after the schema 113 merge.
- Nine focused tests passed after the final sky and reading changes.
- Frozen passages survive changes to the town that wrote them.
- Loans keep ownership and require the exact book's return.
- Copies retain evidence provenance. Hearings choose an available independent
  source when the closest account shares an origin with another account.
- Moving a cited book away removes its availability at the hearing.
- Changing the hidden true date leaves the public conclusion unchanged.
- Field notes require a carried book. Repeated observations preserve existing
  notes. Work at the final supported day fails before consuming supplies.
- Schema 112 migration preserves the prior state and creates new frozen pages.
- Every encoded field contributes to the hash; truncated or extra save data is
  rejected. Daily and batched advancement produce the same state.

## Browser play check

The local WebAssembly build at `73b8de1b` was checked at 1280 × 720. The company
commissioned a tome, spent two watches, read the night sky, and found the dated
observation in the tome's margin. The calendar, signs, and book controls fit the
panel. The owner field and disabled Company tome button identify company property.

- [Calendar and night observation](calendar.png)
- [Thirteen signs](signs.png)
- [Carried field tome](tome.png)
- [Saved sky note](sky-note.png)

The browser reported a completed save. The following reload check reopened the
same world and verified the dated note in the same tome.
