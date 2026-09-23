# Typed fact selection pilot

Fresh 4,945,153-parameter model trained to choose a typed fact reference from a
bounded fact table. The question names an event and a role; the model picks the
matching fact or defers; the renderer owns the wording. This is the fact
analogue of the v3 meaning choice loop.

- Format: `crownless-fact-selection-v1`
- Run: 4,000 CPU steps, batch 32, 256 worlds, seed 19, 337 s
- Reference: `models/dialogue-syntax/model.ccv2` (`321431dc…`)
- Export SHA-256: `5bba76ae50ebd4d155cabfecc89d13ab8bda701fbc513c53cd715845f086dfe7`

| Check | Development | Test |
| --- | ---: | ---: |
| Exact choice | 1068/1095 (97.5%) | 375/381 (98.4%) |
| Different-fact exact | 858/882 (97.3%) | 305/307 (99.3%) |
| Defer exact | 480/485 (99.0%) | 170/170 (100%) |
| Valid choice | 1095/1095 | 381/381 |

Best single-attribute baseline (match role, ignore event) is 77.2% on test; the
model reaches 98.4%. See [`tools/dialogue/FACT-SELECTION.md`](../../tools/dialogue/FACT-SELECTION.md)
for the design, gates, baselines and limitations.

`manifest.json` pins the reference, tokenizer, sources and dataset hashes.
`development-summary.json` and `test-summary.json` are the aggregate results
without the per-row records. The exported model and full records stayed in the
local run directory.
