"""Grammar-source parsing and question-to-field binding for held accounts."""
import json
import os
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/dialogue'))
import fact_binding
import event_facts


def participant(account, kind_name='NOTICE_POSTED'):
    return {'self': {'id': '1'}, 'listener': {'id': '2'}, 'day': 10,
            'held_accounts': [{'event_id': '51', 'source_id': '1', 'day': 1,
                               'kind': event_facts.EVENT_KIND_REGISTRY[kind_name],
                               'confidence': 80, 'private': False, 'account': account}],
            'knowledge': []}


class ParseTests(unittest.TestCase):
    def setUp(self):
        self.rules = fact_binding.load_rules()

    def test_source_template_parses_fields(self):
        rule = next(r for r in self.rules['NOTICE_POSTED'] if r['id'] == 'notice_posted_0')
        fields = fact_binding.parse_account(
            'Yorororholt Kelumeth posts a notice at Yororormere: Yorilashfell Cup.', rule)
        self.assertEqual({f['field']: f['text'] for f in fields},
                         {0: 'Yorororholt Kelumeth', 1: 'Yororormere', 2: 'Yorilashfell Cup'})

    def test_cues_and_context_are_stripped(self):
        rule = next(r for r in self.rules['NOTICE_POSTED'] if r['id'] == 'notice_posted_0')
        text = '- Kelaethfell cut wood.\n- ? ~ Yorororholt Kelumeth posts a notice at Yororormere: Yorilashfell Cup.'
        fields = fact_binding.parse_account(text, rule)
        self.assertEqual(fields[1]['text'], 'Yororormere')

    def test_non_matching_text_returns_none(self):
        rule = next(r for r in self.rules['NOTICE_POSTED'] if r['id'] == 'notice_posted_0')
        self.assertIsNone(fact_binding.parse_account('The mill is quiet.', rule))


class BindingTests(unittest.TestCase):
    def setUp(self):
        self.rules = fact_binding.load_rules()
        self.person = participant('Yorororholt Kelumeth posts a notice at Yororormere: Yorilashfell Cup.')

    def test_routes_each_question_to_its_field(self):
        expected = {'Who posted that?': 'Yorororholt Kelumeth',
                    'Where was the notice posted?': 'Yororormere',
                    'What was the notice about?': 'Yorilashfell Cup'}
        for question, value in expected.items():
            selected = fact_binding.select(self.person, question, self.rules)
            self.assertIsNotNone(selected, question)
            self.assertEqual(selected['value'], value, question)
            self.assertIn(value, fact_binding.render(selected))

    def test_unknown_question_returns_none(self):
        self.assertIsNone(fact_binding.select(self.person, 'Tell me more.', self.rules))

    def test_private_account_is_not_selected(self):
        person = participant('Yorororholt Kelumeth posts a notice at Yororormere: Yorilashfell Cup.')
        person['held_accounts'][0]['private'] = True
        self.assertIsNone(fact_binding.select(person, 'Where was the notice posted?', self.rules))


class PolicyRoutingTests(unittest.TestCase):
    """policy.make_act binds the account that answers a supplied question."""

    def setUp(self):
        import policy
        from policy_data import fixture
        self.policy = policy
        self.rules = fact_binding.load_rules()
        self.person = fixture(1, '1', '2')
        self.person['day'] = 10
        self.person['memories'] = []
        self.person['held_accounts'] = [
            {'event_id': '51', 'source_id': '9', 'day': 1, 'confidence': 80, 'private': False,
             'kind': event_facts.EVENT_KIND_REGISTRY['NOTICE_POSTED'],
             'account': 'Yorororholt Kelumeth posts a notice at Yororormere: Yorilashfell Cup.'},
            {'event_id': '52', 'source_id': '9', 'day': 5, 'confidence': 80, 'private': False,
             'kind': event_facts.EVENT_KIND_REGISTRY['WAR_DECLARED'],
             'account': "Kelenumden Kelowowmo's courier reaches Yorenenford Yoraorme: war now binds the two courts."},
        ]

    def test_routed_claim_is_not_the_default_fact(self):
        typed = fact_binding.typed_facts(self.person, self.rules)
        notice = next(a for a in typed if a['kind_name'] == 'NOTICE_POSTED')
        default_fact = sorted(typed, key=lambda a: (-a['day'], a['account_ref']))[0]
        self.assertNotEqual(default_fact['account_ref'], notice['account_ref'])
        act = self.policy.make_act('report_fact', self.person, [], requested='learn',
                                   question='What was the notice about?')
        self.assertEqual(act['claim']['ref'], notice['account_ref'])

    def test_unmatched_question_keeps_the_default_fact(self):
        typed = fact_binding.typed_facts(self.person, self.rules)
        default_fact = sorted(typed, key=lambda a: (-a['day'], a['account_ref']))[0]
        act = self.policy.make_act('report_fact', self.person, [], requested='learn',
                                   question='Tell me more.')
        self.assertEqual(act['claim']['ref'], default_fact['account_ref'])

    def test_act_validates_with_the_same_question(self):
        act = self.policy.make_act('report_fact', self.person, [], requested='learn',
                                   question='Where was the notice posted?')
        self.assertEqual(self.policy.validate(act, self.person, [], 'learn',
                                              'Where was the notice posted?'), act)


class NativeValidation(unittest.TestCase):
    """The Python parser must recover the native grammar parse exactly."""

    def test_all_grammar_rows_match_the_native_parse(self):
        zero = Path(os.environ.get('ZERO_REPO', '/Users/ratimics/develop/zero'))
        rows = zero / 'experiments/full-event-rehearsal/input/grammar-test.jsonl'
        if not rows.exists():
            self.skipTest('ZERO grammar rows are not available')
        rules = fact_binding.load_rules()
        by_id = {rule['id']: rule for group in rules.values() for rule in group}
        checked = 0
        for line in rows.read_text(encoding='utf-8').splitlines():
            row = json.loads(line)
            rule = by_id.get(row['rule'])
            if rule is None:
                continue
            fields = fact_binding.parse_account(row['prefix'], rule)
            native = {f['field']: f['text'] for f in row['fields']}
            self.assertIsNotNone(fields, row['rule'])
            self.assertEqual({f['field']: f['text'] for f in fields}, native, row['rule'])
            checked += 1
        self.assertGreater(checked, 150)


if __name__ == '__main__':
    unittest.main()