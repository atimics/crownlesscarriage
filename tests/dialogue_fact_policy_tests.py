"""Typed fact selection routes to the fact the question asks for."""
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/dialogue'))
import fact_data
import fact_policy as f


def person(facts, asked):
    return {'self': {'id': '1', 'name': 'Ruk'}, 'listener': {'id': '2', 'name': 'Vesh'},
            'day': 12, 'facts': facts, 'question': asked}


def fact(event, role, value, certainty='told', day=3, private=False, owner='1', fid=None):
    return {'owner': owner, 'fact_id': fid or f'F{event}-{role}', 'event_id': event, 'role': role,
            'value': value, 'source': 'observed' if certainty == 'witnessed' else 'told',
            'certainty': certainty, 'day': day, 'private': private}


class FactPolicyTests(unittest.TestCase):
    def test_teacher_selects_the_asked_fact(self):
        p = person([fact('5000', 'actor', 'Ada'), fact('5000', 'place', 'Ash Hollow'),
                    fact('5001', 'place', 'Vault')], {'event_id': '5000', 'role': 'place'})
        chosen = f.candidates(p)[f.preferred(p)]
        self.assertEqual(chosen['fact_id'], 'F5000-place')

    def test_teacher_prefers_certainty_then_recency(self):
        p = person([fact('5000', 'place', 'Old', certainty='told', day=1, fid='A'),
                    fact('5000', 'place', 'Seen', certainty='witnessed', day=2, fid='B'),
                    fact('5000', 'place', 'New', certainty='told', day=9, fid='C')],
                   {'event_id': '5000', 'role': 'place'})
        self.assertEqual(f.candidates(p)[f.preferred(p)]['fact_id'], 'B')
        ranked = f.facts(p)
        self.assertEqual([x['value'] for x in ranked if x['certainty'] == 'witnessed'], ['Seen'])

    def test_defer_when_the_role_is_not_held(self):
        p = person([fact('5000', 'actor', 'Ada')], {'event_id': '5000', 'role': 'place'})
        chosen = f.candidates(p)[f.preferred(p)]
        self.assertEqual(chosen['kind'], 'defer')

    def test_private_facts_are_not_choices(self):
        p = person([fact('5000', 'actor', 'Ada', private=True), fact('5000', 'place', 'Ash Hollow')],
                   {'event_id': '5000', 'role': 'actor'})
        self.assertNotIn('F5000-actor', [c['fact_id'] for c in f.candidates(p)])
        self.assertEqual(f.candidates(p)[f.preferred(p)]['kind'], 'defer')

    def test_foreign_fact_is_rejected(self):
        p = person([fact('5000', 'actor', 'Ada', owner='2')], {'event_id': '5000', 'role': 'actor'})
        with self.assertRaises(ValueError):
            f.facts(p)

    def test_encoding_carries_a_bounded_candidate_block(self):
        p = person([fact('5000', 'actor', 'Ada'), fact('5000', 'place', 'Ash Hollow')],
                   {'event_id': '5000', 'role': 'place'})
        ids = f.encode_input(p)
        start = ids.index(1580) + 1
        stop = ids.index(1281, start)
        self.assertEqual(ids[start:stop], [1024, 1025, 1026])

    def test_render_names_the_source_and_day(self):
        p = person([fact('5000', 'place', 'Ash Hollow', certainty='witnessed', day=4)],
                   {'event_id': '5000', 'role': 'place'})
        text = f.render(f.candidates(p)[f.preferred(p)], p)
        self.assertIn('I saw it myself', text)
        self.assertIn('Ash Hollow', text)
        self.assertIn('day 4', text)

    def test_changing_the_question_changes_the_choice_not_the_facts(self):
        facts = [fact('5000', 'actor', 'Ada'), fact('5000', 'place', 'Ash Hollow')]
        by_actor = f.encode_input(person(copy.deepcopy(facts), {'event_id': '5000', 'role': 'actor'}))
        by_place = f.encode_input(person(copy.deepcopy(facts), {'event_id': '5000', 'role': 'place'}))
        self.assertNotEqual(by_actor, by_place)

    def test_dataset_splits_are_disjoint(self):
        splits = fact_data.dataset(24)
        groups = {name: {row['state_group'] for row in rows} for name, rows in splits.items()}
        self.assertFalse(groups['train'] & groups['test'])
        self.assertFalse(groups['train'] & groups['development'])
        for rows in splits.values():
            self.assertTrue(all(row['teacher_index'] < row['choice_count'] for row in rows))


if __name__ == '__main__':
    unittest.main()