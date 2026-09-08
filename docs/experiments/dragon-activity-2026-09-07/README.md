# Dragon activity: 200 worlds over 1000 years

Measured on 7 September 2026 with the `crownless_sim_metrics` runner. This is
an autonomous baseline sweep focused on dragon lifecycle, campaigns, and the
human consequences of a surviving dragon.

## Provenance

- Source checkout: `5cb651dc6c3657b82d5eefedca5a4fbf79064439` (main at run
  time).
- `cmake --build out/build/play --target crownless_sim_metrics` rebuilt the
  binary at this revision before the sweep.
- Command:

  ```sh
  python3 tools/sim_sweep.py \
    --binary out/build/play/crownless_sim_metrics \
    --first-seed 1 --seeds 200 --years 1000 --jobs 8 \
    --output out/sweeps/dragon-report-200x1000.csv
  ```

- Seed numbers 1 through 200 use `world_seed = seed * 0x9E3779B9` (see
  [simulation sweeps](../../simulation-sweeps.md)).
- All 200 seeds passed: 200,000 annual checkpoints validated, zero failures.
- Raw endpoint rows (one per world, 109 columns):
  [endpoints.csv](endpoints.csv).
- Charts: [charts/archetype-dragon.png](charts/archetype-dragon.png),
  [charts/archetype-society.png](charts/archetype-society.png),
  [charts/archetype-threats.png](charts/archetype-threats.png), generated with
  `tools/plot_archetypes.py`.

## Headline results

1. **Campaigns kill the original dragon in 92% of worlds.** 183 of 200 worlds
   slay their original dragon at some point. In 152 worlds the dragon stays
   dead; in 31 a successor whelp hatches from the brood hoard and the lineage
   continues. Only 17 worlds (8.5%) end with their original dragon alive.
2. **Slaying is late and bimodal.** Among the 152 worlds where the dragon
   stays dead, death occurs as early as year 202 and as late as year 982, but
   the median death year is 881; 49 of them (32%) die before year 500 and 101
   (66%) after year 750.
3. **A surviving original dragon is a catastrophe for the realm.** The 17
   never-slain worlds end deep wyrm (14) or uncrowned (3), and compare badly
   with the 152 dead-dragon worlds:

   | End-of-run mean | Original survives (17) | Successor dynasty (31) | Original slain, none hatched (152) |
   | --- | ---: | ---: | ---: |
   | Average prosperity | 26.6 | 35.5 | 33.9 |
   | Average hunger | 47.4 | 38.6 | 39.4 |
   | Average security | 40.6 | 51.3 | 49.3 |
   | Average legitimacy | 28.0 | 46.8 | 45.3 |
   | Years with hunger 40+ | 508.0 | 240.7 | 285.1 |
   | Total population | 11,519.6 | 13,058.4 | 13,173.6 |
   | Kingdom treasury (crowns) | 31.8 | 1,742.9 | 2,109.5 |
   | Dragon hoard (crowns) | 3,127.0 | 579.8 | 0 |

   A thousand-year dragon does not merely tax the realm; it ends up holding
   nearly all tracked gold (a mean hoard of ~3,127 crowns against a kingdom
   treasury of ~32 crowns) while hunger and illegitimacy stay extreme for
   most of history.
4. **Successors give a second chance.** The 31 successor worlds recover
   beyond dead-dragon-world outcomes on most measures, because the young
   whelp spends years maturing before it can dominate again. Successors end
   the run as whelps (17), wanderers (7), crowned (5), or deep wyrms (2).
5. **Campaigns are short, rare, and usually decisive.** 193 of 200 worlds
   attempt at least one dragon campaign; the mean world attempts 2.21 (max 5)
   and suffers 0.64 defeats. A campaign spends a mean of only 26 days in the
   field. Defeat means the campaign host breaks against dragon strength plus
   goblin allies, which feeds dragon retaliations (mean 41 per world, max
   137).

## Dragon lifecycle

Mean days per life stage across all 200 worlds (a run is 365,000 days):

| Stage | Mean years | Share of run |
| --- | ---: | ---: |
| Whelp | 11.5 | 1.2% |
| Wanderer | 42.9 | 4.3% |
| Crowned | 506.2 | 50.6% |
| Deep wyrm | 55.3 | 5.5% |
| Uncrowned | 36.9 | 3.7% |
| Afterdragon | 347.2 | 34.7% |

The crowned stage dominates the living dragon's history. Deep-wyrm days
correlate with the dragon surviving (`corr(slain at end, deep wyrm days) =
-0.53`): dragons that reach deep-wyrm strength late in the run tend to repel
their campaigns and are never slain. Every never-slain dragon ends the run as
a deep wyrm or uncrowned dragon; 10 of the 17 repelled a single campaign
attempt and 7 were never attacked.

## Dragon activity between campaigns

- Hunts: mean 212.4 per world, median 235, max 1,155.
- Retaliations against settlements: mean 41.2, max 137.
- Broods laid: mean 1.96 (range 0–4); whelps dispersed: mean 4.34 (range
  0–11).
- Final hoard: mean 364.1 crowns across all worlds, but 0 in dead-dragon
  worlds and ~3,127 in never-slain worlds; max 3,419.

## Correlations with slaying

Across the 200 endpoint rows, the end-state `dragon_slain` flag correlates
with:

| Metric | Correlation |
| --- | ---: |
| Deep wyrm days | −0.53 |
| Dragon hunts | −0.23 |
| Average hunger | −0.14 |
| Average prosperity | +0.08 |

The end-state flag mixes "slain long ago" with "slain recently", which mutes
its prosperity correlation; the three-way group table above is the cleaner
comparison.

## Method notes and limitations

- Endpoint data only: this sweep records end-state rows (`--final-only`), so
  timing claims are derived from counters (death year from `afterdeath_days`)
  rather than from annual time series.
- Successor dynasties are identified as worlds with `dragon_slain = 0` but
  `dragon_campaign_victories > 0`: campaign counters persist when a successor
  hatches, while the slain flag resets. This yields 31 worlds; the 17
  never-slain worlds have zero campaign victories against them.
- `dragon_slain` in the endpoint row reflects the state at run end, not
  whether a slaying occurred earlier in the run; group tables above use the
  corrected three-way split for that reason.
- Validation ran after every simulated year (`--final-only` only suppresses
  yearly CSV output, not checks).

## Reproduce

```sh
cmake --build out/build/play --target crownless_sim_metrics
python3 tools/sim_sweep.py \
  --binary out/build/play/crownless_sim_metrics \
  --first-seed 1 --seeds 200 --years 1000 --jobs 8 \
  --output out/sweeps/dragon-report-200x1000.csv
python3 tools/plot_archetypes.py out/sweeps/dragon-report-200x1000.csv \
  --out out/sweeps/dragon-report-charts
```