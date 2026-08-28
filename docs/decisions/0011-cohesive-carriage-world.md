# 0011 — One cohesive world through local maps and visible roads

**Status:** Accepted

## Context

Showing one outgoing route at a time made forks feel like a menu spread across
several fake junctions. Keeping dungeons and major lairs on the settlement map
also made the world feel compressed instead of connected.

## Decision

At a route choice, every road leaving the current place is visible as physical
geometry at one local junction. The player turns toward a branch and takes it.
Roads beyond that sightline remain unknown unless described by a physical map.

Settlements, dungeons, goblin caves, and dragon caves use separate local maps.
Short, seeded wilderness roads join minor sites to their anchor settlement.
Regional travel uses a low camera behind the carriage and may speed up time,
but it never becomes a top-down world view. Only hand-drawn map objects may
show the whole world.

Local maps share kingdom colors, terrain rules, road materials, and seeded
vegetation. The carriage stays visible at a remote site as the return point and
the home of the map case.

## Consequences

- Every fork corresponds to one real junction instead of a sequence of fake
  choices.
- Players can understand immediate topology from the road in front of them.
- Travel remains embodied even when time is accelerated.
- Major sites have room for their own identity and encounters.
- The world feels connected without exposing an omniscient live map.
- Route charts remain valuable as limited, fallible knowledge.
