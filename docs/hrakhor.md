# Hra'khor: the clan voice

Hra'khor is Crownless's goblin contact language. Its speakers bend shared English
around words for kin, food, shelter, tribute, and dragons. Say the name as
“hra—khor,” with a catch between the roots and a rough kh.

The English micro LM supplies a completed line. `CcHrakhorCorrupt` then changes
its words at a selected strength. This is an output corruption layer in the
native story library. The ZERO runner can pass its completed English through
the existing core-account process bridge. Model weights remain the English
baseline. Training on the paired output is a future experiment.

## Speech rules

English clause order, auxiliaries, tense cues, uncertainty, and punctuation
carry the meaning. Familiar roots become goblin words. An apostrophe joins
roots; `-uk` marks several things. The contact dialect uses `-s` for present
verbs and `'ed` for past verbs. This lets a listener learn the voice through
repeated encounters.

| English | Hra'khor | Clan meaning |
|---|---|---|
| for / the | fo / tak | purpose / a specific thing |
| voice, breath | hra | living voice |
| clan | khor | chosen family |
| stone | grak | lasting matter |
| fire | vrik | heat and hunger |
| food | zhek | shared food |
| small, clever | skrit | quick cunning |
| large, strong | drok | bodily strength |
| home, shelter | nukh | a safe hollow |
| friend | vesh | a trusted guest |
| danger | krath | a threat |
| mushroom | muk | a staple food |
| take, gather | rakh | bring into keeping |
| build | grosh | give something form |
| I, me / you | sha / thu | speaker / listener |
| court | drok'khor | powerful clan |
| dragon | vrik'drok | great fire |
| hoard | rakh'nukh | gathered shelter |
| tribute | khor'rakh | clan gathering |
| raid | krath'rakh | dangerous taking |
| tunnel | grak'nukh | stone shelter |
| trade | vesh'rakh | friendly taking |
| peace / war | vesh'khor / krath'khor | friendship / danger among clans |

“I gathered food. You built shelter.” becomes
“Sha rakh'ed zhek. Thu grosh'ed nukh.”

“I do not trust the court, if the rumour is true.” becomes
“Sha do not trust tak drok'khor, if tak rumour is true.”

Each root has a stable hash. Strength 0 keeps the English line. Strength 50
changes a stable subset of known roots. Strength 100 changes every eligible
root. The same word keeps its form within a line and across repeated lines.
The percentage controls eligibility thresholds; short lines can vary in the
fraction of changed words.

Named actor, recipient, place, object, and group fields take priority. A place
called Fire keeps that spelling. Matching is case-insensitive, so a named
object Food also protects an ordinary mention of food. This conservative choice
keeps copied names safe. Quantities and words outside the lexicon pass through.
The adapter expects a completed, approved English line and its held account.
The English model and its meaning checks own factual accuracy.

## Use with the English model

Build the bridge:

```sh
cmake -S . -B out/build/hrakhor -DCC_BUILD_CLIENT=OFF -DCC_BUILD_BENCHMARKS=OFF -DBUILD_TESTING=ON -DCC_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/hrakhor --target core_account_probe hrakhor_tests -j 4
```

Use a native English realization with dense corruption:

```sh
out/build/hrakhor/core_account_probe 133 80 0 'Mara posts a notice at Thornford: Relief charter.' --hrakhor 100
```

Pass the English model's completed line with `--english`:

```sh
out/build/hrakhor/core_account_probe 133 80 0 'Mara posts a notice at Thornford: Relief charter.' --hrakhor 100 --english 'I hear Mara posted a notice about Relief charter in Thornford.'
```

For batches, `tools/hrakhor_pairs.py` reads JSONL on standard input. Each row
contains numeric `kind`, held account `text`, completed English `output`, and
`confidence`; `variant` defaults to zero. Extra labels, such as world, split,
model hash, and event ID, stay in the `source` record. Each output row adds the
English line, Hra'khor line, strength, schema version, and probe binary hash.

```sh
python3 tools/hrakhor_pairs.py --probe out/build/hrakhor/core_account_probe --strength 100 < english-test.jsonl > hrakhor-test.jsonl
```

Keep each existing world split when collecting pairs. These records support
adapter evaluation and future training-data conversion. Source copy offsets
remain attached to the English source record; each language has its own text.

The game currently uses the shared native English grammar for ordinary NPC
speech. The Hra'khor option is exposed at the model bridge. A caller selects it
for a goblin speaker; the subject of a rumour can be any species. Live goblin
conversation routing and learned goblin weights are later work.

## Checks

`hrakhor_corruption` checks exact phrases, tense, negation, uncertainty,
name collisions, UTF-8, strength, repeatability, and buffer bounds.
`hrakhor_model_pairs` replays all 24 recorded ZERO outputs from the gossip-flow
review and checks named fields and retained split labels. These are stored
model outputs from that review. The existing English grammar, diagnostic,
gossip, and speech checks cover the baseline bridge.

## Voice previews

The [goblin voice effect](../tools/audio/README.md#hrakhor-voice-effect) gives
Hra'khor a higher lead, deeper throat, quiet growl, light rasp, and short stone
echo. The `goblin` preset is subtle; `priest` uses a stronger lower double.
It processes completed voice masters from either local speech engine.
