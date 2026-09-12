#!/usr/bin/env python3
"""Finish mono speech masters and build a volume-matched listening page.

Requires numpy and scipy (included in the Pocket environment).
"""
import argparse
import hashlib
import html
import json
from pathlib import Path

import numpy as np
from scipy import signal
from scipy.io import wavfile

PRESET = dict(version='speech-finish-v1', highpass_hz=65, deess_hz=4200,
              deess_max_db=4, compressor_threshold_db=-23, ratio=2,
              knee_db=6, attack_ms=12, release_ms=140,
              target_active_rms_db=-20, max_makeup_db=6, peak_ceiling_db=-2,
              fade_ms=6)


def db(value):
    return 20 * np.log10(np.maximum(value, 1e-12))


def envelope(values, rate, attack_ms, release_ms):
    attack = np.exp(-1 / (rate * attack_ms / 1000))
    release = np.exp(-1 / (rate * release_ms / 1000))
    result = np.empty_like(values)
    level = 0.0
    for i, value in enumerate(values):
        coefficient = attack if value > level else release
        level = coefficient * level + (1 - coefficient) * value
        result[i] = level
    return result


def active_rms(samples, rate):
    # Fixed 20 ms speech windows keep pauses out of the level target.
    size = max(1, round(rate * .02))
    powers = np.array([np.mean(samples[i:i + size] ** 2)
                       for i in range(0, len(samples), size)])
    selected = powers[powers > max(10 ** (-50 / 10), powers.max(initial=0) * .01)]
    return float(np.sqrt(selected.mean())) if len(selected) else 0.0


def true_peak(samples):
    return float(np.max(np.abs(signal.resample_poly(samples, 4, 1)), initial=0))


def metrics(samples, rate):
    return dict(active_rms_db=float(db(active_rms(samples, rate))),
                sample_peak_db=float(db(np.max(np.abs(samples), initial=0))),
                oversampled_peak_db=float(db(true_peak(samples))),
                seconds=len(samples) / rate)


def finish(samples, rate):
    x = np.asarray(samples, dtype=np.float64)
    if x.ndim != 1 or len(x) == 0 or rate < 16000 or not np.isfinite(x).all():
        raise ValueError('Expected finite mono samples at 16 kHz or higher.')
    if np.max(np.abs(x)) > 1:
        raise ValueError('Expected samples in the range -1 to 1.')
    before = metrics(x, rate)
    if not np.any(x):
        return x.copy(), dict(preset=PRESET, before=before, after=before, makeup_db=0)
    y = signal.sosfilt(signal.butter(2, PRESET['highpass_hz'], 'highpass', fs=rate, output='sos'), x)
    # Smoothly turn down the bright band only when it dominates the voice.
    bright = signal.sosfilt(signal.butter(2, PRESET['deess_hz'], 'highpass', fs=rate, output='sos'), y)
    bright_level = np.sqrt(envelope(bright ** 2, rate, 3, 65))
    voice_level = np.sqrt(envelope(y ** 2, rate, 3, 65))
    reduction = np.clip((db(bright_level / np.maximum(voice_level, 1e-9)) + 12) * .5, 0, 4)
    y -= bright * (1 - 10 ** (-reduction / 20))
    # Soft-knee, 2:1 speech compression, with a smooth RMS detector.
    level = db(np.sqrt(envelope(y ** 2, rate, PRESET['attack_ms'], PRESET['release_ms'])))
    over = level - PRESET['compressor_threshold_db']
    knee = PRESET['knee_db']
    compressed = np.where(over < -knee / 2, 0,
                          np.where(over > knee / 2, over, (over + knee / 2) ** 2 / (2 * knee)))
    gain_reduction = compressed * (1 - 1 / PRESET['ratio'])
    y *= 10 ** (-gain_reduction / 20)
    rms = active_rms(y, rate)
    makeup = min(PRESET['max_makeup_db'], PRESET['target_active_rms_db'] - float(db(rms))) if rms else 0
    y *= 10 ** (makeup / 20)
    fade = min(round(rate * PRESET['fade_ms'] / 1000), len(y) // 2)
    if fade:
        edge = np.sin(np.linspace(0, np.pi / 2, fade)) ** 2
        y[:fade] *= edge
        y[-fade:] *= edge[::-1]
    # A whole-line peak trim preserves timing and avoids limiter pumping.
    ceiling = 10 ** (PRESET['peak_ceiling_db'] / 20)
    trim = min(1.0, ceiling / max(true_peak(y), 1e-12))
    y *= trim
    return y, dict(preset=PRESET, before=before, after=metrics(y, rate),
                   makeup_db=float(makeup), peak_trim_db=float(db(trim)),
                   max_compression_db=float(gain_reduction.max()),
                   max_deess_db=float(reduction.max()))


def read(path):
    rate, data = wavfile.read(path)
    if data.dtype != np.int16 or data.ndim != 1:
        raise ValueError(f'Expected mono 16-bit PCM: {path}')
    return rate, data.astype(np.float64) / 32768


def write(path, rate, data):
    wavfile.write(path, rate, np.round(data * 32767).astype(np.int16))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path, help='Folder of clean WAV masters')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        parser.error('Use a separate output folder to preserve the masters.')
    args.output.mkdir(parents=True, exist_ok=True)
    rows, cards = [], []
    for source in sorted(args.input.glob('*.wav'), key=lambda p: ('-revised-' not in p.name, p.name)):
        if source.stem.endswith('-game'):
            continue
        rate, dry = read(source)
        wet, receipt = finish(dry, rate)
        matched_gain = active_rms(wet, rate) / max(active_rms(dry, rate), 1e-12)
        # Share any comparison headroom trim across both players.
        trim = min(1., 10 ** (-2 / 20) / max(true_peak(dry * matched_gain), true_peak(wet), 1e-12))
        versions = {'raw': dry, 'matched': dry * matched_gain * trim, 'finished': wet * trim}
        files = {}
        for label, data in versions.items():
            path = args.output / f'{source.stem}-{label}.wav'
            write(path, rate, data)
            files[label] = dict(file=path.name, sha256=digest(path), metrics=metrics(read(path)[1], rate))
        # Keep the production-ready level separately from listening-pair trim.
        master = args.output / f'{source.stem}-master.wav'
        write(master, rate, wet)
        receipt.update(source=source.name, source_sha256=digest(source), master=master.name,
                       master_sha256=digest(master), comparison_trim_db=float(db(trim)), files=files)
        rows.append(receipt)
        players = ''.join(f'<label>{label}<audio controls preload="metadata" src="{html.escape(item["file"])}"></audio></label>'
                          for label, item in files.items())
        cards.append(f'<article><h2>{html.escape(source.stem.replace("-second", "").replace("-", " ").title())}</h2><div class="players">{players}</div></article>')
    if not rows:
        raise ValueError('Input folder needs clean WAV masters.')
    (args.output / 'finish.json').write_text(json.dumps(rows, indent=2) + '\n')
    page = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Crownless · Voice finish</title>
<style>:root{color-scheme:dark;font:16px system-ui;background:#17231f;color:#f6eddc}main{max-width:1100px;margin:auto;padding:36px 20px}h1,h2{font-family:Georgia,serif;font-weight:400}h1{font-size:48px}h2{font-size:25px}p{line-height:1.6;max-width:800px}article{background:#21332b;padding:24px;border-radius:12px;margin:20px 0}.players{display:grid;grid-template-columns:repeat(3,1fr);gap:20px}label{text-transform:capitalize;color:#f0d7a4}audio{display:block;width:100%;margin-top:12px}@media(max-width:750px){.players{grid-template-columns:1fr}}</style>
<main><h1>A softer finish</h1><p>Pocket voices with gentle compression, softer sharp consonants, low-rumble cleanup, and smooth edges. Use matched and finished to compare at the same speech level. Raw preserves the original level.</p><p>Start with the revised voices. Each pair uses the same performance, so you can hear the finishing alone.</p>CARDS</main><script>document.addEventListener('play',e=>{if(e.target.tagName==='AUDIO')document.querySelectorAll('audio').forEach(a=>{if(a!==e.target)a.pause()})},true)</script></html>'''
    (args.output / 'index.html').write_text(page.replace('CARDS', ''.join(cards)))
    print(f'Finished {len(rows)} masters: {args.output / "index.html"}')


if __name__ == '__main__':
    main()
