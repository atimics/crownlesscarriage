"""Check JSON against the existing controlled road-recovery fixture."""
import json
import subprocess
import sys
rows = [json.loads(line) for line in subprocess.check_output([sys.argv[1]], text=True).splitlines()]
assert len(rows) == 9
expected = [[], ['food'], ['wood'], ['stone'], ['tools'], [], ['calendar'], ['open'], ['invalid']]
for row, reasons in zip(rows, expected):
    assert row['semantics'] == 'evaluated_plan_snapshot'
    assert row['blocked_reasons'] == reasons
    assert isinstance(row['labor_base_id'], str) and isinstance(row['supplier_id'], str)
assert rows[0] == rows[5]
assert rows[0]['population'] == 300 and rows[0]['food_rations'] == 20
assert rows[0]['wood'] == rows[0]['stone'] == 2 and rows[0]['tools'] == 1
assert rows[0]['effort'] == 6 and rows[0]['people_used'] == 1
assert rows[6]['next_work_day'] == 112
for field in ['population', 'food_rations', 'wood', 'stone', 'tools', 'effort', 'people_used', 'next_work_day']:
    assert rows[8][field] is None
print('Verified actual recovery gates, restored prerequisites, unavailable values, and read-only JSON')
