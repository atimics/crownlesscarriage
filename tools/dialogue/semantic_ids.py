"""Version-one semantic IDs and four-byte act records, independent of BPE."""
import struct
from syntax import shape

VERSION=1
MOVES=('need','request','propose','condition','accept','decline','end')
ARGS=('', 'food','safety','work','check_stores','seek_safe_work','seek_paid_work',
      'vulnerable_first','daylight','pay_before_work')
GOALS=('secure_livelihood','keep_order','survive_crisis','carry_news')
OUTPUT_IDS=(0,*range(9,16),*range(32,42),*range(64,97))


def encode_act(act):
    shape(act)
    arg=next((act[k] for k in ('topic','plan','term') if k in act),'')
    reply=act.get('reply')
    if reply is not None and not 0 <= reply < 32:
        raise ValueError('version-one reply range is 0..31')
    return [9+MOVES.index(act['move']),32+ARGS.index(arg),96 if reply is None else 64+reply]


def decode_act(ids):
    if len(ids)!=3 or any(type(i) is not int for i in ids):
        raise ValueError('act requires three integer semantic IDs')
    move,arg,reply=ids
    if move not in range(9,16) or arg not in range(32,42) or reply not in range(64,97):
        raise ValueError('unknown semantic ID')
    name=MOVES[move-9]; argument=ARGS[arg-32]
    act={'move':name}
    field={'need':'topic','request':'topic','propose':'plan','condition':'term'}.get(name)
    if field:
        act[field]=argument
    elif argument:
        raise ValueError('unexpected argument')
    if reply!=96:
        act['reply']=reply-64
    shape(act)
    return act


def pack_act(act):
    move,arg,reply=encode_act(act)
    return struct.pack('<BBH',move-8,arg-32,65535 if reply==96 else reply-64)


def unpack_act(data):
    if len(data)!=4:
        raise ValueError('act record must contain four bytes')
    move,arg,reply=struct.unpack('<BBH',data)
    if reply!=65535 and reply>=32:
        raise ValueError('unsupported reply index')
    return decode_act([move+8,arg+32,96 if reply==65535 else reply+64])


def encode_input(person,heard):
    own=person['self'];other=person['listener']['id']
    if own['id']==other or len(heard)>31:
        raise ValueError('invalid participant or history length')
    courage,coins=own['courage'],own['coins']
    if type(courage) is not int or not 0<=courage<=100 or type(coins) is not int or not 0<=coins<2**31:
        raise ValueError('state outside version-one bounds')
    ids=[128,129,160+GOALS.index(own['goal']),176+int(own['hungry_days']>0),
         178+int(own['stress']>=60),256+courage]
    ids.extend(512+b for b in struct.pack('<I',coins))
    for event in heard:
        if event['speaker_id'] not in (own['id'],other):
            raise ValueError('unexpected speaker')
        ids.extend([130,136 if event['speaker_id']==own['id'] else 137,*encode_act(event['act'])])
    return ids+[131]
