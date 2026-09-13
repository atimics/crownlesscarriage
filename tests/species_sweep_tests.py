"""Check observational parity, counters, and sampled-history boundaries."""
import csv
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('species', ROOT / 'tools/species_sweep.py')
species = importlib.util.module_from_spec(spec)
spec.loader.exec_module(species)
BINARY, BASELINE = sys.argv[1:3]
del sys.argv[1:3]


def run(binary, *args):
    return list(csv.DictReader(io.StringIO(subprocess.check_output([binary, *args], text=True))))


class SpeciesTests(unittest.TestCase):
    def test_observation_preserves_world_and_count_scope(self):
        observed = run(BINARY, '--seed', '7', '--years', '2')
        baseline = run(BASELINE, '--seed', '7', '--years', '2', '--campaign-metrics')
        self.assertEqual([int(r['state_hash'], 16) for r in observed[1:]], [int(r['state_hash']) for r in baseline])
        self.assertEqual(observed[0]['goblin'], '48')
        for row in observed:
            self.assertEqual(int(row['pony']), int(row['common_ponies']) + 7)
            self.assertIn(int(row['dragon']), (0, 1))
            self.assertEqual(sum(int(row[k]) for k in ('hunger_days', 'equipment_days', 'tribute_days')), int(row['active_days']))
            self.assertLessEqual(int(row['active_days']), int(row['year']) * 365)
            self.assertLessEqual(int(row['empty_raids']), int(row['raids']))
            self.assertEqual(row['saturated_days'], '0')
        for obs, base in zip(observed[1:], baseline):
            self.assertEqual(obs['human'], base['total_population'])
            self.assertEqual(obs['goblin'], base['goblin_members_end'])
            self.assertEqual(obs['tribute_events'], base['goblin_tributes'])

    def test_long_sample_schedule_and_integrity(self):
        rows = run(BINARY, '--seed', '1', '--years', '26')
        self.assertEqual([int(r['year']) for r in rows], list(range(11)) + [25, 26])
        species.validate_rows(rows, 1, 26)
        for broken in (rows[:-1], rows[::-1], [dict(rows[0], world_seed='0')] + rows[1:]):
            with self.assertRaises(ValueError):
                species.validate_rows(broken, 1, 26)

    def test_failed_seed_keeps_partial_rows(self):
        from unittest.mock import patch
        from types import SimpleNamespace
        with patch.object(species.subprocess, 'run', return_value=SimpleNamespace(
            stdout='seed_number,world_seed,year,day\n1,2654435769,0,1\n',
            stderr='validation failure', returncode=1)):
            result = species.run_seed('fake', 1, 1000)
        self.assertFalse(result['passed'])
        self.assertIsNone(result['endpoint'])
        self.assertEqual(result['samples'][0]['year'], '0')
        self.assertIn('validation failure', result['error'])

    def test_input_bounds(self):
        for args in (['--years', '0'], ['--years', '2147483647'], ['--seed', '-1'], ['--seed', 'oops']):
            self.assertNotEqual(subprocess.run([BINARY, *args], capture_output=True).returncode, 0)


if __name__ == '__main__':
    unittest.main()
