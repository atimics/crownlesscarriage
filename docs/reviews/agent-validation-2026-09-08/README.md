# Paired player sweep validation

Release code revision 90a7539, parent 0a5bd03. Built with CC_BUILD_CLIENT=OFF, CC_BUILD_BENCHMARKS=ON, strict warnings as errors. All 74 headless tests passed.

Eight paired ten-year runs and two paired hundred-year runs completed with annual checks. Their entire CSV output matched the parent byte for byte, including state hashes and action counters. Commands and output hashes are in comparison.json; the matching captured files are in ../agent-hunger-report-2026-09-08. Seed indexes start at 1 and map to uint32(index * 0x9e3779b9).

The focused test injects failures at each of the six checks in a one-year paired run: both startups, both annual boundaries, and both endpoints. Each causes a failing process result at that check. A separate fixture gives the player an invalid purse after a year boundary; the real validator catches it while the checkpoint and simulation bytes remain unchanged.

The agent check occurs after a completed action, so its actual day can exceed the scheduled annual boundary. Reports include both days. Long commands that cross several boundaries receive one check of their resulting state. Exact campaign, ritual, and road-recovery gate diagnostics remain work under #266.
