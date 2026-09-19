"""Compile and train the 5M policy on procedural dialogue acts."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import random
import sys
import time

from syntax import FORMAT, decide, shape, validate


def wire(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def prefix(person, heard):
    """Only own state and validated public history enter the policy input."""
    own = person['self']
    if own['id'] == person['listener']['id']:
        raise ValueError('distinct participants required')
    # These are explicit state predicates, shared with the procedural policy.
    state = {'hungry': own['hungry_days'] > 0, 'distressed': own['stress'] >= 60,
             'goal': own['goal'], 'courage': own['courage'], 'coins': own['coins']}
    public=[]
    for event in heard:
        shape(event['act'])
        if event['speaker_id'] not in (own['id'], person['listener']['id']):
            raise ValueError('unexpected speaker')
        public.append({'from': 'self' if event['speaker_id'] == own['id'] else 'other',
                       'act': event['act']})
    return FORMAT+'\nself:'+wire(state)+'\nheard:'+wire(public)+'\nnext:'


def compile_row(person, heard, tokenizer, group):
    target = decide(person, heard)
    text = prefix(person, heard)
    target_text = wire(target)
    x, y = tokenizer.encode(text).ids, tokenizer.encode(target_text).ids
    if not x or len(x) > 352 or not y or len(y) >= 160 or any(t < 9 for t in x+y):
        raise ValueError('native token budget exceeded')
    return {'prompt': {'format': FORMAT, 'text': text, 'tokens': x},
            'target_text': target_text, 'tokens': x+y,
            'labels': [-100]*(len(x)-1)+y+[0], 'world_group': group,
            'input': {'participant': person, 'observed_acts': heard},
            'source': 'procedural-policy-v1', 'review_status': 'policy_validated'}


def histories(person):
    """Valid public prefixes for each topic, including acceptance and refusal."""
    own, other = person['self']['id'], person['listener']['id']
    result=[[]]
    for topic, plan, term in [('food','check_stores','vulnerable_first'),
                             ('safety','seek_safe_work','daylight'),
                             ('work','seek_paid_work','pay_before_work')]:
        acts=[{'move':'request','topic':topic}, {'move':'propose','plan':plan,'reply':0},
              {'move':'condition','term':term,'reply':1}, {'move':'accept','reply':2}]
        for n in range(1,5):
            result.append([{'speaker_id': other if (n-i)%2 else own, 'act':copy.deepcopy(a)}
                           for i,a in enumerate(acts[:n])])
        refused=copy.deepcopy(acts); refused[-1]={'move':'decline','reply':2}
        result.append([{'speaker_id': other if (4-i)%2 else own, 'act':a}
                       for i,a in enumerate(refused)])
    return result


def dataset(tokenizer):
    """Factorial state fixtures, explicitly synthetic, with disjoint inputs."""
    splits={'train':[], 'development':[], 'test':[]}
    for gi,goal in enumerate(('secure_livelihood','keep_order','survive_crisis','carry_news')):
        for hungry in (0,2):
            for stress in (28,70):
                for courage in (20,44,59,73,90):
                    for coins in (0,1,5,16,40):
                        group=f'fixture-{gi}-{hungry}-{stress}-{courage}-{coins}'
                        # Entire own-state combinations remain in one split.
                        bucket=int(hashlib.sha256(group.encode()).hexdigest()[:8],16)%10
                        split='test' if bucket==0 else 'development' if bucket==1 else 'train'
                        person={'self':{'id':'1','goal':goal,'hungry_days':hungry,
                                       'stress':stress,'courage':courage,'coins':coins},
                                'listener':{'id':'2'},'available_actions':['end_conversation']}
                        for heard in histories(person):
                            splits[split].append(compile_row(copy.deepcopy(person),heard,tokenizer,group))
    check_splits(splits)
    return splits


def check_splits(splits):
    seen_groups=set(); seen_inputs=set()
    for rows in splits.values():
        groups={r['world_group'] for r in rows}
        inputs={r['prompt']['text'] for r in rows}
        if groups & seen_groups or inputs & seen_inputs:
            raise ValueError('split overlap in state groups or model inputs')
        seen_groups.update(groups); seen_inputs.update(inputs)


def checked_row(row, tokenizer):
    p=row['input']['participant']; h=row['input']['observed_acts']
    rebuilt=compile_row(p,h,tokenizer,row['world_group'])
    if rebuilt != row:
        raise ValueError('dataset differs from policy or tokenizer')
    return row


def evaluate(model, rows, tokenizer, sample_fn):
    records=[]
    for row in rows:
        sample=sample_fn(model,row,tokenizer)
        valid=False; exact=False; error=None
        try:
            if not sample['native_complete']:
                raise ValueError('incomplete output')
            act=json.loads(sample['text'])
            validate(act,row['input']['participant'],row['input']['observed_acts'])
            valid=True; exact=act==json.loads(row['target_text'])
        except (ValueError,TypeError,KeyError) as failure:
            error=str(failure)
        records.append({'prefix':row['prompt']['text'],'reference':row['target_text'],
                        'source_line':row.get('source_line'), **sample,
                        'valid':valid,'exact':exact,'error':error})
    return {'count':len(records),'valid':sum(r['valid'] for r in records),
            'exact':sum(r['exact'] for r in records),'records':records}


def evaluation_subset(rows, limit):
    # Stable balanced sample across target acts, independent of model outputs.
    pools={}
    for row in rows:
        pools.setdefault(row['target_text'],[]).append(row)
    rng=random.Random(761)
    for pool in pools.values():
        rng.shuffle(pool)
    selected=[]
    while len(selected)<min(limit,len(rows)):
        for key in sorted(pools):
            if pools[key] and len(selected)<limit:
                selected.append(pools[key].pop())
    return selected


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zero',type=Path,required=True,help='ZERO checkout with native-compatible v2 architecture')
    parser.add_argument('--reference',type=Path,required=True)
    parser.add_argument('--tokenizer',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--steps',type=int,default=1200)
    parser.add_argument('--batch-size',type=int,default=8)
    parser.add_argument('--eval-count',type=int,default=112)
    parser.add_argument('--seed',type=int,default=19)
    parser.add_argument('--device',choices=['cpu','mps'],default='cpu')
    args=parser.parse_args()
    if min(args.steps,args.batch_size,args.eval_count)<=0 or args.output.exists():
        parser.error('positive counts and fresh output directory required')
    sys.path.insert(0,str(args.zero.resolve()/'scripts'))
    import torch
    from tokenizers import Tokenizer
    from crownless_v2 import save
    from crownless_v2_export import load_export,export
    from train_crownless_participant import loss,sample
    torch.set_num_threads(4); torch.manual_seed(args.seed)
    tokenizer=Tokenizer.from_file(str(args.tokenizer))
    splits=dataset(tokenizer)
    model,metadata=load_export(args.reference,args.tokenizer)
    dimensions=tuple(getattr(model.config,k) for k in ('dim','layers','heads','ff','vocab','context'))
    if dimensions!=(192,8,6,624,4096,512) or model.mode!='conversation':
        raise ValueError('expected the native 5M conversation architecture')
    args.output.mkdir(parents=True)
    for split,rows in splits.items():
        (args.output/(split+'.jsonl')).write_text(''.join(wire(r)+'\n' for r in rows))
    tests=evaluation_subset(splits['test'],args.eval_count)
    dev=evaluation_subset(splits['development'],args.eval_count)
    manifest={'status':'running','scope':'procedural_policy_distillation',
              'format':FORMAT,'parameters':sum(p.numel() for p in model.parameters()),
              'seed':args.seed,'steps':args.steps,'batch_size':args.batch_size,
              'device':args.device,'torch':torch.__version__,
              'reference_sha256':sha(args.reference),'tokenizer_sha256':sha(args.tokenizer),
              'datasets':{s:{'rows':len(rows),'sha256':sha(args.output/(s+'.jsonl'))} for s,rows in splits.items()},
              'sources':{str(p):sha(p) for p in [Path(__file__),Path(__file__).with_name('syntax.py')]+[
                  args.zero/'scripts'/n for n in ('crownless_v2.py','crownless_v2_export.py','train_crownless_participant.py')]}}
    receipt=args.output/'manifest.json'
    receipt.write_text(json.dumps(manifest,indent=2)+'\n')
    started=time.monotonic()
    try:
        model.eval()
        # Baseline and final use the same frozen test cases and raw greedy decoder.
        baseline=evaluate(model,tests,tokenizer,sample)
        (args.output/'baseline.json').write_text(json.dumps(baseline,indent=2)+'\n')
        print(wire({'baseline':{k:v for k,v in baseline.items() if k!='records'}}),flush=True)
        model.to(args.device)
        optimizer=torch.optim.AdamW(model.parameters(),lr=3e-4,weight_decay=.01)
        rng=random.Random(args.seed)
        with (args.output/'history.jsonl').open('w') as log:
            for step in range(1,args.steps+1):
                model.train(); optimizer.zero_grad(set_to_none=True)
                batch=[rng.choice(splits['train']) for _ in range(args.batch_size)]
                value=loss(model,batch,args.device)
                if not torch.isfinite(value):
                    raise ValueError('nonfinite loss')
                value.backward(); torch.nn.utils.clip_grad_norm_(model.parameters(),1.)
                optimizer.step()
                item={'step':step,'loss':value.item(),'seconds':time.monotonic()-started}
                log.write(wire(item)+'\n');log.flush()
                if step==1 or step%100==0 or step==args.steps:
                    print(wire(item),flush=True)
        model.cpu().eval()
        save(args.output/'last.pt',model,args.tokenizer,{'participant_format':FORMAT,'step':args.steps})
        export(model,args.tokenizer,args.output/'last.ccv2',metadata)
        quantized,_=load_export(args.output/'last.ccv2',args.tokenizer);quantized.eval()
        development=evaluate(quantized,dev,tokenizer,sample)
        (args.output/'development-evaluation.json').write_text(json.dumps(development,indent=2)+'\n')
        final=evaluate(quantized,tests,tokenizer,sample)
        (args.output/'evaluation.json').write_text(json.dumps(final,indent=2)+'\n')
        # Native parity checker reads these complete raw samples.
        (args.output/'samples.json').write_text(json.dumps(final['records'],indent=2)+'\n')
        manifest.update(status='complete',seconds=time.monotonic()-started,
                        export_sha256=sha(args.output/'last.ccv2'),
                        baseline={k:v for k,v in baseline.items() if k!='records'},
                        development={k:v for k,v in development.items() if k!='records'},
                        evaluation={k:v for k,v in final.items() if k!='records'})
    except BaseException as error:
        manifest.update(status='failed',error=repr(error))
        save(args.output/'partial.pt',model,args.tokenizer,{'participant_format':FORMAT,'failed':True})
        raise
    finally:
        receipt.write_text(json.dumps(manifest,indent=2)+'\n')
    print(wire(manifest),flush=True)


if __name__=='__main__':
    main()
