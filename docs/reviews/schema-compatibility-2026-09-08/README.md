# Schema compatibility query

Partial work for #260 and #386. Base: #565 at b25c4e8. Current schema 73, generator 25, SQLite format 32.

CcSimSupportsVersions exposes the existing schema/generator compatibility decision as a read-only query. The compatibility table and version ranges now live in cc_sim_versions.c. CcSimValidate calls the query in its original position, before checking clock and saved state. Version support is one part of complete save validation.

## Acceptance-set evidence

[Probe](parity_probe.c) compares the query with the parent's validator across schemas 0–80 and generators 0–32, plus maximum-integer boundaries. It uses an invalid clock to observe the parent's version decision before later state validation. All 2,676 decisions match, with 1,074 supported pairs. [Accepted pairs](accepted-pairs.txt) and [comparison record](parity.json) preserve the result.

[Fixture metadata](fixtures.json) records the actual schema/generator pair from each of the 44 shipped SQLite files. Every pair is accepted. [Fixture probe](fixture_probe.c) reads each file into memory, decodes and migrates it through the production loader, then advances seven days. [Parent/draft results](loaded-fixtures.txt) match in full, including the loaded schema/generator and both state hashes.

The historical journal and migration path remains owned by the save loader. This change gives the compatibility policy one public query and preserves the historical acceptance set.

Validation: strict Release headless build, all 101 tests, static analysis, 2,676 version decisions, and all 44 fixture comparisons passed. Broader field coverage and the remaining #386 matrix remain follow-up work.
