# Crownless sound design

Use small, close sounds with a light pixel-game texture and space between them.
Boots use stone, wood, or dirt. Water adds a soft splash. Travel has paired hoof
beats and a low wooden rattle. Combat uses a cloth swing, a dull impact, and a
short iron ring for a block. Menus have a quick paper tick. Jumps rise through
three short notes. Trade and promises use two-note triangle and pulse tones.
Effects have short tails and a quiet 8-bit, 7.35 kHz layer at 35% of the mix.

The client makes 14 effects at startup after the first play input. Each has
three variations. Footsteps follow distance travelled. Hooves and wheels use
real frame time with a pace limit, including during fast travel. Effects use
their own random seed. Scene changes and long frame gaps reset their clocks.

Click **Sound** or press **F6** to cycle through full sound, effects, and mute.
The choice is saved beside the campaign. Voices use one stream. A new line
replaces the current voice, and closing the conversation stops it. Effects get
quieter during speech. Losing window focus stops active audio.

## Preview and voice export

Build with `cmake --preset play && cmake --build --preset play`.

```sh
out/build/play/crownless_audio_export --preview out/sound-effects.wav
out/build/play/crownless_audio_export --script out/authored-dialogue.json
out/build/play/crownless_audio_export --opening out/opening-dialogue.json
```

The preview follows the cue order in `src/client/cc_soundscape.h`. The full
script exports the authored catalogue. The opening script runs the starting
campaign through offer, listen, promise, and withdrawal, then exports the exact
spoken words. This includes the destination, quantity, and named recipient.

Use Python 3.11 in a local Chatterbox environment:

```sh
python -m pip install -r tools/audio/requirements.txt
python tools/audio/generate_dialogue.py out/opening-dialogue.json --check
python tools/audio/generate_dialogue.py out/opening-dialogue.json --device mps --allow-download
```

The first generation can fetch public model weights with `--allow-download`.
Later runs use the model cache. Choose `cpu`, `mps`, or `cuda` for the local
device. The default limit is four lines. Use `--prefix` to select a character or
story. A JSON cast file maps speaker names to local reference WAV paths:

```json
{"Mara Venn": "/absolute/path/to/mara-reference.wav"}
```

Pass it with `--cast cast.json`. Use reference voices that you have permission
to use. The pilot uses Chatterbox's built-in voice. Cast choices can be reviewed
with the generated clips before making a larger voice pack.

Each WAV has its line ID and a fingerprint of its speaker and spoken text in the filename.
The client asks for that exact combination. Each clip has a JSON receipt with
the text, speaker, seed, reference fingerprint, and output fingerprint. Dynamic
lines can have several recordings for different text values. For generic
catalogue entries, set the speaker to the intended character and use the
`voice_path` helper in the generation script to refresh the path. Text changes
select a new filename. Subtitles remain available throughout the conversation.

Generated voices are mono 16-bit PCM with short edge fades and peak headroom.
Chatterbox applies its built-in audio watermark. Clean masters are saved in
`tools/audio/masters`. The game uses a blend of 60% clean speech and 40% softened
8-bit speech at 12 kHz for the current 24 kHz masters. Pitch and duration stay
the same. The texture is filtered before reducing its sample rate. Each receipt
records the master fingerprint and processing settings.

Render the shipped voices again from their clean masters with Python alone:

```sh
python tools/audio/voice_style.py
```

This always reads the clean masters, so repeated runs produce the same clips.
The build includes the rendered WAV files
in the macOS bundle and browser startup files. The browser keeps the voice
folder after releasing uploaded graphics. Keep voice packs small: four short
opening clips are the initial scope.

The effects are original code in this repository. The speech generator uses
[Resemble AI's Chatterbox](https://github.com/resemble-ai/chatterbox).

## Hra'khor voice effect

`goblin_voice.py` styles a completed mono 16-bit PCM voice master. It works with
Pocket TTS and Chatterbox output. Use Python 3.11 with the effect dependencies:

```sh
python -m pip install -r tools/audio/requirements-effects.txt
python tools/audio/goblin_voice.py dry.wav goblin.wav --preset goblin
python tools/audio/goblin_voice.py dry.wav priest.wav --preset priest
python tests/goblin_voice_tests.py
```

The goblin preset raises the lead by one semitone and shifts its smoothed
spectral envelope down by 1.5 semitones for a deeper throat sound. A quiet
five-semitone-lower double adds the growl. Light saturation and a dark 70 ms
reflection add rasp and a sense of stone nearby. The priest preset strengthens
the double and uses a lower throat sound with a 90 ms reflection.

The envelope shift approximates vocal formants. It is an artistic effect.
Words and timing come from the original performance. Both presets match the
source loudness before a headroom limit and fade the clip edges. The goblin and priest presets keep
the source duration; reflections end at the clip boundary. Apply this effect
once to a clean master, then use the pixel texture if desired. Each output has
a receipt with settings, source and output hashes, renderer hash, and library
versions. The local DSP checks cover pitch layers, duration, headroom, silence,
repeatability, and master preservation.

### Goblin bass vocoder

Use the `bass` preset for the throbbing goblin voice. It defaults to double-speed
speech. Time stretching happens before the vocoder, so the words speed up while
the low pitch and slower bass pulse keep their character.

```sh
python tools/audio/goblin_voice.py dry-hrakhor.wav goblin.wav --preset bass
python tools/audio/goblin_voice.py dry-hrakhor.wav normal-speed.wav --preset bass --speed 1
```

The v2 carrier has a 49 Hz foundation, detuned harmonics, a rounded 3.1 Hz pulse
with slow drift, and metallic partials. High-frequency breath carries consonants
between pulses. Twenty-eight balanced bands carry the speech, while a low layer
follows each syllable. A little original consonant detail supports clear words.
The vocoder follows pauses and retains headroom. The default output lasts half
as long as its source; `--speed 1` keeps the source duration. Speed accepts
0.5–3.0, with at least 1024 samples remaining after the speed change.

`goblin_voice.render(master, destination, 'bass')` applies the same effect from
Python. It accepts clean mono 16-bit PCM voice masters from either speech engine.
Each receipt records speed, preset settings, source and output hashes, both DSP
source hashes, and library versions. Keep this step before the optional pixel
texture. Clean masters provide a repeatable source for each render.

For a separate carrier audition and normal-speed vocoder experiment:

```sh
python tools/audio/goblin_bass.py --speech dry-hrakhor.wav --output out/goblin-bass
```

This writes `bass-reference.wav`, `bass-speaking.wav`, and a hash receipt.
The bass is an original artistic synthesizer inspired by vortex motion.
The local checks cover speed, duration, pitch layers, peak headroom, silence,
repeatability, input validation, receipts, and source preservation.
