# Carriage yard turn

Follow-up to `../carriage-roof-and-departure-2026-09-30/`, which left two cuts.

## The cut between the stable and the harness

The carriage parks facing +z with its team at the stable rail in front of it,
but every town's carriage lane leaves the parked spot heading +x. Departure
started by snapping the carriage and team about 85 degrees onto the lane and
dropping the stable ponies for a hitched pair in another place; arrival ended
heading along the lane and cut to the parked pose.

- The stable spots are now exactly the hitched spots in front of the parked
  carriage, and `CcLocalCarriageTeamHandOverInternal` moves the gait
  controllers between the stable and harness slots, so the same ponies keep
  their stance at the switch. A lone pony uses the left slot in both.
- `SetConvoyTownPose` adds a yard turn at the parked end of the drive: the
  carriage creeps 2 units along the lane while it swings between the parked
  heading and the lane heading. It is stateless in progress, and the drive's
  length counts the arc the team walks.
- The pole pivots with the carriage, so its team end moves sideways. The
  ponies turn to walk that arc (`team_turn_yaw`) and turn back to the pole
  as it straightens. Without this they walked about 1.4 rad off their facing
  through the turn; now 0.30.

`arrival-end-before-after.png`: `--capture-town-arrival 0 1.0`. Before, the
drive ended facing along the lane, and parking cut to the side-on pose. After,
it ends in the parked pose. `arrival-yard-turn.png`: progress 0.92, 0.94,
0.96, 0.98, 1.0 and the parked carriage.

The coach hitch crossbar in front of the parked carriage is lowered from 0.92
to 0.58 so the traces clear it when the carriage is hitched facing the post.

## Swinging at corners

The heading was a smoothed copy of the current lane segment's direction, so
at a kink the carriage pivoted late and the team, 5.55 units ahead, swung
sideways about 1 unit in a frame. The carriage now faces the point a
hitch-length ahead on the lane, so the team walks the lane itself.

## Tests

`--test-town-departure`, for every town and both directions:
- the drive starts or ends at the parked pose;
- heading changes at most 0.03 rad and the team point moves at most 0.25 per
  1/2000 of the drive (the old drive failed at progress 0);
- the team walks within 0.45 rad of its facing through the yard turn;
- through the whole street departure, as before, the team never loses its
  pose: largest lag 0.46 (was 1.33 at a corner), mean 0.27.

The full play suite passes, 256/256.

## Not changed

The parked carriage noses into the coach hitch and the stable rail, so it
cannot leave forward on a wide arc; the yard turn pivots it in place with a
short creep. The right pony stands close to the hitch's spare wheels, as it
did before.
