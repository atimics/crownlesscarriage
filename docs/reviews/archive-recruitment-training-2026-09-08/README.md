# Named archive training

This draft continues #446 and #470 on top of #610. Schema 80 adds training
progress to the saved recruitment order. The generator stays at 25 and SQLite
at 32. `CcSim` is 184400 bytes.

## Work and supplies

A trained scribe completes seven induction days. An apprentice completes 28
work days with the named trainer present, alive, working and qualified as a
scribe. Training starts after the actual arrival day. The saved last-work date
limits progress to one work day per calendar day. Illness, an absent trainer,
an unavailable archive, missing tools or an incomplete payment leave the
remaining work pending with a reported reason.

The first day of each seven-day work block consumes held food and paper.
Induction uses two wheat and one paper. Each apprenticeship block uses four
wheat and one paper for the recruit and trainer. The original tools remain
available throughout training. Two wheat, one paper and one tool remain for
the first archive task.

A teacher's work reserves one share of the weekly archive recording plan in
each calendar week that includes teaching. A saved week marker keeps this
reservation through cancellation. It expires at the next weekly cycle.
The daily meal routine respects held training food, and quest casting respects
the recruit and trainer while their work is committed.

Completion pays the held 50 crowns into the named recruit's existing purse.
The apprentice gains the scribe occupation and keeps their social role. The
order retains the original donor shares and the paid amount. Cancellation
returns the remaining goods and funds. Paid wages remain with the person.

## Saved progress and evidence

The order stores recruit work days, trainer work days, the last work date and
paid wages. A fifth saved value records the trainer's weekly archive work
reservation. All five have explicit hash and SQLite fields. Validation checks
state, dates, work counts and payment. The SQLite reader checks full-width
integers before narrowing them.

Tests cover both courses, delayed work, duplicate daily updates, payment
capacity, tools, supply use, role and occupation, trainer retirement, recruit
death, the weekly work reservation after cancellation, patron-funded wages,
refund conservation and a full apprenticeship journal replay. Saved fields
are compared directly across progress and completed states, and each new
field changes the hash independently. A wide SQLite integer is rejected.

The schema 79 journal fixture drops the new table before replay and checks the
historical hash after upgrade. `legacy-probe.c` compares two 40-year runs against
parent `2b340741acdc2b98cf44dd4aaddb35fc0ae05257`. All 82 annual schema 79
checkpoints match. `legacy-parity.json` records them.

The strict headless build and all 123 tests passed. Static analysis passed
with one reviewed baseline item. The native play build and seven focused
checks passed: training, journey, reservation, shared carriage bridge, bridge
scene input, world card input parity and adventure input flow. These checks
ran against code commit `b5a483f`.

## Next work

A completed course has status 5 and keeps the first task's supplies. Named
staff appointment and the first task are the next stages of #446. Later
archive work will use the named staff record and its ordinary supply costs.
