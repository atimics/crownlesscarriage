# Dialogue-model promotion pilot: A/B

Matched comparison of the shipped in-game model (`7d1c3cd5…`) against the
account grammar's authored rendering, on the task the Chat path uses: held
account -> spoken line. Issue 809 first delivery.

- Source: 93 held-out accounts from worlds 1901 and 1902.
- Arms: A authored, B shipped `assets/language/core.ccv2`.
- Result: 91/93 exact (97.8%), 136/136 field values preserved, 0 invented
  entities, 0 cases with a missing field.
- Decision: **no promotion on this task.** The shipped model already ties the
  authored reference. The headroom is the new capability (typed fact selection,
  offer scope, remembered outcomes), which is the failure-led recommendation for
  arm C.

See [`tools/dialogue/PROMOTION-PILOT.md`](../../tools/dialogue/PROMOTION-PILOT.md)
for the frozen setup, decision rules, scope and limitations.

`summary.json` is the aggregate result by confidence band. `cases.json` holds
every case with the authored and generated line, the field-presence check and
the invented-entity flags. The model and tokenizer identities are in the
summary; the model stayed in the repository asset path.
