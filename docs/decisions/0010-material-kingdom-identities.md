# 0010 — Derive kingdom identity from material holdings

**Status:** Accepted

## Context

The simulation already gave each kingdom different settlements, resources, and
exposure to war or dungeons. The player-facing descriptions reduced them to a
name, treasury, debt, and legitimacy score. Their strongest differences were
therefore hard to see, while adding fixed lore fields would risk descriptions
that no longer matched the generated world.

## Decision

Derive one kingdom calling from the functions of its controlled settlements:
Road and Granary, Iron and Wall, or Capital and Deep. Derive a live pressure
score from the material threats relevant to that calling, plus hunger, debt,
and legitimacy shared by every realm.

Expose holdings, calling, dependence, contradiction, pressure, factions, and
confirmed relations in the text-first kingdoms report. Local look text may name
the realm and its current strain. The graphical game still has no free kingdom
screen; wider knowledge must remain tied to physical maps, news, and places.

## Consequences

- Kingdom identity stays consistent with generated geography and economics.
- A changed road, missing shipment, unpaid garrison, disturbed dungeon, or
  dragon shadow can change the realm's visible pressure.
- Existing factions become legible as Court, Factors, and Commons with separate
  support and material power.
- Pressure remains a reading. It cannot create or remove goods, danger,
  legitimacy, or faction support.
- New kingdom callings require distinct holdings, dependence, contradiction,
  and at least two visible projections.

## Rejected alternatives

- Fixed biographies attached to kingdom names were rejected because seeded
  names can change while the material role stays the same.
- Slot-based bonuses were rejected because they would survive after their
  economic cause disappeared.
- A free graphical kingdom dashboard was rejected because it would restore the
  omniscient view removed by the physical-cartography contract.
