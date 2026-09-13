"""Check pair changes, evidence labels, byte spans, and game-sized accounts."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('builder', ROOT / 'tools/build_core_diagnostic.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class DiagnosticTests(unittest.TestCase):
    def test_pair_and_native_contract(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'data'
            with contextlib.redirect_stdout(io.StringIO()): builder.build(output, pairs=144)
            data = json.loads((output / 'rules.json').read_text())
            rules = {r['id']: r for r in data['rules']}
            coverage = json.loads((output / 'manifest.json').read_text())['coverage']['events']
            kinds = {r['kind']: r['value'] for r in coverage}
            for split in ('train', 'validation', 'test', 'wording'):
                rows = [json.loads(line) for line in (output / f'{split}.jsonl').read_text().splitlines()]
                for first, second in zip(rows[::2], rows[1::2]):
                    self.assertEqual(first['pair'], second['pair'])
                    changed = [a['field'] for a, b in zip(first['fields'], second['fields']) if a['text'] != b['text']]
                    self.assertEqual(changed, [] if first['changed_field'] is None else [first['changed_field']])
                for row in rows:
                    raw = row['prefix'].encode()
                    for field in row['fields']:
                        self.assertEqual(raw[field['start']:field['end']].decode(), field['text'])
                    for span in row['copies']:
                        self.assertEqual(row['output'].encode()[span['start']:span['end']].decode(), span['text'])
                if split != 'train': continue
                for row in rows[:96]:
                    rule = rules[row['rule']]
                    values = [field['text'] for field in sorted(row['fields'], key=lambda f: f['field'])]
                    account = rule['source'].format(*values)
                    self.assertLessEqual(len(account.encode()), 143)
                    result = subprocess.check_output([sys.argv[1], str(kinds[row['kind']]), '80',
                                                      str(row['variant']), account], text=True).strip()
                    expected = rule['outputs'][row['variant']].format(*values)
                    expected = expected[0].upper() + expected[1:]
                    self.assertEqual(result, expected)


if __name__ == '__main__': unittest.main(argv=[sys.argv[0]])
