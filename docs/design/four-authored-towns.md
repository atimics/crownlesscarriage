# Four authored towns — architecture and composed walks

Implements the approved four-town concept board in the existing procedural
storybook renderer. These are connected town scenes, not painted backgrounds,
new quest instances, or a replacement rendering engine.

| Town | Authored views | Architectural identity |
| --- | --- | --- |
| Thornford | River Crossing; Threshing Green; Granary Rise; Cartwright Yard | Raised cross-braced food stores, broad low threshing barns, mill gable and wheel-faced cartwright sheds among the existing river and field lanes. |
| Gloamgate | Market Circle; Archive Steps; Cloth Yard; Coach Court | An octagonal exchange above the market, a stepped archive gable with paired tall windows, close merchant gables and hanging fabric fronts. The fountain remains the street anchor. |
| Silverwick | Foundry Terrace; Company Store; Workers' Lane; Ore Wagon Yard | Sawtooth workshop roofs, iron headframes, narrow chimney-backed worker houses and the stopped clock on a severe company facade. |
| Alderwatch | Contested Bridge; Muster Spine; Keep; Lower Bailey | Low long barracks rather than a turret on every building, buttressed fronts, crenellated public hall and a single dominant keep. Wider bridge/keep views reveal the fortress's geography. |

## Implementation boundaries

`authored_town_architecture.inc` supplies four distinct building families through
`DrawSettlementBuilding`, shared by the street and wider-world renderer. The
original foundations, plot transforms, collision footprints, service-door
centres, simulation-derived stock and repair presentation, and foreground reveal
masks remain in charge. No saved positions, inventory, economy or schema change.
Roofs remain inside the existing building-height-plus-three envelope. Decorative
facades are not new interiors, walkable galleries or new archive services.
The mill wheel and existing terrain stages remain live; no decorative stock is
invented. Rosespire and Hollowbarrow retain their existing architecture.

The sixteen principal views occupy existing camera slots. Close interaction and
carriage-yard framing remains bounded. Gloamgate's landmark view moves from the
customs keep to the archive; the fortress receives genuine establishing views
instead of using a close camera for every scene. Streets and named pedestrian
routes remain physical and save-compatible.

## Validation and reproduction

`tools/capture_four_towns.py` captures all sixteen scenes through the shipped
native client; `--conditions` adds nine fire/recovery/hunger views. It records the
exact tested revision, commands, PNG dimensions and hashes, rejects stale/missing
captures and duplicate principal images, and retains partial results on failure.
These are **staged captures**, not an ordinary-input end-to-end walkthrough.

The scoped PR workflow builds with warnings as errors and runs town, route,
interaction, terrain, renderer and save regressions. Captures still run after a
test failure when compilation succeeded; the workflow does not hide that failure.
Its artifact includes screenshots, logs, a revision receipt and native review
executables. Browser CI remains a separate check; neither CI proves phone comfort.

### Baseline finding

Before any game code changed, the native build of `6bc48a29` passed and 21 of 22
selected tests passed. `local_collision_space` failed because its Miller's Bend
assertion compared a one-segment **direct-movement preview** with the navigation
waypoint count. A direct target has no active navigation path. The assertion now
compares navigation state, compares counts for navigated previews, and requires
zero stored waypoints for direct targets. Exact command-point equality and the
subsequent movement/arrival checks remain intact. This corrects the fixture's
expectation rather than changing movement or weakening the destination check.

Run locally:

```sh
cmake -S . -B build-towns -DCMAKE_BUILD_TYPE=Release -DCC_WARNINGS_AS_ERRORS=ON -DCC_BUILD_GRAPHICS_PROBES=ON
cmake --build build-towns --parallel 4
xvfb-run -a ctest --test-dir build-towns --output-on-failure -R 'town|local|carriage|interaction|save|renderer|terrain'
xvfb-run -a python3 tools/capture_four_towns.py build-towns/crownless_carriage --output out/four-town-review --conditions
```

On a desktop with a display, omit `xvfb-run -a`. On macOS use the executable
inside the built `.app/Contents/MacOS/` directory.
