# Dialogue-model promotion pilot: A/B

Status: A/B complete · 2026-09-21 · issue 809 first delivery

## Question

Before promoting a checkpoint into the game, issue 809 asks for a matched
comparison. This runs the first two arms on the task the in-game Chat path
actually uses: given a held account, produce the NPC's spoken line.

- **A authored**: the account grammar's authored rendering.
- **B shipped**: the shipped `assets/language/core.ccv2` (`7d1c3cd5…`).

## Frozen setup

- Source: `experiments/grounded-dialogue/input/evaluation-accounts.jsonl`,
  93 accounts with a packet, from held-out worlds 1901 and 1902.
- Encoding: the shipped model's native row (`prefix` + fields + history),
  `slots=True`, `conversation=True`, 80 new tokens, greedy.
- Scored before running, and not adjusted after:
  - exact match to the authored line (whitespace/case normalized),
  - every copyable field value present in the generated line,
  - no invented capitalized entity outside the model input.

## Decision rules, declared before the run

| Rule | Threshold |
| --- | --- |
| Promote B over A on this task | B strictly beats authored on exact and does not lose field preservation or entity fidelity |
| Keep A as the reference | B ties A or is worse |
| Recommend C | B ties A, so the headroom is in a capability A and B do not share |

## Result

| Check | Confident (33) | Uncertain (60) | All (93) |
| --- | ---: | ---: | ---: |
| Exact match to authored | 33/33 | 58/60 | **91/93 (97.8%)** |
| Field values preserved | 49/49 | 87/87 | **136/136 (100%)** |
| Cases with every field preserved | 33/33 | 60/60 | **93/93** |
| Cases with no invented entity | 33/33 | 60/60 | **93/93** |

The two misses are both `cow_slaughtered_0`, and both are valid paraphrases with
every field preserved:

- authored: *Rosespire slaughtered cattle for beef when fodder ran short, so people say.*
- shipped: *A fodder shortage led Rosespire to slaughter cattle for beef, according to the word going round.*

## Decision

**No promotion on this task.** The shipped model already matches the authored
rendering on held-out worlds: 97.8% exact, 100% field preservation, zero
invented entities. A same-architecture checkpoint swap has no headroom here.

The promotion opportunity is in a **capability the shipped model does not have**:
typed fact selection, offer scope, and remembered outcomes. Those are the
distinctions issue 809 lists and the ones the mine encounter needs.

## Failure-led recommendation for arm C

Train C on the **new capability**, not on account-speech fluency:

1. Use the typed fact-selection interface (`tools/dialogue/fact_policy.py`,
   98.4% held-out) and the v3 meaning loop, which already pick a typed candidate
   and let the renderer own the wording.
2. Score C on offer scope, quantities, role binding, source certainty and
   remembered outcomes — not on exact wording, which A and B already tie.
3. Do not fine-tune the grounded-dialogue candidates as a drop-in: they speak a
   four-turn Q&A format, while the game's Chat path speaks account speech, so
   they are not comparable without an adapter.

## Scope and limitations

- This pilot covers the account-speech task only. The sealed 24-family encounter
  pilot still needs offer, memory and return-observation families built on the
  meaning interface.
- The evaluation accounts are held-out worlds but share the account grammar, so
  this measures in-grammar fidelity, not open-ended language.
- One seed and one checkpoint; no candidate was trained for this task.
