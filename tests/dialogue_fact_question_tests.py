"""Question parsing and grammar-field adaptation for typed fact selection."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/dialogue'))
import fact_policy as f
import fact_roles
from fact_question import (ROLE_BY_NUMBER, facts_from_fields, parse_question,
                           person, role_from_question)


class QuestionParserTests(unittest.TestCase):
    def test_role_keywords(self):
        cases = {
            'Where was the notice posted?': 'place',
            'Who posted that?': 'actor',
            'What was the notice about?': 'object',
            'What was the paper made from?': 'material',
            'How many loaves?': 'quantity',
            'Which band was that?': 'group',
            'Who did they declare war on?': 'recipient',
            'Whom did the new ruler replace?': 'recipient',
            'Which band did they join?': 'recipient',
            'Who needed the supplies?': 'recipient',
            'Whose blessing did they seek?': 'group',
        }
        for text, role in cases.items():
            self.assertEqual(role_from_question(text), role, text)

    def test_predicates_are_finer_than_roles(self):
        self.assertEqual(parse_question('Who needed the supplies?'), 'beneficiary')
        self.assertEqual(parse_question('Which band did they join?'), 'joined')
        self.assertEqual(parse_question('Where was it made?'), 'place')

    def test_negation_is_detected(self):
        from fact_question import is_negated
        self.assertTrue(is_negated('Who did not go?'))
        self.assertTrue(is_negated("He wasn't there."))
        self.assertFalse(is_negated('Who went?'))

    def test_unknown_question_returns_none(self):
        self.assertIsNone(role_from_question('Tell me more.'))

    def test_non_text_is_rejected(self):
        with self.assertRaises(ValueError):
            role_from_question(3)


class FieldAdapterTests(unittest.TestCase):
    fields = [{'field': 0, 'role': 1, 'text': 'Ada', 'knowledge': 0},
              {'field': 1, 'role': 3, 'text': 'Ash Hollow', 'knowledge': 0},
              {'field': 2, 'role': 8, 'text': '4', 'knowledge': 0},
              {'field': 3, 'role': 4, 'text': 'someone', 'knowledge': 3}]

    def test_fields_become_typed_facts(self):
        facts = facts_from_fields(self.fields, '1', '900', confidence=80, day=5)
        self.assertEqual([(x['role'], x['value']) for x in facts],
                         [('actor', 'Ada'), ('place', 'Ash Hollow'), ('quantity', '4')])
        self.assertEqual({x['certainty'] for x in facts}, {'witnessed'})
        self.assertEqual(len({x['fact_id'] for x in facts}), len(facts))

    def test_hidden_field_is_excluded(self):
        facts = facts_from_fields(self.fields, '1', '900')
        self.assertNotIn('object', [x['role'] for x in facts])

    def test_person_carries_the_parsed_question(self):
        facts = facts_from_fields(self.fields, '1', '900')
        p = person('1', '2', facts, 'place')
        self.assertEqual(p['question'], {'event_id': '900', 'role': 'place'})
        self.assertEqual(f.candidates(p)[f.preferred(p)]['role'], 'place')

    def test_person_rejects_unknown_role(self):
        with self.assertRaises(ValueError):
            person('1', '2', [], 'nowhere')

    def test_role_map_covers_the_grammar_numbers(self):
        self.assertEqual(ROLE_BY_NUMBER[6], 'material')
        self.assertEqual(ROLE_BY_NUMBER[8], 'quantity')


class ReconciliationTests(unittest.TestCase):
    def test_duplicate_spoken_role_is_resolved_by_predicate(self):
        fields = [{'field': 0, 'role': 3, 'text': 'Kelilowden', 'spoken': True, 'knowledge': 0},
                  {'field': 1, 'role': 3, 'text': 'Yorashormere', 'spoken': True, 'knowledge': 0}]
        self.assertEqual(fact_roles.resolve_field('harvest_failed_0', 'beneficiary', fields), 1)
        self.assertEqual(fact_roles.resolve_field('harvest_failed_0', 'place', fields), 0)

    def test_joined_band_is_the_recipient_field(self):
        fields = [{'field': 0, 'role': 1, 'text': 'Ada', 'spoken': True, 'knowledge': 0},
                  {'field': 1, 'role': 2, 'text': 'The Tallow Knives', 'spoken': True, 'knowledge': 0}]
        self.assertEqual(fact_roles.resolve_field('bandit_pressure_1', 'joined', fields), 1)

    def test_missing_role_returns_none(self):
        fields = [{'field': 0, 'role': 1, 'text': 'Ada', 'spoken': True, 'knowledge': 0}]
        self.assertIsNone(fact_roles.resolve_field('notice_posted_0', 'place', fields))

    def test_audit_flags_duplicate_spoken_roles(self):
        rules = [{'id': 'harvest_failed_0', 'roles': ['place', 'place']},
                 {'id': 'notice_posted_0', 'roles': ['actor', 'place', 'object']},
                 {'id': 'bakery_production_0', 'roles': ['place', 'quantity', 'quantity']}]
        flags = {flag['rule']: flag['duplicate_spoken'] for flag in fact_roles.audit(rules)}
        self.assertEqual(flags, {'harvest_failed_0': {'place': 2}})


class FixtureTests(unittest.TestCase):
    def test_fixture_questions_parse(self):
        fixture = json.loads((ROOT / 'tools/dialogue/fixtures/fact-dialogue.json').read_text())
        for account in fixture['accounts']:
            for question in account['questions']:
                self.assertIsNotNone(parse_question(question), question)


if __name__ == '__main__':
    unittest.main()