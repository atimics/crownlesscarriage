# Four authored towns — architecture and composed walks

Implements the approved four-town concept board in the existing procedural
storybook renderer. These are connected town scenes, not painted backgrounds,
new quest instances, or a replacement rendering engine.

| Town | Authored views | Architectural identity |
| --- | --- | --- |
| Thornford | River Crossing; Threshing Green; Granary Rise; Cartwright Yard | Raised timber food stores, broad low threshing barns, mill gable and wheel-faced cartwright sheds among the existing river and field lanes. |
| Gloamgate | Market Circle; Archive Steps; Cloth Yard; Coach Court | An octagonal exchange above the market, a stepped archive gable with paired tall windows, close merchant gables and hanging fabric fronts. The fountain remains the street anchor. |
| Silverwick | Foundry Terrace; Company Store; Workers' Lane; Ore Wagon Yard | Sawtooth workshop roofs, iron headframes, industrial stacks instead of military compound towers, narrow chimney-backed worker houses and the stopped company clock. |
| Alderwatch | Contested Bridge; Muster Spine; Keep; Lower Bailey | Low long barracks rather than a turret on every building, buttressed fronts, crenellated public hall and a single dominant keep. Wider bridge/keep views reveal the fortress's geography. |

## Implementation boundaries

`authored_town_architecture.inc` supplies four distinct building families through
`DrawSettlementBuilding`, shared by the street and wider-world renderer. The
original foundations, plot transforms, collision footprints, service-door
centres, simulation-derived stock and repair presentation, and foreground reveal
masks remain in charge. No saved positions, inventory, economy or schema change.
Roofs remain inside the existing building-height-plus-three envelope. Decorative
facades are not new interiors, walkable galleries or new archive services.
The mill wheel and existing terrain stages remain live. Fixed hanging shop cloth
is decoration, not saleable inventory; industrial stacks do not invent production
or simulated emissions. Rosespire and Hollowbarrow retain their architecture.

The sixteen principal views occupy existing camera slots. Gloamgate's landmark
view moves from the customs keep to the archive. The fortress receives genuine
establishing views. Cloth Yard and Workers' Lane use wider street compositions;
other close interaction and carriage-yard bounds stay unchanged. Streets and
named pedestrian routes remain physical and save-compatible.

## Rendered review

The first native review binary came from PR head `74170626`, tested as merge
revision `5af13a40`. All sixteen principal views and nine condition views were
captured locally with that binary. They are staged views, not ordinary play.
The inspection exposed clipped rooflines at the keep, tight cloth/worker views,
and the mine compound's inherited military towers. The following code change
widens those compositions and gives Silverwick industrial stacks. The scoped CI
lane recaptures the final revision rather than treating earlier pictures as final.

## Validation and reproduction

`tools/capture_four_towns.py` captures sixteen scenes through the shipped native
client; `--conditions` adds nine fire/recovery/hunger views. It records the tested
checkout revision, commands, PNG dimensions and hashes, rejects stale/missing
captures and duplicate principal images, and retains partial results on failure.
Run it against a binary built from that checkout; a different prebuilt binary
requires its own provenance record. The tool passes relative screenshot names
because the shipped screenshot API prefixes the working directory. Four unit
checks cover named positions, absolute output directories (including spaces),
stale files after a failed process, and invalid/small image receipts.

The scoped PR workflow builds with warnings as errors and runs town, route,
interaction, terrain, renderer and save regressions. Captures still run after a
test failure when compilation succeeded; the workflow does not hide that failure.
Its artifact includes screenshots, logs, a revision receipt and native review
executables. Browser CI remains a separate check; neither CI proves phone comfort.

### Baseline and integration repairs

Before any game code changed, `6bc48a29` built and 21 of 22 selected tests passed.
`local_collision_space` compared a direct-movement preview with a navigation
waypoint count. A direct target has no navigation path. The test now compares
navigation state, compares waypoint counts only for navigated previews, and
requires zero stored waypoints for direct targets. Its movement loop now also
advances a direct exact target; previously it performed zero updates when
`navigation_active` was false. Exact command-point and arrival assertions remain,
and both navigation and exact-target completion are required.

A concurrent main change added pitch and sway to `DrawRoadHorseTeam`. The isolated
pony capture fixture now explicitly supplies zero pitch and sway for its flat-road
take. This is a call-site integration repair, not new carriage behavior.

```sh
python3 -m unittest discover -s tests -p test_four_town_capture.py
cmake -S . -B build-towns -DCMAKE_BUILD_TYPE=Release -DCC_WARNINGS_AS_ERRORS=ON -DCC_BUILD_GRAPHICS_PROBES=ON
cmake --build build-towns --parallel 4
xvfb-run -a ctest --test-dir build-towns --output-on-failure -R 'town|local|carriage|interaction|save|renderer|terrain'
xvfb-run -a python3 tools/capture_four_towns.py build-towns/crownless_carriage --output out/four-town-review --conditions
```

On a desktop with a display, omit `xvfb-run -a`. On macOS use the executable
inside the built `.app/Contents/MacOS/` directory.
