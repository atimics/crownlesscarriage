# Gossip through a real Crownless world

Seed 73, schema 95, advanced for 1,092 days from day 1 to day 1,093. The world
followed its normal simulation with its own travel, production, recruitment,
and archive work. The observation tool preserved the final world hash:
`f76a9d8bf78580cf`, also obtained by the unobserved run.

The run observed 6,415 events across 57 event kinds and 689 distinct gossip
stories. **649 stories reached the scriptorium; two were recorded.** The
scriptorium remained in Gloamgate. Grain was the reported archive blocker on
939 of the 1,093 daily snapshots. Binding was the reported blocker on the
first 17 snapshots. The material check reported ready on 137 snapshots; actual
writing also follows work schedules, available accounts, and worker eligibility.
Food for scribes depends on spare food after the town's reserve needs.

## A harvest report becomes a first page

The original day-1 event said:

> Thornford's drought harvest cannot supply the eastern settlements.

| Day | Observed stage | Account |
|---|---|---|
| 2 | Thornford residents hold the news | Grenwin Longmead, a scribe, holds it at confidence 93 after one retelling. |
| 5 | Forda Millward brings it to Gloamgate | The scriptorium hears it after two retellings, confidence 84. It still says eastern settlements. |
| 10 | Grenwin reaches Gloamgate | He retains his own earlier account, confidence 93. |
| 18 | Grenwin joins the archive | His first page records the eastern-settlements account. A physical **Ledger Alder Concord of 0** is created. |
| 20 | Silverwick hears another version | Four retellings, confidence 65. The account now says southern settlements. |
| 24 | Rosespire hears that version | Six retellings, confidence 44, with fear that worse is coming. |
| 25 | Alderwatch and Hollowbarrow also hold it | Both have the southern-settlements version. Hollowbarrow's telling also credits the crown. |

The archive event names Grenwin, points to the original event, and identifies
the new ledger as its subject. This is a direct recorded link to that book.
Its text is:

> Grenwin Longmead joins the archive. Their first page records: Thornford's drought harvest cannot supply the eastern settlements.

The core model, run separately on these real held accounts, produces:

**Grenwin's account:**

> Thornford's drought harvest fell short of what the eastern settlements needed.

**Silverwick's later account:**

> Thornford's drought harvest fell short of what the southern settlements needed, so people say.

The change from eastern to southern comes from the simulation's retelling
rules. The model faithfully expresses the changed belief. The earlier written
page preserves a different version that a later reader could compare with it.

## Calves acquire politics on the road

On day 224, Rosespire's cattle herd raises a calf and adds an older calf to
the working herd. The town's account starts at confidence 100.

| Day | First observed arrival | Retellings | Confidence |
|---|---|---:|---:|
| 224 | Rosespire | 0 | 100 |
| 234 | Hollowbarrow | 2 | 82 |
| 237 | Alderwatch | 2 | 82 |
| 248 | Silverwick | 4 | 61 |
| 254 | Gloamgate and scriptorium intake, brought by Reidel Parchman | 6 | 37 |
| 255 | Grenwin's own held copy | 7 | 26 |
| 261 | Thornford | 8 | 10 |

These are first arrivals at each place, rather than a claim that all towns
form one uninterrupted route. The raw trace keeps the source person IDs.

The account gains praise for the crown by day 234 and fear of worse by day 254.
On day 266, Grenwin records it. The archive's actual event text is:

> Rosespire's cattle herd raises 1 new calf; 1 older calf join the working herd. Loyal voices credit the crown. They fear worse is coming.

An **Annal Alder Concord of 0** is the sole new physical volume on that day.
This daily observation and the archive-writing code associate it with the work.
The ordinary recording event stores the source event as its subject; the
first-page appointment path above stores the book ID instead.

From Grenwin's confidence-26 account, the core produces:

> Rosespire raised calves and added older calves to the working herd, if the story is right.

This demonstrates the current division: the stored archive text retains the
political additions, while the core writes the main claim with its confidence
and general quantities. Those additions are separate from the core's present
speech task.

## A goblin raid travels widely and remains unwritten

The Cinder Tithe raids Thornford on day 88, taking wheat and crowns. The story
is observed in Thornford on day 89. It reaches Gloamgate and scriptorium intake
on day 92, Hollowbarrow on day 95, Silverwick and Rosespire on day 98, and
Alderwatch on day 105. Grenwin has his own copy by day 93.

The early model sentence is “The Cinder Tithe raided Thornford.” The later
town accounts produce “The Cinder Tithe raided Thornford, so people say.”
The run contains zero archive-writing events for this raid. It is an example
of widely circulating news that leaves a weaker written record.

The 647 heard stories that lacked a recording event across the run should be
read as observed unwritten stories. The live gossip pool holds 32 stories and
changes as new news arrives, so that total is larger than the final waiting pool.

## What this suggests for the core

The useful writing input is **the selected scribe's held account**, including
its own confidence and source. Intake and a scribe can hold different versions
of the same event. In this run that distinction is visible in both recorded
stories.

The next integration can use that account to compose a page and retain the
source event, scribe, confidence, and model identity beside its text. The first
page already has an explicit book link. Ordinary archive entries would benefit
from the same stable link. Current treasure objects hold a book's physical
state, while archive event text carries the recorded wording. The event ledger
also compacts parent links as old events leave, so source IDs and explicit
page membership deserve their own durable fields.

The run makes the tradeoff concrete: news can cross the world quickly, while
food, binding, and scribe work determine which version becomes history.

## Reproduce and inspect

```sh
cmake -S . -B out/build/flow -DCC_BUILD_CLIENT=OFF -DCC_BUILD_BENCHMARKS=OFF -DBUILD_TESTING=ON -DCC_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/flow --target crownless_gossip_flow core_account_probe -j 4
ctest --test-dir out/build/flow -R gossip_flow_integrity --output-on-failure
out/build/flow/crownless_gossip_flow --seed 73 --days 1092 > seed73.jsonl
out/build/flow/crownless_gossip_flow --seed 73 --days 1092 --hash-only > seed73-unobserved.jsonl
```

The tool emits world places, first-observed retained events, changed town and
personal accounts, intake, archive material snapshots, physical treasure
changes, and final validation. It checks the world hash around each observation.
`work_ready` reports the work plan; `blocker` reports the separate material
check. Snapshots are daily, so transient changes within a day may be absent.
Day numbers refer to the simulation clock. Default initialization starts at 1.

The three case IDs are `648518346341351471` (harvest),
`648518346341353064` (cattle), and `648518346341352151` (raid).
[Selected trace](selected-trace.json) preserves their observed accounts,
recording events, the scribe's versions, and resource snapshots.
[Summary and hashes](summary.json) identify the complete local run.

[Speech results](speech-results.json) contain all 24 core inputs and outputs.
Each used the native `core_account_probe` on the prepared held account, its
confidence, and a widely-retold cue at four or more retellings. The saved core
from [ZERO PR 36](https://github.com/atimics/zero/pull/36) then generated the
sentence on CPU. All 24 completed and matched an approved meaning. They are
separate model comparisons; the actual simulation archive text is quoted above.
The model and tokenizer hashes are included with the results.
