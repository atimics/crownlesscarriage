# Reviewed participant corpus

The assembler checks whether the evidence cited by a reviewer survives the
student's compact input. Reviews bind exact source and compact hashes. Source
and compact decisions, reviewer names, notes, and evidence references remain
with every accepted row. These are root-agent reviews.

`reviewed-corpus.json.gz` contains source candidates, reviews, accepted rows,
exclusions and a receipt as text files in a JSON object. Eight of twelve turns
from world 1202 were accepted. Four were excluded for spelling, missing compact
evidence, or repeated proposals. Both scenes share the `1202-main-b3a8` group.

`validation-teachers.json.gz` preserves the full world 1200 snapshot, selected
private packets, four original teacher outputs, compact previews, and reviewed
corpus files. Two fresh independent model sessions played Jory and Bren. Each
received its own private packet and the other person's speech. One held cattle
account was selected before generation. Jory held that account; Bren learned
about it through speech. The world stayed at day 366.

Two validation turns were accepted. Jory's second turn cites a source account
that falls outside the compact budget. Bren's second turn repeats the existing
work proposal. Both originals remain in the archive. This split tests a separate
world with familiar starter characters. Wider tests should also separate people,
places, scene types and wording.

## First reviewed training check

The trainer from Zero PR #53 ran 100 CPU steps from the shipped 4,945,153 parameter
checkpoint, seed 19, batch size 2. It used eight training turns and two validation
turns. Training processed 24,460 target tokens. Final batch loss was 0.00740;
validation target-token loss was 3.69596. These measure different data sets.

Both held-out generations produced complete speech JSON. Both copied an earlier
training answer about sheep work. Jory's input instead held cattle news. Bren's
input contained Jory's cattle-work question. This is direct evidence of memorized
answers and weak response to current information in this tiny pilot.

`reviewed-training-check.json.gz` preserves the manifest, all step history,
samples including exact bytes, native comparison, and artifact hashes. The native
C output matched the Python quantized output on both samples. The exported weight
file remains in the local run directory recorded in the archive. The game keeps
its existing checkpoint.

Next collection should cover different needs, relationships and events, with
multiple grounded alternatives for each kind of exchange. Evaluation should
measure new useful contributions, identity, current-event use, memory recall and
valid actions. Model selection needs a larger held-out set and direct review of
whole exchanges.
