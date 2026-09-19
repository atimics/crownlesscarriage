# More room for a person's account

The v2 participant input writes the directed relationship as four fixed values:
affinity, trust, obligation and history. The relationship cause event remains in
the full source record. Accounts identify the participant sources as `self` or
`other`, and preserve third-party source IDs. This saves input tokens while
keeping the same 352-token prefix and 160-position reply budgets.

Jory's second turn in the world 1200 cattle scene now retains both his day 336
account and Bren's question. The prefix uses 329 tokens. In v1, the account was
omitted. V2 also retains the dragon account throughout the six-turn dragon scene.
These are measured examples; long accounts can still exceed the budget.

`context-v2.json.gz` preserves fresh source and compact reviews, corpus files,
receipts and selection details. Eight training turns and three validation turns
pass review. Every original target is unchanged. The added validation turn has
its own fresh review. Relationship evidence references now name the retained
field; the full relationship and cause event require their own source evidence.

`context-v2-training.json.gz` records a 100-step warm-start check, using seed 19,
batch size two and the shipped 4,945,153 parameter model. Final batch loss is
0.00820. Validation loss is 3.77874 across three turns; the previous v1 pilot used
two validation turns, so these aggregate losses measure different sets.

Both sampled replies are complete JSON and copy prior dragon-scene training
answers. The first assigns Jory the wrong listener, hunger, occupation and event.
The second ignores Bren's current work question. Native C and Python quantized
outputs match both samples exactly. This confirms a data and learning failure
while establishing that the revised input can retain useful evidence.

The native reader accepts both version headers. The Zero trainer records the
chosen version in its manifest and checkpoint and requires one version across
training and validation. Dataset reviews must be repeated after changing input
format. Experimental workers use the current compiler's v2 input; use the earlier
compiler with historical v1 checkpoints. The game retains its shipped checkpoint.

## A new food-reserve exchange

`food-teachers.json.gz` adds six original turns from independent Ilyra Senn and
Tomas Rill sessions in world 1203, day 31. Both held a day 14 account of Gloamgate's
food reserve. The selected source account stays in every v2 compact input, along
with the latest speech. Three turns pass source and compact review.

The accepted turns propose checking stock, asking how much is already promised,
sharing the count, and reviewing rations against food on hand when deliveries
might arrive late. One turn repeats an existing proposal. Two turns assert that
the speaker has eaten based on zero hungry days. The simulation sets that field
to zero for residents at home, so it provides weaker evidence than a recorded
meal. Those original turns remain excluded in the archive. Future teacher
instructions should explain this field as a recorded hunger counter.

This scene has its own `1203-main-b3a8` history group. It was collected after the
100-step experiment above and was used for source and compact review only.
