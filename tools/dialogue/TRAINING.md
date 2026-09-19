# Participant student input

`participant_training.py` converts a teacher candidate into a preview for the
5M student's 512-token context. `student_worker.py` uses the same builder for
experimental native inference. The native tokenizer supplies the token IDs.
`CcCoreModelBeginParticipant` loads those tokens with zero metadata and zero
copy candidates. A checkpoint trained on this format is the next requirement.

## Fixed format

Every line is a label followed by a JSON value. Text inside a value is quoted.
The version is `crownless-person-v1`. Array positions are fixed:

| Label | Array positions or value |
| --- | --- |
| self | name, occupation, age, goal, activity |
| needs | stress, courage, hungry days, unsheltered nights, coins, in transit |
| place | current place name, home name, simulation day |
| other | listener name |
| relationship | directed relationship from the simulation, or null |
| actions | available action names |
| account | event day, confidence, retellings, source ID, held account text |
| group | band name, faction ID |
| heard | source role, optional day, speech or action |
| remembered_speech | same structure, from a prior observed episode |
| memories / knowledge | one personal simulation record |

Self and listener IDs route observations and remain in the audit record.
Third-party observations keep their source ID. The final `turn:` line cues one
JSON object containing speech or an allowed action. The target ends at EOS.
Literal `[EOS]` and field markers in quoted text use a JSON Unicode escape.
JSON decoding recovers their original spelling.

## Budget and review

352 input tokens reserve 160 positions for a reply including EOS. Identity,
current needs, place, relationship, actions and the latest observed turn stay
whole. An example that exceeds this minimum is rejected. Optional evidence is
selected in this order: newest held account, earlier current speech from newest
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
