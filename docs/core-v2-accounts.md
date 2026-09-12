# Core v2 held accounts

Core v2 adds a shared meaning grammar for the game's gossip and a small speech
model. The grammar has 61 account patterns across 44 event kinds. The generated
[coverage table](core-account-coverage.json) lists all 136 kinds in schema 95.
Each covered kind can have several source patterns; further patterns can be
added as the simulation grows.

`tools/data/core_account_rules.json` owns the source patterns, field roles,
permitted values, and two spoken forms per meaning. The compiler produces the
native table in `src/story/cc_core_account_rules.inc`. NPC gossip uses it after
the existing core rules. New production, livestock, succession, dragon, road,
and prophecy accounts therefore gain speech through the ordinary game path.

## Held knowledge

The native parser reads the NPC's held text, with the game's 144-byte account
capacity. It records field offsets, roles, knowledge, and permitted spoken
fields. A caller can mark an individual field unknown. The renderer then uses
words such as “someone” or “somewhere.” The current gossip bridge derives field
uncertainty from the account's overall confidence. Persisted beliefs still use
the simulation's existing account representation.

Source patterns must match completely. Quantity fields accept digits or the
supported English number words. Rules can require positive amounts, constrain
species words, and compare food stores with the reserve target. Spoken forms
keep quantities general. The native API reads its supplied account and preserves
the held perspective, including an old or mistaken account.

## Generate the diagnostic corpus

```sh
python3 tools/compile_core_accounts.py --check
python3 tools/build_core_diagnostic.py --output out/core-v2-data --seed 20260919
cmake -S . -B out/build/core-v2 -DCC_BUILD_CLIENT=OFF -DCC_BUILD_BENCHMARKS=OFF -DBUILD_TESTING=ON -DCC_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/core-v2 --target core_account_probe crownless_gossip_corpus gossip_language_tests -j 4
ctest --test-dir out/build/core-v2 -R 'core_account|core_diagnostic|grounded_gossip_language|gossip_corpus_integrity' --output-on-failure
```

The default corpus has 25,000 training rows, 1,250 validation rows, 1,250 test
rows, and 1,250 test rows with different source wording. These are authored
schema examples with generated field values. Paired rows change one field;
some pairs check that an irrelevant change leaves the statement intact. Names
are separated across training, validation, and test. The manifest records the
source commit, grammar, builder, split hashes, and complete event inventory.

Plain text files contain event lines followed by speech. Companion JSONL files
carry byte offsets and labels for the trainer. The model receives numeric
features and field markers through its encoder. JSON serves the tools' data
exchange.

The `packet` model in [ZERO PR 36](https://github.com/atimics/zero/pull/36) uses
the selected meaning and spoken fields. A parser supplies the meaning; the
model learns its realization in English. The reserved source paraphrases test
that this representation preserves the supplied meaning through wording changes.
Learning a parser for arbitrary prose is a separate experiment.

## Native model bridge

```sh
out/build/core-v2/core_account_probe 133 80 0 'Mara Venn posts a notice at Thornford: Relief charter.' --packet
```

The packet contains the grammar hash, selected rule, exact source bytes, and
field labels. The ZERO speech runner checks that its model uses the same grammar.
The native reference renderer also remains available through this command by
omitting `--packet`.

## Measured game effect

On seed 17 over 120 days, the exporter produced 3,630 spoken rows at baseline
`e27996ae` and 10,448 after the shared grammar was added. Unsupported observed
accounts fell from 3,409 to zero. This run exercised 19 event kinds. The counts
include two speech variants and repeated observations as accounts change. They
describe this world and observation window; the coverage table tracks the
broader inventory separately.

The focused checks cover all 122 native spoken forms, complete source matching,
field omissions, numeric guards, allowed species words, UTF-8 offsets, generated
file freshness, diagnostic pairs, and the existing gossip export invariants.

The shared grammar ships in normal NPC gossip. The learned model runs through
the separate ZERO reference runner for evaluation. Native neural inference and
voice-specific training are later integration steps.
