#!/usr/bin/env python3
"""Compile one person's teacher turns into bounded student input previews."""
import argparse
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import re
import subprocess

from paired_participants import validate_turn

FORMAT = 'crownless-person-v1'
CONTEXT = 512
REPLY_TOKENS = 160
EOS = 0


def wire(value):
    # Quote text, including structural newlines. Escape tokenizer control text
    # inside strings so literal speech survives JSON decoding as literal speech.
    text = json.dumps(value, ensure_ascii=False, separators=(',', ':'))
    return re.sub(r'\[(?:EOS|F[0-7])\]', lambda m: '\\u005b' + m[0][1:], text)


class NativeTokenizer:
    """Use the exact BPE compiled into the game's probe, with no Python dependency."""
    def __init__(self, probe):
        self.probe = Path(probe).resolve()
        self.sha256 = hashlib.sha256(self.probe.read_bytes()).hexdigest()

    @lru_cache(maxsize=4096)
    def encode(self, text):
        if '\0' in text or len(text.encode('utf-8')) > 4096:
            raise ValueError('text exceeds the native tokenizer byte limit')
        result = subprocess.run([str(self.probe), '--encode', text],
                                capture_output=True, text=True, timeout=15)
        if result.returncode:
            raise ValueError('native tokenizer rejected text: ' + result.stderr)
        tokens = tuple(map(int, result.stdout.split()))
        if any(t < 9 or t >= 4096 for t in tokens):
            raise ValueError('literal text encoded as a reserved or invalid token')
        return tokens


def observed(event, own_id):
    speaker = event['speaker_id']
    if not isinstance(speaker, str) or not speaker.isdecimal() or int(speaker) == 0:
        raise ValueError('observation needs a stable speaker ID')
    # Older memories may refer to a third person. Preserve their source ID.
    role = 'self' if speaker == own_id else 'person ' + speaker
    result = {'from': role}
    if 'day' in event:
        result['day'] = event['day']
    if event['kind'] == 'speech':
        result['speech'] = event['text']
    elif event['kind'] == 'action' and event['action'] == 'end_conversation':
        result['action'] = event['action']
    else:
        raise ValueError('unsupported observed event')
    return result


def build_prompt(request, tokenizer, context=CONTEXT, reply_tokens=REPLY_TOKENS):
    """Select solely from the actor input. Reserve the same reply space every time."""
    if not 1 <= reply_tokens < context <= CONTEXT:
        raise ValueError('invalid context or reply budget')
    p = request['participant']
    me, listener = p['self'], p['listener']
    if me['id'] == listener['id']:
        raise ValueError('speaker and listener must differ')
    if p['available_actions'] != ['end_conversation']:
        raise ValueError('unsupported available actions')

    def event_value(e):
        value = observed(e, me['id'])
        if e['speaker_id'] == listener['id']:
            value['from'] = 'other'
        return value

    # Names and active state are mandatory. Opaque IDs live in the audit record
    # and route observations; the model uses stable self/other roles in this pair.
    base = [FORMAT + '\n',
            'self:' + wire([me[k] for k in
                ('name', 'occupation', 'age', 'goal', 'activity')]) + '\n',
            'needs:' +
                wire([me[k] for k in ('stress', 'courage', 'hungry_days',
                     'unsheltered_nights', 'coins', 'in_transit')]) + '\n',
            'place:' + wire([p['place']['name'], me['home'], p['day']]) + '\n',
            'other:' + wire(listener['name']) + '\n',
            'relationship:' + wire(p['relationship']) + '\n',
            'actions:' + wire(p['available_actions']) + '\n']
    turns = request.get('observed_turns', [])
    # This row is always present intact, or the example is rejected. Losing the
    # question while keeping its answer creates the wrong learning problem.
    latest = ('heard:' + wire(event_value(turns[-1])) + '\n') if turns else ''
    cue = 'turn:\n'
    budget = context - reply_tokens
    included, dropped, sections = [], [], {}

    def render():
        # Optional evidence first, conversation in chronological order at end.
        evidence = [v for k, v in sorted(sections.items()) if k[0] != 'turn']
        history = [v for k, v in sorted(sections.items()) if k[0] == 'turn']
        return ''.join(base + evidence + history + [latest, cue])

    text = render()
    if len(tokenizer.encode(text)) > budget:
        raise ValueError('identity, needs and latest observation exceed prefix budget')

    options = []
    # Most recent held account comes first, then heard speech, observed memories,
    # personal memory and knowledge. Every optional record stays whole.
    accounts = []
    for i, a in sorted(enumerate(p.get('held_accounts', [])),
                       key=lambda pair: (-pair[1]['day'], pair[0])):
        value = [a[k] for k in ('day', 'confidence', 'retellings', 'source_id', 'account')]
        accounts.append((('account', i), 'account:' + wire(value) + '\n'))
    options.extend(accounts[:1])
    for i in range(len(turns) - 2, -1, -1):
        options.append((('turn', i), 'heard:' + wire(event_value(turns[i])) + '\n'))
    options.extend(accounts[1:])
    for i in range(len(request.get('remembered_observations', [])) - 1, -1, -1):
        value = event_value(request['remembered_observations'][i])
        options.append((('observed_memory', i), 'remembered_speech:' + wire(value) + '\n'))
    options.append((('group', 0), 'group:' + wire([me['band'], me['faction_id']]) + '\n'))
    for key in ('memories', 'knowledge'):
        for i in range(len(p.get(key, [])) - 1, -1, -1):
            options.append(((key, i), key + ':' + wire(p[key][i]) + '\n'))
    for key, line in options:
        sections[key] = line
        candidate = render()
        if len(candidate.encode()) > 4096 or len(tokenizer.encode(candidate)) > budget:
            del sections[key]
            dropped.append(list(key))
        else:
            included.append(list(key))
    text = render()
    return {'format': FORMAT, 'text': text, 'tokens': list(tokenizer.encode(text)),
            'speaker_id': me['id'], 'listener_id': listener['id'],
            'context': context, 'reply_tokens': reply_tokens,
            'included': included, 'dropped': dropped,
            'source_input_sha256': hashlib.sha256(wire(request).encode()).hexdigest()}


def compile_row(row, tokenizer, context=CONTEXT, reply_tokens=REPLY_TOKENS):
    prompt = build_prompt(row['input'], tokenizer, context, reply_tokens)
    if row.get('speaker_id', prompt['speaker_id']) != prompt['speaker_id']:
        raise ValueError('candidate speaker differs from its private input')
    output = validate_turn(row['output'], row['input']['participant'])
    target_text = wire(output)
    if len(target_text.encode()) >= 512:
        raise ValueError('target JSON exceeds the native output byte limit')
    target = list(tokenizer.encode(target_text))
    if len(target) >= reply_tokens:
        raise ValueError('whole target exceeds reply budget')
    tokens = prompt['tokens'] + target + [EOS]
    # Labels are shifted once here. The last input position predicts the first
    # actor output token; the final target position predicts EOS.
    labels = [-100] * (len(prompt['tokens']) - 1) + target + [EOS]
    return {'prompt': prompt, 'target_text': target_text,
            'tokens': tokens[:-1], 'labels': labels,
            'source_review_status': row.get('review_status', 'pending'),
            'review_status': 'pending_compact_review',
            'episode': row.get('episode'), 'turn': row.get('turn')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True, help='Candidate JSONL')
    parser.add_argument('--probe', type=Path, required=True, help='Native core_model_probe')
    parser.add_argument('--output', type=Path, required=True, help='Fresh artifact directory')
    args = parser.parse_args()
    source = args.input.read_bytes()
    tokenizer = NativeTokenizer(args.probe)
    args.output.mkdir(parents=True, exist_ok=False)
    count, rejected = 0, 0
    with (args.output / 'previews.jsonl').open('w') as previews, \
         (args.output / 'rejected.jsonl').open('w') as failures:
        for i, line in enumerate(source.decode().splitlines()):
            try:
                row = compile_row(json.loads(line), tokenizer)
                row['source_line'] = i + 1
                previews.write(json.dumps(row, ensure_ascii=False) + '\n')
                count += 1
            except (ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
                failures.write(json.dumps({'source_line': i + 1, 'source': line,
                                           'error': str(error)}) + '\n')
                rejected += 1
    receipt = {'format': FORMAT, 'compiled': count, 'rejected': rejected,
               'source_sha256': hashlib.sha256(source).hexdigest(),
               'native_probe_sha256': tokenizer.sha256,
               'compiler_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
               'files': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in args.output.iterdir() if p.is_file()}}
    (args.output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt))
    return 1 if rejected else 0


if __name__ == '__main__':
    raise SystemExit(main())
