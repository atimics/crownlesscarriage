"""Build the offline review and compact text from the reviewed scene records."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent

def build():
    raw = (ROOT / 'scenes.json').read_bytes()
    data = json.loads(raw)
    sources = {}
    for line in (ROOT / 'sources.jsonl').read_text().splitlines():
        record = json.loads(line)
        sources[record['id']] = (record, hashlib.sha256(line.encode()).hexdigest())
    scenes = data['scenes']
    assert len(scenes) == len({s['id'] for s in scenes}) == 12
    assert len({sources[s['source_id']][0]['event']['kind'] for s in scenes}) == 12
    assert all(sum(s['band'] == band for s in scenes) == 4 for band in ('clear', 'retold', 'uncertain'))
    text = ['Crownless dialogue drafts — editorial review', data['scope'], '']
    for i, scene in enumerate(scenes):
        source, digest = sources[scene['source_id']]
        assert digest == scene['source_sha256']
        assert scene['account'] == source['source']['input']['account']
        assert scene['confidence'] == source['source']['input']['confidence']
        assert scene['retellings'] == source['source']['input']['retellings']
        assert scene['provenance'] == source['source']['provenance']
        assert {v['id'] for v in scene['variants']} == {'v1', 'v2'}
        # Alternate the concealed draft order across the twelve pairs.
        scene['display'] = {'A': 'v1' if i % 2 == 0 else 'v2', 'B': 'v2' if i % 2 == 0 else 'v1'}
        text.extend([scene['id'] + ' — ' + scene['title'], 'Held account: ' + scene['account'], 'Authored scene aim: ' + scene['intent']])
        for variant in scene['variants']:
            assert 2 <= len(variant['lines']) <= 6
            assert all(isinstance(line, str) and line.strip() for line in variant['lines'])
            text.extend([variant['id']] + [('Speaker: ' if n % 2 == 0 else 'Listener: ') + line for n, line in enumerate(variant['lines'])] + [''])
    data['digest'] = hashlib.sha256(raw).hexdigest()
    payload = json.dumps(data, ensure_ascii=False).replace('<', '\\u003c')
    template = (ROOT / 'review.html').read_text()
    assert template.count('__STUDY__') == 1
    (ROOT / 'index.html').write_text(template.replace('__STUDY__', payload))
    (ROOT / 'dialogues.txt').write_text('\n'.join(text).rstrip() + '\n')
    files = ['scenes.json', 'sources.jsonl', 'review.html', 'build.py', 'index.html', 'dialogues.txt']
    manifest = {'study_sha256': data['digest'], 'source_review': '../synthetic-speech-2026-09-12', 'source_commit': 'd7663e14588159beb4ad5166a016915bfd94c414', 'authorship': data['authorship'], 'pairs': 12, 'drafts': 24, 'files': {f: hashlib.sha256((ROOT / f).read_bytes()).hexdigest() for f in files}}
    (ROOT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Validated 12 source accounts, 12 event kinds, 24 drafts, and balanced confidence bands.')

if __name__ == '__main__':
    build()
