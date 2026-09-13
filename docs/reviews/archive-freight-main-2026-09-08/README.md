# Archive and freight integration

This draft combines #586, #587, and #588 with main at 85bfb702ba233585d4e3359273f048de1cae7b2f. It supports archive recovery (#446), archive transport (#248), world reports (#266), and simulation boundaries (#260).

Archive queries expose the existing funding donors, amounts, blockers, and recovery dates. The recovery code uses these shared queries. Freight path and capacity rules have their own module and controlled tests. Carriage reports include owner, target, waiting dates, and readable mode names. Shipment reports identify their retained snapshot meaning.

The integration keeps main's quest cast tests alongside the new archive funding and trade path tests. Schema 74 and generator 25 remain the current versions.

## Validation

- Strict release headless build passed.
- All 114 tests passed, including archive funding, trade paths, quest cast continuity, persistence, JSON reports, and runner resume.
- Static analysis passed with one reviewed baseline item.
- Two 40-year runs, seeds 42 and 0x5eed0001, match main at all 82 checkpoints for every existing JSON field, including simulation hashes. The comparison removes only the added archive funding report, carriage context fields, and shipment semantics label. See parity.json.

The component review folders contain their focused fixtures and earlier comparisons. This folder records the combined result against schema 74 main.
