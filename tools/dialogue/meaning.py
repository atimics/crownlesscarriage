"""Bounded participant decisions over typed observations and food agreements."""
import copy
import hashlib
import json

FORMAT = 'crownless-meaning-v3'
INTENTS = ('report_shortage', 'report_stock', 'request_food', 'offer_food',
           'counter_offer', 'accept', 'decline', 'condition', 'recall_success',
           'recall_failure', 'thank', 'end')
REASONS = ('shortage', 'severe_shortage', 'hunger', 'help', 'price',
           'insufficient_money', 'empty_store', 'no_need', 'outcome', 'mistrust')
MAX_FACTS = 4
MAX_CHOICES = 16


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False)


def identity(value):
    if not isinstance(value, str) or not value.isdecimal() or not 0 < int(value) < 2**64:
        raise ValueError('identity needs a nonzero uint64 string')
    return value


def integer(value, low=0, high=2**31-1):
    if type(value) is not int or not low <= value <= high:
        raise ValueError('integer outside its meaning bounds')
    return value


def facts(person):
    own = identity(person['self']['id']); identity(person['listener']['id'])
    if own == person['listener']['id']: raise ValueError('distinct participants required')
    today = integer(person['day'])
    integer(person['self']['coins']); integer(person['self']['hungry_days'], 0, 36500)
    integer(person['self']['stress'], 0, 100)
    items = person.get('facts', [])
    if not isinstance(items, list) or len(items) > MAX_FACTS: raise ValueError('bounded facts required')
    seen = set(); result = []
    for raw in items:
        f = copy.deepcopy(raw)
        if f['kind'] != 'food_store' or f['owner'] != own: raise ValueError('foreign observation')
        identity(f['place_id']); integer(f['stock']); integer(f['target']); integer(f['unit_price'], 1)
        integer(f['day'], 0, today)
        if f['source'] not in ('observed', 'told'): raise ValueError('unknown observation source')
        if type(f.get('private', False)) is not bool: raise ValueError('invalid disclosure flag')
        if not isinstance(f['place_name'], str) or not f['place_name'] or len(f['place_name']) > 100:
            raise ValueError('invalid place name')
        if f['place_id'] in seen: raise ValueError('resolve competing observations before choosing')
        seen.add(f['place_id'])
        if not f.get('private', False): result.append(f)
    return sorted(result, key=lambda f: (f['place_id'], f['day']))


def severity(fact):
    if fact['stock'] >= fact['target']: return 'no_need'
    return 'severe_shortage' if fact['stock'] * 2 < fact['target'] else 'shortage'


def proposal(fact, payer, beneficiary, quantity, condition='now'):
    integer(quantity, 1, 3)
    return {'payer_id': payer, 'beneficiary_id': beneficiary, 'place_id': fact['place_id'],
            'place_name': fact['place_name'], 'quantity': quantity,
            'unit_price': fact['unit_price'], 'total_cost': integer(quantity * fact['unit_price']),
            'condition': condition}


def history_check(person, heard):
    if len(heard) > 12: raise ValueError('conversation budget exceeded')
    participants = {person['self']['id'], person['listener']['id']}
    for i, event in enumerate(heard):
        a = event['act']
        if (event['speaker_id'] != a['actor'] or a['actor'] not in participants or
                a['recipient'] not in participants or a['actor'] == a['recipient'] or
                a['version'] != 3 or a['intent'] not in INTENTS or
                a['reason'] not in REASONS or a['reply'] != (i-1 if i else None)):
            raise ValueError('invalid public act')
        if i and (heard[i-1]['speaker_id'] == a['actor'] or heard[i-1]['act']['intent'] == 'end'):
            raise ValueError('invalid turn order')
        p = a.get('proposal')
        if p:
            if {p['payer_id'], p['beneficiary_id']} != participants:
                raise ValueError('proposal participants differ')
            identity(p['place_id']); integer(p['quantity'], 1, 3); integer(p['unit_price'], 1)
            if p['total_cost'] != p['quantity'] * p['unit_price'] or p['condition'] not in ('now', 'daylight'):
                raise ValueError('invalid proposal terms')
        if a['intent'] in ('offer_food', 'counter_offer', 'accept', 'condition') and not p:
            raise ValueError('proposal required')
        if a['intent'] == 'accept' and (i == 0 or p != heard[i-1]['act']['proposal']):
            raise ValueError('acceptance must retain all terms')
        if a.get('claim') and a['claim']['owner'] != a['actor']:
            raise ValueError('claim belongs to another participant')
    if heard and heard[-1]['speaker_id'] == person['self']['id']:
        raise ValueError('await the other participant')


def candidates(person, heard):
    known = facts(person); history_check(person, heard)
    own = person['self']; other = person['listener']; last = heard[-1]['act'] if heard else None
    choices = []
    def add(intent, reason='help', claim=None, terms=None, memory=None):
        choices.append({'version': 3, 'intent': intent, 'actor': own['id'], 'recipient': other['id'],
            'actor_name': own['name'], 'recipient_name': other['name'],
            'reply': len(heard)-1 if heard else None, 'claim': copy.deepcopy(claim),
            'proposal': copy.deepcopy(terms), 'memory': copy.deepcopy(memory), 'reason': reason})
    if last and last['intent'] == 'end': raise ValueError('conversation has ended')
    if len(heard) >= 10 or (last and last['intent'] in ('thank', 'decline')):
        add('end'); return choices
    if last and last['intent'] in ('recall_success', 'recall_failure'):
        add('thank' if last['intent'] == 'recall_success' else 'end', 'outcome'); return choices
    if last and last['intent'] == 'accept':
        add('end'); return choices
    if not heard:
        for m in person.get('outcomes', [])[-2:]:
            if m.get('outcome') not in ('fulfilled', 'failed'): raise ValueError('unknown outcome')
            if {m['actor_id'], m['beneficiary_id']} != {own['id'], other['id']}: continue
            integer(m['quantity'], 1, 3); integer(m['total_cost']); identity(m['event_id'])
            add('recall_success' if m['outcome'] == 'fulfilled' else 'recall_failure', 'outcome', memory=m)
        for f in known:
            add('report_stock' if severity(f) == 'no_need' else 'report_shortage', severity(f), claim=f)
            if own['hungry_days'] > 0: add('request_food', 'hunger', claim=f)
        add('end', 'no_need')
    elif last['intent'] in ('report_shortage', 'report_stock', 'request_food'):
        for f in known:
            # A concrete purchase uses a current local observation.
            if f['source'] != 'observed' or f['day'] != person['day'] or f['place_id'] != person['place']['id']:
                continue
            for qty in range(1, min(3, f['stock'], own['coins']//f['unit_price'])+1):
                add('offer_food', 'help', claim=f, terms=proposal(f, own['id'], other['id'], qty))
        reason = 'empty_store' if known and all(f['stock'] == 0 for f in known) else 'insufficient_money'
        add('decline', reason); add('decline', 'mistrust'); add('end', 'no_need')
    elif last['intent'] in ('offer_food', 'counter_offer', 'condition'):
        p = last['proposal']
        mine = next((f for f in known if f['place_id'] == p['place_id']), None)
        payable = p['payer_id'] != own['id'] or own['coins'] >= p['total_cost']
        current = (mine and mine['source'] == 'observed' and mine['day'] == person['day'] and
                   mine['place_id'] == person['place']['id'] and mine['stock'] >= p['quantity'] and
                   mine['unit_price'] == p['unit_price'])
        if payable and current:
            add('accept', 'help', terms=p)
            if last['intent'] == 'offer_food' and p['quantity'] > 1 and p['beneficiary_id'] == own['id']:
                add('counter_offer', severity(mine), claim=mine,
                    terms=proposal(mine, p['payer_id'], p['beneficiary_id'], 1, p['condition']))
            if last['intent'] == 'offer_food' and own['stress'] >= 60 and 'daylight' in person.get('conditions', []):
                revised = copy.deepcopy(p); revised['condition'] = 'daylight'
                add('condition', 'help', terms=revised)
        add('decline', 'insufficient_money' if not payable else 'price')
    else:
        add('end')
    if len(choices) > MAX_CHOICES: raise ValueError('candidate bound exceeded')
    return choices


def preferred(person, heard, choices=None):
    choices = candidates(person, heard) if choices is None else choices
    own = person['self']; trust = (person.get('relationship') or {}).get('trust', 0)
    last = heard[-1]['act'] if heard else None
    def score(a):
        i = a['intent']; p = a['proposal']; f = a['claim']
        if i.startswith('recall_'): return 120
        if i == 'request_food': return 100 + (10 if severity(f) == 'severe_shortage' else 0)
        if i == 'report_shortage': return 50 + (10 if severity(f) == 'severe_shortage' else 0)
        if i == 'report_stock': return 20
        if i == 'offer_food':
            if trust < 0 or own['hungry_days'] > 0: return -10
            # Leave funds for one's own meal and conserve a scarce store.
            desired = 1 if severity(f) == 'severe_shortage' else min(3, max(1, own['coins']//p['unit_price']-1))
            return 70 - abs(p['quantity']-desired)*10
        if i == 'condition': return 95
        if i == 'counter_offer': return 90 if a['reason'] == 'severe_shortage' else 30
        if i == 'accept': return 80 if trust >= 0 else -10
        if i == 'decline':
            if a['reason'] == 'mistrust': return 85 if trust < 0 else -30
            return 60 if own['hungry_days'] > 0 and last and last['intent'] == 'request_food' else 10
        if i == 'thank': return 50
        return 0
    return max(range(len(choices)), key=lambda n: score(choices[n]))


def validate(act, person, heard):
    if act not in candidates(person, heard): raise ValueError('act differs from current facts and choices')
    return act


def encode_input(person, heard, choices=None):
    choices = candidates(person, heard) if choices is None else choices
    if choices != candidates(person, heard): raise ValueError('candidate list changed')
    known = facts(person); own = person['self']; other = person['listener']
    ids = [1280, 1603]
    def number(value):
        integer(value, 0, 2**31-1)
        # Exact integers use base-128 digits with an explicit terminator.
        while value >= 128:
            ids.append(2048 + value % 128); value //= 128
        ids.extend((2048 + value, 2304))
    def role(value): return 0 if value == own['id'] else 1 if value == other['id'] else 2
    for value in (own['coins'], own['hungry_days'], own['stress'],
                  max(-100, min(100, (person.get('relationship') or {}).get('trust', 0)))+100): number(value)
    ids.append(1610+len(known))
    for f in known:
        ids += [1620+int(f['place_id'] == person['place']['id']), 1630+int(f['source']=='observed'),
                1640+REASONS.index(severity(f))]
        for value in (f['stock'], f['target'], f['unit_price'], person['day']-f['day']): number(value)
    ids.append(1660+min(len(heard), 12))
    for event in heard[-3:]:
        a = event['act']; p = a['proposal']
        ids.extend((1680+INTENTS.index(a['intent']), 1700+role(a['actor']), 1720+REASONS.index(a['reason'])))
        if p:
            ids += [1740+role(p['payer_id']), 1750+role(p['beneficiary_id'])]
            number(p['quantity']); number(p['total_cost'])
    ids.append(1770)
    for index, a in enumerate(choices):
        ids.extend((1790+index, 1810+INTENTS.index(a['intent']), 1830+REASONS.index(a['reason'])))
        if a['claim']:
            ids.append(1850+known.index(a['claim']))
        if a['proposal']:
            p = a['proposal']; ids.extend((1860+role(p['payer_id']), 1870+int(p['condition']=='daylight')))
            number(p['quantity']); number(p['total_cost'])
        if a['memory']:
            m = a['memory']; ids.extend((1880+role(m['actor_id']), 1890+int(m['outcome']=='fulfilled')))
            number(m['quantity']); number(m['total_cost'])
    ids += [1580] + [1024+i for i in range(len(choices))] + [1281]
    if len(ids) > 352: raise ValueError('native context budget exceeded')
    return ids


def choose(person, heard, model=None, probe=None):
    options = candidates(person, heard)
    if model is None: index = preferred(person, heard, options)
    else:
        import subprocess
        prefix = encode_input(person, heard, options)
        result = subprocess.run([str(probe), str(model), '--policy-prefix', ','.join(map(str, prefix)),
                                 '--generate'], capture_output=True, text=True, check=True, timeout=30)
        tokens = result.stdout.split()
        if len(tokens) != 1: raise ValueError('one candidate choice required')
        index = int(tokens[0])-1024
        if not 0 <= index < len(options): raise ValueError('foreign candidate choice')
    return validate(options[index], person, heard)
