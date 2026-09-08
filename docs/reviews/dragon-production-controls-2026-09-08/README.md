# Dragon policy production controls

This slice of #394 adds the fixed day-1 dragon control to the existing production capture. Run `make production-baseline` from a clean checkout. It now captures four cases, each twice, for seed `0x5EED0001` and 40 years:

| Site fixture | Dragon policy |
| --- | --- |
| Baseline | Natural history |
| Opened/funded mill, forge and farm | Natural history |
| Baseline | Slain at day 1 |
| Opened/funded mill, forge and farm | Slain at day 1 |

Every run records day 1 and each annual checkpoint through day 14601. Annual structural validation and repeated-byte comparison remain enabled. The manifest labels each site fixture, dragon policy and whole-policy comparison, with the original build, command, save and report provenance.

## Exact intervention

The runner flag is `--dragon-slain-day-one`. It applies to a fresh seeded world and changes six dragon fields: `slain=true`, `slain_day=1`, life stage `AFTERDRAGON`, activity `AFTERMATH`, body condition 0 and crown strength 0. The initial hoard, goods and all other world fields retain their seeded values. The normal world loop then advances this state. This is a diagnostic initial condition; it establishes the aftermath state directly.

The capture labels all downstream differences as a whole-policy comparison. Dragon life cycles, trade, diplomacy, production and random draws can diverge together. The current-base control is documented here independently of the older scratchpad audit.

## JSON protocol 3

Each checkpoint includes `dragon_policy`, `comparison_scope` and the current dragon's ID, slain status/day, life stage, activity, body condition, crown strength and egg count. Policy labels describe the initial intervention. Current dragon state can evolve under the ordinary life-cycle rules.

A loaded save uses the `loaded-save` policy label. Its simulation state is preserved; its accounting begins at the loaded day. Capture manifests retain the original intervention command. Fresh-world fixture flags require a fresh seed. JSON and chronicle output remain separate report modes.

World schema 65, generator 25 and SQLite 31 continue from the parent. This change lives in the runner and capture tools. Initial validation also covers zero-year fixture/save captures.

Tests cover all four cases, repeated bytes, exact checkpoint days, initial stock/coin preservation, coherent aftermath state, save/resume, JSON/text parity, loaded-save labels and incompatible options. The complete capture records eight 40-year runs with matched checkpoints; later economic interpretation uses those records.
