# Six fresh cast voices

This second audition revises six of the thirteen existing voices: Hearth,
Flint, Lark, Brook, Reed, and Oak. Pocket is the user's preferred speech engine,
so the original and revised voices both use Pocket for the listening comparison.

The five named characters remain familiar anchors, along with Ash and Stone.
Cedar, Wren, and Copper remain the proposed additions from the first audition.
The resulting proposed cast has sixteen voices.

## Selection

Chatterbox's speaker encoder compared the thirteen saved references reading
the same passage. Its cosine score is a screening signal for this recording
set; listening decides voice identity and performance quality.

| Selected voice | Closest existing reference | Original score | New casting direction |
| --- | --- | --- | --- |
| Hearth | Brook | 0.827 | Mature chesty mezzo; Jamaican English; rolling warmth |
| Flint | Brook | 0.859 | Elder reedy contralto; Polish-accented English; dry precision |
| Lark | Ilyra | 0.846 | Young bright soprano; New Zealand English; springy rhythm |
| Brook | Flint | 0.859 | Forward alto; Mexican Spanish-accented English; measured certainty |
| Reed | Ilyra | 0.797 | Wiry tenor; French-accented English; quick patter and pauses |
| Oak | Jory | 0.790 | Elder bass; Norwegian-accented English; spacious pacing |

Pitch, age, rhythm, texture, and accent all contribute to these directions.
The accents and perceived ages remain casting intentions for listening review.
Each voice can carry warmth, humour, and authority across different game roles.

## Listen and reproduce

From the repository root:

```sh
python3 tools/audio/cast_reaudition.py --output docs/reviews/cast-second-audition
python3 -m http.server 8876 --bind 127.0.0.1 --directory docs/reviews/cast-second-audition
```

Open http://127.0.0.1:8876/. Each card compares two identical lines through
Pocket, with clean and game-textured versions. Expand the reference comparison
to hear the original and revised Qwen recordings.

For fresh generation, use separate Python environments. The voice design uses
Qwen TTS 0.1.1, Torch 2.11.0, Transformers 4.57.3, and Qwen VoiceDesign snapshot
`5ecdb67327fd37bb2e042aab12ff7391903235d3`. Pocket uses version 3.1.0, Torch
2.14.0, and the English checkpoint pinned by that package. Set `HF_HOME` to the
appropriate model cache. Pocket requires the user's accepted model access and
local login.

```sh
python tools/audio/cast_reaudition.py --output out/second-audition --design --device mps --allow-download
python tools/audio/cast_reaudition.py --output out/second-audition --pocket
python tools/audio/cast_reaudition.py --output out/second-audition --compact
```

The compact step uses `lameenc==1.8.4`. It writes 48 kbit/s MP3 copies beside
the WAV masters. The repository review keeps compact copies; the local listening
folder and ZIP keep WAV masters. Receipts bind text, reference fingerprints,
seeds, and recording fingerprints. `review_audio.json` binds the compact files
to their masters. The page prefers a WAV when present.

The design helper reuses existing WAVs in the chosen output folder. Choose a
fresh output folder for a changed casting brief. Pocket generation replaces
its named output takes and report. Keep earlier results in separate folders.

The comparison uses Chatterbox's voice-encoder weights from Nano snapshot
`71ccd1d0081b430592cea481f4307e764e07bc64`. Run `cast_similarity.py --weights
<ve.safetensors> --output <report.json>` in the Chatterbox environment. Add
`--revised <revised-WAV-folder>` to compare the proposed thirteen-voice set.
Each report records the weights and input hashes.

These are audition assets. A later game cast update should give revised voices
new versioned profiles and preserve stable assignments across saved games.
