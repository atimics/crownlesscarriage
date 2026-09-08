# Long-world comparison rule

This extends simulation source 06e5701c6d6c09bc25430685a9fd917b81a42972 with read-only campaign metrics. It uses metrics seeds 1–32, each for 128,000 years. Checkpoints are 1,000 years and repeated doublings through 128,000 years. Every year is validated and contributes to statistics.

For each seed and checkpoint, compare the average over the final quarter of its history, with a minimum window of 1,000 years. A practical plateau requires both the cohort mean and at least 80% of seeds to stay within tolerance at every later doubling. The reported plateau year is the first checkpoint from which those later comparisons hold. The year 128,000 endpoint alone cannot establish a plateau.

Tolerances: 1% for population, treasure value and dragon hoard; one point for hunger, prosperity and legitimacy; 0.1 for live treasure, stored lore, active settlements and closed routes. Relative changes use the older window's absolute mean, with a floor of one. These are descriptive choices for this experiment. The report also gives changes and variation so readers can apply different thresholds.

Plateaus in counts and means are interpreted alongside annual changes, treasure-identity changes, and recent creation dates. Treasure identity changes count annual observations at which the sequence of live treasure IDs differs from the prior year. Short-lived objects between annual observations can be missed. Location, ownership and value changes alone preserve that identity hash.

The plateau charts use the same completed seed cohort at every age. A separate coverage table includes all 32 requested seeds. Failed seeds retain their logs and partial samples. This separates changes with age from changes in which seeds remain valid; selection into the completed cohort still limits the result. Earlier 128-world results remain a broader baseline.
