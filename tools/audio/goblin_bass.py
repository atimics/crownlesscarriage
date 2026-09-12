#!/usr/bin/env python3
"""Make an original bass reference and let speech shape it through a vocoder."""
import argparse
import hashlib
import json
from pathlib import Path
import wave

import numpy as np
from scipy.signal import butter, sosfilt


SETTINGS = dict(base_hz=49.0, pulse_hz=3.1, bands=28, seed=711,
                sub_mix=0.32, consonant_mix=0.28, dry_mix=0.04)


def carrier(frames, rate):
    if rate < 8000 or frames < 1:
        raise ValueError('Use positive frames and a sample rate of at least 8000 Hz')
    t = np.arange(frames) / rate
    hz = SETTINGS['base_hz'] + 0.65 * np.sin(2 * np.pi * 0.37 * t)
    phase = np.cumsum(hz) * (2 * np.pi / rate)
    # A low fundamental with a detuned harmonic throat stays audible on small speakers.
    harmonics = min(48, int(rate * 0.4 / 50))
    body = sum((np.sin(k * phase) + 0.3 * np.sin(k * phase * 1.006)) / k ** 0.85
               for k in range(1, harmonics + 1))
    metal = sum(np.sin(phase * ratio + 0.35 * np.sin(2 * np.pi * 0.7 * t)) / (i + 1)
                for i, ratio in enumerate((2.713, 4.419, 7.137, 11.731)))
    # One pulse per cycle, with a slow sway in rate and a rounded attack.
    pulse_phase = 2 * np.pi * SETTINGS['pulse_hz'] * t + 0.45 * np.sin(2 * np.pi * 0.23 * t)
    pulse = 0.3 + 0.7 * (0.5 + 0.5 * np.sin(pulse_phase)) ** 1.6
    noise = np.random.default_rng(SETTINGS['seed']).normal(0, 1, frames)
    breath = sosfilt(butter(2, 1800, btype='highpass', fs=rate, output='sos'), noise)
    # Keep a little high-frequency energy between throbs for the consonant bands.
    return 0.65 * np.tanh(0.9 * (body + 0.22 * metal)) * pulse + 0.045 * breath


def vocode(speech, rate):
    speech = np.asarray(speech, dtype=np.float64)
    if rate < 8000 or speech.ndim != 1 or len(speech) < 1024:
        raise ValueError('Use at least 1024 mono samples at 8000 Hz or higher')
    if not np.isfinite(speech).all() or np.max(np.abs(speech)) > 1:
        raise ValueError('Samples must be finite and within -1..1')
    sound = carrier(len(speech), rate)
    result = np.zeros(len(speech))
    edges = np.geomspace(55, min(8500, rate * 0.43), SETTINGS['bands'] + 1)
    envelope_filter = butter(2, 45, fs=rate, output='sos')
    for low, high in zip(edges[:-1], edges[1:]):
        band = butter(2, [low, high], btype='bandpass', fs=rate, output='sos')
        envelope = np.maximum(0, sosfilt(envelope_filter, np.abs(sosfilt(band, speech))))
        texture = sosfilt(band, sound)
        # Balance carrier bands so bass energy leaves room for vowel detail.
        texture /= max(0.008, np.sqrt(np.mean(texture ** 2)))
        result += envelope * texture
    syllables = np.maximum(0, sosfilt(butter(2, 18, fs=rate, output='sos'), np.abs(speech)))
    sub = sosfilt(butter(2, 150, fs=rate, output='sos'), sound)
    sub /= max(0.008, np.sqrt(np.mean(sub ** 2)))
    detail = sosfilt(butter(2, 2200, btype='highpass', fs=rate, output='sos'), speech)
    result += (SETTINGS['sub_mix'] * syllables * sub + SETTINGS['consonant_mix'] * detail
               + SETTINGS['dry_mix'] * speech)
    # Match input RMS for fair listening, with fixed headroom for the final WAV.
    rms = np.sqrt(np.mean(result ** 2))
    if rms > 0:
        result *= np.sqrt(np.mean(speech ** 2)) / rms
    result *= min(1, 0.78 / max(1e-12, np.max(np.abs(result))))
    fade = min(round(rate * 0.005), len(result) // 2)
    edge = np.linspace(0, 1, fade)
    result[:fade] *= edge
    result[-fade:] *= edge[::-1]
    return result


def write(path, sound, rate):
    sound = np.asarray(sound).copy()
    sound *= min(1, 0.78 / max(1e-12, np.max(np.abs(sound))))
    fade = min(round(rate * 0.01), len(sound) // 2)
    edge = np.linspace(0, 1, fade)
    sound[:fade] *= edge
    sound[-fade:] *= edge[::-1]
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), 'wb') as output:
        output.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
        output.writeframes(np.rint(sound * 32767).astype('<i2').tobytes())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--speech', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.speech.resolve() in {(args.output / name).resolve() for name in ('bass-reference.wav', 'bass-speaking.wav')}:
        raise ValueError('Choose an output folder that keeps the speech master separate')
    with wave.open(str(args.speech)) as audio:
        if audio.getnchannels() != 1 or audio.getsampwidth() != 2:
            raise ValueError('Use mono 16-bit PCM speech')
        rate = audio.getframerate()
        if rate < 8000:
            raise ValueError('Use a sample rate of at least 8000 Hz')
        speech = np.frombuffer(audio.readframes(audio.getnframes()), dtype='<i2') / 32768.0
        if len(speech) < 1024:
            raise ValueError('Use at least 1024 samples')
    write(args.output / 'bass-reference.wav', carrier(rate * 8, rate), rate)
    write(args.output / 'bass-speaking.wav', vocode(speech, rate), rate)
    receipt = dict(style='hrakhor-bass-v2', settings=SETTINGS, sample_rate=rate,
                   reference='Original synthetic bass; vortex-inspired sound design',
                   source_sha256=hashlib.sha256(args.speech.read_bytes()).hexdigest(),
                   renderer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                   files={name: hashlib.sha256((args.output / name).read_bytes()).hexdigest()
                          for name in ('bass-reference.wav', 'bass-speaking.wav')})
    (args.output / 'synthesis.json').write_text(json.dumps(receipt, indent=2) + '\n')
