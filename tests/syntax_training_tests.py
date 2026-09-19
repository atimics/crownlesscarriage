import copy
import json
from pathlib import Path
from types import SimpleNamespace
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools/dialogue'))
from syntax_training import prefix,compile_row,checked_row,dataset,check_splits,evaluate

PROBE=Path(sys.argv.pop(1)).resolve() if len(sys.argv)>1 else None


class Tokenizer:
    # Cheap deterministic tokens for compiler-contract tests; native parity is
    # exercised separately against the real vocabulary and checkpoint.
    def encode(self,text):
        return SimpleNamespace(ids=[9+sum(map(ord,text[i:i+8]))%4000 for i in range(0,len(text),8)])


def person():
    return {'self':{'id':'1','goal':'secure_livelihood','hungry_days':0,'stress':28,'courage':44,'coins':0},
            'listener':{'id':'2'},'available_actions':['end_conversation']}


class TrainingTests(unittest.TestCase):
    def test_native_syntax_prefix(self):
        if PROBE is None:
            self.skipTest('native probe required')
        import subprocess
        from participant_training import NativeTokenizer
        model=Path(__file__).resolve().parents[1]/'assets/language/core.ccv2'
        text=prefix(person(),[])
        result=subprocess.run([str(PROBE),str(model),'--participant-prefix',text,'--dump-prefix'],
                              capture_output=True,text=True,check=True)
        self.assertEqual(list(map(int,result.stdout.split())),list(NativeTokenizer(PROBE).encode(text)))
        meta=subprocess.run([str(PROBE),str(model),'--participant-prefix',text,'--dump-meta'],
                            capture_output=True,text=True,check=True)
        self.assertTrue(all(t=='0' for t in meta.stdout.split()))

    def test_prefix_uses_own_state_and_public_meaning(self):
        p=person();p['secret_extra']='unused'
        p['listener']['private_extra']='hidden'
        text=prefix(p,[])
        self.assertNotIn('unused',text);self.assertNotIn('hidden',text)
        p['self']['stress']=60
        self.assertNotEqual(text,prefix(p,[]))
        p['listener']['id']='1'
        with self.assertRaises(ValueError): prefix(p,[])

    def test_actor_loss_and_tampering(self):
        tok=Tokenizer();row=compile_row(person(),[],tok,'fixture')
        self.assertEqual(checked_row(row,tok),row)
        self.assertEqual(row['labels'][-1],0)
        self.assertEqual(sum(x!=-100 for x in row['labels']),len(tok.encode(row['target_text']).ids)+1)
        for field in ('target_text','labels'):
            bad=copy.deepcopy(row);bad[field]='changed'
            with self.assertRaises(ValueError): checked_row(bad,tok)

    def test_full_factorial_split(self):
        splits=dataset(Tokenizer())
        self.assertEqual(sum(map(len,splits.values())),6400)
        check_splits(splits)
        splits['test'].append(splits['train'][0])
        with self.assertRaises(ValueError): check_splits(splits)

    def test_raw_metrics_keep_failures(self):
        tok=Tokenizer();row=compile_row(person(),[],tok,'fixture')
        for text,expected in [('bad json',(0,0)), ('{"move":"need","topic":"food"}',(0,0)),
                              ('{"move":"end"}',(1,0)),(row['target_text'],(1,1))]:
            result=evaluate(None,[row],tok,lambda *_:{'text':text,'native_complete':True})
            self.assertEqual((result['valid'],result['exact']),expected)
            self.assertEqual(result['records'][0]['text'],text)

    def test_native_runner_preserves_invalid_output(self):
        import tempfile
        from unittest.mock import patch
        from syntax_student import run
        a=person();a.update(day=1,place={'id':'9'})
        a['self']['in_transit']=False
        b=copy.deepcopy(a);b['self']['id']='2';b['listener']['id']='1'
        snapshot={'version':1,'world_seed':1,'state_hash':'fixture','day':1,'participants':[a,b]}
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); model=root/'model';model.write_bytes(b'fixture')
            response=SimpleNamespace(returncode=0,stdout=b'{bad json}\n',stderr=b'')
            with patch('syntax_student.subprocess.run',return_value=response):
                with self.assertRaises(ValueError):
                    run(snapshot,model,model,root/'attempt')
            record=json.loads((root/'attempt/result.json').read_text())
            self.assertEqual(record['status'],'failed')
            self.assertEqual(bytes.fromhex(record['attempts'][0]['stdout_hex']),response.stdout)


if __name__=='__main__': unittest.main()
