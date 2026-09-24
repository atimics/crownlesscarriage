# README screenshots — 24 September 2026

These six images come from the native game built at
`08fe90581aa40eb1320662df2284e88fd22fa9e8`. They show the current town buildings,
skies, road choices, and mine encounter.

The game's built-in capture commands set up each scene. Town positions, the road
stop, and the haulers' offer are staged for these pictures. The capture presets
also set the displayed day, purse, and pack contents. Each PNG is the complete,
unchanged game frame. The town overview presets use the scene review controls;
Thornford uses the player interface.

| Image | Scene |
| --- | --- |
| [Thornford](thornford.png) | Threshing Green with the player controls |
| [Gloamgate](gloamgate.png) | Market Circle |
| [Silverwick](silverwick.png) | Foundry Terrace |
| [Alderwatch](alderwatch.png) | Keep |
| [Road](road.png) | Low Silver Pit turn-off on the Alderwatch–Silverwick road |
| [Mine](mine-haulers.png) | Lower Passage haulers and the food-for-gold offer |

The [manifest](manifest.json) records the source revision, native binary hash,
capture arguments, image sizes, and image hashes. Reproduce the scenes from
that revision with `cmake --preset play`, then
`cmake --build --preset play --target crownless_carriage`. Run the executable
from the repository root with each argument list in the manifest. On macOS,
the executable is inside `out/build/play/crownless_carriage.app/Contents/MacOS/`.
Keep the output directory in place before running the captures.

The [earlier Silverwick journey](../../reviews/silverwick-first-haul-2026-09-19.md)
records the ordinary-control trip, bargain, return, sale, and save checks from
19 September. The new images document the current presentation.
