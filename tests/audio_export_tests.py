from array import array
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import wave

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "tools/audio"))
import generate_dialogue as dialogue
from voice_style import pixel_voice, render_voice

# Silence stays silent. Loud input gets headroom and clean edges.
assert not any(pixel_voice(array("h", [0] * 1200), 24000))
tone = array("h", (round(32000 * math.sin(2 * math.pi * 440 * i / 24000))
                   for i in range(2400)))
styled_tone = pixel_voice(tone, 24000)
assert len(styled_tone) == len(tone)
assert styled_tone[0] == styled_tone[-1] == 0
assert max(abs(value) for value in styled_tone) <= round(0.82 * 32767)
# The texture keeps a spoken-range fundamental stronger than its harmonics.
def magnitude(samples, frequency):
    return abs(sum(value * complex(math.cos(2 * math.pi * frequency * i / 24000),
                                   math.sin(2 * math.pi * frequency * i / 24000))
                   for i, value in enumerate(samples)))

assert magnitude(styled_tone, 440) > 20 * magnitude(styled_tone, 880)

with tempfile.TemporaryDirectory() as temporary:
    folder = Path(temporary)
    for option in ("--script", "--opening"):
        script = folder / "script.json"
        subprocess.run([sys.argv[1], option, str(script)], check=True)
        lines = json.loads(script.read_text())
        assert len(lines) >= 30 if option == "--script" else len(lines) == 4
        assert len({line["id"] for line in lines}) == len(lines)
        for line in lines:
            assert dialogue.voice_path(line) == line["path"]
        if option == "--opening":
            assert all(line["speaker"] == "Mara Venn" for line in lines)
            assert "Silverwick" in lines[0]["text"]
            assert "8 boxes" in lines[1]["text"]
            assert "Jory Fen" in lines[2]["text"]
        subprocess.run([sys.executable, str(root / "tools/audio/generate_dialogue.py"),
                        str(script), "--check"], check=True)
    preview = folder / "preview.wav"
    subprocess.run([sys.argv[1], "--preview", str(preview)], check=True)
    with wave.open(str(preview)) as audio:
        assert audio.getnchannels() == 1
        assert audio.getsampwidth() == 2
        assert audio.getframerate() == 22050
        assert 7 < audio.getnframes() / 22050 < 10

for receipt in (root / "assets/audio/voice").glob("*.json"):
    data = json.loads(receipt.read_text())
    path = root / dialogue.voice_path(data)
    master = root / "tools/audio/masters" / path.name
    assert receipt.with_suffix(".wav") == path
    assert hashlib.sha256(path.read_bytes()).hexdigest() == data["wav_sha256"]
    assert hashlib.sha256(master.read_bytes()).hexdigest() == data["master_sha256"]
    assert data["processing"]["style"] == "pixel-v1"
    with wave.open(str(path)) as audio:
        assert audio.getnchannels() == 1 and audio.getsampwidth() == 2
        assert audio.getframerate() == data["sample_rate"]
        assert 0.15 <= audio.getnframes() / audio.getframerate() <= 25
        wet = array("h", audio.readframes(audio.getnframes()))
    with wave.open(str(master)) as audio:
        assert audio.getframerate() == data["sample_rate"]
        dry = array("h", audio.readframes(audio.getnframes()))
    if sys.byteorder != "little":
        wet.byteswap()
        dry.byteswap()
    assert len(wet) == len(dry)
    assert wet[0] == wet[-1] == 0
    assert max(abs(value) for value in wet) <= round(0.82 * 32767)
    dry_energy = sum(value * value for value in dry)
    wet_energy = sum(value * value for value in wet)
    # The light texture preserves speech level and remains close to the source.
    assert 0.75 < math.sqrt(wet_energy / dry_energy) <= 1.05
    correlation = sum(a * b for a, b in zip(wet, dry)) / math.sqrt(dry_energy * wet_energy)
    assert correlation > 0.95
    assert wet != dry
    with tempfile.TemporaryDirectory() as temporary:
        rendered = Path(temporary) / path.name
        render_voice(master, rendered, data)
        assert rendered.read_bytes() == path.read_bytes()
        # Re-rendering always starts with the clean master.
        render_voice(master, rendered, data)
        assert rendered.read_bytes() == path.read_bytes()
print("Audio exports and shipped dialogue checked")
