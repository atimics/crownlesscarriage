# Fallen purses

Death is local and permanent (#286, owner-blessed 13 September 2026), and the
carried purse falls with the person (#406). This document is the contract for
that first physical slice, shipped with schema 102.

## The drop

When a person dies, the coins they actually carried leave a custody purse at
the place they fell — their current settlement, or their home if they were
home. The purse is a `CC_CUSTODY_PURSE` entry owned by the dead person,
recorded in the historic ring by name, lying at that place, and carrying the
death event as its provenance. The successor inherits nothing carried: the
schema-60 ghost refund, where the heir silently pocketed the purse across
any distance, is retired (schema 101 saves keep it for journal replay).

The coin never leaves the tracked economy. Custody purses are counted gold;
a drop is a move, not a mint or a burn. If the custody ring is full, the
coins fall to the place's market as found money — never destroyed.

## Claiming

- **The company** reaches the place and lifts the purse
  (`CC_COMMAND_TAKE_BODY_PURSE`): reach-checked against the company's
  current settlement, one transfer, the coins land in the company purse, and
  the lift is recorded as a `PURSE LIFTED` event naming the fallen.
- **Bandits** camping at the place (abandoned towns) or riding a road
  through it lift an unclaimed purse after **a week of grace** — mourners,
  travellers and the company all get there first. The coin is spent straight
  into the local market; the event names the band and the fallen.
- **Time**: when the historic ring turns and a name leaves it, any purse
  still lying under that name resolves to its place's market. Unclaimed
  purses never outlive their owner's name, and never vanish into nothing.

## What this is not (yet)

Bodies are purses in this slice: carried *goods* on dead persons do not yet
drop (characters carry coin, not cargo); the player traveler's death, the
successor transition and mourning follow in #289 once this foundation has
its fixtures in place; witness knowledge of a looting rides the event ledger
and becomes speech when the new event kind gains account rules (it ships
`legacy_or_uncovered` in the corpus, like the crown road events). Lootable
graves, armor and named effects are #288's later deliveries on this same
custody contract.
