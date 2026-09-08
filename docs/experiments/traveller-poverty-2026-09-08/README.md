# Traveller poverty and bandits

Implemented source: `619b485`; baseline: `d478d2e`.

Named adult travellers and refugees away from household support now pay for food and shelter. Three hungry days or three nights without shelter make them seek aid. On their weekly decision day, they can join a nearby bandit camp with space. The event names the person and camp. A camp link prevents repeated recruitment.

Prosperous markets can pay six crowns for casual work. An inn bed costs two crowns; food uses the local price. Food, shelter or earnings can resolve the hardship. Residents at their own living home keep the existing household support. Background town hunger and debt still drive recruitment among the wider population. This first change adds the personal traveller path to that existing population model.

Purses, hardship and camp links survive saves. Coins move between existing holders. A successor inherits money and begins with a fresh camp status. Save schema 58 upgrades older saves.

## Validation

All 71 local tests passed, including the speech test rerun with local-server access. Focused cases cover hunger, lodging, paid work, relief, household support, save reload, succession, old saves and coin conservation.

Eight seeds each ran 16,000 years after the change: all 128,000 annual validations passed. The final snapshot has zero named bandits in all eight worlds. That snapshot measures surviving members, rather than lifetime recruitment. The focused tests prove the causal recruitment path; this sweep supports long-run stability. Measuring recruitment frequency over whole lives is a useful next balance check.

The table compares the first bandit camp at year 16,000. Other camps are outside these two existing metrics. Changes include traveller spending as well as recruitment, so the table measures the whole patch.

| Seed | Camp members, before → after | Camp raids, before → after | Named bandits alive | Personal coins |
|---|---|---|---|---|
| 1 | 120 → 120 | 64846 → 64846 | 0 | 0 |
| 2 | 4 → 4 | 2351 → 2351 | 0 | 0 |
| 3 | 4 → 4 | 6178 → 2711 | 0 | 488 |
| 4 | 4 → 120 | 1619 → 84914 | 0 | 242 |
| 5 | 4 → 4 | 9348 → 9065 | 0 | 1 |
| 6 | 120 → 120 | 173185 → 173189 | 0 | 1573 |
| 7 | 4 → 4 | 21183 → 10021 | 0 | 144 |
| 8 | 120 → 120 | 208508 → 208508 | 0 | 0 |
