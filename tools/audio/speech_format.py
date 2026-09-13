"""Speech records shared by the exporter, voice worker, and pack builder."""

import hashlib
import importlib.metadata
import json
from pathlib import Path
import re
import wave

ROOT = Path(__file__).resolve().parents[2]
DELIVERIES = ('plain', 'warm', 'worried', 'urgent', 'quiet', 'firm')
CACHE_FORMAT = 'cc-speech-cache-v2'
POSTPROCESS_VERSION = 'voice-style-v1'


class SpeechCollision(ValueError):
    pass


def _fnv64(value, field):
    for byte in field.encode('utf-8') + b'\x00':
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


def audio_key(voice, text, delivery='plain', language='en'):
    value = 14695981039346656037
    for field in ('cc-speech-v1', voice, language, delivery, text):
        value = _fnv64(value, field)
    return f'{value:016x}'


def package_version(name, fallback='uninstalled'):
    try:
        return importlib.metadata.version(name)
    except importlib.metadata.PackageNotFoundError:
        return fallback


def reference_sha256(record, references):
    path = Path(references) / (record['voice'] + '.wav')
    if not path.is_file():
        return 'missing'
    return hashlib.sha256(path.read_bytes()).hexdigest()


def expected_cache_metadata(record, references, engine_version=None):
    return {
        'cache_format': CACHE_FORMAT,
        'model': 'pocket',
        'engine_version': engine_version or package_version('pocket-tts'),
        'reference_sha256': reference_sha256(record, references),
        'postprocess_version': POSTPROCESS_VERSION,
    }


def derive_render_key(record, model_version='pocket-tts',
                      reference_hashes=None, effects_version=None):
    """Combine the speech content key with every renderer fingerprint.

    Same content + same renderer = same audio output.
    A change in model, reference voice, effects or format
    produces a different render key and never reuses stale audio.
    """
    speech_key = record.get('key') or audio_key(
        record['voice'], record['text'],
        record.get('delivery', 'plain'), record.get('language', 'en'))
    fields = [speech_key, model_version]
    if isinstance(reference_hashes, dict):
        fields.append(reference_hashes.get(record['voice'], 'missing'))
    else:
        fields.append('unpinned')
    fields.append(effects_version or 'none')
    fields.append('cc-speech-v1')
    value = 14695981039346656037
    for field in fields:
        value = _fnv64(value, field)
    return f'{value:016x}'


def compute_reference_hashes(cast, references_root):
    """Pre-compute voice-reference hashes for render-key derivation."""
    result = {}
    for voice in cast:
        ref_path = Path(references_root) / (voice + '.wav')
        result[voice] = hashlib.sha256(ref_path.read_bytes()).hexdigest() if ref_path.is_file() else 'missing'
    return result


class ObjectStorage:
    """Pluggable object storage for generated speech."""

    def store(self, render_key, wav_data, receipt):
        raise NotImplementedError

    def fetch(self, render_key):
        raise NotImplementedError

    def url(self, render_key):
        return None

    def health(self):
        return {}


class FilesystemStorage(ObjectStorage):
    """Store speech objects as flat WAV + JSON files under a root directory.

    Files are stored as {root}/{render_key[:2]}/{render_key}.wav
    with a matching .json receipt. This layout keeps the hot directory
    small even with millions of objects.
    """

    def __init__(self, root, serve_url=None):
        self.root = Path(root)
        self.serve_url = serve_url
        self.root.mkdir(parents=True, exist_ok=True)

    def _path(self, render_key, ext):
        prefix = render_key[:2]
        directory = self.root / prefix
        directory.mkdir(parents=True, exist_ok=True)
        return directory / f'{render_key}{ext}'

    def store(self, render_key, wav_data, receipt):
        wav_path = self._path(render_key, '.wav')
        json_path = self._path(render_key, '.json')
        if wav_path.exists():
            return self.url(render_key) or wav_path.name
        wav_path.write_bytes(wav_data)
        json_path.write_text(json.dumps(receipt, indent=2) + '\n')
        return self.url(render_key) or wav_path.name

    def fetch(self, render_key):
        wav_path = self._path(render_key, '.wav')
        json_path = self._path(render_key, '.json')
        if not wav_path.is_file() or not json_path.is_file():
            return None
        return wav_path.read_bytes(), json.loads(json_path.read_text())

    def url(self, render_key):
        if self.serve_url is None:
            return None
        return f'{self.serve_url.rstrip("/")}/{render_key[:2]}/{render_key}.wav'

    def health(self):
        wav_count = 0
        total_bytes = 0
        for prefix_dir in self.root.iterdir():
            if prefix_dir.is_dir():
                for f in prefix_dir.glob('*.wav'):
                    wav_count += 1
                    total_bytes += f.stat().st_size
        return {'engine': 'filesystem', 'root': str(self.root), 'objects': wav_count, 'bytes': total_bytes}


def load_cast(path=ROOT / 'assets/audio/cast.json'):
    entries = json.loads(Path(path).read_text())
    cast = {}
    for entry in entries:
        voice = entry['id']
        if not re.fullmatch(r'[a-z]+-v[1-9][0-9]*', voice) or voice in cast:
            raise ValueError('Cast IDs must be unique versioned names')
        if not isinstance(entry['description'], str) or not entry['description'].strip():
            raise ValueError('Each voice needs a description')
        cast[voice] = entry
    return cast


def validate_record(record, cast):
    if not isinstance(record, dict) or record.get('version') != 1:
        raise ValueError('Expected a version-one speech record')
    for field, limit in (('text', 512), ('id', 96), ('speaker', 32)):
        value = record.get(field)
        if not isinstance(value, str) or not value.strip() or len(value.encode('utf-8')) >= limit or '\x00' in value:
            raise ValueError(f'Invalid speech {field}')
    if record.get('voice') not in cast or record.get('language') != 'en':
        raise ValueError('Choose a voice and language from the cast')
    if record.get('delivery') not in DELIVERIES:
        raise ValueError('Choose a supported delivery')
    expected = audio_key(record['voice'], record['text'], record['delivery'], record['language'])
    if record.get('key') != expected:
        raise ValueError('Speech key differs from its words and voice')
    return {key: record[key] for key in ('version', 'key', 'voice', 'language', 'delivery', 'text', 'id', 'speaker')}


def signature(record):
    return {field: record[field] for field in ('version', 'voice', 'language', 'delivery', 'text')}


def check_wav(path):
    with wave.open(str(path), 'rb') as source:
        if source.getnchannels() != 1 or source.getsampwidth() != 2 or source.getframerate() != 24000:
            raise ValueError('Speech must be mono 16-bit PCM at 24 kHz')
        frames = source.getnframes()
        if not 0.15 <= frames / 24000 <= 25:
            raise ValueError('Speech duration must be between 0.15 and 25 seconds')
        pcm = source.readframes(frames)
        if len(pcm) != frames * 2 or not any(pcm):
            raise ValueError('Speech has incomplete or silent samples')
    return frames / 24000


def cached_record(folder, record, expected_metadata=None):
    path = Path(folder) / (record['key'] + '.wav')
    receipt = path.with_suffix('.json')
    if not path.is_file() or not receipt.is_file():
        return None
    try:
        data = json.loads(receipt.read_text())
        if signature(data) != signature(record):
            raise SpeechCollision('Speech cache key collision')
        if expected_metadata is not None:
            for field, expected in expected_metadata.items():
                if data.get(field) != expected:
                    return None
        if path.stat().st_size > 1200100 or data.get('wav_sha256') != hashlib.sha256(path.read_bytes()).hexdigest():
            return None
        if record['voice'] == 'goblin-v1' and (data.get('model') != 'pocket' or
                data.get('style') != 'hrakhor-bass-v2' or data.get('speech_speed') != 2.0):
            return None
        check_wav(path)
    except SpeechCollision:
        raise
    except (OSError, KeyError, ValueError, wave.Error):
        return None
    return path