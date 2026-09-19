# Silverwick connected journey evidence

## Ordinary controls

The review used the in-app browser at 1280 × 720. Movement, scene changes,
packing, the bargain, cache transfers, and saving used visible controls and
keyboard input. The campaign began in Thornford and reached the mine through
the town gates and road turn-off.

The first part used PR #820 revision `56b1e2b1`. Its production WASM SHA-256 was
`707134a821e2525c125253b488764b1b6545d037711da6bff94b2dce6f969dbd`.
The saved Gatehouse visit then loaded in encounter revision `cde362bb`, whose
production WASM SHA-256 was
`e9de063aa0f23a40df03382387591e098b20902d59ed5f513fe37ca7cc64d6e4`.
These observations record those builds. Later fixes have their own checks.

| Seam | Observed result |
| --- | --- |
| Thornford → Gloamgate → Alderwatch → Silverwick | Accepted Mara's eight-Bread promise, inspected the road carriage, and reloaded at Hook Meadow with the same load and promise. |
| Silverwick preparation | Sold eight Bread for 40 crowns plus the existing 18-crown promise payment. Bought four Bread for 24 crowns. The purse was 67 crowns. |
| Road → Low Silver Pit yard | Took the mapped road branch. Saved four Bread in the carriage and an empty pack. |
| Yard → Gatehouse | Packed four Bread and walked through the visible entrance. Entry spent one Bread. Saved and reloaded with three Bread, light 18, and 67 crowns. |
| Schema 103 → 104 | The same saved Gatehouse visit loaded with those quantities and position. |
| Gatehouse → Lower Passage | Walked to the workers' records, used the survey, returned to the Lamp room, selected and opened the bar, and reached the haulers. |
| Bargain | Paid two packed Bread for one Raw Gold. The pack held one Bread and one Raw Gold. Inspection showed eight Iron, two Raw Gold, and two Gems left at the source. Save/reload kept the settled bargain and quantities. |
| Rope Store cache | Cached one Raw Gold, saved, reloaded, and recovered it. The cache returned to zero; the pack again held one Bread and one Raw Gold. |
| Mine → yard → road | Walked out, approached the carriage, and unloaded one Raw Gold. Saved the loaded yard. Explicit boarding returned to the same Silverwick–Alderwatch journey at 16%, with two goods in the carriage. |
| Road → Alderwatch | Paid the displayed 10-crown captain toll. Arrived on day 13 with 57 crowns and two goods. Saved in town. Carriage inspection showed one Bread and one Raw Gold. |
| Alderwatch → Silverwick | Chose the reverse road from the actual town endpoint. Entered and left the mine yard with the same two carriage goods and an empty pack. Returned to Silverwick on day 17 with 57 crowns and two goods. Saved in the ore yard. |

## Findings from that playthrough

The review found a wrong semantic acceptance key, a legacy Mine road action
leading to Saintless Vault, town departure text during road inspection, an
unclear entry ration cost, carriage transfer that immediately boarded, stale
road actions in the mine, and a missing hauler target in the pointer loop.
Those findings were sent to the implementation workstreams. The survey's
readable content and the town's response are checked with the later content
change.

## Road and portrait follow-up

Production revision `e4956d74` loaded the same town checkpoint after the road
save upgrade: Silverwick, day 17, 57 crowns, one Bread and one Raw Gold. Its
WASM SHA-256 was
`5d13bb1f991cebc1e9d177a98693be568f99823cd8879b237c056fcb94ab72bc`.
At 390 × 844, the ordinary Board action opened the carriage. Scene details
showed Bread 1, Raw Gold 1, load 2/12, free 10, one pony, readiness 99/100,
and carriage condition 67/100. The manually opened details stayed open through
scene updates. Step away and Save returned the browser's saved receipt.

The corrected town action led to the Silverwick–Alderwatch road. The displayed
left turn entered Low Silver Pit. Packing one Bread left Raw Gold 1 in the
carriage. Clicking the visible mouth entered Gatehouse, spent the one Bread,
and set light to 18. An observed-floor click reached the Lamp room. Reloading
then restored the explicit day-17 town checkpoint for the final combined check.

## Final content build: ordinary information outing

Production revision `9df2e966a327` was the reviewed head of PR #841, merged as
`3018e4c42648d02dbf29a36d04368a07b707c8d0`. The ordinary browser artifact had
WASM SHA-256 `646391fc5ebb0bcd967c0f21004cf71e28443f796996de5a166edd2bb12b2545`.
This outing continued the saved company through schemas 104, 105, and 106.

| Seam | Observed result |
| --- | --- |
| Town lead | Reached Jory Fen through the town streets and selected Ask about Low Silver Pit. The Book recorded Jory, day 17, the actual Alderwatch–Silverwick turnout, and the workers' records. |
| Road and yard reload | Departed toward Alderwatch, saved and reloaded at the mine turn-off, then took the visible left branch. Packed the remaining Bread. The reloaded yard held Bread 1 in the pack and Raw Gold 1 in the carriage. |
| Entrance and records | Walked through the mouth, spent Bread 1, and reached the records through observed floor. Explicitly reread the records: the west store passage goes around the bar; the Lamp Hall stair remains blocked. Saved and reloaded in that room. |
| Return to the road | Walked back to the exit, approached the carriage, saved and reloaded in the yard, then explicitly boarded. The road resumed at the same Silverwick–Alderwatch 16% anchor. Pack 0 and carriage Raw Gold 1 remained stable. |
| Opposite direction | Reached Alderwatch, chose Silverwick from that endpoint, and passed the same Low Silver Pit hoist. The reverse leg offered the right turn. Continued to Silverwick on day 25 with 57 crowns and Raw Gold 1. Saved and reloaded before reporting. |
| Information return | Reached Jory from the shift board and explicitly told him the acquired account. The Book recorded the day-25 report, the records read on day 18, the earlier unknown observation date, and his response: “This gives the next company a fair path in.” Purse 57 and Raw Gold 1 stayed the same. Saved the report. |
| Ordinary sale | Entered the Company store and approached Oren. Selected Sell, Raw Gold 1, then accepted the displayed 60-crown offer. The purse changed from 57 to 117, and Raw Gold fell from 1 to 0. Further sale was disabled with the empty cargo. |

The gold predates the new provenance tracking. It stays ordinary saleable cargo;
the report uses the newly acquired records. The earlier survey observation keeps
its unknown date. Reading the records gives the new reading its own date.

Captured ordinary controls and scenes:
[lead in the Book](silverwick-first-haul-2026-09-19/book-lead.png),
[outbound road](silverwick-first-haul-2026-09-19/road-outbound.png),
[reloaded yard](silverwick-first-haul-2026-09-19/yard-reload.png),
[workers' records](silverwick-first-haul-2026-09-19/records.png),
[inbound landmark](silverwick-first-haul-2026-09-19/road-inbound.png), and
[town return](silverwick-first-haul-2026-09-19/town-return.png), and
[dated return account](silverwick-first-haul-2026-09-19/book-return.png), and
[completed sale receipt](silverwick-first-haul-2026-09-19/sale.png).

## Save and command coverage

PR #836 saved the segment, direction, distance, destination, and side-road anchor.
Its native and Emscripten fixtures agreed. Pointer and keyboard regressions used
the displayed road choices for junctions, the mill, reversal, and destination.
PR #840 added a real schema-105 save generated by the preceding runtime, with
40 journal actions, a survey, and distinct pack, cache, and carriage custody.
The fixture was generated twice with identical bytes and verified read-only.
PR #841 migrated that fixture, preserved attribution through unload/repack/cache
and sale, and checked loss, stale requests, repeated reports, and journal replay.
Its host action checks exercised successful, stale, and duplicate lead/report
commands. The final reviewed head passed all 49 Python host tests and required CI.

## Timing and build receipts

PR #833 merged the local diagnostic export at
`b29a7089e58668276d5446a8c322ac0df7d0b8e0`. Each record carries a configured
build revision and a typed scene name. Runtime readiness and the first actionable
screen are separate measurements from navigation start. The export holds at most
96 records and includes action, transition, and save timings.

The local browser check at `d090bf7d` recorded 146 frame samples: median 25 ms,
p95 34 ms, and p99 57.5 ms. Runtime readiness was 83.8 ms; the first actionable
screen was 813.3 ms. The final CI check at `f0c65673328fe229d48ffb6e8138196a82914`
recorded 30 samples: median 133.3 ms, p95 300 ms, and p99 316.6 ms. The browser
check uses a 100 ms local p95 ceiling and a 400 ms CI ceiling. A prior CI build
had a 316.7 ms p95 with similar graphics work. These are environment-specific
measurements; a player-device walkthrough supplies a separate performance check.

A separate browser harness built the same `9df2e966a327` source and passed its
desktop, phone-size, touch, menu, save/reload, shader, fullscreen, and recovery
checks. Its WASM SHA-256 was
`94e8a96793c42dc422a45fee5a54c2f2c4b5d648bc349e6abed7d040f27b324d`.
The [timing export](silverwick-first-haul-2026-09-19/local-timings.json) recorded
runtime readiness at 79.4 ms and the first actionable screen at 815.1 ms.
The [frame receipt](silverwick-first-haul-2026-09-19/frame-budget.json) held
173 samples: median 24.9 ms, p95 26.6 ms, p99 33.9 ms, within its 100 ms local
p95 ceiling. This harness artifact and the ordinary outing artifact are recorded
separately because their build outputs differ.

The ordinary return also exported [its local timing record](silverwick-first-haul-2026-09-19/ordinary-timings-before-fix.json).
It exposed stale transition attribution: a later Escape action was recorded as
29,471.4 ms after an earlier Book tab touch. That record is retained as the defect
receipt. Its transition durations require the follow-up attribution repair;
the reported 70.2 ms startup and 599.2 ms first-actionable values describe this
particular reload.

## Evidence boundaries

The ordinary journey demonstrates travel, custody, persistence, bargaining,
cache recovery, unloading, and the return through authored towns. The combat
path and stale/shared commands also require their focused tests. Browser phone
layouts and touch emulation have separate receipts. Physical-phone observation
remains a separate check.

A worker briefly used the shared browser during the earlier records-room
review. That altered state was excluded. The clean saved Gatehouse checkpoint
was restored and verified before repeating the mine path. Later worker captures
used separate browser profiles.
