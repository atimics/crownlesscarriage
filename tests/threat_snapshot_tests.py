"""Check empty, single, and multiple identified threat snapshots."""
import json
import re
import subprocess
import sys
lines = subprocess.check_output([sys.argv[1]], text=True).splitlines()
assert len(lines) == 6
for index, count in enumerate([0, 1, 3]):
    snapshot = json.loads(lines[index * 2])
    summary = lines[index * 2 + 1]
    assert snapshot['semantics'] == 'snapshot'
    bandits = snapshot['bandit_groups']
    monsters = snapshot['monster_groups']
    assert len(bandits) == len(monsters) == count
    assert len({row['id'] for row in bandits + monsters}) == 2 * count
    for i, row in enumerate(bandits):
        assert row['name'] == f'Group "{i}"\n'
        assert row['influence'] == (90 if i == 1 else 10 + i)
        assert isinstance(row['route_id'], str) and isinstance(row['camp_settlement_id'], str)
    for i, row in enumerate(monsters):
        assert row['pressure'] == (80 if i == 1 else 20 + i)
        assert isinstance(row['dungeon_id'], str)
    values = dict(re.findall(r'([a-z_]+)=([^ ]+)', summary))
    assert int(values['bandit_groups']) == count
    assert int(values['monster_groups']) == count
    assert values['bandit_influence_max'] == (str(max(x['influence'] for x in bandits)) if count else 'unavailable')
    assert values['monster_pressure_max'] == (str(max(x['pressure'] for x in monsters)) if count else 'unavailable')
print('Verified identified threats, maximum values, empty groups, and escaped names')
