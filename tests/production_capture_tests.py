"""Exercise capture records through controlled runner failures."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/capture_production.py'
RUNNER = '''import json, sys
from pathlib import Path
save = Path(sys.argv[sys.argv.index('--save') + 1])
manifest = json.loads((save.parent / 'manifest.json').read_text())
assert manifest['status'] == 'running'
assert manifest['runs'][-1]['captures'][-1]['status'] == 'running'
count = sum(len(group['captures']) for group in manifest['runs'])
mode = Path(__file__).with_suffix('.mode').read_text()
print('runner invocation ' + str(count), file=sys.stderr)
if mode == 'exit' and count == 3:
    print('{"partial":', flush=True)
    sys.exit(7)
if mode != 'missing-save':
    save.write_text('controlled fixture')
if mode == 'malformed':
    print('broken JSON')
else:
    print(json.dumps({'day': 2 if mode == 'wrong-day' else 1,
        'state_hash': str(count) if mode == 'mismatch' else 'fixed'}))
'''

class CaptureRecords(unittest.TestCase):
    def capture(self, mode):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        runner = root / 'runner'
        runner.write_text('#!' + sys.executable + '\n' + RUNNER)
        runner.chmod(0o700)
        runner.with_suffix('.mode').write_text(mode)
        output = root / 'capture'
        result = subprocess.run([sys.executable, str(SCRIPT), '--runner', str(runner),
            '--output', str(output), '--years', '0'], capture_output=True, text=True)
        manifest = json.loads((output / 'manifest.json').read_text())
        self.assertFalse((output / 'manifest.json.tmp').exists())
        for group in manifest['runs']:
            for row in group['captures']:
                for label in ('report', 'save', 'stderr'):
                    path = output / row[label]
                    if path.exists():
                        self.assertEqual(row[label + '_sha256'], hashlib.sha256(path.read_bytes()).hexdigest())
        return result, manifest, output

    def test_success(self):
        result, manifest, _ = self.capture('success')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(manifest['status'], 'complete')
        self.assertEqual(len(manifest['runs']), 4)
        for group in manifest['runs']:
            self.assertTrue(group['repeat_match'])
            self.assertEqual([row['status'] for row in group['captures']], ['complete', 'complete'])

    def test_later_failure_preserves_completed_runs(self):
        result, manifest, output = self.capture('exit')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(manifest['status'], 'failed')
        self.assertTrue(manifest['runs'][0]['repeat_match'])
        row = manifest['runs'][1]['captures'][0]
        self.assertEqual(row['status'], 'failed')
        self.assertEqual(row['exit_code'], 7)
        self.assertIn('runner invocation 3', (output / row['stderr']).read_text())
        self.assertNotIn('final_state_hash', row)
        self.assertIsNone(manifest['runs'][1]['repeat_match'])

    def test_invalid_output(self):
        for mode in ('malformed', 'wrong-day', 'missing-save'):
            with self.subTest(mode=mode):
                result, manifest, _ = self.capture(mode)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(manifest['status'], 'failed')
                row = manifest['runs'][0]['captures'][0]
                self.assertEqual(row['status'], 'failed')
                self.assertEqual(row['exit_code'], 0)
                self.assertIn('error', row)

    def test_repeat_mismatch_is_explicit(self):
        result, manifest, _ = self.capture('mismatch')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(manifest['status'], 'failed')
        group = manifest['runs'][0]
        self.assertFalse(group['repeat_match'])
        self.assertEqual([row['status'] for row in group['captures']], ['complete', 'complete'])

if __name__ == '__main__':
    unittest.main()
