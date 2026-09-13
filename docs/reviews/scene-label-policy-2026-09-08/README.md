# Scene label policy review

Partial work for #222, based on #559 (c50176f).

Adventure play now applies the shared ambient-label policy across renderer scenes. The renderer receives the adventure UI state for roads, sites, open world, and interiors. Scene facts remain available through the action controls.

## Visual checks

The parley comparison shows the floating player name, captain name, route, and key hint removed. The approach and return cards remain visible. The road fork retains its action signs and route cards at 1040 pixels wide. The conversation capture retains Mara's speech and response buttons at 1040 pixels wide with large text.

- [Parley before](parley-before.png)
- [Parley after](parley-after.png)
- [Road fork](fork-after.png)
- [Large text conversation](conversation-after.png)

## Validation

Strict Release native build passed. All 119 tests passed, including the existing client input tests. Static analysis passed with its existing reviewed baseline. Eight before/after captures completed; the road, fork, parley, and conversation views were inspected.

## Follow-up scope

Issue #222 still needs speaker-linked caption layout, edge and face avoidance, long-line and simultaneous-speaker evidence. This draft addresses the shared ambient-label policy. Site, open-world, and warehouse coverage follows the common DrawLabels guard; dedicated ordinary-play captures for those views remain follow-up evidence.
