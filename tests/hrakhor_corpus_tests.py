"""Keep authored language pairs grounded, separate, and faithful."""
import copy
import json
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from build_hrakhor_corpus import build, validate, DEFAULT_SOURCE, RULES

PROBE = Path(sys.argv.pop(1)).resolve()


class CorpusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = list(build(PROBE))
        cls.data = json.loads(DEFAULT_SOURCE.read_text())
        cls.rules = {r['id']: r for r in json.loads(RULES.read_text())['rules']}

    def test_native_pairs_and_meaning(self):
        self.assertEqual(len(self.rows), 36)
        self.assertEqual({r['split'] for r in self.rows}, {'train', 'development', 'test'})
        for row in self.rows:
            self.assertEqual(row['origin'], 'authored')
            self.assertEqual(re.findall(r'\d+', row['english']), re.findall(r'\d+', row['hrakhor']))
            for field in row['event_frame']['fields']:
                if field['role'] in ('actor', 'recipient', 'place', 'object', 'group'):
                    if field['value'] in row['english']:
                        self.assertIn(field['value'], row['hrakhor'])
            self.assertEqual(len(row['provenance']['source_sha256']), 64)
        rows = {r['id']: r for r in self.rows}
        self.assertEqual(rows['depart-1']['hrakhor'], 'Only if we go in daylight.')
        self.assertIn('may be hidden', rows['warned-1']['hrakhor'])
        self.assertIn('do not know whether', rows['rumour-2']['hrakhor'])
        self.assertIn('at least twenty years away', rows['rumour-0']['hrakhor'])
        self.assertIn("zhur'nukh", rows['seed-2']['hrakhor'])
        self.assertIn('keshuk', rows['brood-0']['hrakhor'])

    def test_repeatable_and_zero_strength(self):
        self.assertEqual(self.rows, list(build(PROBE)))
        self.assertTrue(all(r['english'] == r['hrakhor'] for r in build(PROBE, strength=0)))

    def test_split_leaks_fail(self):
        data = copy.deepcopy(self.data)
        extra = copy.deepcopy(data['scenarios'][0])
        extra.update(id='leaked-variant', split='test')
        data['scenarios'].append(extra)
        with self.assertRaises(ValueError):
            validate(data, self.rules)
        data = copy.deepcopy(self.data)
        data['scenarios'][-1]['turns'][0]['english'] = data['scenarios'][0]['turns'][0]['english']
        with self.assertRaises(ValueError):
            validate(data, self.rules)

    def test_bad_source_fails(self):
        data = copy.deepcopy(self.data)
        data['scenarios'][0]['fields'].pop()
        with self.assertRaises(ValueError):
            validate(data, self.rules)
        data = copy.deepcopy(self.data)
        data['scenarios'][0]['turns'][0]['english'] = 'first\nsecond'
        with self.assertRaises(ValueError):
            validate(data, self.rules)


if __name__ == '__main__':
    unittest.main()
