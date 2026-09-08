# Journey pace and stop commands

Continues #260 on top of #542. A dedicated command module now owns pace changes,
watch strain, team recovery, midday choices, camps, road-house lodging, and
roadside camp/pass choices. The roadside-stop query joins the existing journey
query module.

The main command dispatcher uses one entry point. It supplies the existing
`PushEvent` recorder, which retains event ordering and player knowledge updates.
The runtime tick loop calls the shared watch-strain function. Eight moved rule
bodies match the parent after the event recorder call is normalized.

## Evidence

- Strict Release native client and browser builds passed.
- All 119 CTest checks passed, including journey choices, roadside stops,
  validation, save fixtures, and journal replay.
- Local Cppcheck 2.20.0 passed the repository static-analysis gate.
- `command_probe.c` compares prepared journey fixtures across two seeds, both
  directions, and schema rules 26, 33, 38, 39, 40, 41, 59 and 60. It records
  736 command outcomes (360 accepted, 376 rejected) and 96 watch transitions.
  All 832 rows match the parent, including errors and complete state hashes.
  The fixtures cover pace bounds, midday/overnight choices, funded/unfunded
  lodging, roadside choices, and day advancement during rest.
- `parity_probe.c` compares 720 annual state hashes: two seeds, 40 years each,
  and nine schema rule modes from 26 through 60. Every row matches.
- `parity.json` records row counts and SHA-256 digests for both comparisons.
- Native road-book QA produced all 19 captures and passed both rendering gates:
  route 469.9 FPS / 4.799 ms p95; network 398.7 FPS / 4.508 ms p95.
  `roadbook.txt` records those reports.

Travel departure and encounter resolution remain the next journey boundaries.
