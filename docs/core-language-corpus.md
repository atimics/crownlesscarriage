# Crownless core language corpus

The dataset shares its claim rules with NPC gossip. Improving a rule improves
both the game's spoken accounts and the training examples. Generation runs
locally with zero provider cost. Voice adapters are a later step.

## Training format

The model reads a short list of held events and continues with a spoken line:

```text
- Alderwatch closes the treaty bridge and delays the relief convoy.
- Willow Republic's courier reaches Ashen Throne: peace now binds the two courts.
Have you heard the news? Willow Republic and Ashen Throne made peace.

```

Each example has one to three events, followed by one line of speech and a blank
line. The final event supplies the topic. Earlier lines give the speaker's held
context. The small evidence cues `?` and `~` mean uncertain and widely retold:

```text
- ? Someone put up a relief charter in Thornford.
Someone put up a relief charter in Thornford.

```

Low-confidence notices and diplomatic accounts can generalise the actor or the
subject. Both the visible event and the spoken line use that reduced detail.
These examples teach the model to preserve vagueness. Other account types keep
the held details and use a qualifier such as "if the story is right."

The training files are `train.txt`, `validation.txt`, `test.txt`, and
`editorial_test.txt`. Load the text directly into the tokenizer. Use an event
list ending in a newline as the runtime prefix. The next generated line is the
speech; a newline ends that output.

Each text file has a matching `.audit.jsonl` file. It stores source accounts,
world and character IDs, confidence, wording choices, and UTF-8 byte offsets.
`text_start`, `output_start`, and `text_bytes` locate each example and its target.
A trainer can use these offsets to apply loss to the spoken output. The model's
text contains the event lines and speech.

## Build and collect

Use Python 3.9 or later and a C17 compiler with CMake. Run from the repository:

```sh
cmake -S . -B out/build/core -DCC_BUILD_CLIENT=OFF -DCC_BUILD_BENCHMARKS=OFF -DBUILD_TESTING=ON -DCC_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/core --target crownless_gossip_corpus gossip_language_tests -j 4
python3 tools/build_gossip_corpus.py --output out/corpus/core-v2 --seeds 96 --days 730 --max-examples 5000
```

Each seed creates an independent world. The exporter reads characters' personal
accounts every day. It samples again when an account, confidence, retelling
count, speaker, or event changes. Two wording variants are generated. Up to two
older held accounts supply context. Every context event occurred by the topic
event's day. The simulation hash is checked before and after each export pass,
and the final world is validated.

The builder publishes a fresh output directory after every world succeeds.
The source and binary must stay stable during collection. `manifest.json`
records source and binary hashes, generation settings, coverage, file hashes,
and the number of examples actually written. Language format version 2 uses
plain event-to-speech text. The world split function remains stable across
versions for comparisons.

## Shared game rules

`CcSpeechPrepareGossip` reads the held telling through `CcGossipText` and composes
its claim. `CcSpeechCoreGossip` uses conversational openings, short hearsay
phrases, and reduced detail. Both the game and dataset use these functions.
NPC speech can add role, bias, and alarm wording. The plain core keeps the
shared language.

Rules cover shortages, raids, omens, dragon fire, cult gatherings, broods,
dragon deaths, personal deaths, crafted treasure, war, peace, harvest trouble,
bridge closures, bandit recruitment, and posted notices. Support is specific
to recognised account forms. The coverage report groups other forms by kind
and includes samples for the next writing pass.

Add new forms in `cc_speech_lexicon.c` with factual regression cases in
`gossip_language_tests.c`. The simulation currently supplies overall confidence;
the actor/subject omission is a conservative language choice recorded in the
packet. Retelling changes and original source accounts remain intact.

## Splits and evaluation

Stable hashes assign whole worlds to training, validation, or test with expected
80/10/10 proportions. Related events and retellings stay in the same split.
SQLite deduplicates the visible event prefix and output pair. Exact spoken text
is reserved for the split where it first appears. Later cross-split copies are
counted and skipped. This can reduce held-out topic coverage; the manifest
shows the resulting distribution.

Selection rotates across event kinds with a per-kind cap. `--max-examples` is
an upper target with 80/10/10 row budgets. `--max-per-kind` defaults to 1000 per
split. More varied writing and richer simulation coverage can expand the corpus.
The manifest reports distinct spoken outputs separately from example counts.

The 16 separately authored editorial examples probe fresh names and phrasing.
They are a small challenge set for human review. Generated holdouts assess fresh
worlds under shared language rules. Broader model evaluation will need more
independent writing. Keep all held-out text files separate from training.

## Checks

```sh
cmake --build out/build/core --target crownless_gossip_corpus gossip_language_tests speech_tests gossip_tests -j 4
ctest --test-dir out/build/core -R 'grounded_gossip_language|gossip_corpus_integrity|speech_identity_and_words|traveler_gossip_network' --output-on-failure
```

Checks cover reduced detail, conversational hearsay, retelling changes, real
export reproducibility, compact event prefixes, source ordering, world splits,
deduplication, byte offsets, file hashes, failed-run cleanup, and game speech.
