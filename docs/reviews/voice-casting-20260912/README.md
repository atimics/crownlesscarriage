# Cast listening trial

The trial contains thirteen existing cast references, three new synthetic voice
auditions, and twenty-four performances across Chatterbox Nano and Pocket TTS.
The listening page offers clean and game-textured versions of every performance.
Both models completed the same twelve prompts using the same cast references.
The repository carries 48 kbit/s MP3 listening copies to fit its research-file
budget. The local trial folder and ZIP keep the full-quality WAV recordings.
`review_audio.json` binds every MP3 to its source WAV hash. Model timings were
measured before audio encoding.

## Listen

From the repository root, build a portable page beside these samples:

```sh
python3 tools/audio/casting_review.py --output docs/reviews/voice-casting-20260912
python3 -m http.server 8874 --bind 127.0.0.1 --directory docs/reviews/voice-casting-20260912
```

Open http://127.0.0.1:8874/. The page builder copies the current references and
creates `index.html`; those two generated outputs are ignored here.

## Proposed core cast

Keep five named voices: Mara, Jory, Tomas, Ilyra, and Bren. Review their identity
and emotional range using the existing reference recordings.

| Reusable voice | Casting target | Contrast to listen for |
| --- | --- | --- |
| Hearth | Mature woman; rounded middle voice; Welsh | Welcoming rhythm and easy laughter |
| Reed | Young adult man; bright tenor; Irish | Fast, precise phrasing |
| Flint | Older woman; dry low voice; northern English | Short phrases and sharp humour |
| Oak | Older man; warm bass; Scottish | Patient weight with playful moments |
| Lark | Young adult woman; bright high voice; west-country English | Lightness with resolve |
| Ash | Mature man; grainy baritone; Welsh | Deliberate storytelling rhythm |
| Brook | Mature woman; soft middle voice; Irish | Calm speech with a ringing warning |
| Stone | Mature man; dry middle voice; northern English | Clipped precision and comic timing |
| Cedar, new audition | Older woman; resonant alto; Ghanaian English | Musical phrasing and warm consonants |
| Wren, new audition | Young androgynous adult; light middle voice; Indian English | Thoughtful pauses and nimble consonants |
| Copper, new audition | Middle-aged man; clear high baritone; Turkish-accented English | Forward resonance and lively pacing |

These are casting directions. Listening review should establish the perceived
age, accent, identity, clarity, and emotional range of each take. The three new
auditions use Qwen VoiceDesign and the same passage as the existing references.
Each receipt preserves its description, seed, text, and recording hash.

Each reusable voice should work for a neighbour, leader, traveller, and opponent.
Role and morality should vary within each accent. A voice's rhythm and texture
should carry as much character as its pitch. Creature performances and younger
child voices can receive separate auditions when their spoken roles are defined.

This proposal fits sixteen core references. Adding voices to the live assignment
pool needs a separate save-compatible mapping change and casting decision.

## CPU measurements

Four references (Mara, Oak, Reed, Flint), three lines each, one seeded take per
line per model. macOS 26.3.1, ARM64, CPU, two Torch threads. Models ran sequentially.
The app and ordinary desktop activity remained present during this small trial.

| Measure | Nano | Pocket |
| --- | --- | --- |
| Audio generated | 51.68 seconds | 57.76 seconds |
| Total generation time | 20.50 seconds | 5.78 seconds |
| Time per complete clip | 0.88–2.26 seconds | 0.30–0.68 seconds |
| Compute seconds per audio second | 0.397 | 0.100 |
| First model audio chunk | Complete clips | 25–37 ms |
| Peak process memory | 4558.5 MiB | 1466.75 MiB |
| Model load | 72.15 seconds, with download | 1.18 seconds, cached weights |
| First voice preparation | 12.43 seconds | 0.42 seconds |
| Later voice preparation | 0.20–0.26 seconds | 0.24–0.27 seconds |

Generation timing excludes voice preparation, file writes, and the game texture
pass. The first generated clip is included. Peak memory covers the full process,
including model loading. Loading used different cache conditions; measure those
under matched conditions before comparing startup costs. Pocket's first chunk
measures model output; audible onset also depends on silence and playback buffers.
Nano's current API returns complete clips. This trial leaves in-game frame time,
perceived playback delay, repeat-run variation, and
listening quality for further measurement. The short warning takes deserve a
specific check for complete words and natural pacing. Pocket is the stronger
runtime speed candidate in this small sample. Listening review should decide
voice identity, clarity, and performance quality.

## Reproduce generation

Use separate Python 3.11 environments. The original trial used:

- Chatterbox source `5de7a54aa4e5e2baadb0182dde554908b48b85c2`, Torch 2.6.0.
- Nano weights `71ccd1d0081b430592cea481f4307e764e07bc64`.
- Pocket TTS 3.1.0, Torch 2.14.0, English checkpoint
  `39592ff23c9ef80098bb74895d104c26275fe2c9`.
- Qwen TTS 0.1.1, Torch and torchaudio 2.11.0, Transformers 4.57.3.
- Qwen VoiceDesign weights `5ecdb67327fd37bb2e042aab12ff7391903235d3`.

Set `HF_HOME` to a chosen local model cache. The model loaders may download
public weights. Pocket voice cloning requires accepted model terms and a local
Hugging Face login. Use the recorded model snapshots when comparing reruns.

```sh
python tools/audio/voice_trial.py --engine nano --output docs/reviews/voice-casting-20260912/trial
python tools/audio/voice_trial.py --engine pocket --output docs/reviews/voice-casting-20260912/trial
python tools/audio/casting_review.py --output docs/reviews/voice-casting-20260912 --design-additions --device mps --allow-download
```

The trial runner writes exact text, reference and output hashes, package
versions, seed, per-clip timings, and errors. A rerun replaces that engine's
report and named takes; copy an earlier trial first when retaining comparisons.
The voice-design helper reuses auditions already present in the output folder.
To make compact copies, install `lameenc==1.8.4` in the trial environment and run
the page builder with `--compress-review`. It writes MP3 files beside the WAV
masters. The page prefers WAV when available and uses MP3 in the compact folder.

## Validation

- Both new scripts compile and `git diff --check` passes.
- Both models completed twelve calls; all sixty-four WAV links passed format and
  nonempty-frame checks. The three new audition hashes matched their receipts.
- All fifty-one compact MP3 copies decoded and matched their recorded hashes.
- The listening page was inspected in the in-app browser.
- Existing speech-pack checks passed for 13 references and 84 campaign clips.
- All eight existing speech-worker tests passed using the local play exporter.
- Listening approval is open. CI status belongs to the pull request.

Upstream references: [Chatterbox](https://github.com/resemble-ai/chatterbox),
[Pocket TTS](https://github.com/kyutai-labs/pocket-tts),
[Qwen VoiceDesign](https://huggingface.co/Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign).
