"""Check retained history bounds independently of ring insertion order."""
import json
import subprocess
import sys
rows = [json.loads(line) for line in subprocess.check_output([sys.argv[1]], text=True).splitlines()]
assert len(rows) == 3
for row in rows:
    assert row['semantics'] == 'retained_records_snapshot'
    assert row['lifetime_event_count'] is None
assert rows[0]['retained_count'] == 0
assert rows[0]['earliest_retained_day'] is None and rows[0]['latest_retained_day'] is None
assert rows[1]['retained_count'] == 3
assert rows[1]['earliest_retained_day'] == 3 and rows[1]['latest_retained_day'] == 10
assert rows[2]['retained_count'] == rows[2]['capacity'] == 256
assert rows[2]['earliest_retained_day'] == 1 and rows[2]['latest_retained_day'] == 256
print('Verified empty, wrapped, unordered, and full retained history snapshots')
