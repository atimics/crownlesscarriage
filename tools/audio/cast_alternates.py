#!/usr/bin/env python3
"""Make a small batch of alternate takes for the closest revised voices."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    import numpy as np
    import soundfile as sf
    import torch
    from qwen_tts import Qwen3TTSModel
    brief = json.loads(Path(__file__).with_name('cast_reaudition.json').read_text())
    chosen = [v for v in brief['voices'] if v['name'] in ('Flint', 'Reed')]
    batch = [dict(v, variant=n) for v in chosen for n in range(1, 4)]
    text = 'The road is quiet this morning. Come inside, and tell me where you are going. We can find a place for your horses.'
    model = Qwen3TTSModel.from_pretrained('Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign',
                                         device_map='mps', dtype=torch.float32, attn_implementation='sdpa')
    seed = 20260913
    torch.manual_seed(seed)
    wavs, rate = model.generate_voice_design(text=[text] * len(batch), language='English',
                                             instruct=[v['description'] for v in batch], max_new_tokens=400)
    args.output.mkdir(parents=True, exist_ok=True)
    for index, (voice, samples) in enumerate(zip(batch, wavs)):
        samples = np.asarray(samples).reshape(-1)
        if not np.isfinite(samples).all() or not 0.15 <= len(samples) / rate <= 25:
            raise ValueError('Alternate needs finite samples and a duration within 0.15–25 seconds.')
        path = args.output / f'{voice["id"]}-alt{voice["variant"]}.wav'
        sf.write(path, samples, rate, subtype='PCM_16')
        receipt = dict(voice, text=text, model='Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign',
                       seed=seed, batch_index=index, batch_size=len(batch), sample_rate=rate,
                       sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        path.with_suffix('.json').write_text(json.dumps(receipt, indent=2) + '\n')
        print(path.name, flush=True)


if __name__ == '__main__':
    main()
