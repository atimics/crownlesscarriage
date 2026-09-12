# Grain supply report validation

The production report now includes the saved grain account and the current
`CcSimGrainDeliveryPlan` for each town. This supports issues #266 and #400.

Validation:

- Release build with warnings treated as errors passed.
- All 137 headless tests passed, including JSON repeatability, text/JSON world
  hash parity, save reload parity, grain disruption, and SQLite account checks.
- Seed `0x5EED0001`, 40 years: all 41 annual hashes match the baseline natural
  history report in `../production-capture-checkpoints-2026-09-12/`.
- The same final report shows Thornford at wheat 320 and hunger 0, Gloamgate
  at wheat 0 and hunger 100. Every town has an inactive grain delivery account.
  Road recovery and production gates remain relevant to the policy decision.
- The funded probe starts seed 42, funds Gloamgate through the public command,
  and advances 240 days. The account reports 192 spent, 168 ordered, 156
  delivered, and 12 lost. Every stored field matches SQLite. Its plan status
  and reason match a direct planner call before saving.

The probe source and its compact result are retained here. Compile the probe
against the build's `libcrownless_persistence.a` and `libcrownless_sim.a` with
`-I src -lsqlite3 -lm`. It writes `/private/tmp/grain-report-funded.ccsave`.
Load that save with `crownless_sim_runner --load PATH --years 0 --json` to
inspect the account. The first applicable gate in this fixture is carriage
availability; the remaining purse is also zero. Interpret the plan in its
stated evaluation order.
