"""Check endpoint state and official carriage rules as separate snapshots."""
import json
import subprocess
import sys
rows = [json.loads(line) for line in subprocess.check_output([sys.argv[1]], text=True).splitlines()]
assert len(rows) == 10
for row in rows:
    assert row['semantics'] == 'snapshot'
    assert all(isinstance(rule['kingdom_id'], str) for rule in row['royal_carriage_rules'])
assert rows[0]['from']['population'] == 300 and rows[0]['to']['population'] == 100
assert rows[0]['from']['inhabited'] and rows[0]['to']['inhabited']
assert rows[0]['from']['kingdom_id'] != rows[0]['to']['kingdom_id']
assert rows[0]['crosses_kingdom_border'] and not rows[0]['crosses_war_border']
assert rows[1]['from']['inhabited'] and not rows[1]['to']['inhabited']
assert not rows[2]['from']['inhabited'] and not rows[2]['to']['inhabited']
assert rows[0] == rows[3]  # Official eligibility is separate from physical closure.
assert all(rule['official_route_eligible'] for rule in rows[0]['royal_carriage_rules'])
assert rows[4]['crosses_war_border'] and rows[5]['smuggler_route']
for index in [4, 5, 6, 9]:
    assert all(not rule['official_route_eligible'] for rule in rows[index]['royal_carriage_rules'])
assert not rows[7]['crosses_kingdom_border']
assert all(rule['official_route_eligible'] for rule in rows[7]['royal_carriage_rules'])
assert [rule['official_route_eligible'] for rule in rows[8]['royal_carriage_rules']] == [True, False, True]
assert rows[9]['to'] is None
print('Verified inhabited endpoints, ruins, borders, official carriage rules, and state preservation')
