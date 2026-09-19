"""Distil the same 5M policy directly into semantic token IDs."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import sys
import time

from semantic_ids import OUTPUT_IDS, encode_input, encode_act, decode_act
from syntax import validate
from syntax_training import dataset, evaluation_subset, sha


def compile_row(row):
    request=row['input']
    x=encode_input(request['participant'],request['observed_acts'])
    y=encode_act(json.loads(row['target_text']))
    return {**row,'prompt':{'format':'crownless-semantic-ids-v1','tokens':x},
            'tokens':x+y,'labels':[-100]*(len(x)-1)+y+[0]}


def generate(model,row):
    import torch
    with torch.no_grad():
        x=torch.tensor([row['prompt']['tokens']],dtype=torch.long)
        hidden,cache=model.hidden(x,torch.zeros((*x.shape,16),dtype=torch.long))
        allowed=torch.tensor(OUTPUT_IDS,dtype=torch.long)
        weights=model.embedding.weight[allowed]
        output=[];eos=False
        for _ in range(8):
            token=int(allowed[(weights@hidden[0,-1]).argmax()])
            if token==0:
                eos=True;break
            output.append(token)
            hidden,cache=model.hidden(torch.tensor([[token]]),torch.zeros((1,1,16),dtype=torch.long),cache)
    return {'ids':output,'eos':eos}


def evaluate(model,rows):
    records=[]
    for row in rows:
        output=generate(model,row);error=None;valid=False;exact=False
        try:
            if not output['eos']:raise ValueError('missing EOS')
            act=decode_act(output['ids'])
            validate(act,row['input']['participant'],row['input']['observed_acts'])
            valid=True;exact=act==json.loads(row['target_text'])
        except (ValueError,TypeError,KeyError) as failure:error=str(failure)
        records.append({'prefix_ids':row['prompt']['tokens'],'reference':json.loads(row['target_text']),
                        **output,'valid':valid,'exact':exact,'error':error})
    return {'count':len(records),'valid':sum(r['valid'] for r in records),
            'exact':sum(r['exact'] for r in records),'records':records}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('zero','reference','tokenizer','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--steps',type=int,default=800)
    args=parser.parse_args()
    if args.steps<=0 or args.output.exists():parser.error('positive steps and a fresh directory required')
    sys.path.insert(0,str(args.zero.resolve()/'scripts'))
    import torch
    from tokenizers import Tokenizer
    from crownless_v2 import save
    from crownless_v2_export import load_export,export
    from train_crownless_participant import loss
    torch.set_num_threads(4);torch.manual_seed(19)
    source=dataset(Tokenizer.from_file(str(args.tokenizer)))
    splits={k:[compile_row(r) for r in rows] for k,rows in source.items()}
    model,metadata=load_export(args.reference,args.tokenizer)
    dimensions=tuple(getattr(model.config,k) for k in ('dim','layers','heads','ff','vocab','context'))
    if dimensions!=(192,8,6,624,4096,512) or model.mode!='conversation':
        raise ValueError('expected native 5M architecture')
    args.output.mkdir(parents=True)
    manifest={'status':'running','format':'crownless-semantic-ids-v1',
              'parameters':sum(p.numel() for p in model.parameters()),'steps':args.steps,
              'batch_size':16,'seed':19,'reference_sha256':sha(args.reference),
              'torch':torch.__version__,'tokenizer_sha256':sha(args.tokenizer),
              'sources':{str(p):sha(p) for p in [Path(__file__),Path(__file__).with_name('semantic_ids.py'),
                  Path(__file__).with_name('syntax_training.py'),Path(__file__).with_name('syntax.py')]+[
                  args.zero/'scripts'/n for n in ('crownless_v2.py','crownless_v2_export.py','train_crownless_participant.py')]}}
    for split,rows in splits.items():
        p=args.output/(split+'.jsonl');p.write_text(''.join(json.dumps(r)+'\n' for r in rows))
    manifest['datasets']={k:{'rows':len(rows),'sha256':sha(args.output/(k+'.jsonl'))} for k,rows in splits.items()}
    test=evaluation_subset(splits['test'],112)
    manifest['token_comparison']={
        'json_prefix_mean':sum(len(r['prompt']['tokens']) for r in source['test'])/len(source['test']),
        'semantic_prefix_mean':sum(len(r['prompt']['tokens']) for r in splits['test'])/len(splits['test']),
        'json_output_mean_including_eos':sum(sum(t!=-100 for t in r['labels']) for r in source['test'])/len(source['test']),
        'semantic_output_including_eos':4,'binary_record_bytes':4}
    started=time.monotonic();receipt=args.output/'manifest.json'
    receipt.write_text(json.dumps(manifest,indent=2)+'\n')
    try:
        model.eval();baseline=evaluate(model,test)
        (args.output/'baseline.json').write_text(json.dumps(baseline,indent=2)+'\n')
        model.train();optimizer=torch.optim.AdamW(model.parameters(),lr=3e-4,weight_decay=.01)
        rng=random.Random(19)
        with (args.output/'history.jsonl').open('w') as history:
            for step in range(1,args.steps+1):
                optimizer.zero_grad(set_to_none=True)
                value=loss(model,[rng.choice(splits['train']) for _ in range(16)],'cpu')
                if not torch.isfinite(value):raise ValueError('nonfinite loss')
                value.backward();torch.nn.utils.clip_grad_norm_(model.parameters(),1.);optimizer.step()
                item={'step':step,'loss':value.item(),'seconds':time.monotonic()-started}
                history.write(json.dumps(item)+'\n');history.flush()
                if step==1 or step%100==0:print(json.dumps(item),flush=True)
        model.eval();save(args.output/'last.pt',model,args.tokenizer,{'format':manifest['format']})
        export(model,args.tokenizer,args.output/'last.ccv2',metadata)
        model,_=load_export(args.output/'last.ccv2',args.tokenizer);model.eval()
        for name,rows in [('development',evaluation_subset(splits['development'],112)),('test',splits['test'])]:
            result=evaluate(model,rows)
            (args.output/(name+'-evaluation.json')).write_text(json.dumps(result,indent=2)+'\n')
            manifest[name]={k:v for k,v in result.items() if k!='records'}
        manifest.update(status='complete',seconds=time.monotonic()-started,
                        export_sha256=sha(args.output/'last.ccv2'))
    except BaseException as error:
        manifest.update(status='failed',error=repr(error));save(args.output/'partial.pt',model,args.tokenizer,{})
        raise
    finally:receipt.write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest),flush=True)


if __name__=='__main__':main()
