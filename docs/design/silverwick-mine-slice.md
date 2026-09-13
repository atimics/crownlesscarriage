# Silverwick mine: first playable level

The Low Silver Pit branch lies on the Alderwatch–Silverwick road. Travel stops at the branch. Choose the left or right turn shown for the current direction, or continue along the road.

The branch opens a small mine yard. The carriage stays beside the road. Pack Bread or Meat beside it, then walk north to the timber doorway. Use the doorway to enter Mine Mouth in a mostly overhead 3D view.

Both places use the town renderer, world lighting, and character model. The yard has two fixed King's Quest style compositions, covering the carriage court and mine doorway. Each dungeon room has a fixed high camera with visible wall depth. Ground clicks use the camera for the current view.

## Controls

- WASD, arrow keys, or the ground: walk.
- E or Use: interact with a nearby doorway, bar, or survey.
- P or Pack food: move one Bread or Meat from carriage to pack.
- F5: save the campaign and current position.
- Escape: open or close the pause menu.

The pack holds eight goods. Entry consumes one ration. Explore six rooms joined by narrow passages. The western store provides a loop around the barred middle passage. The workers' survey is in the eastern records room. The lower stair marks the future connection to Lamp Hall.

Return through the entrance, walk to the carriage, and use it to resume the same road journey. Remaining supplies return to the carriage. The survey and opened bar remain saved for later visits.

## Shared rules and saves

The simulation owns position, collision, carried goods, elapsed time, opened passages, and the road anchor. The screen, text controls, and shared company commands use those rules. Each movement carries the current mine revision so a repeated request cannot take another step.

Save schema 59 adds the mine visit and pack. Schema 57 and 58 saves retain their old hash during verification, then upgrade with an empty visit. Journal replay restores the same position and supplies.

Text controls: `mine visit`, `mine look`, `mine move north`, `mine use`, `mine pack Bread`, `mine unpack Bread`, and `road pass`.

## Scope

This slice builds the Silverwick road branch, surface yard, and zone 01. The remaining 23 levels, Hollowbarrow entrance, goblin encounters, and route to the dragon cave follow the wider Underroad design. The existing abstract Underroad expedition remains available through its earlier commands while the mapped levels are built.

## Checks

`silverwick_mine_roundtrip` covers both road approaches, parking, pack accounting, collision, the survey, the bar, save/load, journal replay, old-save migration, shared commands, and text commands. `silverwick_mine_input` covers the branch card, keyboard movement, food packing, the view change at the doorway, F5, and the return to the road. Native capture options are `--capture-mine-yard filename.png` and `--capture-mine-level filename.png`. `--capture-mine-menu filename.png` captures the pause menu over the underground view.

The schema 58 migration fixture was written with naming PR revision `6e3436940a667461821b928269224cd6f33f08fc`. Seed 42 has all 24 death dates set to day 2, followed by one flushed journal day. Its replay hash is `1053288272468887993`. The standalone SQLite fixture verifies the original naming replay before schema 59 adds an empty mine visit.
