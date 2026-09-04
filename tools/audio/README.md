# Crownless sound design

Use small, close sounds with space between them. Boots use stone, wood, or dirt.
Water adds a soft splash. Travel has paired hoof beats and a low wooden rattle.
Combat uses a cloth swing, a dull impact, and a short iron ring for a block.
Menus rustle like paper. Trade has two small coin strikes. A promise has a soft
two-note bell.

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
Chatterbox applies its built-in audio watermark. The build includes WAV files
in the macOS bundle and browser startup files. The browser keeps the voice
folder after releasing uploaded graphics. Keep voice packs small: four short
opening clips are the initial scope.

The effects are original code in this repository. The speech generator uses
[Resemble AI's Chatterbox](https://github.com/resemble-ai/chatterbox).
