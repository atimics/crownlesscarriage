#!/usr/bin/env python3
"""Collect fresh simulated accounts and replay the shipped language model for review."""
import argparse
from collections import Counter, defaultdict, deque
import gzip
import hashlib
import html
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def encode(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def identity(value):
    return hashlib.sha256(encode(value).encode()).hexdigest()[:16]


def band(row):
    return 'uncertain' if row['confidence'] < 40 else 'retold' if row['retellings'] >= 4 else 'clear'


def balanced(rows, count, group):
    buckets = defaultdict(list)
    for row in rows:
        buckets[group(row)].append(row)
    queues = [deque(sorted(buckets[key], key=identity)) for key in sorted(buckets)]
    result = []
    while len(result) < count and any(queues):
        for queue in queues:
            if queue and len(result) < count:
                result.append(queue.popleft())
    return result


def pairs(observations, seed):
    """Both people must be observed holding this event in this place on this day."""
    groups = defaultdict(dict)
    for row in observations:
        if row['type'] == 'held' and row['place_id'] != '0':
            key = row['day'], row['event_id'], row['place_id']
            groups[key][row['person_id']] = row
    result = []
    for (day, event, place), people in sorted(groups.items()):
        people = sorted(people.values(), key=identity)
        if len(people) < 2:
            continue
        first = people[0]
        # Prefer a different telling or confidence level before looking at model output.
        second = max(people[1:], key=lambda p: (p['account'] != first['account'],
                     abs(p['confidence'] - first['confidence']), identity(p)))
        result.append(dict(seed=seed, day=day, event_id=event, place_id=place,
                           kind=first['kind'], people=[first, second]))
    return result


def meeting_cases(meetings):
    unique = {}
    for meeting in meetings:
        people = meeting['people']
        key = (meeting['seed'], meeting['event_id'], *(band(p) for p in people),
               people[0]['account'] != people[1]['account'])
        unique.setdefault(key, meeting)
    return list(unique.values())


def collect(binary, seed, days, path):
    result = subprocess.run([str(binary), '--seed', str(seed), '--days', str(days)],
                            capture_output=True, check=True)
    path.write_bytes(gzip.compress(result.stdout, mtime=0))
    return [json.loads(line) for line in result.stdout.splitlines()], result.stderr.decode()


def model_account(account):
    # Flow records retain social commentary beside the game's prepared speech account.
    return account.get('core_account', account['account'])


def packet(probe, kind, account):
    result = subprocess.run([str(probe), str(kind), str(account['confidence']), '0',
                             model_account(account), '--packet'], capture_output=True, text=True)
    if result.returncode == 1:
        return None
    result.check_returncode()
    return json.loads(result.stdout)


def speak(probe, model, kind, account, history):
    result = subprocess.run([str(probe), str(model), str(kind), str(account['confidence']),
                             str(account['retellings']), model_account(account), *history[-4:]],
                            capture_output=True, text=True)
    if result.returncode == 1:
        return None
    result.check_returncode()
    return result.stdout.rstrip('\n')


def prefix(account):
    cue = ('? ' if account['confidence'] < 40 else '') + ('~ ' if account['retellings'] >= 4 else '')
    return '- ' + cue + account['account'] + '\n'


def render_page(directory, singles, chats, summary):
    esc = html.escape
    cards = []
    for row in singles:
        source = row['source']
        known = source['input']
        flags = ', '.join(row['flags']) or 'Review meaning and wording'
        cards.append(f'<article data-type="account" data-search="{esc(encode(row), quote=True)}">'
            f'<small>{row["id"]} · {esc(known["kind"])} · {band(known)} · world {source["provenance"]["world_seed"]}</small>'
            f'<h2>{esc(row["speaker"])}</h2><p class="label">Held account · confidence {known["confidence"]} · {known["retellings"]} retellings</p>'
            f'<p>{esc(known["account"])}</p><p class="label">Shared game wording</p><p>{esc(source["output"])}</p>'
            f'<p class="label">5M model</p><p class="speech">{esc(row["model_text"] or "Model could not produce a complete line.")}</p>'
            f'<p class="note">{esc(flags)}</p><details><summary>Original event and source</summary>'
            f'<pre>{esc(encode(row["event"]))}</pre><pre>{esc(encode(source["provenance"]))}</pre></details></article>')
    for row in chats:
        people = row['people']
        accounts = ''.join(f'<p><b>{esc(p["name"])}</b> · confidence {p["confidence"]} · {p["retellings"]} retellings<br>{esc(p["account"])}</p><p class="label">Prepared speech account</p><p>{esc(model_account(p))}</p>' for p in people)
        lines = ''.join(f'<p class="turn"><b>{esc(t["speaker"])}</b><span>{esc(t["text"])}</span></p>' for t in row['turns'])
        cards.append(f'<article data-type="conversation" data-search="{esc(encode(row), quote=True)}">'
            f'<small>{row["id"]} · world {row["seed"]} · day {row["day"]} · {esc(row["place"])}</small>'
            f'<h2>{esc(people[0]["name"])} and {esc(people[1]["name"])}</h2>'
            f'<details><summary>What each speaker knows</summary>{accounts}</details>{lines}'
            f'<p class="note">{esc(", ".join(row["flags"]) or "Review the conversation")}</p>'
            f'<details><summary>Original event</summary><pre>{esc(encode(row["event"]))}</pre></details></article>')
    template = (ROOT / 'tools/data/speech_review.html').read_text()
    page = template.replace('<!-- CARDS -->', '\n'.join(cards)).replace('<!-- SUMMARY -->',
        esc(f'{len(singles)} accounts · {len(chats)} staged conversations · {summary["worlds"]} fresh worlds · {summary["days_per_world"]} days each'))
    (directory / 'index.html').write_text(page)


def build(args):
    if args.output.exists():
        raise ValueError('Choose a fresh output directory')
    if min(args.days, args.accounts, args.conversations, args.turns) < 1 or args.turns > 16:
        raise ValueError('Use positive counts and at most sixteen turns')
    if len(set(args.seeds)) != len(args.seeds):
        raise ValueError('Use distinct world seeds')
    args.output.mkdir(parents=True)
    raw = args.output / 'raw'
    raw.mkdir()
    candidates, meetings, reports, events, names, places = [], [], [], {}, {}, {}
    for seed in args.seeds:
        flow, _ = collect(args.bin_dir / 'crownless_gossip_flow', seed, args.days, raw / f'{seed}-flow.jsonl.gz')
        if flow[0]['seed'] != seed or flow[-1]['type'] != 'finish' or not flow[-1]['valid']:
            raise ValueError('Simulation did not finish with a valid world')
        for row in flow:
            if row['type'] == 'world':
                places.update({(seed, p['id']): p['name'] for p in row['settlements']})
            elif row['type'] == 'story':
                events[seed, row['event_id']] = dict(text=row['original'], day=row['event_day'], kind=row['kind'])
            elif row['type'] == 'held':
                names[seed, row['person_id']] = row['name']
        meetings.extend(pairs(flow, seed))
        rows, log = collect(args.bin_dir / 'crownless_gossip_corpus', seed, args.days, raw / f'{seed}-corpus.jsonl.gz')
        report = json.loads(log)
        if report['world_seed'] != seed or report['rows'] != len(rows):
            raise ValueError('Corpus report differs from its rows')
        reports.append(dict(seed=seed, finish=flow[-1], corpus=report, flow_rows=len(flow)))
        # One renderer variant per source observation, then one representative per account/band.
        candidates.extend(r for r in rows if r['input']['variant'] == 0)
        print(f'World {seed}: {len(rows)} reference rows, {len(flow)} flow observations', flush=True)
    unique = {}
    for row in candidates:
        key = row['input']['kind'], row['input']['account'], band(row['input'])
        unique.setdefault(key, row)
    selected = balanced(unique.values(), args.accounts, lambda r: (r['input']['kind'], band(r['input'])))
    singles = []
    account_probe, model_probe = args.bin_dir / 'core_account_probe', args.bin_dir / 'core_model_probe'
    for i, source in enumerate(selected):
        kind = int(source['rule'].split(':')[0])
        account = source['input']
        parsed = packet(account_probe, kind, account)
        text = speak(model_probe, args.model, kind, account, []) if parsed else None
        flags = []
        if parsed is None:
            flags.append('Account outside the model parser')
        elif text is None:
            flags.append('Generation did not complete')
        else:
            if text != source['output']:
                flags.append('Different from the shared game wording')
            missing = [f['text'] for f in parsed['fields'] if f['spoken'] and 1 <= f['role'] <= 5
                       and f['knowledge'] != 3 and f['text'] not in text]
            if missing:
                flags.append('Spoken source fields absent: ' + ', '.join(missing))
        provenance = source['provenance']
        singles.append(dict(id=f'A{i+1:03}', source=source, packet=parsed, model_text=text, flags=flags,
            speaker=names.get((provenance['world_seed'], provenance['speaker_id']), provenance['speaker_id']),
            event=events[provenance['world_seed'], provenance['event_id']]))
    # Preserve changes in personal knowledge as stories circulate across days.
    chats = []
    chosen = balanced(meeting_cases(meetings), args.conversations, lambda r: r['kind'])
    for i, meeting in enumerate(chosen):
        history, turns, flags = [], [], []
        packets = [packet(account_probe, meeting['kind'], p) for p in meeting['people']]
        for turn in range(args.turns):
            person = meeting['people'][turn % 2]
            if packets[turn % 2] is None:
                flags.append(f'Account outside the model parser at turn {turn+1}')
                break
            text = speak(model_probe, args.model, meeting['kind'], person, history)
            if text is None:
                flags.append(f'Model could not complete turn {turn+1}')
                break
            turns.append(dict(speaker=person['name'], person_id=person['person_id'],
                              text=text, history=history[-4:], confidence=person['confidence']))
            history.append(text)
        duplicate_count = len(history) - len(set(history))
        if duplicate_count:
            flags.append(f'{duplicate_count} repeated lines')
        chats.append(dict(meeting, id=f'C{i+1:03}', turns=turns, flags=flags, packets=packets,
            event=events[meeting['seed'], meeting['event_id']],
            place=places.get((meeting['seed'], meeting['place_id']), meeting['place_id'])))
        print(f'Conversation {i+1}: {len(turns)} turns', flush=True)
    for name, rows in [('accounts', singles), ('conversations', chats)]:
        (args.output / f'{name}.jsonl').write_text(''.join(encode(row) + '\n' for row in rows))
    (args.output / 'reference-pairs.txt').write_text(''.join(prefix(r['source']['input']) + r['source']['output'] + '\n\n' for r in singles))
    (args.output / 'model-pairs.txt').write_text(''.join(prefix(r['source']['input']) + r['model_text'] + '\n\n' for r in singles if r['model_text']))
    (args.output / 'conversations.txt').write_text('\n\n'.join(r['id'] + ' · ' + r['place'] + '\n' + '\n'.join(t['speaker'] + ': ' + t['text'] for t in r['turns']) for r in chats) + '\n')
    utterances = [t['text'] for r in chats for t in r['turns']]
    summary = dict(worlds=len(args.seeds), days_per_world=args.days, seeds=args.seeds,
        observations=sum(r['corpus']['rows'] for r in reports), distinct_accounts=len(unique),
        accounts=len(singles), model_accounts=sum(r['model_text'] is not None for r in singles),
        exact_shared_wording=sum(r['model_text'] == r['source']['output'] for r in singles),
        account_kinds=dict(Counter(r['source']['input']['kind'] for r in singles)),
        bands=dict(Counter(band(r['source']['input']) for r in singles)),
        conversations=len(chats), complete_conversations=sum(len(r['turns']) == args.turns for r in chats),
        conversation_bands=dict(Counter('/'.join(band(p) for p in r['people']) for r in chats)),
        different_held_accounts=sum(r['people'][0]['account'] != r['people'][1]['account'] for r in chats),
        different_model_accounts=sum(model_account(r['people'][0]) != model_account(r['people'][1]) for r in chats),
        conversation_lines=len(utterances), distinct_conversation_lines=len(set(utterances)),
        frequent_lines=Counter(utterances).most_common(12))
    (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    render_page(args.output, singles, chats, summary)
    manifest = dict(schema='crownless.speech_review.v1',
        scope='Review sample of simulated held accounts and staged conversations between co-located holders. All generated turns use the shipped English core and the preceding four spoken lines. Human review determines suitability for training.',
        selection='Stable hash order, balanced by kind and confidence band for accounts; by kind for conversations. Selection precedes model generation.',
        source_commit=subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
        parameters=vars(args) | {'output': str(args.output), 'bin_dir': str(args.bin_dir), 'model': str(args.model)},
        model_sha256=sha(args.model), tools={n: sha(args.bin_dir / n) for n in
            ('core_model_probe', 'core_account_probe', 'crownless_gossip_flow', 'crownless_gossip_corpus')},
        sources={n: sha(ROOT / n) for n in ('tools/build_speech_review.py', 'tools/data/speech_review.html')},
        worlds=reports, files={str(p.relative_to(args.output)): sha(p) for p in sorted(args.output.rglob('*')) if p.is_file()})
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', type=Path, required=True)
    parser.add_argument('--model', type=Path, default=ROOT / 'assets/language/core.ccv2')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seeds', nargs='+', type=int, default=[901, 902, 903, 904])
    parser.add_argument('--days', type=int, default=365)
    parser.add_argument('--accounts', type=int, default=96)
    parser.add_argument('--conversations', type=int, default=24)
    parser.add_argument('--turns', type=int, default=6)
    build(parser.parse_args())
