import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import wave

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("dialogue", root / "tools/audio/generate_dialogue.py")
dialogue = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dialogue)

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
    import hashlib

    data = json.loads(receipt.read_text())
    path = root / dialogue.voice_path(data)
    assert receipt.with_suffix(".wav") == path
    assert hashlib.sha256(path.read_bytes()).hexdigest() == data["wav_sha256"]
    with wave.open(str(path)) as audio:
        assert audio.getnchannels() == 1 and audio.getsampwidth() == 2
        assert audio.getframerate() == data["sample_rate"]
        assert 0.15 <= audio.getnframes() / audio.getframerate() <= 25
print("Audio exports and shipped dialogue checked")
