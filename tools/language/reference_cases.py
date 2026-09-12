#!/usr/bin/env python3
"""Freeze Python-reference cases for the native runtime using the reviewed ZERO model."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--zero', type=Path, required=True)
p.add_argument('--account-probe', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
sys.path.insert(0, str(a.zero / 'scripts'))
import torch
from tokenizers import Tokenizer
from crownless_v2 import encode_row, generate
from crownless_v2_export import load_export
from speak_crownless_v2 import packet_record

torch.set_num_threads(2)
root = Path(__file__).resolve().parents[2]
model_path = root / 'assets/language/core.ccv2'
tokenizer_path = root / 'assets/language/tokenizer.json'
model, meta = load_export(model_path, tokenizer_path)
tokenizer = Tokenizer.from_file(str(tokenizer_path))
rows = json.loads((a.zero / 'experiments/crownless-core-v2/evidence/game-q8.json').read_text())['rows']
cases = []
for row in rows:
    if not row['parsed']: continue
    held = row['account']
    cases.append({'kind': row['kind'], 'account': held['account'], 'confidence': held['confidence'],
                  'retellings': held['retellings'], 'history': []})
for confidence in (20,80):
    for question in ('How sure are you?', 'Who told you?', 'We should ask someone who was there.', 'Agreed. Let us leave it there for now.'):
        cases.append({'kind':0,'account':"Élodie's drought harvest cannot supply the southern settlements.",
                      'confidence':confidence,'retellings':5,'history':[question]})
chat = json.loads((a.zero / 'experiments/crownless-conversation/chat.json').read_text())
for i, turn in enumerate(chat['turns']):
    avatar = chat['avatars'][i % 2]
    cases.append({**{k:avatar[k] for k in ('kind','account','confidence','retellings')},
                  'history':[h['text'] for h in turn['history']], 'text':turn['text']})
for case in cases:
    packet = json.loads(subprocess.check_output([str(a.account_probe),str(case['kind']),str(case['confidence']),'0',case['account'],'--packet'],text=True))
    row = packet_record(packet,retold=case['retellings']>=4)
    row['kind_id'] = meta['meaning_ids'][packet['rule']]
    history = case['history']
    row['history'] = [{'speaker':'other' if (len(history)-1-i)%2==0 else 'self','text':text} for i,text in enumerate(history)]
    output = generate(model,tokenizer,encode_row(tokenizer,row,slots=True,conversation=True))
    if not output['stopped']: raise ValueError('Reference did not stop')
    if 'text' in case and case['text'] != output['text']: raise ValueError('Saved conversation differs')
    case['text'] = output['text']
rng = random.Random(41)
texts = ['self: [F0] has heard a different account.\n- ~ [F0]\n', "I'm unsure. It's hers. We'll see."]
texts += [''.join(rng.choice(['word','Élodie','_','  ','\n','!',"'re",'[F0]','42','🤔','家']) for _ in range(10)) for _ in range(300)]
result = {'model_sha256':hashlib.sha256(model_path.read_bytes()).hexdigest(),
          'zero_sources':{name:hashlib.sha256((a.zero/'scripts'/name).read_bytes()).hexdigest() for name in
                          ('crownless_v2.py','crownless_v2_export.py','speak_crownless_v2.py')},
          'cases':cases, 'tokenizer':[{'text':text,'ids':tokenizer.encode(text).ids} for text in texts]}
a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(len(cases),'reference sentences and',len(texts),'tokenizer cases')
