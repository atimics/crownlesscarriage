# Participant student input

`participant_training.py` converts a teacher candidate into a preview for the
5M student's 512-token context. `student_worker.py` uses the same builder for
experimental native inference. The native tokenizer supplies the token IDs.
`CcCoreModelBeginParticipant` loads those tokens with zero metadata and zero
copy candidates. A checkpoint trained on this format is the next requirement.

## Fixed format

Every line is a label followed by a JSON value. Text inside a value is quoted.
The version is `crownless-person-v2`. Array positions are fixed:

| Label | Array positions or value |
| --- | --- |
| self | name, occupation, age, goal, activity |
| needs | stress, courage, hungry days, unsheltered nights, coins, in transit |
| place | current place name, home name, simulation day |
| other | listener name |
| relationship | affinity, trust, obligation, history; or null |
| actions | available action names |
| account | event day, confidence, retellings, source role or ID, held account text |
| group | band name, faction ID |
| heard | source role, optional day, speech or action |
| remembered_speech | same structure, from a prior observed episode |
| memories | one personal simulation record |
| knowledge | kind, learned day, certainty, private flag, source role or ID, recorded source name, event day, event text; unresolved records keep their original object |

Self and listener IDs route observations and remain in the audit record.
Account sources use `self` and `other` for the two participants. Third-party
accounts and observations keep their source ID. The full relationship cause ID
stays in the source record. Reviews can cite a retained relationship field with
`["relationship", "history"]`, for example. Native inference accepts v1 and v2
headers; each checkpoint and its training data should use one explicit version. The final `turn:` line cues one
JSON object containing speech or an allowed action. The target ends at EOS.
Literal `[EOS]` and field markers in quoted text use a JSON Unicode escape.
JSON decoding recovers their original spelling.

## Budget and review

352 input tokens reserve 160 positions for a reply including EOS. Identity,
current needs, place, relationship, actions and the latest observed turn stay
whole. An example that exceeds this minimum is rejected. Optional evidence is
selected in this order: resolved personal knowledge from newest to oldest,
newest held account, earlier current speech from newest
to oldest, remaining accounts by date, past observed speech, group membership,
personal memories and knowledge. The rendered speech order is chronological.
Each omitted record appears in `dropped` with its original index. This is a
first selection policy; recall experiments should compare other priorities.

Selection uses the input alone. The answer has a fixed reserved budget. A target
that exceeds that budget or the native output byte limit is rejected whole.
Labels mask all input tokens; the final prefix position predicts the first
output token. Labels are already shifted. A trainer should use them directly
with token-position logits. Existing typed metadata channels should be zero,
and the copy loss should be disabled for these rows.

Each preview has `pending_compact_review` status. Review the target against the
retained evidence as well as the full teacher view. Carry the source semantic
review and split related world histories together when assembling a corpus.
The current six-turn sample provides a format check. Training needs a larger
set of reviewed encounters and held-out worlds.

## Commands

Build `core_model_probe`, then compile the paired runner's candidate JSONL:

```sh
python3 tools/dialogue/participant_training.py \
  --input /tmp/pair/training-candidates.jsonl \
  --probe BUILD/core_model_probe --output /tmp/compact-preview
```

The fresh output directory contains previews, original rejected lines and a
receipt with source, compiler, probe and result hashes. The command returns a
failure status when any rows are rejected, while retaining every result.

Once a participant checkpoint has been trained, each paired worker command is:

```sh
python3 tools/dialogue/student_worker.py \
  --probe BUILD/core_model_probe --model /path/to/participant.ccv2
```

The worker logs the compact input and raw generation to stderr, validates one
JSON turn, and preserves its actor ID across requests. The paired runner stores
those logs and handles episode memory. The shipped game continues to use its
current account checkpoint while this training path is evaluated.

## Assemble reviewed turns

`reviewed_corpus.py` binds each review to hashes of the full source candidate
and its exact compact preview. The reviewer records source and compact decisions,
a name, notes, and the evidence needed for the target. Evidence references use
arrays such as `["self", "coins"]`, `["account", 0]`, or `["turn", 1]`.
Indices refer to the original request. The assembler checks that each cited
record remains in the compact input. Reviewers still judge truth, meaning and
whether the turn contributes to the exchange.

```sh
python3 tools/dialogue/reviewed_corpus.py \
  --candidates /tmp/pair/training-candidates.jsonl \
  --reviews /tmp/reviews.json --probe BUILD/core_model_probe \
  --world-group world-and-shared-history --output /tmp/reviewed
```

The output preserves source candidates, reviews and exclusions, and writes
`trainable.jsonl` plus a hash receipt. Duplicate candidates count once. Use a
fresh output directory. The command succeeds when at least one row is accepted.
Related snapshots and forks share one world group. The Zero participant trainer
requires separate groups for training and validation.

The reviewed pilot and its held-out replies are recorded in
`docs/reviews/participant-minds-2026-09-19/reviewed-corpus.md`.


## Known event details

Participant snapshots resolve the event directly referenced by each own knowledge
record. `event_text` and `event_day` are present when the event survives and its
date is at or before the learned date and current day. Unavailable details use
JSON null. Certainty, source and privacy remain on the owned knowledge record.
Certainty uses 1 for doubtful, 2 for told and 3 for witnessed. The raw event text
describes the recorded account at that time. Advice and new
plans remain separate from completed actions.

The source record preserves subject and event IDs. The compact knowledge array
keeps source attribution, learned date, event date, certainty, privacy and the
complete text. Each record fits whole or appears in the omission list.
