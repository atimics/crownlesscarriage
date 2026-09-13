#!/usr/bin/env python3
"""Compare local cast references with Chatterbox's speaker encoder."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--weights', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--revised', type=Path, help='Folder containing the six revised reference WAVs')
    parser.add_argument('--select-alternates', type=Path, help='Choose less overlapping takes from this folder and copy them into --revised')
    args = parser.parse_args()
    if args.select_alternates and not args.revised:
        parser.error('--select-alternates requires --revised')
    import librosa
    import numpy as np
    import torch
    from safetensors.torch import load_file
    from chatterbox.models.voice_encoder import VoiceEncoder
    torch.set_num_threads(2)
    model = VoiceEncoder()
    model.load_state_dict(load_file(args.weights))
    model.eval()
    voices = json.loads((ROOT / 'assets/audio/cast.json').read_text())
    replacements = {}
    if args.revised:
        brief = json.loads(Path(__file__).with_name('cast_reaudition.json').read_text())
        replacements = {v['original']: args.revised / (v['id'] + '.wav') for v in brief['voices']}
    embeddings = []
    sources = {}
    with torch.inference_mode():
        for voice in voices:
            path = replacements.get(voice['id'], ROOT / 'assets/audio/cast' / (voice['id'] + '.wav'))
            samples, _ = librosa.load(path, sr=16000)
            vector = model.embeds_from_wavs([samples], sample_rate=16000)[0]
            embeddings.append(vector / np.linalg.norm(vector))
            sources[voice['id']] = hashlib.sha256(path.read_bytes()).hexdigest()
    selections = []
    if args.select_alternates:
        for target in [v for v in brief['voices'] if v['name'] in ('Flint', 'Reed')]:
            index = next(i for i, v in enumerate(voices) if v['id'] == target['original'])
            original = replacements[target['original']]
            candidates = [original] + sorted(args.select_alternates.glob(target['id'] + '-alt*.wav'))
            scored = []
            with torch.inference_mode():
                for path in candidates:
                    samples, _ = librosa.load(path, sr=16000)
                    vector = model.embeds_from_wavs([samples], sample_rate=16000)[0]
                    vector /= np.linalg.norm(vector)
                    maximum = max(float(np.dot(vector, other)) for i, other in enumerate(embeddings) if i != index)
                    scored.append((maximum, path, vector))
            best = min(scored, key=lambda item: item[0])
            if best[1] != original:
                shutil.copy2(best[1], original)
                shutil.copy2(best[1].with_suffix('.json'), original.with_suffix('.json'))
            embeddings[index] = best[2]
            sources[target['original']] = hashlib.sha256(original.read_bytes()).hexdigest()
            selections.append(dict(voice=target['original'], selected=best[1].name,
                                   candidates=[dict(file=p.name, nearest_cosine=score) for score, p, _ in scored]))
    pairs = sorted((dict(first=a['id'], second=b['id'], cosine=float(np.dot(embeddings[i], embeddings[j])))
                    for i, a in enumerate(voices) for j, b in enumerate(voices) if i < j),
                   key=lambda row: row['cosine'], reverse=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(dict(
        method='Chatterbox speaker encoder, cosine similarity, common reference passage',
        note='A screening signal for this recording set. Listening decides perceived identity and performance.',
        weights_sha256=hashlib.sha256(args.weights.read_bytes()).hexdigest(),
        sources=sources, selections=selections, pairs=pairs), indent=2) + '\n')
    print(json.dumps(pairs[:15], indent=2))


if __name__ == '__main__':
    main()
