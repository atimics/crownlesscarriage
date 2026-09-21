"""Question parsing and grammar-field adaptation for typed fact selection."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/dialogue'))
import fact_policy as f
from fact_question import (ROLE_BY_NUMBER, facts_from_fields, person,
                           role_from_question)


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
        }
        for text, role in cases.items():
            self.assertEqual(role_from_question(text), role, text)

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
        self.assertEqual({x['certainty'] for x in facts}, {'told'})
        self.assertEqual(len({x['fact_id'] for x in facts}), len(facts))

    def test_confident_hearsay_stays_received(self):
        known = facts_from_fields(self.fields, '1', '900', confidence=100, day=5, source='told')
        p = person('1', '2', known, 'actor', day=8)
        answer = f.render(f.candidates(p)[f.preferred(p)], p)
        self.assertTrue(answer.startswith('I was told:'))
        self.assertNotIn('saw', answer)
        self.assertEqual(known[0]['confidence'], 100)
        self.assertIn('day 5', answer)

    def test_uncertain_eyewitness_remains_an_eyewitness(self):
        known = facts_from_fields(self.fields, '1', '900', confidence=20, day=5, source='observed')
        p = person('1', '2', known, 'actor', day=8)
        answer = f.render(f.candidates(p)[f.preferred(p)], p)
        self.assertIn('saw it myself, but I am uncertain', answer)
        self.assertEqual(known[0]['source'], 'observed')
        self.assertEqual(known[0]['certainty'], 'doubtful')

    def test_acquisition_not_legacy_certainty_controls_rendering(self):
        known = facts_from_fields(self.fields, '1', '900', confidence=90, day=5)
        known[0]['certainty'] = 'witnessed'
        p = person('1', '2', known, 'actor', day=8)
        self.assertTrue(f.render(f.candidates(p)[f.preferred(p)], p).startswith('I was told:'))

    def test_invalid_source_or_confidence_rejected(self):
        with self.assertRaises(ValueError):
            facts_from_fields(self.fields, '1', '900', source='omniscient')
        for invalid in (True, -1, 101, float('nan'), 'high'):
            with self.assertRaises(ValueError):
                facts_from_fields(self.fields, '1', '900', confidence=invalid)

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


class FixtureTests(unittest.TestCase):
    def test_fixture_questions_parse(self):
        fixture = json.loads((ROOT / 'tools/dialogue/fixtures/fact-dialogue.json').read_text())
        for account in fixture['accounts']:
            for question in account['questions']:
                self.assertIsNotNone(role_from_question(question), question)


if __name__ == '__main__':
    unittest.main()