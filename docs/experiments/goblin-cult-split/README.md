# Goblin society and the dragon cult

Goblins acquire treasure and carry it through the Underroad. The dragon cult is
an institution whose members include humans and goblins. Cult membership is a
subset of each species population. Initiate, Bearer, Keeper and Voice are open
to both species. Gifts earn service, which promotes members through those ranks.
The cult keeps its own offering chest for the egg ritual.

Red, Purple and Blue each have a population, a lair, a treasure account and a
porter party. Their lairs occupy the Red Cap Barracks, Ledger Vault and Cinder
Market in the existing Underroad. Porters share the room caches. They follow
open, known passages one room per day. Each item moves between a room, a porter,
a lair and the dragon's hoard. Closed passages can stop a delivery. Named
artifacts retain their existing ownership and delivery route.

When a dragon lives, all three factions carry treasure towards it. Its first
365 days in the new system form the founding tribute contest. Coins count at
face value, gold at 40 and gems at 70; named artifacts use their appraised value.
The largest delivered total chooses the crown colour. Ties extend the contest
until one faction leads. The crown keeps that colour for the dragon's life.
The visible dragon colour follows the winning crown.

**The goblins keep believing that another gift can win favour.** All three
continue acquiring treasure and making deliveries after the choice. Tribute
counters keep rising. Later gifts from a rival can exceed the original winner's
tribute. The crown stays with the original winner. The dragon hunts the other
two factions in turn when it needs food; the winning faction is spared. Surviving
porters continue their deliveries. A faction with no members holds its stored
wealth until civic recruitment supplies members again.

When the dragon dies, deliveries turn back towards each faction's own lair.
The three factions compete for stored treasure. The separate cult gathers its
ritual supplies and prepares eggs. A successor starts a fresh founding contest.
Existing lair wealth stays with its faction. The tribute score starts again for
the new dragon; delivery and hunting counts cover the whole run.

## Model boundaries

This slice uses population groups for cult ranks. Human members belong to the
settlement above the goblin lair. Goblin members are a subset of goblin society.
The shared surface raid system supplies civic food and equipment; factions take
turns owning its acquired coins and minerals. Portage within the Underroad has
separate parties and stored positions. Ritual work proceeds during goblin raids.
Goblin cohesion still affects their support for a ritual.

The current civic model distributes recruits to the smallest faction and general
losses to the largest. Dragon hunts apply losses to the chosen enemy faction.
Existing food, tool and recruitment rules set the pace of recovery. These rules
are useful targets for later balance experiments.

## Save and observation contract

Schema 75 stores the faction accounts, porters, founding contest, cult ranks and
cult offering chest. Older saves verify and replay under their original rules
before these new fields are initialized. Their existing coins, items, religious
devotion and egg countdown remain intact. A loaded older world starts a fresh
founding contest and an empty cult offering chest.

The `goblins` text view shows species population, all three lairs, separate cult
membership, ranks and the offering chest. The dragon cave panel shows human and
goblin cult members and the crown colour. Events show departures, deliveries,
promotions and hunts. They express the goblins' continuing hope of favour.

The metrics CSV adds `goblin_crown_color` (-1 unchosen, 0 red, 1 purple, 2 blue),
`cult_human_members`, `cult_goblin_members`, `cult_devotion`, per-colour population,
lair value, tribute and hunting losses, and each species' four cult ranks.
Existing `goblin_devotion` remains a compatibility alias for cult devotion.
JSON reports include a `goblin_society` section and use the cult's actual
membership and chest in the ritual plan.

## Checks

`goblin_cult_split_tests` covers:

- Both species reaching each rank through the same service costs.
- Each colour winning, and late rival tribute leaving that choice intact.
- Losing factions continuing to dispatch treasure.
- Hunts reaching both enemy colours and sparing the winner.
- Ties, closed passages, cargo returning after death and a fresh successor contest.
- Conservation of coins and gold during portage.
- Save/load equality during a journey and identical continued simulation.
- Invalid porter positions and cult population counts.
- Twenty seeds over one hundred years with yearly world validation.

The older dragon, ritual, journal, save migration and conservation tests cover
the connected systems. The ritual fixtures now provision the cult chest and
name the worshippers separately from the goblin population.
