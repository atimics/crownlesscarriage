# Shared gossip topic rules

Base: main `4f7635cc3a115492470a60ef9223dab0d3c859db`.
Related issues: #470, #467, #260.

The letter probe now uses the shared `cc_gossip_topics` module for topic names,
parsing and event membership. The public enum keeps the probe's existing IDs:
all, dragon, goblin, war, throne, wheat, herds, ponies, road, bandit and treasure.
An event can belong to several topics. The pony topic remains reserved for
future companion accounts. The parser reports failure and preserves the
caller's value for an unknown name. The probe keeps its existing default of all.

This extraction gives occupation-based observation and research commissions
one event vocabulary. Schema 74, generator 25 and the saved character layout
remain the baseline for the next stage. Occupation allocation, saved identity,
direct observations and herd page measurements remain follow-up work in #470.

## Evidence

- Strict headless build passed.
- All 115 headless tests passed, including the existing letter probe contracts
  and a new shared topic contract for herd membership, overlapping subjects,
  reserved topics, parsing and invalid input.
- Static analysis passed with one reviewed baseline item.
- `python3 docs/reviews/shared-gossip-topics-2026-09-08/verify.py` compared all
  11 topics against all 135 current event kinds: 1,485 decisions matched the
  original source at the base commit. The script extracts the original switch
  from Git and compiles it beside the shared module.
- `output-parity.json` records byte-for-byte equality for 24 complete letter
  probe outputs: seeds 42 and 1592590337, two years each, scan, compare, and
  all ten research missions. Each row includes its arguments, byte count and
  SHA-256. The control binary came from the play build used for the main
  capture acceptance audit; its source matches the base in src, tools and
  CMakeLists.txt. The candidate came from this branch's strict headless build.
