# Procedural speech

`CcSpeech` holds the exact words, speaker, voice, delivery, priority, and source
event for a spoken turn. The story module creates these records from the same
state used for dialogue text. Its builders leave the simulation unchanged.

The version-one cast has five named voices and eight voices for generated
people. Each profile includes a description for voice design. A generated
person's assignment comes from the world seed, birthplace, and character ID.
Local people use the place and interaction object ID. Travel, work, stress, and
appearance changes preserve the assignment. Successors use their own IDs.
Keep the first eight generated profiles and their selection rule fixed when
extending this cast. Give a revised reference recording a new profile version.

Recordings share a key when their cast profile, language, delivery, and exact
words match. The key includes the speech format version. This allows people
with a shared voice to reuse a greeting. Character and source event IDs remain
on the turn so playback can follow the correct person and event. Cache tools
should validate the complete record before reusing a file with the same key.

The speech builder also gives generated mine testimony a line ID. This lets
successor characters use the same speech path as the opening cast.

The client uses speech records for named conversations, town greetings, current
trade quotes, and road demands. Both conversation layouts show the record's
words. Other spoken interactions have a caption beside the playback controls.
Replay with F7 and skip with F8, or use their on-screen buttons. Closing the
interaction clears its turn. The original opening WAVs remain usable while
the larger cast pack is prepared.

## Preparing voices

The exporter writes the versioned cast and complete starting dialogue records:

```sh
<build>/crownless_audio_export --cast assets/audio/cast.json
<build>/crownless_audio_export --speech out/speech.json
python tools/audio/speech_pack.py out/speech.json --check
```

Use a separate Python 3.11 environment for Qwen voice design:

```sh
python -m pip install -r tools/audio/requirements-qwen.txt
python tools/audio/speech_pack.py --design-cast --device mps --allow-download
```

This writes a reference WAV and a receipt for each profile under
`assets/audio/cast`. Each receipt keeps its prompt, model, seed, spoken text,
and recording fingerprint. Use `--voice mara-v1` to prepare a single voice.
Choose `cpu`, `mps`, or `cuda` to match the local machine.

Use the existing Chatterbox environment to prepare dialogue from those voices:

```sh
python tools/audio/speech_pack.py out/speech.json --device mps --limit 32
```

Chatterbox and Qwen use separate dependency sets. `--engine qwen` uses the
Qwen Base model in the Qwen environment. Downloads are enabled explicitly with
`--allow-download`. Existing checked recordings are reused. Keep reference
recordings fixed within a voice version.

## Local worker

```sh
python tools/audio/speech_worker.py --device mps --cache out/voice-cache
```

The worker binds to `127.0.0.1:8766`. Submit an exported speech record to
`POST /v1/speech`. The response contains its key and queue state. Fetch
`GET /v1/speech/<key>` for a completed WAV. Pending work returns HTTP 202;
failed generation returns 503. The default queue holds 16 jobs and the cache
holds 256 MiB. Duplicate requests share a job. Old recordings leave the cache
as it fills. Exact words, cast profile, delivery, and WAV fingerprints are
checked before reuse.

For a browser on another local port, pass its exact origin with
`--allow-origin http://localhost:8000`. The worker uses a single model instance
and serial generation. `/health` reports that the request service is ready;
the model loads with the first generation request.

Run the foundation checks with `ctest --test-dir <build> -R
'speech_identity|authored_story' --output-on-failure`.
