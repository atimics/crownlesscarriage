# People, carriages, and gossip

These are the society rules agreed during the communication review.

People keep possessions, needs, memories, relationships, allegiance, and a current purpose. A scout is someone doing a quest to find knowledge. Travellers walk between towns, trade goods and gossip, pay for inns, and buy carriage passage. A paid carriage can offer a safe overnight camp.

Crowns have a physical holder. A person has a purse. A carriage can have a shared purse and shared cargo. Crew and passengers also keep their own purses. A destination can hand a travelling fund to its carriage. The crew spends that fund on the journey. Sales of stranded cargo put crowns in the carriage purse. A later handover can return crowns to a treasury.

A traveller who loses every crown becomes a wayfarer. Paying for an inn can restore their traveller life. Bandit recruitment changes allegiance. A wealthy outlaw keeps that allegiance. Suggested bandit ranks: Worm, Slug, Leech, Louse, Dung Baron, His Filthiness Lord of the Ditch.

Bandits carry their own crowns, food, and loot. They gather at camps and consume what they have taken. When they capture a carriage, they camp around it where it stands. Treasure in that camp attracts goblins, who can drive the bandits away.

Each royal faction has one carriage. A stranded carriage can create a quest to transport its crew back to it. A faction can send a rider to recover it. Recovering that same carriage restores the faction's transport.

Each carriage uses one draft animal. Common ponies are livestock, alongside cattle and sheep. Town herds breed and consume food. Carriages and recovery riders draw on that population. The seven rare named ponies remain special encounters.

Gossip stays readable. Retellings gradually change counts, people, directions, or motives. An NPC keeps the version they learned. Scribes hear reports at the Scriptorium and bind their account as the public record. The original world event continues to drive the physical simulation.

## Delivery order

Status, checked 27 September 2026:

- Step 1 is done.
- Step 2 is mostly done. Towns with a farm or stable keep common pony herds that
  eat and breed (schema 51), and the company carriage pulls one animal (schema
  52). Royal carriages do not draw on the herds.
- Step 3 is not started. Royal carriages have no purse, and there are no
  stranded-carriage quests, recovery riders, wayfarer state (the client has
  only a wayfarer figure), bandit ranks, or paid passage.
- Step 4 is partial. Characters carry a personal purse (`travel_coins`) that
  falls as a custody purse when they die. Bandits hold coins and supplies per
  group, not per person. Traveller needs, inn lodging, and goblin pressure on
  bandit camps are not in the simulation.

1. Personal gossip, spoken accounts, source questions, and exchanges. PR #371.
2. Common pony herds and one animal per carriage.
3. Carriage purses, physical sale proceeds, abandoned carriages, and recovery.
4. Traveller needs, lodging, personal bandit loot, and goblin pressure on camps.

## Code references

The gossip work follows Signal's exchange-at-contact model and Cosyworld's personal beliefs and valid action choices. The Crownless simulation owns the state, the save journal records actions, and speech reads that state.
