# Bounded Luna batch

User requested a batch with quota monitoring. Six fresh `gpt-5.6-luna`
teacher sessions each played one person. Each received only their own selected
simulant state and the other person's speech. The requested model slug is
recorded; the provider revision was unavailable.

The cap was 24 turns across three encounters. All three ended naturally,
producing 14 turns. Root agent review accepted seven: four speech turns and
three conversation endings. Seven were excluded. This is agent review.

| Scene | Raw | Accepted | Excluded |
| --- | ---: | ---: | ---: |
| Thornford raid and work | 5 | 3 | 2 |
| Gloamgate food reserve | 5 | 3 | 2 |
| Silverwick tunnel fear | 4 | 1 | 3 |

Failures included invented stewards or a foreman, weak observation grounding,
repeated proposals, and one malformed JSON object. The malformed object was
saved unchanged. Its speech value was explicitly extracted for relay to the
other person. A longer food opening exceeded the teacher's requested 120-byte
limit but fit the student target budget and passed grounding review.

The native tokenizer compiled every schema-valid candidate. The existing
reviewed-corpus assembler checked exact source and compact hashes and verified
that each accepted turn retained its cited evidence. Accepted prefixes were
206–339 tokens, within the 352-token allowance. Full source snapshots, selected
views, original outputs, compact previews, review decisions, exclusions and
assembler receipts are in `luna-quota-batch.json.gz`.

These are alternate encounters in existing training worlds 1201, 1202 and
1203. Keep them in those training groups. They add teacher samples at familiar
states; fresh states remain the next coverage task. Student training was left
for a larger corpus.

## Quota

| Check | Luna used | Main allowance used |
| --- | ---: | ---: |
| Start | 0% | 35% |
| Raid end | 0% | 36% |
| Food end | 0% | 36% |
| Generation end | 0% | 36% |
| Publication | 0% | 37% |

The stop threshold was an increase of five percentage points in either
allowance. Readings are account-wide integer percentages and may update with
delay. Other activity can affect them. They support only a small-batch quota
observation, not an exact token-cost estimate.

The archive records a summary of teacher setup rather than a verbatim prompt
log. Two temporary agent thread-limit errors were retried before generation;
each delivered output appears once.
