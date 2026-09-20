# Crownless Carriage

Crownless is a travelling company in a changing world. Prepare in town, take the
carriage along the road, explore a dungeon, and bring goods or information home.
The company, its people, its carriage, and the things they carry persist between
these places.

## One world, three views

| Place | What the player does | View |
| --- | --- | --- |
| Town | Meet people, prepare, trade, and return with news or goods. | Authored streets and interiors. |
| Road | Travel with the carriage, inspect its load and team, stop, and choose a way onward. | The same world seen from a camera that keeps the carriage and road readable. |
| Dungeon | Explore, meet occupants, choose a load, and find the way back. | The same visual system seen from inside the place. |

This table is the product contract. The connected acceptance checks and remaining
work live in [One continuous world](docs/design/outside-city-redesign.md).
Town, road, and mine already share lighting and model helpers. Completing the
contract means that their objects also share clear targeting, approach, reach,
and state rules. A change of camera preserves the company's location and goods.

The current playable mine is [Low Silver Pit](docs/design/silverwick-mine-slice.md)
on the Alderwatch–Silverwick road. Its first outing includes a finite hauler load,
a food-for-gold bargain, a cache, useful workers' records, and a report to Jory.
The [journey record](docs/reviews/silverwick-first-haul-2026-09-19.md) shows the
ordinary-control return, sale, save checks, and exact builds.
[Issue #762](https://github.com/atimics/crownlesscarriage/issues/762) collects the
connected acceptance.

## Play and controls

Choose Offline for a campaign saved in this browser, or Online for shared play.
Read the current action labels for the choices available at your location.

- Click the ground to walk. In town, select an object or its card to approach it.
- On the road, use the displayed travel, pace, stop, and boarding actions.
- Underground, click visible floor to walk. W/S move forward/back; A/D turn.
  E uses a nearby mine object. Looking and turning keep the campaign position.
- B opens the Company Book; M opens the map; F5 saves; Escape opens the menu.
- At the mine carriage, use the pack controls to prepare for entry.

The [mine guide](docs/design/silverwick-mine-slice.md) explains its saved state,
entry cost, and current controls. The pilot road supports carriage inspection,
reachable boarding, connected junction choices, and reversal from its saved
physical position.

## Build and check

Use CMake 3.24 or newer and a C toolchain. The native presets fetch raylib during
configuration and enable strict warnings.

```sh
cmake --preset play
cmake --build --preset play
ctest --preset play
```

The macOS executable is
`out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage`.
Other native builds place `crownless_carriage` in `out/build/play`.
The `development` and `release` presets use the same build and test pattern.

For a browser build, activate Emscripten and run:

```sh
emcmake cmake --preset web
cmake --build --preset web
python3 -m http.server 8000 --bind 127.0.0.1 --directory out/build/web/site
```

Open `http://127.0.0.1:8000`. See the [web workflow](.github/workflows/web.yml)
for the browser regression suite and its Playwright setup. Native tests,
browser tests, rendered inspection, and a physical-phone walkthrough each
provide distinct evidence.

## Code map

- [Simulation](src/sim): campaign state, movement decisions, people, goods, and costs.
- [Persistence](src/persistence): saves, journal replay, and schema migration.
- [Local renderer](src/client/cc_local3d.c): shared lighting, models, and scene drawing.
- [Client](src/client/main.c): input, views, and the town/road/mine transitions.
- [World](src/world): route geometry and bounded world presentation.
- [Tests](tests): simulation, input, persistence, and browser checks.

## Images

<img width="1536" height="1024" alt="24009e35-f498-40d1-a134-7f8f67433309" src="https://github.com/user-attachments/assets/43cec7bf-e28e-4915-bbe4-f6ab454aa301" />

<img width="1536" height="1024" alt="image" src="https://github.com/user-attachments/assets/0c762c6a-95a1-4fc6-82fb-b49dae396e2d" />
<img width="1536" height="1024" alt="image" src="https://github.com/user-attachments/assets/3163e010-3569-4927-ac42-7d291db24296" />
