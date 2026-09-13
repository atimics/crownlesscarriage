# Pony charge travel

Hunger never strands the carriage. The team's condition — health, fatigue,
and hunger rolled into `CcSimHorseTeamReadiness`, 0 to 100 — is the pony's
charge: feeding and rest fill it, roads and hunger drain it, and a low
charge slows the company instead of stopping it. This document is the
design contract, shipped with schema 101.

## Departure is always possible

The two departure refusals are gone:

- **A tired or hungry team** (readiness below 30) used to be locked in the
  stable. It now departs at a careful pace — the slow mode — and cannot
  choose a faster pace until it is fed or rested. The careful pace is not a
  label: it is the existing pace system's lowest rate, so a hungry journey
  simply takes longer on the road, and the already slower road-watch count
  for a low team (the +1 day steps below readiness 70 and 45) still applies.
- **A market without fodder** used to refuse the departure. It now feeds
  what it has: the team eats up to its required rations from the origin
  market's stock, and short-fed horses travel hungry, which means slow.
  The departure event says how much fodder was actually loaded, and adds
  "hungry and careful" when the team is below 30.

Feeding at town is a boost, not a toll: a stocked market still provisions
the team exactly as before (the full reduction to horse hunger), and the
weekly stable care at settlements is unchanged. A mare near foaling still
stays home; that gate is about the foal, not the food.

## Grazing on the road

Camps are where the ponies graze:

- **Overnight camp** now recovers 8 hunger, up from 5, and the flavor says
  the team grazes by lantern light rather than eating reserved fodder.
- **Roadside site camps** recover the same 8.
- **The midday break** now lets the team crop roadside grass: 2 hunger, up
  from 0.
- Road houses keep their larger recovery.

A company that camps every night crosses the map slower but arrives fed,
without buying anything.

## Pace

A team below 30 readiness may only travel carefully. `SET_JOURNEY_PACE`
refuses a faster pace while the team is that hungry — the road is not
blocked, the speed is. Once fed or rested above 30, any pace can be
chosen again.

## What is not here yet

The charge itself is already readable — `CcSimHorseTeamReadiness`, and the
travel preview's `horse_readiness` — but the client has no progress bar
for it. That work, plus showing the forced careful pace in the travel UI,
is tracked as a follow-up issue. The Underroad expedition still requires a
Bread or Meat to enter: that is dungeon-delving, not travel.
