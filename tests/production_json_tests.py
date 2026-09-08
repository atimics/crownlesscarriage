#!/usr/bin/env python3
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

runner = str(Path(sys.argv[1]).resolve())

def run(*args):
    return subprocess.check_output([runner, *args], text=True)

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    for fixture in [[], ['--opened-production-pilots'], ['--dragon-slain-day-one'],
                    ['--opened-production-pilots', '--dragon-slain-day-one']]:
        base = ['--seed', '0x5eed0001', '--years', '2', *fixture]
        report = run(*base, '--json', '--save', str(root / 'json.ccsave'))
        assert report == run(*base, '--json')
        rows = [json.loads(line) for line in report.splitlines()]
        assert [row['day'] for row in rows] == [1, 366, 731]
        assert all(row['threats']['semantics'] == 'snapshot' for row in rows)
        policy = 'slain-at-day-1' if '--dragon-slain-day-one' in fixture else 'natural-history'
        assert all(row['dragon_policy'] == policy and row['comparison_scope'] == 'whole-policy' for row in rows)
        text = run(*base, '--save', str(root / 'text.ccsave'))
        hashes = re.findall(r'\bhash=([0-9a-f]+)', text)
        assert hashes == [row['state_hash'] for row in rows[1:]]
        for name in ['json', 'text']:
            loaded = json.loads(run('--load', str(root / f'{name}.ccsave'), '--years', '0', '--json'))
            assert loaded['state_hash'] == rows[-1]['state_hash']
            assert loaded['threats'] == rows[-1]['threats']
            assert loaded['campaign_launch'] == rows[-1]['campaign_launch']
            assert loaded['ritual_offering'] == rows[-1]['ritual_offering']
            assert loaded['retained_history'] == rows[-1]['retained_history']
            assert loaded['accounting_start_day'] == 731
            assert loaded['dragon_policy'] == 'loaded-save'
            assert all(sum(site['input']) == 0 for site in loaded['sites'])
        assert all(isinstance(town['id'], str) for town in rows[-1]['towns'])
        assert rows[-1]['protocol'] == 6
        for town in rows[-1]['towns']:
            herds = town['herds']
            assert herds['dairy_nutrition'] == herds['dairy_used'] + herds['dairy_unused']
            for species in ['cows', 'sheep', 'ponies']:
                for field in ['feed', 'output', 'cap_loss']:
                    assert len(herds[species][field]) == len(rows[-1]['goods'])
                    assert all(value >= 0 for value in herds[species][field])
            assert sum(herds['ponies']['output']) == 0
            production = town['production']
            assert production['active_weeks'] + production['inactive_weeks'] == 104
            assert production['bakery']['input'][7] == production['bakery']['output'][0]
            assert production['paper']['output'][rows[-1]['goods'].index('Paper')] <= production['paper']['work'] * 4

        assert sum(sum(town['herds']['cows']['feed']) for town in rows[-1]['towns']) > 0
        assert sum(town['herds']['sheep']['output'][rows[-1]['goods'].index('Wool')] for town in rows[-1]['towns']) > 0
        assert all(route['open_days'] + route['closed_days'] == 730 for route in rows[-1]['routes'])
        assert all(route['recovery']['semantics'] == 'evaluated_plan_snapshot' for row in rows for route in row['routes'])
        for start, end in zip(rows[0]['sites'], rows[-1]['sites']):
            assert end['condition'] == start['condition'] + end['site_repair'] - end['wear']
            for good in range(len(rows[-1]['goods'])):
                assert end['stock'][good] == start['stock'][good] + end['output'][good] - end['input'][good] - end['maintenance_input'][good] + end['received'][good] - end['shipped'][good]
                aboard = sum(cargo['quantity'] for cargo in rows[-1]['shipments']
                             if cargo['good'] == good and cargo['status'] in [1, 4]
                             and end['id'] in [cargo['origin_id'], cargo['destination_id']])
                assert end['sent'][good] + end['shipped'][good] == end['received'][good] + end['delivered'][good] + end['lost'][good] + aboard
        if '--opened-production-pilots' in fixture:
            assert rows[-1]['sites'][2]['output'][0] >= 4
            assert rows[-1]['sites'][11]['output'][2] >= 4
            assert rows[-1]['sites'][15]['output'][7] >= 16
    natural = json.loads(run('--json', '--seed', '0x5eed0001', '--years', '0'))
    controlled = json.loads(run('--json', '--seed', '0x5eed0001', '--years', '0', '--dragon-slain-day-one'))
    assert natural['dragon']['slain'] is False
    assert controlled['dragon']['slain'] is True and controlled['dragon']['slain_day'] == 1
    assert controlled['dragon']['body_condition'] == 0 and controlled['dragon']['crown_strength'] == 0
    assert controlled['dragon']['eggs'] == natural['dragon']['eggs']
    assert controlled['state_hash'] != natural['state_hash']
    # The deliberate slain policy adds exactly its evaluated campaign gate.
    slain_gate = 1 << 3
    natural_plan = natural['campaign_launch']
    controlled_plan = controlled['campaign_launch']
    assert controlled_plan['blocked_mask'] == natural_plan['blocked_mask'] | slain_gate
    assert controlled_plan['blocked_reasons'] == ['dragon_slain', *natural_plan['blocked_reasons']]
    controlled_plan['blocked_mask'] &= ~slain_gate
    controlled_plan['blocked_reasons'].remove('dragon_slain')
    for field in ['dragon', 'dragon_policy', 'state_hash']:
        natural.pop(field)
        controlled.pop(field)
    assert natural == controlled
    invalid = subprocess.run([runner, '--dragon-slain-day-one', '--load', str(root / 'json.ccsave')], capture_output=True)
    assert invalid.returncode != 0 and not invalid.stdout
    invalid = subprocess.run([runner, '--json', '--chronicle'], capture_output=True)
    assert invalid.returncode != 0 and not invalid.stdout
    invalid = subprocess.run([runner, '--opened-production-pilots', '--load', str(root / 'json.ccsave')], capture_output=True)
    assert invalid.returncode != 0
    script = Path(__file__).resolve().parents[1] / 'tools/capture_production.py'
    subprocess.run([sys.executable, str(script), '--runner', runner, '--output', str(root / 'capture'), '--years', '0'], check=True, capture_output=True)
    manifest = json.loads((root / 'capture/manifest.json').read_text())
    assert len(manifest['commit']) == 40 and len(manifest['runner_sha256']) == 64
    assert len(manifest['runs']) == 4
    assert all(row['repeat_match'] for row in manifest['runs'])
print('Production JSON: repeatability, save/text parity, custody accounting and capture manifest passed')
