# Funded player repairs at road sites

Related: #259, #391, #393, #396. Base: draft #514 at `87143ff`.

At an open roadside stop, the company can restore up to ten condition points in two watches. The carriage must hold two Tools and one Wood. One Tools bundle and one Wood are consumed; one working Tools bundle remains. The same common production recipe supplies the preview and execution costs. Full repairs near condition 100 pay the declared cost and stop at 100.

The action is available through the site card, its world interaction, text command `road repair`, and shared command `repair_road_site`. The preview reads the actual current stop and carriage cargo. The work preserves route, direction, carriage progress and journey progress. World time advances for both watches, including midnight updates. Its event reports the actual gain and final condition.

Clearing and repair are separate actions with separate costs. An open mill below condition 50 can be repaired above that threshold. Its next weekly batch then consumes the Wheat already in its local store through the common site-production path.

## Persistence

Simulation schema 66 introduces command 56. Generator 25, SQLite schema 31 and the saved field layout remain unchanged. Schema 65 saves retain their site condition, access, stores and travel anchors on upgrade. Older journals replay under their recorded rule version before upgrade. The new command requires schema 66. Existing saved-field tests cover the site condition and cargo fields used by this action.

## Evidence

- `roadside_camp_choices`: repairs at all 24 sites, both road directions, exact material/time costs, preview hash stability, direct save, journal replay, full-condition cap, legacy upgrade and atomic refusal cases.
- The same test drives `road repair` through the text interface, then records one weekly mill batch: two Wheat consumed and two Bread produced after condition rises from 44 to 54.
- `world_card_input_parity`: the site card and world target both apply the repair with the same saved result.
- Shared-world tests: a second player repairs the cleared site, duplicate delivery returns the same result, and reload preserves the result.

Funded unattended maintenance and wear remain the next part of #393. This PR supplies the player repair path for the working-site pilot.
