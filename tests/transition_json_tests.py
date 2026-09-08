"""Check campaign and ritual JSON against their engine plan fixtures."""
import json
import subprocess
import sys
campaign, ritual = [[json.loads(line) for line in subprocess.check_output([binary], text=True).splitlines()] for binary in sys.argv[1:]]
assert len(campaign) == len(ritual) == 4
for rows, reason in [(campaign, 'food'), (ritual, 'coins')]:
    assert rows[0]['blocked_reasons'] == []
    assert rows[1]['blocked_reasons'] == [reason]
    assert rows[0] == rows[2]
assert campaign[0]['food_rations'] == 32
assert campaign[1]['food_rations'] == 0 and campaign[1]['prepare_eligible']
assert campaign[0]['pledged_count'] == 2
assert campaign[3]['cooldown_days'] == 9 and not campaign[3]['prepare_eligible']
assert campaign[3]['blocked_reasons'] == ['cooldown']
assert all(row['policy_intent'] is None for row in campaign)
assert all(row['semantics'] == 'held_supply_plan_snapshot' for row in campaign)
assert ritual[0]['coins'] == 120 and ritual[0]['planned_eggs'] == 1
assert ritual[1]['coins'] == 119
assert ritual[3]['days_remaining'] == 9 and ritual[3]['blocked_reasons'] == []
assert all(row['semantics'] == 'held_offering_plan_snapshot' for row in ritual)
print('Verified held prerequisites, restoration, separate timers, and read-only transition JSON')
