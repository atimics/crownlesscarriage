# First Crownless core-language pilot

The shared gossip generator produced 5,000 examples from 96 real simulation
worlds, each observed for 730 days. It ran locally with zero provider cost.
The source commit is `1f53e4e`. The manifest records the full commit, source file
hashes, exporter binary hash, settings, world summaries, and dataset hashes.

| Split | Examples | Distinct spoken lines | Output words |
| --- | ---: | ---: | ---: |
| Training | 4,000 | 684 | 59,448 |
| Validation | 500 | 128 | 8,076 |
| Test | 500 | 124 | 7,888 |

The corpus contains 936 distinct spoken lines. A further 16 separately authored
challenge examples probe fresh phrasing and names. These are a small evaluation
seed for human review. The 5,000 generated examples are suitable for an initial
training experiment; broader language ability needs more varied writing.

The exporter observed 3,694,200 candidate rows. The builder removed 3,068,216
repeated pairs and 606,624 copies of text already assigned to another split.
That left 19,360 distinct input/output pairs before balanced selection. Multiple
contexts can lead to the same spoken line; the table shows that distinction.

Training covers bandits, cult gatherings, deaths, dragon fire, dragon omens,
goblin raids, harvests, notices, peace, bridge closures, shortages, and treasure.
Rare cult gatherings contribute two training examples. The held-out splits have
five and six event kinds, respectively. Their manifests show the distribution.
The corpus tests verify separate world seeds and separate exact spoken outputs.

## What improves in the game

The shared claim composer adds readable accounts for harvest trouble, bridge
closures, bandit recruitment, posted notices, deaths, crafted treasure, war, and
peace. The existing character registers continue to use those claims.

Examples from this run:

- "This account has passed through several people: Alderwatch closed the treaty bridge, delaying the relief convoy."
- "I am unsure of this account: Forgen Miller put up a notice at Thornford: Relief charter."
- "This account has passed through several people: Willow Republic and Ashen Throne were bound by peace."

See [samples.jsonl](samples.jsonl) for one full training row per event kind and
[manifest.json](manifest.json) for counts and hashes.

## Next writing pass

All 250,310 unsupported observations in this run were decrees. Common examples
concern market funding and grain purchases. Those account forms are the next
useful extension to the shared game language. More independently written
phrasing and broader held-out topic coverage would strengthen core training.

## Reproduce

After the build steps in [Core language corpus](../../core-language-corpus.md):

```sh
python3 tools/build_gossip_corpus.py --output out/corpus/core-v1 --seeds 96 --days 730 --max-examples 5000
```

Use a fresh output directory. The source and binary must stay stable during
collection. Dataset file hashes are recorded in the manifest. Build-tool and
platform differences can change the binary hash.

Local validation passed the strict headless build, all 100 headless CTest checks,
and repository static analysis. The speech worker test passed with local socket
access. The four focused gossip and corpus checks passed, including two
byte-identical builder runs and the real simulation export.
