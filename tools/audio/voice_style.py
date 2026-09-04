#!/usr/bin/env python3
"""Render a light retro voice texture from the saved Chatterbox masters."""

import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import sys
import wave


def pixel_voice(samples, sample_rate):
    """Blend 40% filtered, held 8-bit speech with 60% clean speech."""
    if sample_rate < 8000 or not samples:
        raise ValueError("Voice needs samples and a sample rate of at least 8000 Hz")
    stride = max(1, round(sample_rate / 12000))
    cutoff = 0.45 / stride
    radius = 16
    taps = []
    for offset in range(-radius, radius + 1):
        sinc = (2 * cutoff if offset == 0 else
                math.sin(2 * math.pi * cutoff * offset) / (math.pi * offset))
        window = 0.54 + 0.46 * math.cos(math.pi * offset / radius)
        taps.append(sinc * window)
    total = sum(taps)
    taps = [tap / total for tap in taps]
    result = []
    held = 0.0
    for index, dry in enumerate(samples):
        if index % stride == 0:
            # A centred filter keeps word timing intact and softens aliasing.
            filtered = sum(samples[source] * tap
                           for offset, tap in enumerate(taps)
                           if 0 <= (source := index + offset - radius) < len(samples))
            held = round(filtered / 256) * 256
        result.append(0.6 * dry + 0.4 * held)
    gain = min(1.0, 0.82 * 32767 / max(1.0, max(abs(value) for value in result)))
    fade = min(round(sample_rate * 0.004), len(result) // 2)
    for index in range(fade):
        edge = index / max(1, fade - 1)
        result[index] *= edge
        result[-1 - index] *= edge
    return array("h", (round(value * gain) for value in result))


def render_voice(master, destination, receipt):
    with wave.open(str(master), "rb") as source:
        if source.getnchannels() != 1 or source.getsampwidth() != 2:
            raise ValueError(f"Voice master must be mono 16-bit PCM: {master}")
        sample_rate = source.getframerate()
        samples = array("h", source.readframes(source.getnframes()))
    if sys.byteorder != "little":
        samples.byteswap()
    styled = pixel_voice(samples, sample_rate)
    if sys.byteorder != "little":
        styled.byteswap()
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(".wav.tmp")
    with wave.open(str(temporary), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(styled.tobytes())
    temporary.replace(destination)
    receipt = dict(receipt, sample_rate=sample_rate,
                   master_sha256=hashlib.sha256(master.read_bytes()).hexdigest(),
                   wav_sha256=hashlib.sha256(destination.read_bytes()).hexdigest(),
                   processing={"style": "pixel-v1", "bits": 8, "mix": 0.4,
                               "texture_sample_rate": sample_rate / max(1, round(sample_rate / 12000))})
    destination.with_suffix(".json").write_text(json.dumps(receipt, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    for receipt_path in sorted((args.root / "assets/audio/voice").glob("*.json")):
        destination = receipt_path.with_suffix(".wav")
        master = args.root / "tools/audio/masters" / destination.name
        render_voice(master, destination, json.loads(receipt_path.read_text()))
        print(f"Styled {destination.name}")


if __name__ == "__main__":
    main()
