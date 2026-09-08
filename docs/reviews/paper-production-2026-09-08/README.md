# Paper recipe migration

Advances #392. Base PR #503 at 9b726ff, schema 62 / generator 25. Save schema stays 62 and SQLite layout stays 30.

Paper mills now use the same local stock execution path as bakeries and smithies. A recipe can allow a final partial output batch. That final batch pays its full input and work cost: one through four Paper costs one input bundle; five through eight costs two. The output ceiling also respects the mill's weekly capacity.

Historical policies remain in their original order: schema 33 uses Wood; schemas 34 through 36 use Wheat with the food floor; schema 37 onward uses Wood behind the civilian food buffer. Current mills require their service, workers with food, and Tools. Tool wear runs once after a successful weekly act. Prices and events use the shared receipt quantities.

Validation:

- Strict Debug build and all 74 headless tests passed.
- The common recipe test checks partial outputs from one through nine, full input/work charges, reserve floors, repeated full-store attempts and integer limits.
- Weekly mill tests exercise capacities one through nine, exact Wood use, unchanged Wheat and the existing once-per-act tool wear.
- The parity probe matched 480 annual hashes against the prior commit: two seeds, 40 years each, across rule versions 33, 34, 36, 37, 58 and 62.
- These probes select historical rules after current initialization. The full SQLite suite separately covers shipped saves, legacy journals and save/resume.

To reproduce parity, compile parity_probe.c with each checkout's src include path and libcrownless_sim.a, run both executables, and compare their complete output. Seeds and output digest are in parity.json.
