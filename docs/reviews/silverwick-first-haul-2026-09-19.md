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
