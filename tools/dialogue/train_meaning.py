"""Train a fresh native 5M choice model on v3 meaning acts."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import random
import sys
import time

from meaning_data import dataset


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zero', type=Path, default=Path('/tmp/crownless-policy-zero'))
    parser.add_argument('--reference', type=Path, default=Path('models/dialogue-syntax/model.ccv2'))
    parser.add_argument('--tokenizer', type=Path, default=Path('assets/language/tokenizer.json'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--steps', type=int, default=4000)
    parser.add_argument('--worlds', type=int, default=256)
    parser.add_argument('--batch-size', type=int, default=32)
    parser.add_argument('--device', choices=('cpu', 'mps', 'cuda'), default='cpu')
    args = parser.parse_args()
    if args.output.exists() or min(args.steps, args.worlds, args.batch_size) <= 0:
        parser.error('output must be fresh and counts must be positive')
    sys.path.insert(0, str(args.zero / 'scripts'))
    import torch
    from crownless_v2 import Crownless, save
    from crownless_v2_export import export, load_export
    from train_crownless_participant import loss
    torch.set_num_threads(4); torch.manual_seed(19)
    splits = dataset(args.worlds)
    for name, rows in splits.items():
        if not rows:
            raise ValueError(f'empty {name} split')
    args.output.mkdir(parents=True)
    paths = {}
    for name, rows in splits.items():
        path = args.output / f'{name}.jsonl'
        path.write_text(''.join(json.dumps(row, sort_keys=True) + '\n' for row in rows))
        paths[name] = path
    reference, metadata = load_export(args.reference, args.tokenizer)
    dimensions = tuple(getattr(reference.config, key) for key in ('dim', 'layers', 'heads', 'ff', 'vocab', 'context'))
    if dimensions != (192, 8, 6, 624, 4096, 512):
        raise ValueError('reference must match native 5M dimensions')
    model = Crownless(reference.config, mode='conversation').to(args.device)
    sources = [Path(__file__), Path(__file__).with_name('meaning_data.py'), args.zero / 'scripts' / 'crownless_v2.py',
               args.zero / 'scripts' / 'crownless_v2_export.py', args.zero / 'scripts' / 'train_crownless_participant.py']
    manifest = {'status': 'running', 'format': 'crownless-meaning-v3', 'initialization': 'fresh',
                'parameters': sum(p.numel() for p in model.parameters()), 'seed': 19,
                'steps': args.steps, 'batch_size': args.batch_size, 'worlds': args.worlds,
                'reference_sha256': sha(args.reference), 'tokenizer_sha256': sha(args.tokenizer),
                'sources': {str(path): sha(path) for path in sources},
                'datasets': {name: {'rows': len(rows), 'sha256': sha(paths[name])} for name, rows in splits.items()}}
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    optimizer = torch.optim.AdamW(model.parameters(), lr=3e-4, weight_decay=.01)
    rng = random.Random(19); started = time.monotonic(); target_tokens = 0
    try:
        with (args.output / 'history.jsonl').open('w') as history:
            for step in range(1, args.steps + 1):
                selected = [rng.choice(splits['train']) for _ in range(args.batch_size)]
                optimizer.zero_grad(set_to_none=True)
                value = loss(model, selected, args.device)
                if not torch.isfinite(value):
                    raise ValueError('non-finite training loss')
                value.backward(); torch.nn.utils.clip_grad_norm_(model.parameters(), 1.); optimizer.step()
                target_tokens += len(selected)
                item = {'step': step, 'loss': value.item(), 'target_tokens': target_tokens,
                        'seconds': time.monotonic() - started}
                history.write(json.dumps(item) + '\n'); history.flush()
                if step == 1 or step == args.steps or step % 100 == 0:
                    print(json.dumps(item), flush=True)
        model.cpu()
        model.eval()
        def evaluate(rows):
            records = []
            with torch.no_grad():
                for row in rows:
                    prefix = row['prompt']['tokens']
                    try:
                        start = prefix.index(1580) + 1
                        stop = prefix.index(1281, start)
                        allowed = prefix[start:stop]
                        tokens = torch.tensor([prefix], dtype=torch.long)
                        hidden, _ = model.hidden(tokens, torch.zeros((*tokens.shape, 16), dtype=torch.long))
                        weights = model.embedding.weight[allowed]
                        token = int(allowed[(weights @ hidden[0, -1]).argmax()])
                        valid = token in allowed
                        exact = valid and token == row['target'][0]
                        error = None
                    except (ValueError, IndexError, RuntimeError) as failure:
                        token, valid, exact, error = None, False, False, str(failure)
                    records.append({'prefix_ids': prefix, 'reference': {'choiceindex': row['teacher_index']},
                                    'ids': [] if token is None else [token], 'eos': token is not None,
                                    'valid': valid, 'exact': exact, 'error': error})
            return {'count': len(records), 'valid': sum(r['valid'] for r in records),
                    'exact': sum(r['exact'] for r in records), 'records': records}
        for name in ('development', 'test'):
            result = evaluate(splits[name])
            (args.output / f'{name}-evaluation.json').write_text(json.dumps(result, indent=2) + '\n')
            manifest[name] = {key: result[key] for key in ('count', 'valid', 'exact')}
        save(args.output / 'last.pt', model, args.tokenizer, {'format': 'crownless-meaning-v3'})
        export(model, args.tokenizer, args.output / 'last.ccv2', metadata)
        manifest.update(status='complete', target_tokens=target_tokens,
                        seconds=time.monotonic() - started, export_sha256=sha(args.output / 'last.ccv2'))
    except BaseException as error:
        manifest.update(status='failed', error=repr(error), target_tokens=target_tokens)
        save(args.output / 'partial.pt', model, args.tokenizer, {'format': 'crownless-meaning-v3', 'failed': True})
        raise
    finally:
        (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest), flush=True)


if __name__ == '__main__':
    main()
