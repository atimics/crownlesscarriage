#!/usr/bin/env python3
"""Freeze Python-reference cases for the native runtime using the reviewed ZERO model."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import re as _rex
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
import crownless_conversation
import crownless_moves
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
# Recorded multi-turn conversation: history four deep exercises the encoder's
# history budget on both sides. Only the turn inputs are reused; the text is
# regenerated, since the recording belongs to an older checkpoint.
chat = json.loads((a.zero / 'experiments/crownless-conversation/chat.json').read_text())
for i, turn in enumerate(chat['turns']):
    avatar = chat['avatars'][i % 2]
    cases.append({**{k: avatar[k] for k in ('kind', 'account', 'confidence', 'retellings')},
                  'history': [h['text'] for h in turn['history']]})
# Every row of the move corpus carries a stance, so CcCoreModelBegin gives a
# caller with no character in mind an unremarkable one rather than none. These
# cases pin that path, so they have to be built the way the runtime builds it:
# same stance, and open or answer depending on whether anyone has spoken.
PLAIN = {'goal': 'secure_livelihood', 'stress': 'medium', 'courage': 'medium',
         'memories': [], 'thoughts': []}
for case in cases:
    packet = json.loads(subprocess.check_output([str(a.account_probe),str(case['kind']),str(case['confidence']),'0',case['account'],'--packet'],text=True))
    row = packet_record(packet,retold=case['retellings']>=4)
    row['kind_id'] = meta['meaning_ids'][packet['rule']]
    history = case['history']
    row['history'] = [{'speaker':'other' if (len(history)-1-i)%2==0 else 'self','text':text} for i,text in enumerate(history)]
    row['voice'] = 'resident'
    row['mind'] = dict(PLAIN)
    row['control'] = 'open' if not history else 'answer'
    output = generate(model,tokenizer,encode_row(tokenizer,row,slots=True,conversation=True,
                                                 typed_stance=True, situation=True))
    if not output['stopped']: raise ValueError('Reference did not stop')
    case['text'] = output['text']
# Mind-context reference cases: character voice, goal, stress, memories, and
# control cues. The stance rides the meta channel now, so a wrong field id is
# invisible in the decoded prompt and these cases are the parity check.
# Every field the probe receives has to match what the Python
# encoder saw, or the case pins a disagreement instead of the encoding.
rng = random.Random(20260912)
# The cue names the move now, matching the shipped checkpoint and the probe.
CONTROLS = ('open', 'answer', 'remark', 'affirm', 'dispute', 'hedge',
            'attribute', 'defer', 'settle', 'part', 'recall', 'muse')
for index, base in enumerate(rows):
    if not base['parsed'] or index % 13 != 0: continue
    held = base['account']
    case = {'kind': base['kind'], 'account': held['account'], 'confidence': held['confidence'],
            'retellings': held['retellings']}
    packet = json.loads(subprocess.check_output([str(a.account_probe),str(case['kind']),str(case['confidence']),'0',case['account'],'--packet'],text=True))
    row = packet_record(packet,retold=case['retellings']>=4)
    row['kind_id'] = meta['meaning_ids'][packet['rule']]
    # An empty voice segment leaves the probe's voice NULL, matching a row
    # with no voice: both sides then fall back to the resident id on the meta
    # channel, and no stance text appears on either side.
    voice = rng.choice(['baker','scribe','farmer','smith']) if index % 3 else ''
    control = CONTROLS[index % len(CONTROLS)]
    row['voice'] = voice or None
    row['control'] = control
    row['mind'] = {'goal': rng.choice(['secure_livelihood','survive_crisis','carry_news','keep_order']),
                   'stress': rng.choice(['low','medium','high']),
                   'courage': rng.choice(['low','medium','high']),
                   'memories': [rows[0]['text']] if index % 5 == 0 else [],
                   'thoughts': ["I will keep calm. Every loaf counts."] if index % 4 == 0 else []}
    # Situation bits ride the meta channel beside the stance; randomize them so
    # the parity cases exercise the new tables, and append them to the probe
    # spec in the same order the runtime fills them.
    row['situation'] = {'hungry': bool(rng.getrandbits(1)),
                        'sheltered': bool(rng.getrandbits(1)),
                        'in_transit': bool(rng.getrandbits(1))}
    row['social'] = {'owes_listener': bool(rng.getrandbits(1)),
                     'trusts_listener': bool(rng.getrandbits(1)),
                     'faction': rng.choice([None, 'crown', 'guild', 'commons']),
                     'far_from_home': bool(rng.getrandbits(1))}
    row['history'] = [{'speaker':'other','text':'What have you heard?'}] if index % 3 == 0 else []
    case['mind'] = ':'.join([voice, row['mind']['goal'], row['mind']['stress'],
                             row['mind']['courage'], control] +
                            ['1' if row['situation'][axis] else '0'
                             for axis in ('hungry', 'sheltered', 'in_transit')] +
                            ['1' if row['social'][axis] else '0'
                             for axis in ('owes_listener', 'trusts_listener')] +
                            [row['social']['faction'] or ''] +
                            ['1' if row['social']['far_from_home'] else '0'])
    case['memories'] = list(row['mind']['memories'])
    case['thoughts'] = list(row['mind']['thoughts'])
    case['history'] = [h['text'] for h in row['history']]
    output = generate(model,tokenizer,encode_row(tokenizer,row,slots=True,conversation=True,
                                                 typed_stance=True, situation=True,
                                                 social=True))
    if not output['stopped']: raise ValueError('Mind reference did not stop')
    case['text'] = output['text']
    cases.append(case)
# Recall and muse controls, which the index cycle above does not reach, and a
# held memory: these exercise the memory copy candidate (field 8, marker [F7]).
# The memory text is fixed so both runtimes offer the same candidate and the
# copy parity is pinned, not left to chance.
MEMORY = 'Silverwick repaired public buildings with stone.'
for index, base in enumerate([r for r in rows if r['parsed']][:2]):
    held = base['account']
    case = {'kind': base['kind'], 'account': held['account'], 'confidence': held['confidence'],
            'retellings': held['retellings']}
    packet = json.loads(subprocess.check_output(
        [str(a.account_probe),str(case['kind']),str(case['confidence']),'0',case['account'],'--packet'],text=True))
    row = packet_record(packet,retold=case['retellings']>=4)
    row['kind_id'] = meta['meaning_ids'][packet['rule']]
    row['voice'] = 'baker'
    row['control'] = 'recall'
    row['mind'] = {'goal':'keep_order','stress':'high','courage':'low',
                   'memories':[MEMORY],'thoughts':[]}
    row['situation'] = {'hungry': False, 'sheltered': True, 'in_transit': False}
    row['social'] = {'owes_listener': False, 'trusts_listener': False,
                     'faction': None, 'far_from_home': False}
    row['history'] = [{'speaker':'other','text':'What have you heard?'}] if index else []
    case['mind'] = 'baker:keep_order:high:low:recall:0:1:0:0:0::0'
    case['memories'] = [MEMORY]
    case['thoughts'] = []
    case['history'] = [h['text'] for h in row['history']]
    output = generate(model,tokenizer,encode_row(tokenizer,row,slots=True,conversation=True,
                                                 typed_stance=True, situation=True, social=True))
    if not output['stopped']: raise ValueError('Recall reference did not stop')
    case['text'] = output['text']
    cases.append(case)
rng = random.Random(41)
texts = ['self: [F0] has heard a different account.\n- ~ [F0]\n', "I'm unsure. It's hers. We'll see."]
texts += [''.join(rng.choice(['word','Élodie','_','  ','\n','!',"'re",'[F0]','42','🤔','家']) for _ in range(10)) for _ in range(300)]
# Every word the authored sources can legitimately produce: the account
# grammar, the hand-written conversational pools, and the accounts themselves.
# Nothing here comes from the model, so a checkpoint that starts inventing
# spelling cannot quietly widen its own allowance.
WORD = _rex.compile(r"[^\W\d_]+", _rex.UNICODE)
def _words(value, into):
    if isinstance(value, str): into.update(w.lower() for w in WORD.findall(value))
    elif isinstance(value, dict):
        for item in value.values(): _words(item, into)
    elif isinstance(value, (list, tuple)):
        for item in value: _words(item, into)
allowed = set()
_words(json.loads((root / 'tools/data/core_account_rules.json').read_text()), allowed)
for pool in ('QUESTIONS', 'CERTAINTY', 'CHECK', 'ASK', 'CLOSE',
             'VOICE_LINES', 'MEMORY_LINES', 'THOUGHT', 'STRESS_PREFIX'):
    _words(getattr(crownless_conversation, pool), allowed)
# The move wordings are authored pools too: the checkpoint quotes them
# verbatim (e.g. a DEFER line containing "hearsay"), so a reply built only
# from them must not read as invented spelling.
for pool in ('AFFIRM', 'DEFER', 'SETTLE', 'PART', 'HEDGE', 'ATTRIBUTE',
             'DISPUTE_OPEN', 'DISPUTE_CLOSE'):
    _words(getattr(crownless_moves, pool), allowed)
# Predicament and company openings are authored pools too, quoted verbatim.
_words(getattr(crownless_moves, 'SITUATION_MARKS'), allowed)
_words(getattr(crownless_moves, 'SOCIAL_MARKS'), allowed)
_words(getattr(crownless_moves, 'STANCE_MARKS'), allowed)
# The reviewed conversation transcript supplies the ordinary dialogue words a
# reply may reach for without inventing anything.
_words([turn['text'] for turn in chat['turns']], allowed)
_words([line['text'] for turn in chat['turns'] for line in turn['history']], allowed)
for case in cases:
    _words(case['account'], allowed); _words(case.get('history', []), allowed)
    _words(case.get('memories', []), allowed); _words(case.get('thoughts', []), allowed)
result = {'model_sha256':hashlib.sha256(model_path.read_bytes()).hexdigest(),
          'vocabulary':sorted(allowed),
           'zero_sources':{name:hashlib.sha256((a.zero/'scripts'/name).read_bytes()).hexdigest() for name in
                           ('crownless_v2.py','crownless_v2_export.py','speak_crownless_v2.py',
                            'crownless_conversation.py','crownless_moves.py')},
          'cases':cases, 'tokenizer':[{'text':text,'ids':tokenizer.encode(text).ids} for text in texts]}
a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(len(cases),'reference sentences and',len(texts),'tokenizer cases')
