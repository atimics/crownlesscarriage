# Crownless Carriage

Crownless is a game about a travelling company in a changing world. Walk through
town streets, meet the people who live there, and prepare your carriage for the
road. Follow a lead into Low Silver Pit, bargain with its haulers, and bring
goods or news home.

[Play in your browser](https://crownless.ratimics.com).

<img width="800" alt="Thornford gameplay view: timber granary, townspeople, harvest board, and talk and trade controls under an amber sky" src="docs/screenshots/2026-09-24/thornford.png" />

*Thornford's Threshing Green. In-game captures below are from the 24 September
2026 native build. [Capture notes and scene setup](docs/screenshots/2026-09-24/README.md).*

## Gameplay today

**Walk through towns with their own character.** Thornford has timber granaries
and farm lanes. Gloamgate gathers around its market fountain. Silverwick climbs
past workshops and ore yards. Alderwatch stands behind a fortified bridge and
keep. Skies, clouds, and distant hills frame the streets as light and weather
change.

| Gloamgate — Market Circle | Silverwick — Foundry Terrace | Alderwatch — Keep |
| --- | --- | --- |
| [<img width="260" alt="Gloamgate's market fountain and timber shop fronts beneath a warm sky" src="docs/screenshots/2026-09-24/gloamgate.png" />](docs/screenshots/2026-09-24/gloamgate.png) | [<img width="260" alt="Silverwick's workshop fronts along the Foundry Terrace road" src="docs/screenshots/2026-09-24/silverwick.png" />](docs/screenshots/2026-09-24/silverwick.png) | [<img width="260" alt="The traveller at Alderwatch's tall stone keep and gate" src="docs/screenshots/2026-09-24/alderwatch.png" />](docs/screenshots/2026-09-24/alderwatch.png) |

**Travel with the company.** Choose a destination and the carriage sets off.
Stop to inspect the team, make camp, or choose a side road. Your supplies,
carriage condition, and pace shape the trip.

| Road — Low Silver Pit turn-off | Mine — the haulers' bargain |
| --- | --- |
| [<img width="400" alt="Carriage and pony stopped beside the Low Silver Pit hoist, with Enter, Travel on, and Site options controls" src="docs/screenshots/2026-09-24/road.png" />](docs/screenshots/2026-09-24/road.png) | [<img width="400" alt="Two goblin haulers in the mine offer one Gold for two Bread, with Bargain and Contest actions" src="docs/screenshots/2026-09-24/mine-haulers.png" />](docs/screenshots/2026-09-24/mine-haulers.png) |

The playable town–road–mine trip gives you a small journey with lasting choices:

1. **Prepare in Silverwick.** Buy food and ask Jory Fen about Low Silver Pit.
2. **Take the road.** Travel toward Alderwatch, inspect your carriage, and take
   the mine turn-off. Pack supplies beside the carriage before walking inside.
3. **Explore the mine.** Entry costs one ration. Find the workers' records,
   trade two Bread for one Raw Gold, and use the Rope Store cache. Your pack
   holds eight goods, so supplies and treasure share the space.
4. **Bring something home.** Unload at the carriage, board, and return to town.
   Sell your gold to Oren or tell Jory what you found. Save and resume with your
   goods, position, and Company Book notes intact.

The [19 September playthrough](docs/reviews/silverwick-first-haul-2026-09-19.md)
includes the road journey, food-for-gold bargain, cache recovery, return report,
sale, and save checks. The [mine guide](docs/design/silverwick-mine-slice.md)
explains the rooms, supplies, and choices in more detail.

## Play and controls

Choose Offline for a campaign saved in this browser, or Online for shared play.
Read the current action labels for the choices available at your location.

- Click the ground to walk. In town, select an object or its card to approach it.
- Choosing a destination starts road travel. Space uses the displayed **Stop**
  or **Travel** action. Open **Road options** while stopped to camp, step down,
  or turn back. At a landmark, choose **Travel on** or the named side road.
- Underground, click visible floor to walk. W/S move forward/back; A/D turn.
  E uses a nearby mine object. Looking and turning keep the campaign position.
- B opens the Company Book; M opens the map; F5 saves; Escape opens the menu.
- At the mine carriage, use the pack controls to prepare for entry.
- Open **Settings & controls** to change text size, contrast, motion, or key bindings.

The [mine guide](docs/design/silverwick-mine-slice.md) explains its saved state,
entry cost, and current controls. The pilot road supports carriage inspection,
reachable boarding, connected junction choices, and reversal from its saved
physical position.

## Roadmap

These concept images show longer-term ideas for Crownless. Click an image to
open it at full size.

<p>
  <a href="https://github.com/user-attachments/assets/43cec7bf-e28e-4915-bbe4-f6ab454aa301"><img width="240" alt="Roadmap concept 1: planned visual direction for Crownless" src="https://github.com/user-attachments/assets/43cec7bf-e28e-4915-bbe4-f6ab454aa301" /></a>
  <a href="https://github.com/user-attachments/assets/0c762c6a-95a1-4fc6-82fb-b49dae396e2d"><img width="240" alt="Roadmap concept 2: planned visual direction for Crownless" src="https://github.com/user-attachments/assets/0c762c6a-95a1-4fc6-82fb-b49dae396e2d" /></a>
  <a href="https://github.com/user-attachments/assets/3163e010-3569-4927-ac42-7d291db24296"><img width="240" alt="Roadmap concept 3: planned visual direction for Crownless" src="https://github.com/user-attachments/assets/3163e010-3569-4927-ac42-7d291db24296" /></a>
</p>

The goal is one continuous world across town, road, and dungeon. The
[continuous-world plan](docs/design/outside-city-redesign.md) covers shared
movement and interaction rules. The [mine plan](docs/design/silverwick-mine-slice.md#scope)
sets out the wider Underroad beyond the first playable level.

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
