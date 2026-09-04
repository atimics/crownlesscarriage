#!/usr/bin/env python3

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
FRAMES_DIR = Path("/tmp/crownless_hero_turntable_frames")
OUTPUT_PATH = ROOT / "assets" / "previews" / "hero" / "hero_turntable_v02.gif"


paths = sorted(FRAMES_DIR.glob("frame_*.png"))[::2]
if len(paths) != 48:
    raise RuntimeError(f"Expected 48 sampled animation frames, found {len(paths)}")

frames: list[Image.Image] = []
for path in paths:
    frame = Image.open(path).convert("RGB")
    frame.thumbnail((512, 512), Image.Resampling.LANCZOS)
    frame = frame.quantize(colors=128, method=Image.Quantize.MEDIANCUT)
    frames.append(frame)

OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
frames[0].save(
    OUTPUT_PATH,
    save_all=True,
    append_images=frames[1:],
    duration=83,
    loop=0,
    optimize=True,
    disposal=2,
)
print(f"Encoded {len(frames)} frames to {OUTPUT_PATH}")
