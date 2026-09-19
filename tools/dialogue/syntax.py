"""One-person decisions in a small public meaning language.

Private participant state supplies grounding. Public acts carry proposals and
reply links. Renderers supply words. Version one represents discussion only.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess

from paired_participants import validate_snapshot

FORMAT = 'crownless-dialogue-syntax-v1'
PLANS = {'food': 'check_stores', 'safety': 'seek_safe_work', 'work': 'seek_paid_work'}
TERMS = {'check_stores': 'vulnerable_first', 'seek_safe_work': 'daylight',
         'seek_paid_work': 'pay_before_work'}
NEEDS = {'food': ('hungry_days', 1), 'safety': ('stress', 60)}


def shape(act):
    """Closed syntax: symbols and references, with no free prose slots."""
    if not isinstance(act, dict):
        raise ValueError('act must be an object')
    move = act.get('move')
    fields = {'need': {'topic'}, 'request': {'topic'}, 'propose': {'plan', 'reply'},
              'condition': {'term', 'reply'}, 'accept': {'reply'},
              'decline': {'reply'}, 'end': set()}
    if not isinstance(move, str) or move not in fields or set(act) != fields[move] | {'move'}:
        raise ValueError('unknown move or fields')
    if 'topic' in act and (not isinstance(act['topic'], str) or act['topic'] not in PLANS):
        raise ValueError('unknown topic')
    if 'plan' in act and (not isinstance(act['plan'], str) or act['plan'] not in TERMS):
        raise ValueError('unknown plan')
    if 'term' in act and (not isinstance(act['term'], str) or act['term'] not in TERMS.values()):
        raise ValueError('unknown term')
    if 'reply' in act and (type(act['reply']) is not int or act['reply'] < 0):
        raise ValueError('reply must be a turn index')
    return act


def validate(act, participant, heard):
    """Check an own turn against own state and the public exchange."""
    shape(act)
    if heard and heard[-1]['act']['move'] == 'end':
        raise ValueError('conversation has ended')
    own = participant['self']
    if heard and heard[-1]['speaker_id'] == own['id']:
        raise ValueError('await the other person')
    move = act['move']
    if move in ('need', 'request') and heard:
        raise ValueError('opening requires a fresh exchange')
    if move == 'need':
        topic = act['topic']
        if topic == 'work':
            if own['goal'] != 'secure_livelihood':
                raise ValueError('work need requires livelihood goal')
        else:
            key, threshold = NEEDS[topic]
            if own[key] < threshold:
                raise ValueError('need lacks own-state grounding')
    if move == 'end':
        if 'end_conversation' not in participant['available_actions']:
            raise ValueError('ending unavailable')
        return act
    if 'reply' in act:
        if not heard or act['reply'] != len(heard) - 1:
            raise ValueError('reply must address the latest public act')
        last = heard[-1]['act']
        if move == 'propose':
            if last['move'] not in ('need', 'request') or PLANS[last['topic']] != act['plan']:
                raise ValueError('proposal must address the requested topic')
        elif move == 'condition':
            if last['move'] != 'propose' or TERMS[last['plan']] != act['term']:
                raise ValueError('condition must belong to this proposal')
        elif last['move'] not in ('propose', 'condition'):
            raise ValueError('decision requires a proposal or condition')
    return act


def decide(participant, heard):
    """Deterministic baseline; a trained policy can replace this function."""
    own = participant['self']
    if not heard:
        if own['hungry_days'] > 0:
            act = {'move': 'need', 'topic': 'food'}
        elif own['stress'] >= 60:
            act = {'move': 'need', 'topic': 'safety'}
        elif own['goal'] == 'secure_livelihood':
            act = {'move': 'need', 'topic': 'work'}
        else:
            act = {'move': 'request', 'topic': 'food'}
    else:
        last = heard[-1]['act']; reply = len(heard) - 1
        if last['move'] in ('need', 'request'):
            act = {'move': 'propose', 'plan': PLANS[last['topic']], 'reply': reply}
        elif last['move'] == 'propose':
            act = {'move': 'condition', 'term': TERMS[last['plan']], 'reply': reply}
        elif last['move'] == 'condition':
            # A distressed person can refuse a work discussion and close it.
            move = 'decline' if own['stress'] >= 60 and last['term'] == 'pay_before_work' else 'accept'
            act = {'move': move, 'reply': reply}
        else:
            act = {'move': 'end'}
    return validate(act, participant, heard)


def english(act, variant=0):
    """Authored clause realization; two styles share the same meaning."""
    shape(act)
    move = act['move']
    forms = {
        ('need', 'food'): ('I need food.', 'I am hungry.'),
        ('need', 'safety'): ('I am worried about danger.', 'I feel unsafe.'),
        ('need', 'work'): ('I need paid work.', 'I need a way to earn a living.'),
        ('request', 'food'): ('How should we arrange food?', 'What should we do about food?'),
        ('request', 'safety'): ('How could we stay safe?', 'What could we do about danger?'),
        ('request', 'work'): ('How could we find paid work?', 'Where could we seek paid work?'),
        ('propose', 'check_stores'): ('Let us ask for a count of the food in store.', 'We could ask how much food is in store.'),
        ('propose', 'seek_safe_work'): ('Let us look for work away from danger.', 'We could seek work somewhere safer.'),
        ('propose', 'seek_paid_work'): ('Let us ask about paid work together.', 'We could look for paid work together.'),
        ('condition', 'vulnerable_first'): ('I would put children and elders first when sharing food.', 'Let us give children and elders first share of the food.'),
        ('condition', 'daylight'): ('Only if we go in daylight.', 'I would go during daylight.'),
        ('condition', 'pay_before_work'): ('Only if we agree the pay before starting work.', 'Let us settle the pay before we begin work.'),
        ('accept', ''): ('I agree to those terms.', 'Those terms suit me.'),
        ('decline', ''): ('I will pass on that plan.', 'I decline that proposal.'),
        ('end', ''): ('Goodbye.', 'Farewell.'),
    }
    key = next((act[k] for k in ('topic', 'plan', 'term') if k in act), '')
    return forms[move, key][variant % 2]


def render(act, language='human', variant=0, probe=None):
    text = english(act, variant)
    if language == 'human':
        return text
    if language != 'goblin' or probe is None:
        raise ValueError('goblin rendering requires the native account probe')
    # Every word here is authored grammar text. This interface has no name slots.
    return subprocess.run([str(probe), '--literal', '100', text], check=True,
                          capture_output=True, text=True).stdout.rstrip('\n')


def run(snapshot, probe=None, max_turns=8):
    validate_snapshot(snapshot)
    if type(max_turns) is not int or not 1 <= max_turns <= 32:
        raise ValueError('turn cap must be 1..32')
    history=[]; rows=[]; transcript=[]
    for i in range(max_turns):
        # Each call gets only one private view and public acts. Rendered speech
        # never enters the policy input or the training target.
        person=copy.deepcopy(snapshot['participants'][i % 2])
        observed=copy.deepcopy(history)
        act=decide(person, observed)
        rows.append({'format': FORMAT, 'world_seed': snapshot['world_seed'],
                     'state_hash': snapshot['state_hash'], 'turn': i,
                     'input': {'participant': person, 'observed_acts': observed},
                     'target': copy.deepcopy(act), 'source': 'procedural-policy-v1'})
        public={'speaker_id': person['self']['id'], 'act': copy.deepcopy(act)}
        entry={**public, 'human': render(act, variant=i % 2)}
        if probe:
            entry['goblin']=render(act, 'goblin', i % 2, probe)
        transcript.append(entry); history.append(public)
        if act['move'] == 'end':
            break
    return {'format': FORMAT, 'snapshot': snapshot, 'rows': rows,
            'transcript': transcript, 'ended': history[-1]['act']['move'] == 'end'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--probe', type=Path)
    args=parser.parse_args()
    raw=args.snapshot.read_bytes()
    result=run(json.loads(raw), args.probe)
    result['snapshot_sha256']=hashlib.sha256(raw).hexdigest()
    result['generator_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    if args.probe:
        result['probe_sha256']=hashlib.sha256(args.probe.read_bytes()).hexdigest()
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    (args.output/'training.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in result['rows']))
    print(json.dumps({'turns': len(result['rows']), 'ended': result['ended']}))


if __name__ == '__main__':
    main()
