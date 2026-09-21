# Typed fact selection

Status: pilot complete · 2026-09-21

## Problem

The grounded-dialogue follow-up (ZERO PR #59) fixed most question routing with
data, but two of eight held-out different-fact questions still answered the
place field when the question asked for the object or material. Free-text
generation has to both choose a fact and phrase it.

This pilot separates the choice from the wording. The model chooses a typed fact
reference and the renderer owns the language. This is the fact analogue of the
v3 meaning loop in `meaning.py`, where the model already picks a typed candidate
ID at 99.57% on the food policy.

## Design

A participant holds a bounded fact table. Each fact has an owner, a stable id,
an event, a role (`actor`, `recipient`, `place`, `object`, `group`, `material`,
`quantity`), a value, a source (`observed`, `told`), a certainty (`witnessed`,
`told`, `doubtful`), a day, and a private flag. A typed question names an event
and a role. `fact_policy.candidates` offers one `report(fact_id)` per
disclosable fact plus a `defer`. The teacher is the fact matching the asked
event and role, most certain then most recent, or `defer` when the role is not
held.

`fact_policy.encode_input` writes the asked role and event slot, the fact table,
and the legal candidate IDs into the native 4,096-token vocabulary. It uses the
same `1580 … 1281` candidate block as the meaning loop, so the same
choice-scoring loss applies. The renderer turns the chosen reference into a
short attributed claim.

The pilot is synthetic: `fact_data.py` builds one to three events per person
with two to four typed facts each, splits whole world profiles 80/10/10, and
keeps exact inputs disjoint across splits. `train_fact_policy.py` trains fresh
native 4,945,153-parameter weights from `models/dialogue-syntax/model.ccv2`.

## Gates, declared before the run

| Metric | Gate |
| --- | --- |
| Held-out exact choice | ≥ 0.95 |
| Held-out different-fact exact (asked role is not the event's first role) | ≥ 0.95 |
| Held-out defer exact (asked role not held) | ≥ 0.95 |
| Valid choice (in the legal candidate set) | 1.00 |

## Result

4,000 CPU steps, batch 32, 256 worlds, seed 19, 337 s. The exported model is
`5bba76ae50ebd4d155cabfecc89d13ab8bda701fbc513c53cd715845f086dfe7`.

| Check | Development | Test |
| --- | ---: | ---: |
| Exact choice | 1068/1095 (97.5%) | **375/381 (98.4%)** |
| Different-fact exact | 858/882 (97.3%) | **305/307 (99.3%)** |
| Defer exact | 480/485 (99.0%) | **170/170 (100%)** |
| Valid choice | 1095/1095 | **381/381** |

Baselines on the same 381 test rows:

| Baseline | Exact |
| --- | ---: |
| Always defer | 44.6% |
| First candidate | 11.0% |
| Match role only, ignore event | 77.2% |
| Match event only, ignore role | 19.4% |
| Random (mean 1/choices) | ~17.7% |
| **Trained model** | **98.4%** |

The model must combine event and role to beat the best single-attribute baseline
by 21 points.

## Scope and limitations

- Synthetic, single-seed pilot. It measures typed selection, not natural
  language understanding.
- The typed question is already parsed. Mapping free wording ("Where was the
  notice posted?") to the typed role is a separate parser step and is not part
  of this result.
- Values are never encoded; the renderer owns them. The model routes, it does
  not quote.
- `defer` is a single negative class. Absent events, conflicting accounts and
  disclosure rules beyond a private flag are not tested.

## Integration path

`event_facts.build_facts` already produces owner-bound, source-tagged,
certainty-tagged facts for all 139 simulation event kinds. Its `report`, `ask`
and `warn` acts map onto this candidate shape. The next step is to feed that
table into `fact_policy.encode_input`, add a natural-question parser that emits
the typed role, and render the chosen fact through the language packs.

## Reproduction

```sh
python tests/dialogue_fact_policy_tests.py
python tools/dialogue/fact_data.py
python tools/dialogue/train_fact_policy.py \
  --zero /path/to/zero --reference models/dialogue-syntax/model.ccv2 \
  --tokenizer assets/language/tokenizer.json --output /tmp/fact-policy \
  --steps 4000 --worlds 256 --batch-size 32 --device cpu
```

Run summaries are in `docs/reviews/fact-selection-2026-09-21/`.
