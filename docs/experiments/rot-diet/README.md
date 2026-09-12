# Rot as food for goblins and dragons

Schema 76 gives goblins and dragons a scavenger diet. Rotten Meat supplies one
ration per unit. Rotten Grain supplies half a ration per unit, like raw wheat.
Rotten wheat uses the existing Rotten Grain good and the `rotten-wheat` text
command alias.

Goblin food checks, weekly meals, recruitment provisions, brood provisions and
food raids all recognise rot. Meals consume rot before fresh food. Food raids
can choose a town's rotten stores and carry them back to the lair. The goblin
market accepts both kinds through the usual trade action. For example:

```
goblins trade rotten-meat 4
goblins trade rotten-wheat 8
```

The lair view shows its rotten food stocks. The mixed human–goblin cult keeps
its existing ritual food requirements.

A hungry dragon's crown still chooses its goblin enemies. During other food
hunts, the dragon chooses the largest rot supply among its hoard, the goblin
lair and settlement stores, including ruins. It eats up to its normal appetite.
A full rot meal gives a 42-day feeding cooldown; a partial meal gives 14 days.
Rot supplies body condition. Livestock and fresh food remain available for later
hunts as rot runs out. Chronicle entries give the quantities eaten.

Eating subtracts actual items from their holder. Goblin raids and trade move
items into the lair before they are eaten. These paths form a sink for rot that
accumulates from spoilage. Existing civilian, travel and animal diets keep their
own nutrition values and consumption order.

The save format uses the existing rot goods. Schema 75 and earlier replay their
original food rules before upgrading to schema 76. The goblin faction accounts,
founding contest and cult state carry through the upgrade.

`rot_diet_tests` covers diet values, meal order, large stocks, goblin survival,
trade and the rotten-wheat command, rot raids, town/lair/hoard dragon meals,
partial meals, stock removal and schema-75 save replay. The existing faction,
conservation, ritual, nutrition and long simulation tests cover connected rules.
