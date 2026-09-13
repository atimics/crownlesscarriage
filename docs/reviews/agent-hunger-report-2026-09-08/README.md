# Player sweep hunger reporting

Code revision deff730 compared with parent fadf76b using the commands in manifest.json. CSV files preserve all endpoint values and the new checkpoint fields.

All 73 headless Release tests passed. The reporting fixture covers healthy towns beside a hungry ruin, all-abandoned snapshots, distinct endpoint days, state hashes, and byte-for-byte unchanged simulation state.

Eight ten-year paired runs kept all existing columns equal. Two hundred-year paired runs changed only the hunger columns: seed 1 control 50 to 26, agent 36 to 23; seed 2 control and agent 36 to 5. These changes reflect the inhabited population measure. The simulation policy remains the same.

These are whole-policy endpoint comparisons. The CSV records actual final days because the agent completes its current journey. Population, prosperity, road closures, and action counters matched the parent in all ten paired cases.
