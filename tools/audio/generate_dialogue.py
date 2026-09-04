#!/usr/bin/env python3
"""Bake the exported Crownless script with a local Chatterbox Python environment."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import wave

from voice_style import render_voice


def voice_path(line):
    if not re.fullmatch(r"[a-z0-9_.]+", line["id"]):
        raise ValueError("Line ID must use letters, numbers, dots, and underscores")
    if not isinstance(line["text"], str) or not line["text"]:
        raise ValueError("Each line needs spoken text")
    fingerprint = 2166136261
    for byte in (line["speaker"] + "\n" + line["text"]).encode("utf-8"):
        fingerprint = ((fingerprint ^ byte) * 16777619) & 0xFFFFFFFF
    return f"assets/audio/voice/{line['id']}-{fingerprint:08x}.wav"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("script", type=Path)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--cast", type=Path, help="JSON mapping speaker names to local reference WAV paths")
    parser.add_argument("--prefix", default="")
    parser.add_argument("--limit", type=int, default=4)
    parser.add_argument("--device", choices=("cpu", "mps", "cuda"), default="cpu")
    parser.add_argument("--allow-download", action="store_true", help="Allow Chatterbox to fetch model weights")
    parser.add_argument("--check", action="store_true", help="Check script and cast paths")
    args = parser.parse_args()
    if args.limit < 1:
        parser.error("--limit must be positive")
    script = json.loads(args.script.read_text())
    cast = json.loads(args.cast.read_text()) if args.cast else {}
    for speaker, reference in cast.items():
        if not Path(reference).is_file():
            parser.error(f"Reference clip for {speaker} is missing: {reference}")
    ids = set()
    for line in script:
        if line["id"] in ids or line["path"] != voice_path(line):
            parser.error(f"Duplicate or stale script entry: {line['id']}")
        ids.add(line["id"])
    selected = [line for line in script if line["id"].startswith(args.prefix)][:args.limit]
    if not selected:
        parser.error("The selected prefix has no lines")
    if args.check:
        print(f"Checked {len(script)} lines; selected {len(selected)} clips")
        return
    if not args.allow_download:
        os.environ["HF_HUB_OFFLINE"] = "1"
        os.environ["TRANSFORMERS_OFFLINE"] = "1"
    import torch
    from chatterbox.tts import ChatterboxTTS

    model = ChatterboxTTS.from_pretrained(device=args.device)
    for line in selected:
        destination = args.root / line["path"]
        master = args.root / "tools/audio/masters" / destination.name
        master.parent.mkdir(parents=True, exist_ok=True)
        reference = cast.get(line["speaker"])
        seed = int(hashlib.sha256(line["id"].encode()).hexdigest()[:8], 16)
        torch.manual_seed(seed)
        options = {"audio_prompt_path": reference} if reference else {}
        with torch.inference_mode():
            samples = model.generate(line["text"], **options).detach().float().cpu().reshape(-1)
        if not torch.isfinite(samples).all() or not 0.15 <= samples.numel() / model.sr <= 25.0:
            raise ValueError(f"Clip has invalid samples or duration: {line['id']}")
        samples = samples.clone()
        peak = samples.abs().max().item()
        if peak < 0.001:
            raise ValueError(f"Clip is silent: {line['id']}")
        samples *= min(1.0, 0.82 / peak)
        fade = min(int(model.sr * 0.004), samples.numel() // 2)
        samples[:fade] *= torch.linspace(0, 1, fade)
        samples[-fade:] *= torch.linspace(1, 0, fade)
        pcm = (samples * 32767).round().to(torch.int16).numpy().astype("<i2").tobytes()
        temporary = master.with_suffix(".wav.tmp")
        with wave.open(str(temporary), "wb") as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(model.sr)
            output.writeframes(pcm)
        temporary.replace(master)
        receipt = {
            "id": line["id"], "text": line["text"], "speaker": line["speaker"],
            "model": "ChatterboxTTS", "seed": seed, "sample_rate": model.sr,
            "reference_sha256": hashlib.sha256(Path(reference).read_bytes()).hexdigest() if reference else None,
        }
        render_voice(master, destination, receipt)
        print(f"Baked {line['id']} ({samples.numel() / model.sr:.1f}s)")


if __name__ == "__main__":
    main()
