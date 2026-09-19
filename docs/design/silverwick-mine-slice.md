# Silverwick mine: first playable level

The Low Silver Pit branch lies on the Alderwatch–Silverwick road. Travel stops at the branch. Choose the left or right turn shown for the current direction, or continue along the road.

The branch opens a small mine yard. The carriage stays beside the road. Pack Bread or Meat beside it, then walk north to the timber doorway. Walk through the doorway to enter Mine Mouth in first person.

Both places use shared world lighting and model helpers from the town renderer. The yard has a composed exterior camera. Underground, the camera follows the saved tile at eye height and looks in a local cardinal direction. The company model remains visible in the yard. Ground clicks use the camera for the current view. The [continuous-world contract](outside-city-redesign.md) defines how these views and their interactions connect.

## Controls

- Yard: WASD, arrow keys, or a ground click walk.
- Underground: W/S or up/down step forward/back; A/D or left/right turn. Click visible floor to walk through observed space.
- Click a visible object to approach that object. Use its named action when within reach. A floor click remains a movement choice.
- E or Use: interact with a nearby doorway, bar, or survey.
- Beside the carriage, select a good and quantity, then choose Pack or Unload. The panel shows the pack, carriage, and free capacity.
- Use the displayed Take, Cache, and Recover actions at their reachable holders.
- F5: save the campaign and current position.
- Escape: open or close the pause menu.

The pack holds eight goods, including supplies and recovered goods. Entry consumes one ration. Explore six rooms joined by narrow passages. The western store provides a loop around the barred middle passage. The workers' survey is in the eastern records room. The lower stair marks the future connection to Lamp Hall.

The source in the Lower Passage begins with eight Iron, three Raw Gold, and two Gems. Each transfer changes that finite source. The Rope Store cache holds seven goods and persists between visits. Select the quantity that fits, and inspect the remaining load before choosing another trip.

## The Lower Passage haulers

Two haulers guard the source. Their offer is two packed Bread for one Raw Gold.
Pack at least three Bread before a first descent: entry spends one, leaving two
for the offer. The bargain spends the Bread and transfers the Gold together. It
settles once, and the source keeps its remaining goods.

The western passages provide a physical way around the barred route. Exploring
that route keeps the choice of returning to the haulers. Walking out is also a
valid end to the visit.

A local company can choose Contest to use the existing combat controls. Target
selects a hauler, Strike attacks, and Break contact returns to exploration at the
current mine position with the injury retained. The fight and its result persist
with the local scene. Shared companies can bargain and explore; the displayed
contest guidance directs that fight to local play.

Return through the entrance and walk to the carriage. Transfer the chosen goods, then choose Board to resume the same road journey. Remaining supplies return to the carriage. The source, cache, survey, and opened bar remain saved for later visits.

On a narrow portrait screen, the scene appears above readable action buttons. Open Scene details for the full scene text. The save footer reports the browser save result.

## Information and the town return

In Silverwick, ask Jory Fen about his known mine concern. The shift board offers
a dated document when that lead needs a source. The lead names the Low Silver
Pit turnout on the Alderwatch–Silverwick road and points to the workers' records.

Read the workers' records in the eastern room for the document's route claim.
Walking the western passage records the company's own observation. The Company
Book keeps the source and date in Mine notes. Its visible page controls also
work by touch.

Return to Jory and choose to tell him what the company found. A tracked partial
load and an attributed route account produce their own responses. Oren's store
handles an optional sale through the ordinary quantity and payment controls.
The report and sale remain separate choices. The Book keeps the return account
after a sale or reload.

## Shared rules and saves

Facing is local camera state. Turning is free. A fresh descent faces east into the level; the yard uses its fixed camera. Reload restores the saved mine tile and resets local facing to the phase default. Submitted movement already names a cardinal direction, so a different client camera cannot reinterpret it.

The simulation owns position, collision, carried goods, elapsed time, opened passages, and the road anchor. The screen, text controls, and shared company commands use those rules. Each movement carries the current mine revision so a repeated request cannot take another step.

Schema 59 introduced the mine visit and pack. Schema 103 adds finite source and cache custody. Schema 104 adds the hauler encounter, and local scene version 9 preserves its combat state. Older saves verify their original hash and replay before migration. Journal replay restores the same position, supplies, goods, and holder identities.

Text controls: `mine visit`, `mine look`, `mine move north`, `mine use`, `mine pack Bread`, `mine unpack Bread`, and `road pass`.

## Delivery

[#797](https://github.com/atimics/crownlesscarriage/issues/797) completes visible target picking and deliberate use. [#484](https://github.com/atimics/crownlesscarriage/issues/484) exposes pack/unpack quantities and connects the finite load and cache. [#763](https://github.com/atimics/crownlesscarriage/issues/763) binds the hauling party to that load. [#764](https://github.com/atimics/crownlesscarriage/issues/764) connects attributed information and the town return. [#762](https://github.com/atimics/crownlesscarriage/issues/762) owns connected ordinary-input acceptance.

## Scope

This slice builds the Silverwick road branch, surface yard, zone 01, and its hauling encounter. The remaining 23 levels, Hollowbarrow entrance, and route to the dragon cave follow the wider Underroad design. The existing abstract Underroad expedition remains available through its earlier commands while the mapped levels are built.

## Checks

`silverwick_mine_roundtrip` covers both road approaches, parking, pack accounting, collision, the survey, the bar, save/load, journal replay, old-save migration, shared commands, and text commands. `silverwick_mine_input` covers the branch card, keyboard movement, food packing, the view change at the doorway, F5, and the return to the road. Native capture options are `--capture-mine-yard filename.png` and `--capture-mine-level filename.png`. `--capture-mine-menu filename.png` captures the pause menu over the underground view.

The schema 58 migration fixture was written with naming PR revision `6e3436940a667461821b928269224cd6f33f08fc`. Seed 42 has all 24 death dates set to day 2, followed by one flushed journal day. Its replay hash is `1053288272468887993`. The standalone SQLite fixture verifies the original naming replay before schema 59 adds an empty mine visit.
