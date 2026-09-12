#!/usr/bin/env python3
"""Compare local cast references with Chatterbox's speaker encoder."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--weights', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--revised', type=Path, help='Folder containing the six revised reference WAVs')
    args = parser.parse_args()
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
    pairs = sorted((dict(first=a['id'], second=b['id'], cosine=float(np.dot(embeddings[i], embeddings[j])))
                    for i, a in enumerate(voices) for j, b in enumerate(voices) if i < j),
                   key=lambda row: row['cosine'], reverse=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(dict(
        method='Chatterbox speaker encoder, cosine similarity, common reference passage',
        note='A screening signal for this recording set. Listening decides perceived identity and performance.',
        weights_sha256=hashlib.sha256(args.weights.read_bytes()).hexdigest(),
        sources=sources, pairs=pairs), indent=2) + '\n')
    print(json.dumps(pairs[:15], indent=2))


if __name__ == '__main__':
    main()
