"""Speech records shared by the exporter, voice worker, and pack builder."""

import hashlib
import json
from pathlib import Path
import re
import wave

ROOT = Path(__file__).resolve().parents[2]
DELIVERIES = ('plain', 'warm', 'worried', 'urgent', 'quiet', 'firm')


class SpeechCollision(ValueError):
    pass


def audio_key(voice, text, delivery='plain', language='en'):
    value = 14695981039346656037
    for field in ('cc-speech-v1', voice, language, delivery, text):
        for byte in field.encode('utf-8') + b'\0':
            value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return f'{value:016x}'


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
        if not isinstance(value, str) or not value.strip() or len(value.encode('utf-8')) >= limit or '\0' in value:
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


def cached_record(folder, record):
    path = Path(folder) / (record['key'] + '.wav')
    receipt = path.with_suffix('.json')
    if not path.is_file() or not receipt.is_file():
        return None
    try:
        data = json.loads(receipt.read_text())
        if signature(data) != signature(record):
            raise SpeechCollision('Speech cache key collision')
        if path.stat().st_size > 1200100 or data.get('wav_sha256') != hashlib.sha256(path.read_bytes()).hexdigest():
            return None
        check_wav(path)
    except SpeechCollision:
        raise
    except (OSError, KeyError, ValueError, wave.Error):
        return None
    return path
