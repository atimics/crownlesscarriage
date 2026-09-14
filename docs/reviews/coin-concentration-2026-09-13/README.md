# Coin concentration measurement — 13 September 2026

Base: `2f88402b` (schema 101). This is the measurement half of #398 and the
acceptance evidence for the #399 currency decision (fixed coined supply;
the mint valve opens only if the circulating share crosses a floor with
recovery proven blocked). It reproduces the historical 48-seed/60-year
battery at the current revision.

## What ran

`crownless_sim_metrics --seed N --seeds 4 --years 60` over 48 metrics seeds
(1–48) in twelve parallel batches, annual rows, all seeds passing validation
at every year. The runner now carries four derived columns —
`hoard_share_permille`, `treasury_share_permille`, `goblin_share_permille`,
`circulating_share_permille` — computed from `CcSimTrackedGold`. Concentrated
holders are the dragon hoard, kingdom treasuries and war chests, goblin
holdings, and campaign coin in flight; what remains is circulating: markets,
companies, travellers, custody purses. Data: `medians-by-year.csv`,
`endpoints-year60.csv`.

## Findings

| Year | Tracked gold (median) | Hoard share | Treasury share | Circulating share (median / p10) |
|---:|---:|---:|---:|---:|
| 1 | 3,371 | 5.7% | 43.7% | 50.3% / 41.2% |
| 10 | 3,371 | 71.5% | 3.0% | 25.3% / 16.8% |
| 20 | 3,371 | 78.4% | 0.7% | 17.6% / 12.7% |
| 40 | 3,371 | 79.9% | 0.0% | 16.8% / 10.6% |
| 60 | 3,371 | 82.2% | 0.0% | 16.5% / 10.5% |

1. **Conservation holds.** The coined total is constant across sixty years in
   every seed (median 3,371; the old audit's 3,459 differs by generator
   revision, not by leak). The #399 decision's accounting rule is already
   true in the data.
2. **The hoard is a near-absorber on the median path.** Median hoard share
   rises monotonically to 82% at year 60; 27 of 48 worlds hold ≥80% in the
   dragon hoard. Kingdom treasuries drain to zero — the historical
   `SettleDragonDebt` drain is confirmed as the treasury path.
3. **But it is a slowly returning system, not a mathematical absorber.** The
   hoard's share growth flattens from +267 points per decade in the first
   decade to +11 points per decade after year 50 — NPC raids and campaign
   returns demonstrably push coin back out (the #455 correction to the old
   "nothing returns" claim is confirmed). Circulating share stabilizes
   around 16–17% median rather than reaching zero.
4. **Goblin holdings are irrelevant to concentration** (median share 0). The
   goblins route coin to the dragon or hold goods, not coin.

## For the #399 gate

The mint valve's floor is now settable against measured reality: at sixty
years, circulating share is 16.5% median and 10.5% at the tenth percentile,
with the curve plateauing. If ~10% circulating with slow recovery counts as
livable, the valve stays closed and #407 remains deferred. If the owner sets
the floor above ~15%, the recovery-rate question (raids returning faster, or
a mint recipe) reopens. Recording the floor is the owner's call; this
measurement is the number to set it against.

## Not yet measured (next slices of #398)

- **Gross flows** are not separated in this pass — the annual share curve
  shows net movement. A hoard inflow/outflow ledger needs schema work;
  the late-decade slope already bounds net recovery at ~11 points/decade.
- **Activities blocked for lack of funds** — market activity at p10 seeds
  vs median seeds is the natural comparison; not computed here.
- **Dragon-alive/slain branches** — the historical audit's whole-policy
  counterfactual (slain-on-day-1: hoard 30, town coin 669) needs a fixture
  seed, not a sweep column.
- **Recovery intervention** — the controlled raid/interception case that
  #398's acceptance wants (matched time, named activity changed) is a
  designed fixture for the next PR, alongside #462's extraction consumers.

## Reproduce

```sh
out/build/release/crownless_sim_metrics --seed 1 --seeds 48 --years 60
```

Column definitions live in `tools/sim_metrics.c` beside the share block;
the same four columns appear in every annual sweep from this revision on.