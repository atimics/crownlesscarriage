# 0014 — Use one finite, streamed world map

**Status:** Accepted

## Context

The strategic simulation already creates settlements, routes, dungeons,
goblin territory, and the dragon roost from one seed. The 3D client used to
show those places as separate fixed-size scenes. Position inside a town or a
road scene had no shared meaning, and each scene kept its whole terrain in
memory.

The game does not need an endless world. It needs one world whose roads and
places agree with the strategic simulation, and whose memory cost stays
bounded as the player moves.

## Decision

Every campaign builds a finite world manifest from the simulation seed. The
manifest places all settlements, curves every strategic route between them,
and places the Underroad, goblin trail, and dragon roost. The same coordinates
are used by terrain, movement, travel, rendering, and local-session saves.

Terrain is generated in 32 metre chunks. Each chunk has 17 by 17 shared-edge
height samples. A stream owns at most a 5 by 5 ring around the player. Moving
the focus marks new chunks as pending, generates a small number per frame, and
evicts the least recently used chunk outside the ring. The renderer mirrors
the same ring on the GPU and drops a mesh when its source chunk is evicted.

Terrain is a pure function of the world manifest and coordinate. A missing or
pending chunk can therefore use the direct generator without changing the
result. Settlement plateaus and roads are part of that function, so chunk
edges remain continuous.

The strategic simulation and append-only journal remain authoritative.
Procedural terrain is derived data and is never written into the campaign
save. The small client session stores the player's world coordinate. Older
scene-local sessions are converted relative to their saved settlement.

## Consequences

- Normal play presents towns and roads in one continuous coordinate space.
- The world is finite and deterministic for a campaign seed.
- CPU terrain memory and GPU terrain meshes have fixed upper bounds.
- The wide open-world view is carriage mode. The carriage, horses, cargo,
  heading, and terrain support follow the same curved route used by the
  simulation.
- Town play stays in the same map at a closer camera scale. Choosing a road
  eases the camera out toward the carriage; arrival eases it back into the
  destination town instead of cutting to another road scene.
- Existing captures and focused scene tests can continue using the legacy
  local layouts while the remaining interiors and sites move into the world.
- Changing the generator requires a deliberate generator-version migration.

## Rejected alternatives

- Keeping one complete terrain mesh resident for the whole map
- Generating an endless world unrelated to the strategic map
- Saving every generated height sample
- Loading a separate 3D scene for every settlement and road segment
