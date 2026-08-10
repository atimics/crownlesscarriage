# Glossary

## Authoritative state

Data owned by the strategic simulation and persisted in saves. Rendered objects
are projections unless explicitly promoted through a validated command.

## Bandit influence

A strategic measure of a bandit group's ability to recruit, raid, negotiate,
and control a route or area. It is supported by people and material resources.

## Causal event

A dated record of a meaningful state change containing responsible entities,
parents, consequences, and visibility requirements.

## Causal event ledger

The structured history connecting world changes to situations, dialogue,
projections, delayed consequences, and debugging explanations.

## Causal pressure

A strategic condition capable of creating or escalating a playable situation,
such as hunger, route closure, faction rivalry, bandit recruitment, or monster
migration.

## Cohort

An aggregated population group used instead of simulating every individual.

## Command

A validated request to change authoritative simulation state. Local gameplay
reports outcomes through commands rather than directly editing the simulation.

## Commitment

An explicit player action that advances strategic time, including travel, rest,
recovery, construction, and long contracts.

## Contract

In the manual, a mandatory design or technical rule. In the game world, an
agreement accepted by the carriage company. Context should distinguish them.

## Delayed echo

A later, player-facing consequence of an earlier decision, delivered through a
changed place, recurring character, letter, passenger, route, or situation.

## Dungeon state

A persistent strategic classification describing access, control, inhabitants,
regional effects, and current use of a dungeon.

## Generator version

An explicit version identifying the procedural rules used to create immutable
world or route data. It prevents code changes from silently invalidating saves.

## Immediate projection

A visible local change produced soon after a player decision.

## Knowledge packet

A delayed, bounded piece of information derived from a causal event and sent
through a known channel such as a letter, traveller, official notice, or rumour.

## Local projection

A visual, behavioural, dialogic, or interactive expression of strategic state
inside a city, wilderness segment, interior, or dungeon.

## Monster pressure

A strategic summary of how a monster population affects routes, resources,
settlements, and other species.

## Named-character promotion

The creation of a persistent individual from aggregate population when play
requires a recurring actor, passenger, office holder, proprietor, or witness.

## Province

The smallest strategic territorial unit. It contains geography, resources,
connections, control, population capacity, and regional pressures.

## Route segment

A deterministic local wilderness space generated for a portion of a strategic
route and parameterized by current world state.

## Situation

A playable structure compiled from causal events. It contains interested
actors, interventions, deadlines, escalation, and visible outcomes.

## Strategic snapshot

An immutable view of authoritative simulation state consumed by presentation
and UI systems.

## Typed stable ID

A persistent identifier whose type and generation prevent stale or cross-type
references from silently resolving to the wrong entity.

## Vertical slice

A narrow but complete proof containing strategic cause, player commitment,
local adventure, persistent consequence, and production-quality validation.
