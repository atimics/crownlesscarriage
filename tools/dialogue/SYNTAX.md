# Meaning before wording

The first procedural policy speaks a small abstract syntax. Each policy call
receives one person's private view and the public acts heard in this exchange.
The target is one act. Human and goblin renderers use that same act.

```json
{"move":"propose","plan":"seek_paid_work","reply":0}
{"move":"condition","term":"pay_before_work","reply":1}
{"move":"accept","reply":2}
```

The reply index identifies the exact proposal or condition. Acceptance carries
the terms of that referenced act. The transcript preserves the reference chain.
An end act closes the exchange. Agreements describe intentions; execution needs
the simulation's actor command support.

## First vocabulary

| Move | Meaning |
| --- | --- |
| need | Express an own-state need for food, safety or paid work |
| request | Ask how to address one of those topics |
| propose | Suggest counting stored food or seeking paid/safer work |
| condition | Add priority for vulnerable people, daylight, or agreed pay |
| accept | Agree to the referenced proposal and its terms |
| decline | Refuse the referenced proposal |
| end | Close the exchange |

Validation checks the closed symbol vocabulary, exact fields, turn ownership,
own-state requirements, reply position, topic and condition compatibility.
The runner supplies previously validated public history. The policy is a small
deterministic baseline: need/request, proposal, condition, decision, ending.
Food need requires positive hungry_days; work need requires secure_livelihood.
Stress of at least 60 selects concern about safety as an explicit policy
heuristic. It expresses emotion; named danger and witnessed events require
further event-aware rules. A distressed listener can decline a work proposal.

This prototype covers work discussions and proposed pay conditions. Quantities,
prices, inventories, binding bargains, love, grief and physical actions are
future additions to the meaning language and the simulation checks.

## Renderers

`english` realizes two authored clause variants for each act. `render` can pass
these literals through the existing native Hra'khor vocabulary at strength 100.
This preserves Crownless's current contact dialect and clause order. Authored
literal text contains no character-name slots. Future names should be inserted
separately or use the existing account renderer's protected fields.

For example the same `need(food)` act becomes `I need food.` or
`Sha need zhek.` The recipient observes `need(food)` in either case. Surface
language changes leave the model's input and target unchanged. In-game language
comprehension, including partial understanding, needs its own listener rule.

## Run against a real pair

Build `core_account_probe` and `crownless_participant_probe`, then:

```sh
BUILD/crownless_participant_probe --seed 1202 --days 0 \
  --first 1369094286720630838 --second 1369094286720630847 > /tmp/pair.json
python3 tools/dialogue/syntax.py --snapshot /tmp/pair.json \
  --probe BUILD/core_account_probe --output /tmp/syntax-pair
```

The fresh output directory contains `result.json` with the source snapshot,
public transcript and both renderings. `training.jsonl` contains only per-person
inputs and abstract targets. Input rows remain separate from the whole-pair
evaluation artifact. Snapshot, generator and native probe hashes are recorded.
Generation preserves the source world. Existing snapshot groups must stay in
their existing training or evaluation split.

## The 5M model

The next model task is `own state + heard public acts -> next act`. Keep the
versioned syntax and semantic validator as its output contract. The procedural
policy supplies examples; the renderer remains independent. The current
participant trainer expects speech/action JSON, so a syntax dataset adapter,
token budget and loss-mask checks are the next training step.

Before scaling training, expand the decision rules and test state changes that
should change the act. Hold out world/character groups and combinations of
needs, proposals and terms. Measure valid replies, grounding, term retention,
useful decisions and repetition separately from surface wording. Repeated
five-turn scripts alone support learning this baseline's narrow behaviour.

The prototype lives in the offline tools. Runtime policy integration and a
new trained checkpoint follow after the meaning contract has been reviewed.
