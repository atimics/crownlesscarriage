# Treasure production contract

This slice of #392 moves treasure work onto the production recipe engine used by bakeries, smithies, paper mills and road sites.

The first weekly act commits one Raw Gold and one Gem from the actual town store and adds one unit of work. The next two acts add work to that saved order. The shared recipe provides the input checks, local store identity, capacity and work receipt. The existing completion function gives the finished treasure its identity, name, owner, location, contents and event. A full treasure pool retains the completed work order until a slot opens.

The public `CcSimPlanTreasureWork` preview reports one weekly act and its unmet gate. Market/capital and smithy-service requirements follow the existing rule. Hunger, working Tools and fire condition keep their existing policy. World schema 65, generator 25 and SQLite 31 continue from the parent.

Validation:

- Strict Debug build and all 79 headless tests.
- The focused test checks atomic material commitment, service and town-kind gates, three weekly steps, exact local artifact contents and value, a full pool, and save/journal replay from each work stage.
- Against parent 7578fe8, 640 annual state hashes match across two 40-year seeds and schemas 26, 27, 38, 46, 61, 63, 64 and 65. The comparison covers current and historical production, event order and saved work.

Compile `parity_probe.c` separately against each checkout's headers and `libcrownless_sim.a`, run both programs and compare their complete output. `parity.json` holds the schema/seed list and output digest.
