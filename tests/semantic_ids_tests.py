import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools/dialogue'))
from semantic_ids import encode_act,decode_act,pack_act,unpack_act,encode_input
from syntax_training import histories
from syntax import decide
from audit_grammar import inventory


class SemanticTests(unittest.TestCase):
    def test_roundtrip_policy(self):
        p={'self':{'id':'1','goal':'secure_livelihood','hungry_days':0,'stress':28,'courage':73,'coins':16},'listener':{'id':'2'},'available_actions':['end_conversation']}
        for h in histories(p):
            act=decide(p,h)
            self.assertEqual(decode_act(encode_act(act)),act)
            self.assertEqual(unpack_act(pack_act(act)),act)
            self.assertEqual(len(pack_act(act)),4)
            ids=encode_input(p,h)
            self.assertEqual(len(ids),11+5*len(h))
            self.assertEqual(ids[0],128);self.assertEqual(ids[-1],131)
        self.assertEqual(pack_act({'move':'propose','plan':'seek_paid_work','reply':0}),b'\x03\x06\x00\x00')

    def test_invalid_records_and_symbols(self):
        for data in (b'',b'\x07\x01\xff\xff',b'\x03\x06\x20\x00',b'\xff\x00\xff\xff'):
            with self.assertRaises(ValueError):unpack_act(data)
        for ids in ([9,32,96],[15,32,64],[14,32,96],[True,33,96],[9,33]):
            with self.assertRaises(ValueError):decode_act(ids)
        with self.assertRaises(ValueError):encode_act({'move':'accept','reply':32})

    def test_private_fields_and_state_bounds(self):
        p={'self':{'id':'1','goal':'keep_order','hungry_days':0,'stress':0,'courage':50,'coins':0},'listener':{'id':'2'}}
        original=encode_input(p,[])
        p['listener']['secret']='private';p['knowledge']=['private facts']
        self.assertEqual(original,encode_input(p,[]))
        p['self']['stress']=60
        self.assertNotEqual(original,encode_input(p,[]))
        for value in (-1,2**31,True):
            bad=copy.deepcopy(p);bad['self']['coins']=value
            with self.assertRaises(ValueError):encode_input(bad,[])

    def test_audit_covers_declared_rule_kinds(self):
        result=inventory(Path(__file__).resolve().parents[1])
        self.assertEqual(result['unknown_rule_events'],[])
        self.assertEqual(set(result['event_kinds']),set(result['account_covered_events'])|set(result['account_missing_events']))
        self.assertIn('CC_KNOWLEDGE_WITNESS_ACCOUNT',result['personal_knowledge_kinds'])
        self.assertEqual(result['policy_concrete_event_claims'],0)


if __name__=='__main__':unittest.main()
