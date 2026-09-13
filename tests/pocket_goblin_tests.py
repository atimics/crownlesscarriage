"""Live Pocket-only smoke test for the shipped speech engine and goblin effect."""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/audio'))
from speech_engine import SpeechEngine
from speech_format import audio_key, cached_record, check_wav, load_cast, validate_record

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
cast = load_cast()
engine = SpeechEngine()
words = 'Vesh. 17 crowns fo thu. Tak khor thanks thu.'
results = []
for voice in ('flint-v1', 'goblin-v1'):
    record = validate_record(dict(version=1, voice=voice, language='en', delivery='firm',
        text=words, id='goblin.trade.paid', speaker='Nara Soot-Tongue',
        key=audio_key(voice, words, 'firm')), cast)
    path = args.output / (record['key'] + '.wav')
    engine(record, path)
    assert cached_record(args.output, record) == path
    receipt = json.loads(path.with_suffix('.json').read_text())
    assert receipt['model'] == 'pocket'
    if voice == 'goblin-v1':
        assert receipt['style'] == 'hrakhor-bass-v2' and receipt['speech_speed'] == 2
    else:
        assert receipt['processing']['style'] == 'pixel-v1'
    results.append(dict(voice=voice, file=str(path.resolve()), seconds=check_wav(path)))
    print(results[-1], flush=True)
assert len(engine.prompts) == 2
(args.output / 'integration.json').write_text(json.dumps(results, indent=2) + '\n')
