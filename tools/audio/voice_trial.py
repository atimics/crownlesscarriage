#!/usr/bin/env python3
"""Make a small local casting trial with measured generation times."""

import argparse
import hashlib
import importlib.metadata
import json
import platform
from pathlib import Path
import resource
import time

ROOT = Path(__file__).resolve().parents[2]
LINES = {
    'welcome': 'The road is quiet this morning. Come inside, and tell me where you are going.',
    'trade': 'Jory Fen needs seventeen sacks in Silverwick. The price is thirty-eight crowns.',
    'warning': 'Get the horses across the bridge! I will stay with you.',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', choices=('pocket',), default='pocket')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--voices', nargs='+', default=['mara-v1', 'oak-v1', 'reed-v1', 'flint-v1'])
    parser.add_argument('--threads', type=int, default=2)
    args = parser.parse_args()
    cast = {v['id']: v for v in json.loads((ROOT / 'assets/audio/cast.json').read_text())}
    if args.threads < 1 or any(v not in cast for v in args.voices):
        parser.error('Choose known cast voices and a positive thread count.')
    args.output.mkdir(parents=True, exist_ok=True)
    report = dict(engine=args.engine, platform=platform.platform(), machine=platform.machine(),
                  threads=args.threads, seed=20260912, status='loading', samples=[],
                  timing_note='Model load includes first-use downloads. Voice setup is separate. '
                  'Generation includes the first call; audio styling and file writes are outside the timer. '
                  'This is a small idle-machine trial; game frame time remains to be measured.')
    report_path = args.output / f'{args.engine}.json'

    def save():
        report_path.write_text(json.dumps(report, indent=2) + '\n')

    save()
    try:
        import numpy as np
        import torch
        from scipy.io.wavfile import write
        from voice_style import render_voice
        torch.set_num_threads(args.threads)
        report['packages'] = {name: importlib.metadata.version(name) for name in ('torch', 'pocket-tts')}
        started = time.perf_counter()
        from pocket_tts import TTSModel
        model = TTSModel.load_model(language='english')
        rate = model.sample_rate
        report['load_seconds'] = time.perf_counter() - started
        report['status'] = 'generating'
        report['voice_setup_seconds'] = {}
        save()
        for voice in args.voices:
            reference = ROOT / 'assets/audio/cast' / f'{voice}.wav'
            started = time.perf_counter()
            state = model.get_state_for_audio_prompt(str(reference))
            report['voice_setup_seconds'][voice] = time.perf_counter() - started
            for line, words in LINES.items():
                torch.manual_seed(report['seed'])
                np.random.seed(report['seed'])
                started = time.perf_counter()
                first_chunk = None
                # Pocket's stream updates its cache on a background thread.
                # no_grad keeps that cache writable across thread boundaries.
                with torch.no_grad():
                    chunks = []
                    for chunk in model.generate_audio_stream(state, words):
                        if first_chunk is None:
                            first_chunk = time.perf_counter() - started
                        chunks.append(chunk)
                    samples = torch.cat(chunks).detach().cpu().numpy().reshape(-1)
                elapsed = time.perf_counter() - started
                duration = len(samples) / rate
                if not np.isfinite(samples).all() or not 0.15 <= duration <= 30:
                    raise ValueError('Generated audio needs finite samples and a duration within 0.15–30 seconds.')
                peak = float(np.max(np.abs(samples)))
                if peak < 0.001:
                    raise ValueError('Generated audio needs an audible signal.')
                samples = samples * min(1.0, 0.82 / peak)
                filename = f'{args.engine}-{voice}-{line}.wav'
                master = args.output / filename
                write(master, rate, (samples * 32767).astype(np.int16))
                row = dict(voice=voice, line=line, text=words, file=filename,
                           styled_file=filename.replace('.wav', '-game.wav'),
                           seconds=duration, generation_seconds=elapsed,
                           first_chunk_seconds=first_chunk, real_time_factor=elapsed / duration,
                           sample_rate=rate, seed=report['seed'],
                           reference_sha256=hashlib.sha256(reference.read_bytes()).hexdigest(),
                           sha256=hashlib.sha256(master.read_bytes()).hexdigest())
                render_voice(master, args.output / row['styled_file'], row)
                report['samples'].append(row)
                save()
                print(f'{args.engine}: {voice} {line}: {elapsed:.2f}s for {duration:.2f}s audio', flush=True)
        report['status'] = 'complete'
    except Exception as error:
        report['status'] = 'failed'
        report['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        report['peak_process_memory_mib'] = rss / (1024 ** 2 if platform.system() == 'Darwin' else 1024)
        save()


if __name__ == '__main__':
    main()
