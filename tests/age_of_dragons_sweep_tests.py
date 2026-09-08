"""Data integrity checks for the long-running paired sweep."""
import importlib.util
import contextlib
import io
import json
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
    def test_failure_artifacts_and_resume_identity(self):
        with tempfile.TemporaryDirectory() as folder:
            binary = Path(folder) / 'metrics'
            binary.write_text('binary version one')
            output = Path(folder) / 'study'
            args = ['age', '--binary', str(binary), '--source', 'recorded-commit',
                    '--output', str(output), '--seeds', '2', '--years', '1', '--jobs', '1']
            def fake_run(_binary, seed, _years):
                return dict(seed=seed, passed=seed == 1, returncode=0 if seed == 1 else 1,
                            annual_rows=1, seconds=0, error='' if seed == 1 else 'invalid state',
                            endpoint=row(seed=seed) if seed == 1 else None,
                            samples=[row(seed=seed)])
            with patch('sys.argv', args), patch.object(age, 'run_seed', side_effect=fake_run), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(age.main(), 1)
            manifest = json.loads((output / 'manifest.json').read_text())
            self.assertEqual(manifest['passed'], 1)
            with age.gzip.open(output / 'history.csv.gz', 'rt') as stream:
                self.assertEqual([r['seed_number'] for r in age.csv.DictReader(stream)], ['1'])
            with age.gzip.open(output / 'partial-history.csv.gz', 'rt') as stream:
                self.assertEqual([r['seed_number'] for r in age.csv.DictReader(stream)], ['2'])
            with patch('sys.argv', args), patch.object(age, 'run_seed') as rerun:
                self.assertEqual(age.main(), 1)
                rerun.assert_not_called()
            binary.write_text('binary version two')
            with patch('sys.argv', args), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                age.main()
            self.assertEqual(error.exception.code, 2)

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
