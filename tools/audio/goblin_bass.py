#!/usr/bin/env python3
"""Make an original bass reference and let speech shape it through a vocoder."""
import argparse
import hashlib
import json
from pathlib import Path
import wave

import numpy as np
from scipy.signal import butter, sosfilt


def carrier(frames, rate):
    t = np.arange(frames) / rate
    hz = 65.4 + 2.0 * np.sin(2 * np.pi * 0.37 * t) + 1.2 * np.sin(2 * np.pi * 1.13 * t)
    phase = np.cumsum(hz) * (2 * np.pi / rate)
    bass = sum(np.sin(k * phase + 0.3 * np.sin(2 * np.pi * 0.71 * t)) / k
               for k in range(1, 31))
    noise = np.random.default_rng(711).normal(0, 1, frames)
    metal = np.sin(phase * 2.713 + 2.2 * np.sin(phase * 1.419))
    pulse = 0.55 + 0.45 * np.sin(2 * np.pi * 3.2 * t) ** 2
    return np.tanh(1.4 * (bass + 0.25 * metal + 0.08 * noise)) * pulse


def vocode(speech, rate):
    sound = carrier(len(speech), rate)
    result = np.zeros(len(speech))
    edges = np.geomspace(80, min(7500, rate * 0.43), 21)
    envelope_filter = butter(2, 35, fs=rate, output='sos')
    for low, high in zip(edges[:-1], edges[1:]):
        band = butter(2, [low, high], btype='bandpass', fs=rate, output='sos')
        envelope = sosfilt(envelope_filter, np.abs(sosfilt(band, speech)))
        result += envelope * sosfilt(band, sound) * 12
    # A little original consonant detail keeps the words readable.
    detail = sosfilt(butter(2, 2200, btype='highpass', fs=rate, output='sos'), speech)
    return result + 0.22 * detail + 0.12 * speech


def write(path, sound, rate):
    sound = np.asarray(sound).copy()
    sound *= 0.78 / max(1e-12, np.max(np.abs(sound)))
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
    receipt = dict(style='hrakhor-bass-v1', seed=711, base_hz=65.4,
                   pulse_hz=6.4, bands=20, sample_rate=rate,
                   reference='Original synthetic bass; vortex-inspired sound design',
                   source_sha256=hashlib.sha256(args.speech.read_bytes()).hexdigest(),
                   renderer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                   files={name: hashlib.sha256((args.output / name).read_bytes()).hexdigest()
                          for name in ('bass-reference.wav', 'bass-speaking.wav')})
    (args.output / 'synthesis.json').write_text(json.dumps(receipt, indent=2) + '\n')
