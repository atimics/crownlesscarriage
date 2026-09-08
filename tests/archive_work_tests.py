import json
import subprocess
import sys
rows = [json.loads(line) for line in subprocess.check_output([sys.argv[1]], text=True).splitlines()]
assert len(rows) == 9
for row in rows:
    assert row['semantics'] == 'held_supply_plan_snapshot'
    assert isinstance(row['seat_id'], str)
assert rows[0] == rows[2]
assert rows[0]['nominal_scribes'] == 3 and rows[0]['eligible_scribes'] == 2
assert rows[0]['wheat_required'] == 4 and rows[0]['recording_ready']
for index in [1, 5, 6, 7]:
    assert rows[index]['eligible_scribes'] == rows[index]['wheat_required'] == 0
    assert not rows[index]['recording_ready']
for index in [3, 4]:
    assert rows[index]['eligible_scribes'] == 2 and rows[index]['wheat_required'] == 4
    assert not rows[index]['recording_ready']
assert rows[6]['seat_id'] == '0'
assert rows[8]['seat_id'] == '0' and rows[8]['eligible_scribes'] == 3
assert rows[8]['wheat_required'] == 0 and rows[8]['recording_ready']
print('Verified grain limits, paper and tool gates, restored grain, empty seat, and legacy rules')
