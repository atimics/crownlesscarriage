# Fact binding: from held accounts to the answered field

Status: bridge complete · 2026-09-21 · Path B step 2

## What this connects

`event_facts.build_facts` gives account-level facts: an owner, an event, a
source, a certainty and the held account text. The v2 policy chooses a speech
act and binds a claim to `facts[0]`. It does not answer the question that was
asked.

`fact_binding.py` closes that gap. It parses each held account's grammar
`source` template into typed fields, builds the `fact_policy` table, and routes a
question predicate to the account and field that answer it.

```
held account text ──grammar source template──> typed fields
question wording ──fact_question.parse_question──> predicate
predicate + rule ──fact_roles.resolve_field──> field index
        └──> selected account_ref, field, role, value ──> claim
```

## Validated parser

The parser is not a replacement for the native grammar. It is validated against
it: on all **200** rows of ZERO's `grammar-test.jsonl`, `parse_account`
recovers the native field values exactly, including quantity and detail fields
that routing ignores. `tests/dialogue_fact_binding_tests.py` asserts this and
skips when the ZERO rows are not available.

## Routed example

```
# notice_posted_0
  account: Yorororholt Kelumeth posts a notice at Yororormere: Yorilashfell Cup.
  Q: Who posted that?          -> Yorororholt Kelumeth (actor)
  Q: Where was the notice posted? -> Yororormere (place)
  Q: What was the notice about? -> Yorilashfell Cup (object)

# harvest_failed_0
  account: Kelilowden's drought harvest cannot supply the Yorashormere.
  Q: Where did the harvest fail?  -> Kelilowden (place)
  Q: Who needed the supplies?     -> Yorashormere (place, via the beneficiary override)

# bandit_pressure_1
  account: Kelumilwick Yorethas joins Kelashorgate Kelenas after 115 days seeking food at Yorashenfell.
  Q: Which band did they join?    -> Kelashorgate Kelenas (recipient, via the joined override)
  Q: Where did they look for food? -> Yorashenfell (place)

# paper_milled_0
  account: Kelilashmere's mill uses 25 Rags to make 64 Paper.
  Q: What was the paper made from? -> Rags (material)
  Q: Where is the mill?            -> Kelilashmere (place)
```

## Policy integration

`policy.make_act`, `validate` and `decide` accept an optional `question` text.
When `report_fact` or `explain_cause` is chosen with a question, the claim is
routed to the held account that answers it; otherwise the usual most-recent
fact is used. The model predicts only the action ID, and the claim is derived
from context, so **the trained action wire is unchanged** and no retraining is
needed. Existing callers pass no question and keep the previous behaviour.

```python
act = policy.make_act('report_fact', person, heard, requested='learn',
                      question='What was the notice about?')
# act['claim']['ref'] names the notice account, not the most recent fact
```

## Native act protocol

The v2 policy carries a typed act in an 18-byte record: version, action, and a
16-byte digest of the full act (`policy.pack_act`). The native probe decodes the
action ID and checks the candidate mask; the claim is bound from context in
Python. This bridge chooses **which** `account_ref` and field to bind; it does
not change the record format.

To bind the same routed claim natively, the C runtime needs the question
predicate at the point it reconstructs the act. That is a separate port: the
parser keyword set and the `fact_roles` overrides would move to C, or the
digest would be checked against a Python-bound claim supplied alongside the
action.

## Scope

- The parser matches the grammar's `source` template. Accounts that the native
  grammar parses with rules outside these templates need their own rule.
- Routing is by predicate and role. Rules with a duplicate spoken role need the
  `fact_roles` override or predicate-level routing (17 of 158 rules).
- The claim binds the routed **account**; the renderer still quotes the whole
  account, so field-level rendering is a follow-up.
- The routing is Python-side. The native claim binding is a separate port.
