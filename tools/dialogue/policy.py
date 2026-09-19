"""Nine participant goals with typed acts and a language-independent choice wire."""
import hashlib
import json
import struct
import subprocess
from event_facts import build_facts, render_fact_act

FORMAT = 'crownless-policy-v2'
GOALS = ('help', 'trade', 'safety', 'care', 'grief', 'trust', 'conflict', 'clan', 'learn')
GROUPS = {
    'help': ('request_help', 'explain_need', 'offer_help', 'offer_exchange'),
    'trade': ('offer_trade', 'counter_offer', 'explain_price'),
    'safety': ('warn', 'seek_shelter', 'offer_escort', 'daylight'),
    'care': ('ask_feeling', 'comfort', 'offer_company'),
    'grief': ('express_grief', 'recall_loss', 'ask_space'),
    'trust': ('share_memory', 'thank', 'apologise', 'promise'),
    'conflict': ('grievance', 'explain_motive', 'demand_repair', 'forgive'),
    'clan': ('request_tribute', 'invoke_duty', 'bargain', 'refuse_duty', 'threaten'),
    'learn': ('ask_fact', 'report_fact', 'explain_cause', 'uncertain'),
    'common': ('accept', 'decline', 'acknowledge', 'end'),
}
ACTIONS = tuple(action for group in GROUPS.values() for action in group)
ACTION_ID = {a: i for i, a in enumerate(ACTIONS)}
OUTPUT_IDS = (0, *range(1024, 1024 + len(ACTIONS)))
LOSS_KINDS = {'CHARACTER_DIED', 'DRAGON_RETALIATION', 'DRAGON_BATTLE',
              'GOBLIN_EXPEDITION_INTERCEPTED', 'SETTLEMENT_RAZED', 'DRAGON_ATTACKED'}
PROPOSALS = {'offer_help', 'offer_exchange', 'offer_trade', 'counter_offer',
             'offer_escort', 'seek_shelter', 'offer_company', 'promise',
             'demand_repair', 'request_tribute', 'bargain'}


def wire(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'))


def view(person):
    own = person['self']; listener = person['listener']['id']
    if own['id'] == listener:
        raise ValueError('distinct participants required')
    for field in ('coins', 'hungry_days', 'stress', 'courage'):
        if type(own[field]) is not int or own[field] < 0:
            raise ValueError('invalid own state')
    if own['coins'] >= 2**31 or own['stress'] > 100 or own['courage'] > 100:
        raise ValueError('own state outside bounds')
    relation = person.get('relationship') or {}
    facts = [f for f in build_facts(person) if not f.private]
    facts.sort(key=lambda f: (-f.day, f.account_ref))
    memories = [m for m in person.get('memories', [])
                if m.get('kind') in (1, 2, 3, 4) and 0 <= m.get('day', -1) <= person['day']]
    return {'hungry': own['hungry_days'] > 0, 'afraid': own['stress'] >= 60,
            'rich': own['coins'] >= 3, 'coin': own['coins'] > 0,
            'brave': own['courage'] >= 60, 'trust': relation.get('trust', 0) > 0,
            'close': relation.get('affinity', 0) > 0,
            'wronged': relation.get('trust', 0) < 0 or relation.get('obligation', 0) < 0,
            'duty': relation.get('obligation', 0) > 0,
            'clan': str(own.get('faction_id', '0')) != '0',
            'loss': any(f.kind_name in LOSS_KINDS for f in facts),
            'memory': bool(memories), 'helped': any(m['kind'] == 3 for m in memories),
            'withdrew': any(m['kind'] == 4 for m in memories),
            'fact': bool(facts), 'facts': facts, 'memories': memories}


FEATURES = ('hungry', 'afraid', 'rich', 'coin', 'brave', 'trust', 'close',
            'wronged', 'duty', 'clan', 'loss', 'memory', 'helped', 'withdrew', 'fact')


def goal(person, heard, requested=None):
    if requested is not None:
        if requested not in GOALS:
            raise ValueError('unknown goal')
        return requested
    if heard:
        return heard[-1]['act']['goal']
    v = view(person)
    if v['loss'] and v['afraid']: return 'grief'
    if v['afraid']: return 'safety'
    if v['hungry']: return 'help'
    if v['wronged']: return 'conflict'
    if v['duty'] and v['clan']: return 'clan'
    if v['memory']: return 'trust'
    if v['close']: return 'care'
    if v['coin']: return 'trade'
    return 'learn'


def history_check(person, heard):
    if len(heard) > 16:
        raise ValueError('conversation turn budget exceeded')
    own, other = person['self']['id'], person['listener']['id']
    for i, event in enumerate(heard):
        if event['speaker_id'] not in (own, other):
            raise ValueError('unknown speaker')
        act = event['act']
        if act['intent'] not in ACTION_ID or act['goal'] not in GOALS:
            raise ValueError('unknown public act')
        if act['actor'] != event['speaker_id'] or act['recipient'] not in (own, other) or act['recipient'] == act['actor']:
            raise ValueError('public act identity mismatch')
        required_act = {'version', 'goal', 'intent', 'actor', 'recipient', 'subject',
                        'reply', 'proposal', 'claim', 'memory'}
        if set(act) != required_act or act['version'] != 2:
            raise ValueError('invalid public act schema')
        proposal = act.get('proposal')
        if proposal is not None:
            required = {'actor', 'recipient', 'action', 'resource', 'cost', 'payer', 'condition'}
            if (not isinstance(proposal, dict) or set(proposal) != required or
                    proposal['actor'] not in (own, other) or proposal['recipient'] not in (own, other) or
                    proposal['actor'] == proposal['recipient'] or proposal['payer'] not in (own, other) or
                    proposal['action'] not in PROPOSALS or proposal['resource'] not in ('coins', 'time') or
                    type(proposal['cost']) is not int or not 0 <= proposal['cost'] <= 3 or
                    (proposal['resource'] == 'time') != (proposal['cost'] == 0) or
                    proposal['condition'] != 'mutual_agreement'):
                raise ValueError('invalid public proposal')
        if (act['intent'] in PROPOSALS or act['intent'] == 'accept') != (proposal is not None):
            raise ValueError('proposal must belong to the speech intent')
        if proposal is not None and act['intent'] != 'accept' and (proposal['action'] != act['intent'] or proposal['actor'] != act['actor']):
            raise ValueError('proposal actor or intent mismatch')
        if act['intent'] == 'accept' and (i == 0 or proposal != heard[i-1]['act'].get('proposal')):
            raise ValueError('acceptance must retain the offered terms')
        if act.get('claim') is not None and act['claim'].get('owner') != act['actor']:
            raise ValueError('public claim owner mismatch')
        if act['reply'] != (i - 1 if i else None):
            raise ValueError('public act reply mismatch')
        if i and heard[i-1]['speaker_id'] == event['speaker_id']:
            raise ValueError('speakers must alternate')
        if i and heard[i-1]['act']['intent'] == 'end':
            raise ValueError('conversation has ended')
    if heard and (heard[-1]['speaker_id'] == own or heard[-1]['act']['intent'] == 'end'):
        raise ValueError('await another speaker or a fresh conversation')


def affordable(person, proposal):
    if proposal is None: return False
    payer = proposal.get('payer')
    amount = proposal.get('cost')
    return (type(amount) is int and 0 <= amount <= 3 and
            (payer != person['self']['id'] or amount <= person['self']['coins']))


def allowed(person, heard, requested=None):
    history_check(person, heard)
    v = view(person); g = goal(person, heard, requested)
    legal = set(GROUPS[g]) | {'end', 'acknowledge', 'decline'}
    # A safety concern can interrupt another goal.
    if v['afraid']: legal |= {'warn', 'seek_shelter', 'ask_space'}
    else: legal.discard('warn')
    if v['hungry']: legal |= {'explain_need', 'request_help'}
    if heard:
        last_intent = heard[-1]['act']['intent']
        responses = {'express_grief': {'comfort'}, 'recall_loss': {'comfort'},
                     'grievance': {'apologise', 'explain_motive'},
                     'apologise': {'forgive'}, 'share_memory': {'thank', 'promise'},
                     'warn': {'seek_shelter'}, 'seek_shelter': {'offer_escort'},
                     'offer_escort': {'daylight'}}
        legal |= responses.get(last_intent, set())
    if not v['hungry']: legal.discard('explain_need')
    if not v['coin']: legal -= {'offer_help', 'offer_trade', 'bargain'}
    if not v['rich']: legal.discard('explain_price')
    if v['afraid'] or not v['brave']: legal.discard('offer_escort')
    if not v['loss']: legal -= {'express_grief', 'recall_loss'}
    if not v['memory']: legal.discard('share_memory')
    if not v['helped']: legal.discard('thank')
    if not v['withdrew']: legal.discard('apologise')
    if not v['wronged']: legal -= {'grievance', 'demand_repair'}
    if not v['clan']: legal -= {'request_tribute', 'invoke_duty', 'threaten'}
    if not v['duty']: legal.discard('invoke_duty')
    if not v['brave'] or v['trust']: legal.discard('threaten')
    if not v['fact']: legal -= {'report_fact', 'explain_cause'}
    last = heard[-1]['act'] if heard else None
    if last and last.get('proposal') and affordable(person, last['proposal']): legal.add('accept')
    if not last: legal -= {'accept', 'decline', 'acknowledge', 'counter_offer', 'forgive'}
    if len(heard) >= 12: return {'end'}
    return legal


def choose(person, heard, requested=None):
    legal = allowed(person, heard, requested); v = view(person)
    g = goal(person, heard, requested); last = heard[-1]['act']['intent'] if heard else None
    if len(heard) >= 12: return 'end'
    if heard and last in ('accept', 'decline', 'acknowledge', 'forgive', 'refuse_duty', 'ask_space'):
        return 'end'
    if heard and v['afraid'] and g not in ('safety', 'grief') and last not in ('warn', 'seek_shelter', 'comfort'):
        preferred = 'seek_shelter'
    elif heard and v['hungry'] and g not in ('help', 'trade') and last not in ('comfort', 'ask_space'):
        preferred = 'explain_need'
    elif last == 'seek_shelter': preferred = 'offer_escort' if 'offer_escort' in legal else 'accept'
    elif last == 'warn': preferred = 'seek_shelter'
    elif last == 'offer_escort': preferred = 'daylight'
    elif last == 'daylight': preferred = 'acknowledge'
    elif not heard:
        preferred = {'help': 'explain_need' if v['hungry'] else 'request_help',
            'trade': 'offer_trade', 'safety': 'warn' if v['afraid'] else 'seek_shelter',
            'care': 'ask_feeling', 'grief': 'express_grief' if v['afraid'] else 'recall_loss',
            'trust': 'share_memory' if v['memory'] else 'promise',
            'conflict': 'grievance', 'clan': 'invoke_duty' if v['duty'] else 'request_tribute',
            'learn': 'ask_fact'}[g]
    elif last in ('request_help', 'explain_need'):
        preferred = 'offer_help' if v['coin'] and v['trust'] else 'offer_exchange'
    elif last == 'offer_trade': preferred = 'counter_offer' if v['trust'] else 'decline'
    elif last == 'counter_offer': preferred = 'accept' if v['trust'] else 'explain_price'
    elif last == 'explain_price': preferred = 'decline'
    elif last == 'ask_feeling': preferred = 'offer_company' if v['close'] else 'comfort'
    elif last in ('express_grief', 'recall_loss'): preferred = 'comfort'
    elif last == 'comfort': preferred = 'ask_space' if v['afraid'] else 'acknowledge'
    elif last == 'share_memory': preferred = 'thank' if v['helped'] else 'promise'
    elif last == 'apologise': preferred = 'forgive' if v['trust'] else 'decline'
    elif last == 'grievance': preferred = 'apologise' if v['withdrew'] else 'explain_motive'
    elif last == 'explain_motive': preferred = 'demand_repair' if v['wronged'] else 'forgive'
    elif last in ('request_tribute', 'invoke_duty'):
        preferred = 'bargain' if v['coin'] else 'refuse_duty'
    elif last == 'bargain': preferred = 'accept' if v['trust'] else 'threaten'
    elif last == 'threaten': preferred = 'refuse_duty'
    elif last == 'ask_fact': preferred = 'report_fact' if v['fact'] else 'uncertain'
    elif last == 'report_fact': preferred = 'explain_cause' if v['fact'] else 'acknowledge'
    elif last in ('explain_cause', 'uncertain', 'thank'): preferred = 'acknowledge'
    elif last in PROPOSALS: preferred = 'accept' if v['trust'] else 'decline'
    else: preferred = 'acknowledge'
    if preferred in legal: return preferred
    # A grounded alternative keeps missing evidence and means explicit.
    for alternative in ('uncertain', 'ask_feeling', 'offer_exchange', 'refuse_duty', 'decline', 'end'):
        if alternative in legal: return alternative
    return 'end'


def make_act(action, person, heard, requested=None):
    if action not in allowed(person, heard, requested):
        raise ValueError('act is unavailable in this participant state')
    v = view(person); own, other = person['self']['id'], person['listener']['id']
    g = goal(person, heard, requested)
    for family, actions in GROUPS.items():
        if family != 'common' and action in actions: g = family
    act = {'version': 2, 'goal': g, 'intent': action, 'actor': own, 'recipient': other,
           'subject': own, 'reply': len(heard)-1 if heard else None,
           'proposal': None, 'claim': None, 'memory': None}
    if action in PROPOSALS:
        cost = min(person['self']['coins'], 3) if action == 'offer_trade' else 1 if action in ('offer_help', 'counter_offer', 'bargain') else 0
        payer = other if action in ('request_tribute', 'demand_repair', 'counter_offer') else own
        if action in ('request_tribute', 'demand_repair'): cost = 1
        act['proposal'] = {'actor': own, 'recipient': other, 'action': action,
            'resource': 'coins' if cost else 'time', 'cost': cost, 'payer': payer,
            'condition': 'mutual_agreement'}
    if action == 'accept':
        act['proposal'] = json.loads(wire(heard[-1]['act']['proposal']))
    if action in ('report_fact', 'explain_cause', 'express_grief', 'recall_loss'):
        facts = [f for f in v['facts'] if action not in ('express_grief', 'recall_loss') or f.kind_name in LOSS_KINDS]
        f = facts[0]
        act['claim'] = {'owner': own, 'ref': f.account_ref, 'event_id': f.event_id,
                        'source_id': f.source_id, 'day': f.day, 'certainty': f.certainty,
                        'confidence': f.confidence, 'text': f.text}
        act['subject'] = f.event_id
    if action in ('share_memory', 'thank', 'apologise'):
        kind = {'thank': 3, 'apologise': 4}.get(action)
        m = next(m for m in v['memories'] if kind is None or m['kind'] == kind)
        act['memory'] = json.loads(wire(m)); act['subject'] = str(m['subject_id'])
    return act


def validate(act, person, heard, requested=None):
    if make_act(act['intent'], person, heard, requested) != act:
        raise ValueError('act differs from current state, references, or proposal')
    return act


def decide(person, heard, requested=None):
    return make_act(choose(person, heard, requested), person, heard, requested)


def encode_input(person, heard, requested=None):
    history_check(person, heard)
    v = view(person); g = goal(person, heard, requested)
    # Position-specific symbols keep boolean features distinct without English text.
    ids = [1280, 1290 + GOALS.index(g)]
    ids += [1340 + 2*i + int(v[key]) for i, key in enumerate(FEATURES)]
    ids += [1400 + min(len(heard), 12)]
    ids += [1450 + ACTION_ID[heard[-1]['act']['intent']] if heard else 1449]
    ids += [1520 + int(bool(heard and heard[-1]['act'].get('proposal') and affordable(person, heard[-1]['act']['proposal'])))]
    candidates = [1024 + ACTION_ID[a] for a in sorted(allowed(person, heard, requested))]
    return ids + [1580] + candidates + [1281]


def encode_choice(action):
    return [1024 + ACTION_ID[action]]


def decode_choice(ids):
    if len(ids) != 1 or type(ids[0]) is not int or not 1024 <= ids[0] < 1024 + len(ACTIONS):
        raise ValueError('expected one known policy action ID')
    return ACTIONS[ids[0] - 1024]


def pack_act(act):
    # Version and action are accompanied by a digest binding the current full act.
    return struct.pack('<BB', 2, ACTION_ID[act['intent']]) + hashlib.sha256(wire(act).encode()).digest()[:16]


def unpack_act(data, person, heard, requested=None):
    if len(data) != 18 or data[0] != 2 or data[1] >= len(ACTIONS):
        raise ValueError('invalid policy record')
    act = make_act(ACTIONS[data[1]], person, heard, requested)
    if pack_act(act) != data: raise ValueError('stale or foreign policy record')
    return act
