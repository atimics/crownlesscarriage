"""Meaning choices respond to evidence, costs, privacy, and remembered outcomes."""
import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools/dialogue'))
import meaning as m
from meaning_language import render


def person(actor='1', listener='2', stock=16, target=75, price=2, coins=8, hunger=0):
    return {'self': {'id': actor, 'name': 'Ruk' if actor=='1' else 'Vesh', 'coins': coins,
                     'hungry_days': hunger, 'stress': 20},
            'listener': {'id': listener, 'name': 'Vesh' if listener=='2' else 'Ruk'},
            'place': {'id': '3', 'name': 'Ash Hollow'}, 'day': 2, 'relationship': {'trust': 2},
            'facts': [{'kind': 'food_store', 'owner': actor, 'place_id': '3', 'place_name': 'Ash Hollow',
                       'stock': stock, 'target': target, 'unit_price': price, 'day': 2, 'source': 'observed'}]}


def heard(act): return [{'speaker_id': act['actor'], 'act': act}]


class MeaningPolicyTests(unittest.TestCase):
    def test_shortage_severity_changes_actual_offer(self):
        asker = person(hunger=2); history=heard(m.choose(asker, []))
        scarce=person('2','1'); almost=person('2','1',stock=74)
        self.assertNotEqual(m.encode_input(scarce,history), m.encode_input(almost,history))
        self.assertEqual(m.choose(scarce,history)['proposal']['quantity'],1)
        self.assertEqual(m.choose(almost,history)['proposal']['quantity'],3)

    def test_exact_price_and_purse_change_offer(self):
        h=heard(m.choose(person(hunger=2), [])); p=person('2','1',stock=74)
        dear=copy.deepcopy(p);dear['facts'][0]['unit_price']=5
        self.assertNotEqual(m.encode_input(p,h),m.encode_input(dear,h))
        self.assertEqual(m.choose(dear,h)['proposal']['total_cost'],5)
        poor=copy.deepcopy(p);poor['self']['coins']=0
        self.assertEqual(m.choose(poor,h)['intent'],'decline')
        self.assertEqual(m.choose(poor,h)['reason'],'insufficient_money')

    def test_owned_current_facts_and_private_boundaries(self):
        p=person(); p['facts'][0]['private']=True
        self.assertEqual(m.choose(p,[])['intent'],'end')
        p['facts'][0]['owner']='2'
        with self.assertRaises(ValueError):m.candidates(p,[])
        p=person('2','1');p['facts'][0]['day']=1
        h=heard(m.choose(person(hunger=2),[]))
        self.assertFalse(any(a['proposal'] for a in m.candidates(p,h)))

    def test_counteroffer_changes_quantity_and_acceptance_keeps_terms(self):
        a=person(hunger=2);b=person('2','1',stock=74)
        h=heard(m.choose(a,[]));offer=m.choose(b,h);h.append({'speaker_id':'2','act':offer})
        counter=m.choose(a,h);self.assertEqual(counter['intent'],'counter_offer')
        self.assertEqual(counter['proposal']['quantity'],1)
        h.append({'speaker_id':'1','act':counter});accepted=m.choose(b,h)
        self.assertEqual(accepted['intent'],'accept');self.assertEqual(accepted['proposal'],counter['proposal'])
        altered=copy.deepcopy(accepted);altered['proposal']['total_cost']=0
        with self.assertRaises(ValueError):m.validate(altered,b,h)

    def test_memory_has_people_and_outcome(self):
        p=person();p['outcomes']=[{'outcome':'fulfilled','quantity':1,'total_cost':2,
            'event_id':'99','actor_id':'2','beneficiary_id':'1','reason':'outcome'}]
        a=m.choose(p,[]);self.assertEqual(a['intent'],'recall_success')
        self.assertIn('You bought',render(a))
        q=copy.deepcopy(p);q['outcomes'][0]['actor_id']='9'
        self.assertFalse(any(a['memory'] for a in m.candidates(q,[])))
        q=copy.deepcopy(p);q['outcomes'][0]['outcome']='failed'
        self.assertNotEqual(m.encode_input(p,[]),m.encode_input(q,[]))
        self.assertEqual(m.choose(q,[])['intent'],'recall_failure')

    def test_public_name_and_proposal_terms_are_bound(self):
        a=person(hunger=2);b=person('2','1');h=heard(m.choose(a,[]))
        forged=copy.deepcopy(h);forged[0]['act']['actor_name']='Another Person'
        with self.assertRaises(ValueError):m.candidates(b,forged)
        offered=m.choose(b,h);h.append({'speaker_id':'2','act':offered})
        forged=copy.deepcopy(h);forged[-1]['act']['proposal']['place_name']='Foreign Store'
        self.assertFalse(any(x['intent']=='accept' for x in m.candidates(a,forged)))
        forged=copy.deepcopy(h);forged[-1]['act']['proposal']['payer_id']='1'
        with self.assertRaises(ValueError):m.candidates(a,forged)

    def test_native_context_and_both_language_packs(self):
        a=person(hunger=2);b=person('2','1');h=[]
        for i in range(8):
            p=(a,b)[i%2];prefix=m.encode_input(p,h)
            self.assertLessEqual(len(prefix),352);self.assertEqual(prefix.count(1580),1)
            act=m.choose(p,h)
            for language in ('human','hrakhor'):
                text=render(act,language);self.assertTrue(text);self.assertNotIn('event ',text)
            h.append({'speaker_id':act['actor'],'act':act})
            if act['intent']=='end':break
        self.assertEqual(h[-1]['act']['intent'],'end')


if __name__=='__main__':unittest.main()
