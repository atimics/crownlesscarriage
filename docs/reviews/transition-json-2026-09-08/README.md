# Campaign and ritual plan snapshots

Partial work for #266. Base: #569 at 4c937b2. Schema 73 and SQLite format 32.

JSON protocol 6 gains campaign_launch and ritual_offering objects. They consume CcSimCampaignLaunchPlan and CcSimRitualOfferingPlan, which are also used by execution and the existing text diagnostics. Text and JSON share blocker names.

The campaign object reports held supplies, pledges, leadership and origin IDs, preparation eligibility, phase, cooldown, and lifetime attempts. Policy intent is unavailable and represented as null. Its semantics are held_supply_plan_snapshot because preparation may gather supplies before departure.

The ritual object reports held offerings and planned eggs alongside cult/lair IDs, membership, devotion, cohesion, seed phase/timer, tribute phase, afterdeath time, and existing eggs. Its semantics are held_offering_plan_snapshot. Offering readiness and phase/timer state are separate facts.

## Controlled evidence

The JSON fixtures include the existing engine plan fixtures directly. [Campaign cases](campaign-fixtures.json) cover supplied, missing food, restored, and cooldown states. [Ritual cases](ritual-fixtures.json) cover supplied, missing coins, restored, and active timer states. Every report checks full-state immutability. Existing engine tests cover conditional execution after real prerequisites are restored.

The production JSON test checks save/load snapshots. Its day-one slain control now requires exactly the added dragon_slain campaign gate, then compares all other existing fields. [Parent comparison](parity.json) covers 82 checkpoints across two forty-year runs; every pre-existing JSON field matches after removing the two new objects.

These engine checkpoints report evaluated prerequisites and represented activity. The broader intent, reachability, and player-understanding requirements in #266 remain follow-up work.

Validation: strict Release headless build, all 105 tests, static analysis, eight controlled fixtures, and all existing-field comparisons passed.
