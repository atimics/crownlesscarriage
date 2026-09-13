# Road site clearing

Issue #390. Stacked on the character-history foundation in #501.

At any of the 24 roadside stops, the player can clear the blocker with the action card, a world click, or `road clear` in the text interface while at the stop. The shared-world command is `clear_road_site`.

| Blocker | Required cargo | Consumed cargo | Time | Condition gain |
| --- | --- | --- | --- | --- |
| Fallen tree | 1 Tool, 1 Wood | 1 Wood for supports | 1 watch | 3 |
| Rocks | 2 Tools | 2 Tools | 2 watches | 6 |

Tools include the axe used on a fallen tree. Each work watch advances world time and adds three condition points, capped at 100. Clearing sets the blocker to NONE and opens the site. The carriage stays at its current road position. The player can then camp or continue. The renderer reads the live accessible state before drawing a blocker.

Schema 62 / generator 25 uses the existing saved blocker, accessible and condition fields. SQLite layout stays 30. Schema 61 saves retain their contents during migration. Older journals use their original command rules; clearing becomes available after migration.

Validation:

- Strict Debug headless build and 73 CTest checks passed.
- All 24 sites, both travel directions: exact material cost, midnight crossing, gradual condition gain, event wording, unchanged road position, journal replay and direct saved field checks.
- Repeated work, missing materials, wrong target, out-of-reach commands and pre-62 commands preserve the state on refusal.
- A schema-61 save preserves its old hash through migration and supports clearing afterward.
- Strict full client build passed. `--test-world-cards` exercises both action cards and world clicks, compares final state, and reloads the journal.
- Shared-world bridge and server tests cover command routing, two-player state, retry handling and reload.

Site production and road-house trading have their own backlog issues. This change supplies their persistent access state.
