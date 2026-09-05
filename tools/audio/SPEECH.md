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

## Playback delivery

Native builds fetch missing recordings from `http://127.0.0.1:8766` on a
background thread. Set `CROWNLESS_VOICE_URL` to another worker URL, or to an
empty value for packaged audio only. Each request has a 45-second deadline
and a 1.2 MB download budget. Changing the turn cancels its request. Late
results are checked against the turn before playback. Replay can retry a
failed request. Captions remain available while generation runs.

Browser builds fetch `speech/<key>.wav` beside the game, then use an optional
worker configured through `globalThis.CROWNLESS_VOICE_URL` before game startup.
They keep 32 clips in memory and up to 64 in browser storage. Configure the
worker's allowed origin to match the page. Native worker caches survive
restarts. Audio downloads stay outside the render loop on both platforms.

Delivery tests use small WAV fixtures and cover the actual HTTP queue, oversized
responses, cancellation, late completion, cache reuse, and playback lifetime:

```sh
ctest --test-dir <build> -R 'speech_delivery|audio_playback_lifetime' --output-on-failure
```

## Campaign pack

`--campaign` exports representative campaign states and every named authored
performance, including relationship branches. It covers first meetings,
questions, promises, withdrawal, success, failure, and mine discovery. The
version-one pack contains 84 unique recordings. Runtime names and quantities
come from the same speech builders. A changed line gets a new key.

```sh
<build>/crownless_audio_export --campaign out/campaign-speech.json
python tools/audio/speech_pack.py out/campaign-speech.json --device mps --limit 1000
ctest --test-dir <build> -R campaign_speech_pack --output-on-failure
```

CMake copies campaign recordings into native bundles and the browser's
`speech` directory. Browser clients download each clip on demand. Cast references
stay in the source tree for the generation worker. Each cast receipt records
its description, model, seed, text, and WAV fingerprint. Each speech receipt
links to that reference fingerprint and records the voice texture step.

The references were generated from the descriptions in `cast.json` with
[Qwen3-TTS VoiceDesign](https://huggingface.co/Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign).
Dialogue was generated with [Chatterbox](https://github.com/resemble-ai/chatterbox)
and processed with the existing pixel voice texture. Generation is local.
Listening review continues through playtests; the automated checks verify
record identity, audio format, duration, and fingerprints.

For a reviewed replacement, select its record in a small script and pass
`--take 1`. A positive take replaces only the selected recordings. The receipt
keeps the take number and its seed. Keep the cast reference fixed.

For a fast voice, `--cfg-weight 0.3` can slow the delivery, following the
[Chatterbox tuning guide](https://github.com/resemble-ai/chatterbox#original-chatterbox-tips).
The receipt records this setting.

## Spoken play

Sound & voices in the pause menu offers voice volume, eight player voices,
text-only player replies, optional page reading, and nearby voices. Preferences
use format five; older preference files load with the full voice volume and
Hearth as the player voice. These settings leave the saved simulation intact.

Chosen replies enter a four-slot speech queue before the next NPC line. The
caption follows the active speaker. Trade confirmations use the accepted
quantity. Paid road demands receive a reply from the same company voice.
Nearby greetings use visible people within talking distance and a one-minute
cooldown. Combat calls have a separate cooldown. Warnings take priority, and
short field lines expire after six seconds. Page reading uses the visible
book or selected notice; changing pages clears the previous reading.

Closing a turn, changing places, pausing, losing focus, or muting clears its
pending audio as appropriate. A party wipe also clears the former company's
speech. Replay and skip use F7 and F8 or their on-screen buttons. Native
packaged clips are tried before the local worker. The browser fetches the
versioned cast pack before requesting generated speech.
