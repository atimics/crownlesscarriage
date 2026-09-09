# Crownless core language corpus

The core language dataset shares its claim rules with NPC gossip. Improving a
rule improves both the game's spoken accounts and the training examples.
The first version uses local simulation and authored language patterns. Provider
cost is zero. Voice adapters are a later step.

## Build and collect

Use Python 3.9 or later and a C17 compiler with CMake. Run from the repository:

```sh
cmake -S . -B out/build/core -DCC_BUILD_CLIENT=OFF -DCC_BUILD_BENCHMARKS=OFF -DBUILD_TESTING=ON -DCC_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/core --target crownless_gossip_corpus gossip_language_tests -j 4
python3 tools/build_gossip_corpus.py --output out/corpus/pilot --seeds 48 --days 365 --max-examples 5000
```

Each seed creates an independent world. The exporter reads each living
character's personal accounts every day. It samples a telling again when its
account, confidence, retelling count, speaker, or event changes. It exports two
wording variants for each supported telling. The simulation hash is checked
before and after every export pass, and the final world is validated.

The command writes a fresh output directory after every world succeeds. Its
files are `train.jsonl`, `validation.jsonl`, `test.jsonl`, `editorial_test.jsonl`,
and `manifest.json`. Each generated row includes:

- `input`: held account, event kind, confidence, retellings, and wording variant.
- `output`: a plain spoken report with appropriate uncertainty.
- `prompt`: the same context in a reusable training prompt.
- `provenance`: world seed, simulation schema, observation day, event, speaker,
  and immediate source IDs.
- `rule`: the event kind and wording variant used by the shared C generator.
- `id`: SHA-256 of the input and output pair.

Training uses `prompt` and `output`. Keep provenance and rule IDs as audit data.
At runtime, variant is a stable choice of phrasing. It has two values in v1.

## Shared game rules

`CcSpeechPrepareGossip` gets the held telling through `CcGossipText`, then
composes a supported claim. Both the existing NPC speech path and the core
exporter call this function. NPC speech retains its existing role, bias, and
alarm wording. `CcSpeechCoreGossip` supplies the plain core wording.

Rules cover shortages, raids, omens, dragon fire, cult gatherings, broods,
dragon deaths, personal deaths, crafted treasure, war, peace, harvest trouble,
the treaty bridge closure, bandit recruitment, and posted notices. Support is
specific to recognised account forms. The coverage report records other forms
by kind and includes an example for the next writing pass.

The held account supplies names, direction, actors, and diplomatic state.
Retelling changes remain part of that account. Statements describe past reports.
Confidence and retelling count control expressions of uncertainty. Numerical
claims remain general in gossip; trade and quest speech retain their own rules.

To extend coverage, add a matching clause in `cc_speech_lexicon.c`, add a factual
regression in `gossip_language_tests.c`, and run the gossip and corpus tests.
Use live account samples from the manifest to choose the next useful rule.

## Diversity, splits, and evaluation

A stable SHA-256 rule assigns whole worlds to train, validation, or test with
80/10/10 expected proportions. Related events and their retellings stay in the
same split. SQLite removes repeated input/output pairs. Exact output text is
reserved for the split where it first appears; later copies in other splits
are counted and skipped. Collection follows seed order, so common fixed names
can leave small held-out splits. The manifest shows the resulting sizes.

Selection rotates across event kinds with a per-kind cap. `--max-examples` is
an upper target with 80/10/10 row budgets. `--max-per-kind` defaults to 1000 per
split. A larger target may need a larger cap, more worlds, and more authored
language. The generator records the number actually written.

The manifest includes per-kind counts, rule counts, unique spoken outputs,
output word counts, file hashes, binary hash, source commit, source file hashes,
and unsupported observations. Repeated observations can recur as the rolling
gossip list changes slots. Corpus deduplication handles those copies. Word and
byte counts support planning; token counts require the chosen model tokenizer.
The source and binary must remain stable throughout collection.

Generated holdouts measure new simulation worlds using shared language rules.
The 16 separately authored editorial examples use fresh names and phrasing to
probe broader language understanding. These examples are a small challenge set
for review. Model quality will need a larger independently reviewed evaluation.
Keep every held-out file separate from training.

## Checks

```sh
cmake --build out/build/core --target crownless_gossip_corpus gossip_language_tests speech_tests gossip_tests -j 4
ctest --test-dir out/build/core -R 'grounded_gossip_language|gossip_corpus_integrity|speech_identity_and_words|traveler_gossip_network' --output-on-failure
```

Checks cover actual deterministic export, world assignment, repeat removal,
rare-kind selection, argument limits, failed-run cleanup, quantity handling,
source-account stability, retelling mutations, small output buffers, and the
existing game gossip path.
