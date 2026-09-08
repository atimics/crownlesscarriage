"""Data integrity checks for the long-running paired sweep."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location('age', Path(__file__).resolve().parents[1] / 'tools/age_of_dragons_sweep.py')
age = importlib.util.module_from_spec(spec)
spec.loader.exec_module(age)


def row(year=1, seed=1):
    return dict(seed_number=str(seed), world_seed=str(seed * 0x9E3779B9 & 0xFFFFFFFF),
                year=str(year), day=str(year * 365 + 1))


class Integrity(unittest.TestCase):
    def test_complete_sequence(self):
        age.validate_rows([row(1), row(2)], 1, 2)

    def test_wrong_or_missing_year_seed_day(self):
        for rows in ([row()], [row(2), row(1)], [row(1, 2), row(2)],
                     [dict(row(), day='365'), row(2)],
                     [dict(row(), world_seed='1'), row(2)]):
            with self.assertRaises(ValueError):
                age.validate_rows(rows, 1, 2)

    def test_truncated_csv(self):
        with self.assertRaises(ValueError):
            age.validate_rows([dict(row(), state_hash=None)], 1, 1)

    def test_failed_runner_keeps_partial_history(self):
        output = 'seed_number,world_seed,year,day\n1,2654435769,1,366\n'
        with patch.object(age.subprocess, 'run', return_value=SimpleNamespace(
                stdout=output, stderr='Seed 1 failed in year 2: invalid state', returncode=1)):
            result = age.run_seed('/fake/metrics', 1, 2)
        self.assertFalse(result['passed'])
        self.assertIsNone(result['endpoint'])
        self.assertEqual(len(result['samples']), 1)
        self.assertIn('invalid state', result['error'])

    def test_successful_process_with_short_output_fails(self):
        with patch.object(age.subprocess, 'run', return_value=SimpleNamespace(
                stdout='seed_number,world_seed,year,day\n', stderr='', returncode=0)):
            self.assertFalse(age.run_seed('/fake/metrics', 1, 1000)['passed'])

    def test_gzip_roundtrip(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'rows.csv.gz'
            age.write_csv(path, [row()])
            with age.gzip.open(path, 'rt') as stream:
                self.assertEqual(list(age.csv.DictReader(stream)), [row()])


if __name__ == '__main__':
    unittest.main()
