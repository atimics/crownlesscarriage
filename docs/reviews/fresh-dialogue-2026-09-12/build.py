"""Validate and publish the fresh, assistant-authored dialogue batch."""
import hashlib
import json
from collections import Counter
from pathlib import Path
ROOT = Path(__file__).resolve().parent

def sha(b):
    return hashlib.sha256(b).hexdigest()

def build():
    sources = {r['id']: r for r in map(json.loads, (ROOT/'accounts.jsonl').read_text().splitlines())}
    generation = json.loads((ROOT/'generation.json').read_text())
    assert sha((ROOT/'accounts.jsonl').read_bytes()) == generation['files']['accounts.jsonl']
    assert all(w['finish']['valid'] for w in generation['worlds'])
    assert {w['seed'] for w in generation['worlds']} == {1201,1202,1203,1204}
    raw = (ROOT/'drafts.json').read_bytes()
    drafts = json.loads(raw)
    assert len(drafts) == len({d['source_id'] for d in drafts}) == 48
    assert {d['source_id'] for d in drafts} == set(sources)
    review = json.loads((ROOT.parent/'dialogue-pairs-2026-09-12/scenes.json').read_text())
    previous = {d['account'] for d in review['scenes']}
    prior_rows = [json.loads(l) for l in (ROOT.parent/'synthetic-speech-2026-09-12/accounts.jsonl').read_text().splitlines()]
    prior_accounts = {r['source']['input']['account'] for r in prior_rows}
    lines, records, bands = [], [], Counter()
    for i, d in enumerate(drafts, 1):
        s = sources[d['source_id']]
        known = s['source']['input']
        assert s['source']['provenance']['world_seed'] in [1201,1202,1203,1204]
        assert 2 <= len(d['lines']) <= 6
        assert all(isinstance(l,str) and l.strip() and '\n' not in l for l in d['lines'])
        assert all(not any(c.isdigit() for c in l) for l in d['lines']), d['source_id']
        band = 'uncertain' if known['confidence'] < 40 else 'retold' if known['retellings'] >= 4 else 'clear'
        bands[band] += 1
        record = dict(id=f'F{i:02}', **d, account=known['account'], confidence=known['confidence'], retellings=known['retellings'], band=band, provenance=s['source']['provenance'])
        records.append(record)
        cue = ('? ' if known['confidence'] < 40 else '') + ('~ ' if known['retellings'] >= 4 else '')
        lines.extend([record['id']+' — '+d['title'], '- '+cue+known['account']])
        lines.extend(('Speaker: ' if n%2==0 else 'Listener: ')+l for n,l in enumerate(d['lines']))
        lines.append('')
    assert len({r['source']['input']['kind'] for r in sources.values()}) == 16
    assert dict(bands) == {'clear':16,'retold':16,'uncertain':16}
    # Four examples per band and twelve event types; sample chosen before preference review.
    sample_ids = ['F01','F05','F09','F13','F17','F21','F22','F26','F33','F34','F38','F48']
    sample = [next(r for r in records if r['id']==i) for i in sample_ids]
    assert Counter(r['band'] for r in sample) == {'clear':4,'retold':4,'uncertain':4}
    all_lines = [l for d in drafts for l in d['lines']]
    summary = dict(dialogues=48, lines=len(all_lines), distinct_lines=len(set(all_lines)), event_kinds=16, bands=dict(bands), worlds=[1201,1202,1203,1204], matching_prior_review_accounts=sum(r['account'] in previous for r in records), matching_prior_96_accounts=sum(r['account'] in prior_accounts for r in records), repeated_lines=[{'line':l,'count':n} for l,n in Counter(all_lines).most_common() if n>1], turn_lengths=dict(Counter(len(d['lines']) for d in drafts)), authorship='Assistant-written candidates; preference review pending.', simulation_commit='90dc717e745103e38351307cd82df0ced93f27e7')
    data = dict(title='Crownless: fresh conversations',digest=sha(raw + (ROOT/'accounts.jsonl').read_bytes() + (ROOT/'reference-selection.json').read_bytes()),scenes=sample,summary=summary)
    payload=json.dumps(data,ensure_ascii=False).replace('<','\\u003c')
    template=(ROOT/'review.html').read_text()
    assert template.count('__STUDY__')==1
    (ROOT/'index.html').write_text(template.replace('__STUDY__',payload))
    (ROOT/'dialogues.txt').write_text('\n'.join(lines).rstrip()+'\n')
    (ROOT/'summary.json').write_text(json.dumps(summary,indent=2,ensure_ascii=False)+'\n')
    (ROOT/'records.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in records))
    files=['accounts.jsonl','drafts.json','generation.json','reference-selection.json','build.py','review.html','index.html','dialogues.txt','summary.json','records.jsonl']
    (ROOT/'manifest.json').write_text(json.dumps({'digest':data['digest'],'files':{f:sha((ROOT/f).read_bytes()) for f in files}},indent=2)+'\n')
    print(json.dumps(summary,ensure_ascii=False))
if __name__=='__main__':
    build()
