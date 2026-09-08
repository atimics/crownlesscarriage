# Plain archive volumes

This draft addresses the useful-work requirement in #446. It follows #620.

Ordinary seed-42 and seed-24301 recruits reached the archive and completed training, then waited for gold and gems to make a book. Issue #446 requires bounded work with real food, paper, tools, and paid labor. Schema 84 lets a new scribe bind a plain volume from the reserved paper and first-task food. Later work at that staffed archive also produces plain volumes from its usual paper, food, and tool wear.

Each volume uses the existing treasure ID, owner, maker, location, craft work, value, and creation day. Plain volumes contain zero gold and zero gems and start at value one. Precious books keep their existing recipe. The earlier schema-83 supply plan still buys precious binding goods for its original recipe; the schema-84 plain recipe uses wheat, paper, and tools through shared freight.

The first account comes from the recruit's held story. Its event names the person, source account, book, and local seat. Existing local books can receive an index page. Ruined and destroyed plain volumes retain valid material fields while physical lore falls. Other treasure types retain their precious-material requirements.

Extra recording exposed a full-ledger case where a new theft's omen could remove its tribute parent. Schema 84 keeps the active hoard's five-event causal chain, matching the existing motive reader's bound. The dragon cycle regression now passes with the fuller archive.

## Evidence

- Staff tests verify plain first-task cost, exact money and rare-material conservation, real paper and wheat consumption, local custody, save loading, destruction, ruin, and invalid ordinary or partly precious objects.
- Later named work records the scribe's held account with local gold and gems at zero.
- The ordinary seed-42 journal reaches a named appointment on day 19 and replays to the same world hash.
- Earlier supply and dispatch tests run under schema 83. A legacy save fixture now advances under its declared schema before saving.
- `legacy-probe.c` compares schema 83 against parent 225607a at 41 annual checkpoints for each of two seeds.
- `world-probe.c` measures daily 40-year runs at schemas 83 and 84 for seeds 42 and 24301, with annual world validation. Results are in `world-measurements.json`.

## Final results

| Seed | Schema | Appointments | Days with named staff | Peak named staff |
| --- | --- | --- | --- | --- |
| 42 | 83 | 0 | 0 | 0 |
| 42 | 84 | 1 | 2285 | 1 |
| 24301 | 83 | 0 | 0 | 0 |
| 24301 | 84 | 2 | 11720 | 2 |

Each trial spans 14,600 daily advances. Both earlier-schema comparisons match all 41 annual hashes, for 82 matches total. Initial inherited staff remain separate from these named counts.

Code head b37a432 passed the strict headless build and all 125 tests, the native build and all 14 archive/persistence/dragon checks, and static analysis with one reviewed baseline entry. The ordinary appointment journal reaches one new named scribe alongside the inherited staff on day 19 and matches the replay hash. Generator 25, SQLite schema 32, and the 184448-byte simulation structure remain unchanged.

These are local results. Remote CI is tracked on PR #621. Viable seat selection, sustained institutional recovery, and player controls remain further issue work.
