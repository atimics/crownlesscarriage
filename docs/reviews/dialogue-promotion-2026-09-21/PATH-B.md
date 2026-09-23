# Path B: typed dialogue capability and candidate C

Status: built and trained · 2026-09-21

Issue 809's A/B pilot showed the shipped account-speech model already ties the
authored reference, so the promotion must come from a capability it does not
have. This records the offline build of that capability and the candidate
checkpoint.

## Built

| Step | Deliverable | PR |
| --- | --- | --- |
| 1 | Question predicate parser and grammar-role reconciliation | #873 |
| 2 | Held-account parsing and question-to-field binding | #874 |
| 3 | Policy claim routed to the account that answers the question | #875 |
| 4 | Candidate C trained on the v2 policy | this record |

The pipeline is:

```
held account text ──grammar source template──> typed fields
question wording  ──parse_question──> predicate
predicate + rule  ──resolve_field──> field index
        └──> account_ref ──policy.claim──> typed act ──render──> line
```

`fact_binding.parse_account` is validated against the native grammar parse on
all 200 grammar rows. `policy.make_act` routes `report_fact`/`explain_cause` to
the held account that answers a supplied question. The model predicts only the
action ID, so the trained action wire and native parity are unchanged.

## Candidate C

Trained fresh on the v2 policy data, 4,000 CPU steps, batch 32, seed 19, 220 s.

| Check | Development | Test |
| --- | ---: | ---: |
| Valid act | 1271/1271 | 1704/1704 |
| Exact reference choice | 1271/1271 | 1702/1704 |

Export SHA-256: `c997c505544eaa1bdce271ef825f2b320af781d04e9e3557cab8cc3edb908cf8`.

This matches the previously recorded policy run (`889d062a…`), so the training
is reproducible. Native parity is checked by the `Semantic 5M training` release
workflow against the built probes; that step was not run locally.

## Promotion path

The capability is offline. It is not in the game client, which still speaks the
account-conversation path in `src/story/cc_core_conversation.c`. Promotion
needs, in order:

1. **A game-side caller** for the policy path. The client does not use it today.
2. **Native parity for the routed claim.** The probe decodes the action ID; the
   claim is bound in Python. Either the game calls the Python claim binder, or
   the routing moves into C.
3. **Encounter content.** The 24-family encounter pilot (offers, route evidence,
   remembered returns) still needs authoring; the account-speech A/B and the
   policy data are not it.
4. **Play verification** and a version pin.

## Scope

- The candidate is scored on synthetic reference-policy preference, not on
  encounter quality or human review.
- The claim binds the routed account; the renderer still quotes the whole
  account, so field-level rendering is a follow-up.
- 17 of 158 grammar rules have a duplicate spoken role and need the
  `fact_roles` override or predicate-level routing.