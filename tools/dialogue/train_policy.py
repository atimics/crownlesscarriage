"""Train the native 5M from fresh weights on nine participant goal policies."""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import random
import sys
import time
from policy import ACTIONS, encode_choice, decode_choice, make_act, FORMAT
from policy_data import dataset

OUTPUT_IDS = (0, *range(1024, 1088))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(model, row):
    import torch
    with torch.no_grad():
        x = torch.tensor([row['prompt']['tokens']], dtype=torch.long)
        hidden, cache = model.hidden(x, torch.zeros((*x.shape, 16), dtype=torch.long))
        prefix = row['prompt']['tokens']
        allowed = torch.tensor(prefix[prefix.index(1580)+1:-1], dtype=torch.long)
        weights = model.embedding.weight[allowed]
        token = int(allowed[(weights @ hidden[0, -1]).argmax()])
        return {'ids': [token], 'eos': True}



def evaluate(model, rows):
    records = []
    for row in rows:
        output = generate(model, row); valid = exact = False; error = None
        reference = json.loads(row['target_text'])
        try:
            if not output['eos']: raise ValueError('missing EOS')
            action = decode_choice(output['ids']); request = row['input']
            make_act(action, request['participant'], request['observed_acts'], request['requested_goal'])
            valid = True; exact = action == reference['choice']
        except (ValueError, KeyError, TypeError) as failure: error = str(failure)
        records.append({'prefix_ids': row['prompt']['tokens'], 'reference': reference,
                        **output, 'valid': valid, 'exact': exact, 'error': error})
    return {'count': len(records), 'valid': sum(r['valid'] for r in records),
            'exact': sum(r['exact'] for r in records), 'records': records}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('zero', 'reference', 'tokenizer', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--steps', type=int, default=3000)
    args = parser.parse_args()
    if args.steps <= 0 or args.output.exists(): parser.error('positive steps and fresh output required')
    sys.path.insert(0, str(args.zero.resolve() / 'scripts'))
    import torch
    from crownless_v2 import save
    from crownless_v2_export import load_export, export
    from train_crownless_participant import loss
    torch.set_num_threads(4); torch.manual_seed(19)
    reference, metadata = load_export(args.reference, args.tokenizer)
    dimensions = tuple(getattr(reference.config, k) for k in ('dim', 'layers', 'heads', 'ff', 'vocab', 'context'))
    if dimensions != (192, 8, 6, 624, 4096, 512): raise ValueError('expected native 5M architecture')
    model = type(reference)(reference.config, mode='conversation')
    del reference
    splits = dataset(); args.output.mkdir(parents=True)
    sources = [Path(__file__), Path(__file__).with_name('policy.py'), Path(__file__).with_name('policy_data.py'),
               Path(__file__).with_name('event_facts.py')]
    sources += [args.zero / 'scripts' / name for name in ('crownless_v2.py', 'crownless_v2_export.py', 'train_crownless_participant.py')]
    manifest = {'status': 'running', 'format': FORMAT, 'initialization': 'fresh',
                'parameters': sum(p.numel() for p in model.parameters()), 'seed': 19,
                'steps': args.steps, 'batch_size': 32, 'torch': torch.__version__,
                'reference_sha256': sha(args.reference), 'tokenizer_sha256': sha(args.tokenizer),
                'sources': {str(p): sha(p) for p in sources}, 'actions': list(ACTIONS), 'datasets': {}}
    for name, rows in splits.items():
        path = args.output / (name + '.jsonl')
        path.write_text(''.join(json.dumps(r) + '\n' for r in rows))
        manifest['datasets'][name] = {'rows': len(rows), 'sha256': sha(path)}
    pools = defaultdict(list)
    for row in splits['train']: pools[row['target_text']].append(row)
    if len(pools) != len(ACTIONS): raise ValueError('training must cover every policy action')
    pools = list(pools.values()); rng = random.Random(19)
    optimizer = torch.optim.AdamW(model.parameters(), lr=3e-4, weight_decay=.01)
    started = time.monotonic(); receipt = args.output / 'manifest.json'
    receipt.write_text(json.dumps(manifest, indent=2) + '\n')
    try:
        model.train()
        with (args.output / 'history.jsonl').open('w') as history:
            for step in range(1, args.steps + 1):
                optimizer.zero_grad(set_to_none=True)
                value = loss(model, [rng.choice(rng.choice(pools)) for _ in range(32)], 'cpu')
                if not torch.isfinite(value): raise ValueError('nonfinite loss')
                value.backward(); torch.nn.utils.clip_grad_norm_(model.parameters(), 1.); optimizer.step()
                item = {'step': step, 'loss': value.item(), 'seconds': time.monotonic()-started}
                history.write(json.dumps(item) + '\n'); history.flush()
                if step == 1 or step % 100 == 0: print(json.dumps(item), flush=True)
        model.eval(); save(args.output / 'last.pt', model, args.tokenizer, {'format': FORMAT})
        export(model, args.tokenizer, args.output / 'last.ccv2', metadata)
        model, _ = load_export(args.output / 'last.ccv2', args.tokenizer); model.eval()
        for name in ('development', 'test'):
            result = evaluate(model, splits[name])
            (args.output / (name + '-evaluation.json')).write_text(json.dumps(result, indent=2) + '\n')
            manifest[name] = {k: v for k, v in result.items() if k != 'records'}
        manifest.update(status='complete', seconds=time.monotonic()-started,
                        export_sha256=sha(args.output / 'last.ccv2'))
    except BaseException as error:
        manifest.update(status='failed', error=repr(error)); save(args.output / 'partial.pt', model, args.tokenizer, {})
        raise
    finally:
        receipt.write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest), flush=True)


if __name__ == '__main__': main()
