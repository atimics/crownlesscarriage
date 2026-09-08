# Wyrmheart recovery rule

A dragon forms its own Wyrmheart at its first deep-wyrm transformation. Later recoveries require that same intact object, owned by the dragon and held at its lair. The usual age, crown, and territory requirements still apply. A successor forms its own heart.

Schema 57 stores this identity through theft, destruction, treasure-slot reuse, and save reloads. The first transformation waits for an available treasure slot. Older saves use the earliest matching Wyrmheart from the dragon's lifetime. Existing duplicate objects stay in the world. A saved deep wyrm whose heart is already missing receives a spent identity. Older uncrowned saves with all evidence of a former heart erased retain the first-transformation path; their former heart identity is unknown.

## Local validation

The headless Release build treats warnings as errors. All 70 local checks passed; the speech test used permission to open its local server. The final persistence and dragon test changes passed their three affected suites again.

Regression cases cover first formation, territory loss and recovery, repeated recovery, ownership and location, destruction, reused slots, full treasure capacity, succession, save round trips, and older saves with duplicate or missing hearts.

A fresh seed 2 run passed all 16,000 annual validations with the fix at `3d90ae8`. It ended with three live treasures worth 1,566 crowns. Its dragon remained alive. The endpoint is saved in [seed2-16000.csv](seed2-16000.csv).

```sh
crownless_sim_metrics --seed 2 --years 16000 --final-only --campaign-metrics
```

This run uses the main-branch rules at `5f13096` plus the fix. The earlier 128,000-year investigation used `06e5701`; the focused regression tests isolate the heart rule.
