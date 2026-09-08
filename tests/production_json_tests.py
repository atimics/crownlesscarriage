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
    for fixture in [[], ['--opened-production-pilots']]:
        base = ['--seed', '0x5eed0001', '--years', '2', *fixture]
        report = run(*base, '--json', '--save', str(root / 'json.ccsave'))
        assert report == run(*base, '--json')
        rows = [json.loads(line) for line in report.splitlines()]
        assert [row['day'] for row in rows] == [1, 366, 731]
        text = run(*base, '--save', str(root / 'text.ccsave'))
        hashes = re.findall(r'\bhash=([0-9a-f]+)', text)
        assert hashes == [row['state_hash'] for row in rows[1:]]
        for name in ['json', 'text']:
            loaded = json.loads(run('--load', str(root / f'{name}.ccsave'), '--years', '0', '--json'))
            assert loaded['state_hash'] == rows[-1]['state_hash']
            assert loaded['accounting_start_day'] == 731
            assert all(sum(site['input']) == 0 for site in loaded['sites'])
        assert all(isinstance(town['id'], str) for town in rows[-1]['towns'])
        assert rows[-1]['protocol'] == 2
        for town in rows[-1]['towns']:
            production = town['production']
            assert production['active_weeks'] + production['inactive_weeks'] == 104
            assert production['bakery']['input'][7] == production['bakery']['output'][0]
            assert production['paper']['output'][8] <= production['paper']['work'] * 4

        assert all(route['open_days'] + route['closed_days'] == 730 for route in rows[-1]['routes'])
        for start, end in zip(rows[0]['sites'], rows[-1]['sites']):
            for good in range(len(rows[-1]['goods'])):
                assert end['stock'][good] == start['stock'][good] + end['output'][good] - end['input'][good] + end['received'][good] - end['shipped'][good]
                aboard = sum(cargo['quantity'] for cargo in rows[-1]['shipments']
                             if cargo['good'] == good and cargo['status'] in [1, 4]
                             and end['id'] in [cargo['origin_id'], cargo['destination_id']])
                assert end['sent'][good] + end['shipped'][good] == end['received'][good] + end['delivered'][good] + end['lost'][good] + aboard
        if fixture:
            assert rows[-1]['sites'][2]['output'][0] >= 4
            assert rows[-1]['sites'][11]['output'][2] >= 4
            assert rows[-1]['sites'][15]['output'][7] >= 16
    invalid = subprocess.run([runner, '--json', '--chronicle'], capture_output=True)
    assert invalid.returncode != 0 and not invalid.stdout
    invalid = subprocess.run([runner, '--opened-production-pilots', '--load', str(root / 'json.ccsave')], capture_output=True)
    assert invalid.returncode != 0
    script = Path(__file__).resolve().parents[1] / 'tools/capture_production.py'
    subprocess.run([sys.executable, str(script), '--runner', runner, '--output', str(root / 'capture'), '--years', '0'], check=True, capture_output=True)
    manifest = json.loads((root / 'capture/manifest.json').read_text())
    assert len(manifest['commit']) == 40 and len(manifest['runner_sha256']) == 64
    assert all(row['repeat_match'] for row in manifest['runs'])
print('Production JSON: repeatability, save/text parity, custody accounting and capture manifest passed')
