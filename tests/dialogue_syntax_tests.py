"""Policy boundaries and shared human/goblin meaning."""
import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/dialogue'))
from syntax import decide, render, run, shape, validate

PROBE = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else None


def person(identifier='1', listener='2', stress=28):
    return {'self': {'id': identifier, 'name': 'Person '+identifier,
                    'goal': 'secure_livelihood', 'stress': stress,
                    'hungry_days': 0, 'coins': 0, 'in_transit': False},
            'listener': {'id': listener, 'name': 'Person '+listener},
            'place': {'id': '10'}, 'day': 1,
            'available_actions': ['end_conversation']}


class SyntaxTests(unittest.TestCase):
    def test_own_state_changes_opening(self):
        p=person()
        self.assertEqual(decide(p, []), {'move': 'need', 'topic': 'work'})
        p['self']['stress']=70
        self.assertEqual(decide(p, [])['topic'], 'safety')
        p['self']['hungry_days']=1
        self.assertEqual(decide(p, [])['topic'], 'food')

    def test_unsupported_need_rejected(self):
        with self.assertRaises(ValueError):
            validate({'move': 'need', 'topic': 'food'}, person(), [])

    def test_prose_and_invalid_symbols_rejected(self):
        for act in ({'move': 'need', 'topic': 'food', 'text': 'I have bread'},
                    {'move': 'buy', 'quantity': 3}, {'move': []},
                    {'move': 'accept', 'reply': True}):
            with self.assertRaises(ValueError):
                shape(act)

    def test_reply_must_match_live_proposal(self):
        heard=[{'speaker_id': '2', 'act': {'move': 'request', 'topic': 'work'}}]
        for act in ({'move': 'propose', 'plan': 'check_stores', 'reply': 0},
                    {'move': 'accept', 'reply': 0},
                    {'move': 'propose', 'plan': 'seek_paid_work', 'reply': 1}):
            with self.assertRaises(ValueError):
                validate(act, person(), heard)

    def test_condition_and_refusal(self):
        heard=[{'speaker_id': '2', 'act': {'move': 'propose', 'plan': 'seek_paid_work', 'reply': 0}}]
        with self.assertRaises(ValueError):
            validate({'move': 'condition', 'term': 'daylight', 'reply': 0}, person(), heard)
        heard=[{'speaker_id': '2', 'act': {'move': 'condition', 'term': 'pay_before_work', 'reply': 0}}]
        self.assertEqual(decide(person(stress=70), heard)['move'], 'decline')

    def test_episode_private_inputs_and_no_mutation(self):
        snap={'version': 1, 'world_seed': 1, 'state_hash': 'fixture', 'day': 1,
              'participants': [person(), person('2','1')]}
        snap['participants'][1]['secret_test']='private memory'
        before=copy.deepcopy(snap)
        result=run(snap, PROBE)
        self.assertEqual(snap,before)
        self.assertTrue(result['ended'])
        self.assertEqual([r['target']['move'] for r in result['rows']],
                         ['need','propose','condition','accept','end'])
        for row in result['rows'][::2]:
            self.assertNotIn('private memory', json.dumps(row))
        for row in result['rows']:
            self.assertNotIn('human', row['input'])
            self.assertNotIn('goblin', row['target'])
        if PROBE:
            self.assertNotEqual(result['transcript'][0]['human'], result['transcript'][0]['goblin'])
        self.assertEqual(result['rows'][3]['target']['reply'],2)

    def test_terminal_and_turn_ownership(self):
        for heard in ([{'speaker_id':'2','act':{'move':'end'}}],
                      [{'speaker_id':'1','act':{'move':'request','topic':'work'}}]):
            with self.assertRaises(ValueError):
                decide(person(),heard)

    def test_native_literal(self):
        if not PROBE:
            self.skipTest('native probe required')
        act={'move':'need','topic':'food'}
        original=copy.deepcopy(act)
        self.assertEqual(render(act, 'goblin', probe=PROBE), 'Sha need zhek.')
        self.assertEqual(act,original)
        import subprocess
        for strength in ('-1','101','abc'):
            self.assertEqual(subprocess.run([str(PROBE),'--literal',strength,'food'],
                                           capture_output=True).returncode,2)


if __name__ == '__main__':
    unittest.main()
