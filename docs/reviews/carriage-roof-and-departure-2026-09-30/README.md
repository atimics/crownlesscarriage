# Carriage roof rack and town-departure team

Review of the carriage and pony presentation, 30 September.

## Roof rack turned sideways

`DrawRoadCarriage` drew the carriage asset at `yaw - 90°` (the asset's long
axis is x) but drew the roof rack and its cargo at the road `yaw`. The rack is
laid out in the asset frame, so on every road, open-world, street-convoy,
fork, site and mine-yard carriage it sat across the roof. It now turns with
the asset. The parked street carriage (`DrawCarriage3D`) already used one yaw
for both and is unchanged.

`roof-rack-before-after.png`: `--capture-travel`, before (top) and after.

## Pony skipping on departure

The drive from the stable to the town gate (`road_choice_active`) and the
remote-site drive (`site_travel_active`) return from input before the general
world update. Their team targets were published every frame, but nothing
stepped the gait controllers. The rigs stood frozen in one pose, drawn where
the controller was last stepped, until the carriage was 3 units away; then
the controller was re-seated at the carriage and froze again. That is the
spot-to-spot clipping and the hooves held off the ground.

`StepLocalRoadTeam` now steps the local world and the gaits in both branches,
also while the carriage is stopped, so a halted team settles. The open world
steps its own team and is skipped.

`--test-town-departure` now drives the whole street departure through
`UpdateTownDepartureFrame` (the same function input calls), publishing targets
between frames at mixed frame rates with a stop in the middle. It fails when
the team's controller loses its pose, trails by more than 2.5 units, or
trails by more than 0.5 on average. Without the fix it fails at frame 59
(lag 2.59); with it the lag is at most 1.33 (a sharp corner in the town
path) and 0.28 on average.

## Not changed here

- The parked team stands beside the carriage at the stable
  (`DrawStableHorseTeam`), and is drawn hitched once departure starts. That
  switch is still a cut, not a walk into harness.
- At sharp corners of the authored town path the hitch point swings about
  1 unit in one frame because the team is placed from the smoothed heading.
