"""State, privacy, proposals, wire integrity, and nine-goal conversation checks."""
import copy
import gzip
import json
from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/dialogue'))
from policy import *
from policy_data import fixture, dataset
from policy_language import render, FORMS


def person(identity='1', listener='2'):
    p = fixture(1, identity, listener)
    p['self'].update(coins=3, hungry_days=0, stress=28, courage=80, faction_id='7')
    p['relationship'] = {'trust': 2, 'affinity': 2, 'obligation': 2}
    p['held_accounts'] = []; p['memories'] = []
    return p


def event(p, history, action, requested=None):
    return {'speaker_id': p['self']['id'], 'act': make_act(action, p, history, requested)}


class PolicyTests(unittest.TestCase):
    def test_every_goal_and_renderer(self):
        self.assertEqual(set(ACTIONS), set(FORMS))
        for g in GOALS:
            people = [person(), person('2', '1')]; h = []
            for i in range(14):
                p = people[i % 2]; request = g if not h else None
                a = decide(p, h, request)
                self.assertEqual(unpack_act(pack_act(a), p, h, request), a)
                self.assertTrue(render(a, p, h, request))
                h.append({'speaker_id': p['self']['id'], 'act': a})
                if a['intent'] == 'end': break
            self.assertEqual(h[-1]['act']['intent'], 'end', g)

    def test_hunger_and_danger_interrupt(self):
        a, b = person(), person('2', '1')
        h = [event(a, [], 'request_tribute', 'clan')]
        b['self']['hungry_days'] = 2
        self.assertEqual(decide(b, h)['intent'], 'explain_need')
        b['self']['stress'] = 70
        self.assertEqual(decide(b, h)['intent'], 'seek_shelter')

    def test_resource_change_and_stale_record(self):
        a, b = person(), person('2', '1')
        h = [event(a, [], 'request_tribute', 'clan')]
        accepted = make_act('accept', b, h)
        b['self']['coins'] = 0
        with self.assertRaises(ValueError): validate(accepted, b, h)
        a['self']['hungry_days'] = 2
        h = [event(a, [], 'explain_need', 'help')]
        b['self']['coins'] = 3
        offered = make_act('offer_help', b, h); record = pack_act(offered)
        b['self']['coins'] = 0
        with self.assertRaises(ValueError): unpack_act(record, b, h)

    def test_private_and_foreign_facts(self):
        p = person(); source = fixture(0)
        for seed in range(100):
            source = fixture(seed)
            if source['held_accounts']: break
        p['held_accounts'] = source['held_accounts']
        p['held_accounts'][0]['private'] = True
        self.assertNotIn('report_fact', allowed(p, [], 'learn'))
        p['held_accounts'][0]['private'] = False
        a = make_act('report_fact', p, [], 'learn')
        a['claim']['owner'] = '2'
        with self.assertRaises(ValueError): validate(a, p, [], 'learn')
        a = make_act('report_fact', p, [], 'learn')
        p['held_accounts'][0]['account'] = 'A different account.'
        with self.assertRaises(ValueError): validate(a, p, [], 'learn')

    def test_no_private_state_in_public_act(self):
        p = person(); a = decide(p, [], 'care')
        p['knowledge'] = [{'event_id': '99', 'private': True, 'certainty': 2}]
        self.assertEqual(a, decide(p, [], 'care'))

    def test_forged_proposal_and_reply(self):
        a, b = person(), person('2', '1')
        h = [event(a, [], 'offer_trade', 'trade')]
        for field, bad in [('cost', True), ('cost', -1), ('payer', '999'), ('condition', 'already_paid')]:
            forged = copy.deepcopy(h); forged[0]['act']['proposal'][field] = bad
            with self.assertRaises(ValueError): allowed(b, forged)
        forged = copy.deepcopy(h); forged[0]['act']['reply'] = 8
        with self.assertRaises(ValueError): allowed(b, forged)
        forged = copy.deepcopy(h); forged[0]['act']['intent'] = 'ask_feeling'
        with self.assertRaises(ValueError): allowed(b, forged)

    def test_daylight_terms_and_closing(self):
        a, b = person(), person('2', '1')
        h = [event(a, [], 'seek_shelter', 'safety')]
        h.append(event(b, h, 'offer_escort'))
        h.append(event(a, h, 'daylight'))
        agreed = make_act('accept', b, h)
        self.assertEqual(agreed['proposal']['condition'], 'daylight')
        h.append({'speaker_id': '2', 'act': agreed})
        self.assertEqual(allowed(a, h), {'end'})

    def test_shelter_loss_changes_opening(self):
        p = person(); p['self']['unsheltered_nights'] = 2
        self.assertEqual(decide(p, [])['intent'], 'seek_shelter')

    def test_memory_owner_and_kind(self):
        p = person(); p['memories'] = [{'kind': 3, 'day': 1, 'subject_id': '3', 'event_id': '50'}]
        a = make_act('thank', p, [], 'trust')
        self.assertIn('help', render(a, p, [], 'trust'))
        p['memories'][0]['kind'] = 4
        with self.assertRaises(ValueError): validate(a, p, [], 'trust')

    def test_regret_and_owned_reason(self):
        a, b = person(), person('2', '1')
        a['relationship']['trust'] = -2
        h = [event(a, [], 'grievance', 'conflict')]
        b['memories'] = [{'kind': 4, 'day': 1, 'subject_id': '3', 'event_id': '50'}]
        self.assertEqual(decide(b, h)['intent'], 'apologise')
        self.assertIn('help fell through', render(decide(b, h), b, h))
        from event_facts import EVENT_KIND_REGISTRY
        for p in (a, b):
            p['held_accounts'] = [{'event_id': '51', 'source_id': p['self']['id'], 'day': 1,
                'kind': EVENT_KIND_REGISTRY['DRAGON_RETALIATION'], 'confidence': 80,
                'account': 'Embermaw burns Thornford because 7 stolen crowns remain missing.'}]
        h = [event(a, [], 'report_fact', 'learn')]
        act = decide(b, h)
        self.assertEqual(act['intent'], 'explain_cause')
        self.assertEqual(act['claim']['owner'], '2')

    def test_saved_participant_snapshot(self):
        data = json.load(gzip.open(ROOT / 'docs/reviews/participant-minds-2026-09-19/syntax-prototype.json.gz'))
        for scene in data.values():
            if not isinstance(scene, dict) or 'snapshot' not in scene: continue
            people = scene['snapshot']['participants']; h = []
            for i in range(14):
                p = people[i % 2]; a = decide(p, h)
                validate(a, p, h); self.assertTrue(render(a, p, h))
                h.append({'speaker_id': p['self']['id'], 'act': a})
                if a['intent'] == 'end': break
            self.assertEqual(h[-1]['act']['intent'], 'end')


if __name__ == '__main__': unittest.main()
