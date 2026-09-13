#!/usr/bin/env python3
"""Render Crownless goblin voices from mono 16-bit PCM masters."""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import wave

import librosa
import numpy as np
from scipy.ndimage import gaussian_filter1d
from scipy.signal import butter, sosfilt
from goblin_bass import SETTINGS as BASS_SETTINGS, vocode

PRESETS = {
    'bass': BASS_SETTINGS,
    'goblin': dict(pitch=1.0, formant=-1.5, growl_pitch=-5.0, growl_mix=0.13,
                   rasp_mix=0.08, echo_mix=0.07, echo_ms=70),
    'priest': dict(pitch=0.5, formant=-2.5, growl_pitch=-7.0, growl_mix=0.25,
                   rasp_mix=0.13, echo_mix=0.13, echo_ms=90),
}


def throat(samples, semitones):
    """Shift a smoothed spectral envelope while retaining harmonic positions."""
    spectrum = librosa.stft(samples, n_fft=1024, hop_length=256)
    envelope = gaussian_filter1d(np.log(np.maximum(np.abs(spectrum), 1e-5)),
                                sigma=10, axis=0)
    bins = np.arange(envelope.shape[0])
    source_bins = bins / (2 ** (semitones / 12))
    shifted = np.column_stack([np.interp(source_bins, bins, frame)
                               for frame in envelope.T])
    gain = np.exp(np.clip(shifted - envelope, -1.1, 1.1))
    return librosa.istft(spectrum * gain, hop_length=256, length=len(samples))


def process(samples, rate, preset='goblin', speed=None):
    samples = np.asarray(samples, dtype=np.float64)
    if preset not in PRESETS or rate < 8000 or samples.ndim != 1 or len(samples) < 1024:
        raise ValueError('Use a known preset and at least 1024 mono samples at 8000 Hz or higher')
    if not np.isfinite(samples).all() or np.max(np.abs(samples)) > 1:
        raise ValueError('Samples must be finite and within -1..1')
    speed = (2.0 if preset == 'bass' else 1.0) if speed is None else speed
    if not np.isfinite(speed) or not 0.5 <= speed <= 3.0:
        raise ValueError('Use a speech speed between 0.5 and 3.0')
    if round(len(samples) / speed) < 1024:
        raise ValueError('Speech must retain at least 1024 samples after the speed change')
    if speed != 1.0:
        samples = librosa.effects.time_stretch(samples, rate=speed, n_fft=1024)
        samples *= min(1, 1 / max(1e-12, np.max(np.abs(samples))))
    if not np.any(samples):
        return samples.copy()
    if preset == 'bass':
        return vocode(samples, rate)
    settings = PRESETS[preset]
    lead = librosa.effects.pitch_shift(samples, sr=rate, n_steps=settings['pitch'])
    lead = throat(lead, settings['formant'])
    growl = librosa.effects.pitch_shift(samples, sr=rate, n_steps=settings['growl_pitch'])
    growl = sosfilt(butter(2, 1600, fs=rate, output='sos'), growl)
    # Saturation follows the voice, including its quiet pauses.
    rasp = np.tanh(2.5 * lead) / 2.5
    result = ((1 - settings['rasp_mix']) * lead + settings['rasp_mix'] * rasp
              + settings['growl_mix'] * growl)
    reflection = sosfilt(butter(2, 2000, fs=rate, output='sos'), result)
    delay = round(rate * settings['echo_ms'] / 1000)
    if delay < len(result):
        result[delay:] += settings['echo_mix'] * reflection[:-delay]
    # Match dry loudness for comparison, then retain peak headroom.
    dry_rms = np.sqrt(np.mean(samples ** 2))
    wet_rms = np.sqrt(np.mean(result ** 2))
    if wet_rms > 0:
        result *= dry_rms / wet_rms
    result *= min(1, 0.82 / max(1e-12, np.max(np.abs(result))))
    fade = min(round(rate * 0.005), len(result) // 2)
    edge = np.linspace(0, 1, fade)
    result[:fade] *= edge
    result[-fade:] *= edge[::-1]
    return result


def render(master, destination, preset='goblin', speed=None):
    master, destination = Path(master), Path(destination)
    if master.resolve() == destination.resolve():
        raise ValueError('Choose a separate output file to preserve the master')
    with wave.open(str(master), 'rb') as source:
        if source.getnchannels() != 1 or source.getsampwidth() != 2:
            raise ValueError('Use a mono 16-bit PCM WAV master')
        rate = source.getframerate()
        samples = np.frombuffer(source.readframes(source.getnframes()), dtype='<i2') / 32768.0
    result = process(samples, rate, preset, speed)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(destination), 'wb') as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(rate)
        output.writeframes(np.rint(result * 32767).astype('<i2').tobytes())
    receipt = dict(style='hrakhor-bass-v2' if preset == 'bass' else 'hrakhor-voice-v1', preset=preset, settings=PRESETS[preset],
                   sample_rate=rate, frames=len(result),
                   speech_speed=(2.0 if preset == 'bass' else 1.0) if speed is None else speed,
                   source_sha256=hashlib.sha256(master.read_bytes()).hexdigest(),
                   wav_sha256=hashlib.sha256(destination.read_bytes()).hexdigest(),
                   renderer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                   packages={p: importlib.metadata.version(p) for p in ('numpy', 'scipy', 'librosa')},
                   bass_renderer_sha256=hashlib.sha256(Path(__file__).with_name('goblin_bass.py').read_bytes()).hexdigest())
    destination.with_suffix('.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return receipt


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('master', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--preset', choices=PRESETS, default='goblin')
    parser.add_argument('--speed', type=float, help='Speech rate; bass defaults to 2, other presets to 1')
    args = parser.parse_args()
    render(args.master, args.output, args.preset, args.speed)
    print(args.output)
