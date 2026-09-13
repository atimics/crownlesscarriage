# Crownless synthetic speech review

12 September 2026. Review by Codex of the shipped English 5M model, using fresh simulated accounts. All quoted speech below is saved model output.

## What we collected

Four worlds, seeds 901–904, ran for 365 days each. The shared game wording generator emitted 132,950 reference rows. Each supported observation has two wording variants. After choosing one variant and grouping identical accounts by kind and confidence band, 391 distinct account cases remained.

The review samples 96 cases across 19 event kinds: 33 clear accounts, 32 widely retold accounts, and 31 uncertain accounts. Selection uses a stable order and balances event kinds and confidence bands before model generation.

We also selected 24 meetings between two people observed holding the same event in the same place on the same day. Each staged exchange runs for six model turns. The exact previous four lines enter the next turn. The speakers' names and jobs remain in the source records; the model receives the prepared account, confidence, circulation, and relative spoken history.

These are staged conversations based on real simulation observations. Seven pairs have different raw tellings because social commentary changed. All 24 pairs share the same prepared factual account. Fourteen pairs include a retold or uncertain speaker. This sample measures agreement and differing confidence; a future disagreement sample should supply different factual accounts explicitly.

## Results

| Check | Result |
| --- | ---: |
| Single-account lines completed by the model | 93 / 96 |
| Complete six-turn conversations | 22 / 24 |
| Generated conversation lines | 132 |
| Distinct conversation lines | 28 |
| Conversations with the same final three lines | 22 / 22 completed |

I reviewed all 93 completed account lines against their held accounts. They preserve the main event, named people and places, and intended confidence under the game's general-quantity wording. The checked spoken source fields also appear in all 93 outputs. This is an assistant review of a bounded sample; broader factual generalization needs other names, worlds, and sentence forms.

Only 25 lines exactly match the selected shared-game reference. Most differences are permitted rephrasing or a different evidence phrase. Exact string equality is therefore a wording diagnostic here.

## What works

The model produces useful short accounts. For example, A035 holds:

> Gloamgate defers public masonry repairs for want of 2 Stone.

At confidence 67 after four retellings, the model says:

> Gloamgate delayed public repairs for lack of stone, so people say.

It keeps the shortage and delay distinct from completed repairs. It generalizes quantities and carries the circulation cue into the sentence. All 29 completed uncertain examples have an uncertainty phrase. The held account remains the basis for the line.

## Where the language becomes repetitive

Every completed conversation uses this second line:

> That's what I heard too. How sure are you?

The next reply varies by the first speaker's confidence: twelve say “That is the account I hold,” seven say “The account has passed through several people,” and three say “I am unsure.” Each then suggests asking someone who was there.

All 22 conversations end with:

> Let's ask around before we pass it on.
>
> Agreed. Let's leave it there for now.
>
> Very well.

Confidence changes a short phrase while the exchange follows the same sequence. Hunger, a closed bridge, a treasure, and ordinary bakery work receive the same follow-up. More samples of this sequence would give those replies still more weight in training.

The single lines also show narrow phrasing. “So people say” and “if the story is right” appear frequently. Four paper-making examples retain the capitalized material name “Wood.” C018 mixes past and present tense: “Rosespire slaughtered sheep for mutton after winter fodder runs short.” “Food reserve target” also sounds like the simulation's bookkeeping vocabulary.

## Coverage gaps

The three single-account misses are A006, A023, and A081. They involve bandit recruitment with road-control wording, or goblin raids that include a named treasure. The two conversation misses are C004, the same recruitment shape, and C011, a pony birth with zero animals joining the working herd. The parser rejects those source shapes before model generation.

The corpus exporter separately reported 676 unsupported held observations: 367 bandit cases involving seeking a bed, 214 pony cases with zero joining the herd, and 95 night-road decrees. The selected records and these full-world coverage reports are saved for the next grammar expansion. Zero joining the herd needs its own truthful target when the event wording is expanded.

## What this means for the Gutenberg experiment

The account pairs provide a useful source of grounded facts and confidence cues. The conversations show where expression needs work: distinct questions, answers about sources, reactions to consequences, and natural ways to continue or finish.

Keep these generated model lines as the baseline for comparison. Curated training targets should broaden those conversation choices and use more natural phrasing. A Gutenberg mixture can then be judged by whether those improvements appear in actual exchanges while the facts remain correct.

## Files and reproduction

- `index.html`: searchable review with held accounts, shared-game references, model lines, and conversation history context.
- `reference-pairs.txt`: one compact event line followed by its shared-game sentence.
- `model-pairs.txt`: the same format with each completed model candidate.
- `accounts.jsonl` and `conversations.jsonl`: selected observations, identities, packets, outputs, and exact preceding speech.
- `summary.json` and `manifest.json`: counts, coverage, world hashes, source and model hashes, and file hashes.

This is a review set. Assign training and evaluation worlds when preparing the later experiment. Names and towns recur across these four worlds.

The shipped model hash is `244a809aba71679efe129495bb981ca8d7fbe0d9012948a5f88731dabcb9c48b`. The simulation and inference tools were built from Crownless main `90dc717e745103e38351307cd82df0ced93f27e7`. The sampler source is recorded in the manifest. The Python runner uses only the standard library and the native tools.

```sh
cmake -S . -B out/build/speech-review -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/speech-review --target crownless_gossip_flow crownless_gossip_corpus core_account_probe core_model_probe -j4
python3 tools/build_speech_review.py --bin-dir out/build/speech-review --output out/fresh-speech-review
python3 tests/speech_review_tests.py
```

The generator writes complete compressed world traces into its output's `raw` folder. This review publishes the selected evidence and hashes; the 4.7 MB compressed source traces remain in the local run at `out/speech-review-2026-09-12-final/raw`. Re-running the command reproduces the worlds and selected data. Binary hashes vary with the compiler and platform.

Validation passed: five sampling tests, all saved file hashes, co-location and event identities for every meeting, and the exact four-line history chain for all 132 generated turns. Every source world finished valid, and the observation tool checked that collecting its records preserved simulation state.
