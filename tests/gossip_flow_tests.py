"""Compare observed and unobserved worlds and check trace identities."""
import json
import subprocess
import sys

binary = sys.argv[1]
command = [binary, '--seed', '73', '--days', '40']
first = subprocess.check_output(command, text=True)
assert first == subprocess.check_output(command, text=True)
rows = [json.loads(line) for line in first.splitlines()]
plain = [json.loads(line) for line in subprocess.check_output([*command, '--hash-only'], text=True).splitlines()]
assert rows[-1] == plain[-1] and rows[-1]['valid']
events = {r['id']: r for r in rows if r['type'] == 'event'}
assert len(events) == sum(r['type'] == 'event' for r in rows)
assert all(r['event_id'] in events for r in rows if r['type'] in ('held', 'story'))
stories = [r for r in rows if r['type'] == 'story']
assert any(r['recorded'] for r in stories)
assert any(len(r['towns']) > 1 for r in stories)
assert any(r['heard_day'] > 0 for r in stories)
assert len([r for r in rows if r['type'] == 'archive']) == 41
assert all(r['heard_day'] <= r['day'] for r in stories)
assert all(r['heard_event_id'] in events for r in stories if r['heard_day'] > 0)
for bad in ('-1', 'abc', '36501'):
    assert subprocess.run([binary, '--days', bad], capture_output=True).returncode == 2
print('Flow trace preserves the world and links observed accounts to events')
