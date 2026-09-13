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
- **The market is not the horses' trough.** The carriage carries a feed
  tray holding one crate of wheat (`CC_FEED_TRAY_CAPACITY`, the wheat trade
  unit). Wheat the company buys in town pours from cargo into the tray
  while parked; the team eats from the tray in town, one wheat a head a
  day, and a hungry team at departure is a team whose tray ran dry days
  ago — it leaves careful and slow. The departure event reports the tray
  and adds "hungry and careful" when the team is below 30. A market's own
  stock is never eaten from at departure.

Feeding at town is a boost, not a toll: a full tray is several days of
strong recovery while parked, and the weekly stable care at settlements is
unchanged. A mare near foaling still stays home; that gate is about the
foal, not the food.

## The tray

- **Capacity:** one crate of wheat — ten units, the trade unit of the
  wheat good.
- **Filling:** while the company is parked, wheat from cargo pours into the
  tray up to capacity. Buying a crate in town and sleeping on it fills the
  tray; cargo and tray are separate stores, so a full tray frees cargo
  space.
- **Eating:** each horse eats one wheat a day from the tray while parked,
  recovering 6 hunger a head. On the road the tray is not eaten from —
  camps graze the team instead.
- **Old saves** load with an empty tray; horses fed under the old market
  rules are unaffected until the next feed.

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
