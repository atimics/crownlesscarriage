# Goblin speech corpus

The corpus has 36 authored, single-person turns in 12 scenarios. They cover
tribute, travel, raids, dragon eggs, ritual, defence, stores, barter, fear,
loss, remembrance, and uncertainty. Each scenario uses an existing simulation
event rule. The names and values are authored examples. Personal feelings and
plans are authored speech, with that origin recorded on every output row.

Hra'khor grows around goblin life. English carries borrowed and everyday ideas.
“Only if we go in daylight.” stays English. Ash, shrines, offerings, caves, and
dragon clutches use roots from the clan voice. Names retain their spelling.

## Build the pairs

Build `core_account_probe`, then run:

```sh
python3 tools/build_hrakhor_corpus.py --probe out/build/hrakhor/core_account_probe > hrakhor-turns.jsonl
python3 tests/hrakhor_corpus_tests.py out/build/hrakhor/core_account_probe
```

The source is `tools/data/hrakhor_corpus.json`. The builder checks each account
with the native parser, then uses the native Hra'khor renderer. Every row holds
its event frame, intent, English speech, Hra'khor speech, source rule, and source
hashes. Reusing the same inputs and probe gives the same rows.

The train split has 21 turns, development has 6, and test has 9. Event kinds stay
in one scenario and split. Exact repeated utterances fail validation. These
splits support language rendering experiments. The data is small and shares
vocabulary across splits; model quality needs a separate evaluation.

The normal CTest suite runs `hrakhor_cultural_corpus`. It checks native parsing,
name and number retention, uncertainty, daylight, repeatability, and split
separation. Source wording is also covered by the existing event coverage guard.

## Meaning and language

The current Hra'khor lexicon lives in `src/story/cc_hrakhor.c`. A future worldpack
can own this language data and its sentence rules.

There are three separate layers:

1. Event meaning: kind and role fields, such as actor, place, and quantity.
2. Participant choice: a speech intent and the event or earlier turn it concerns.
3. Language realization: vocabulary, word order, tense, agreement, and style.

The compact 5M policy learns meaning IDs for seven dialogue moves. English
sentences are supplied by the renderer. The current event parser still extracts
meaning from English simulation messages. Direct typed events would make that
boundary independent of the simulation's display language.

This corpus exercises the third layer. Its `intent` label is an authored
annotation; future participant training must add the speaker's own state,
knowledge, and reply context. The existing model remains the seven-move policy.

## Examples at full strength

| English | Hra'khor |
|---|---|
| I would rather trade mushrooms than go to war. | Sha would rather vesh'rakh mukuk than go to krath'khor. |
| I keep ash by the shrine for those we lost. | Sha keep zhur by tak zhur'nukh fo those we lost. |
| Who will guard the clutch while we gather food? | Who will guard tak kesh'khor while we rakh zhek? |
| I do not know whether the offering will help. | Sha do not know whether tak khor'rakh will help. |
| Only if we go in daylight. | Only if we go in daylight. |
