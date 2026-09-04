# Rainbow pony companions

Each campaign starts with two different ponies, chosen from its world seed.
The seven identities are Ember (red), Marmalade (orange), Dandelion (yellow),
Clover (green), Puddle (blue), Ink (indigo), and Velvet (violet).

The other five live along the roads. When the carriage meets one, travel pauses
and a quest card opens. Each request combines that pony's personality with a
small supply quest: bread, wool, or tools. Bring the requested amount and choose
**Help pony** or press **Enter**. The completed quest increases your bond and
allows recruitment.

Choose **Release [name]** or press **1** or **2** to swap that companion for the
pony you helped. The outgoing pony keeps their bond, completed quest count, and
condition. They move to another road with a fresh request. **Keep travelling**
or **Backspace** leaves the meeting; a completed request remains ready for a later visit.
Ponies greet the carriage at most once per game day.

Open the **Pony book** with its button or **F7**. Met ponies show their faces, names,
personalities, bonds, completed quests, releases, and last seen roads.
Unmet ponies show a shared gray face and **??** until you meet them. The carriage and stable show the colors of your current companions.

## Save and replay

The simulation owns the seven pony records and the two occupied companion slots.
Meeting, helping, swapping, and leaving use journalled commands. Campaign schema
40 stores the roster, requests, active meeting, bonds, history, and roaming
condition in SQLite. Older campaigns receive a deterministic pair after their
original save hash and journal have been checked.

The starting assignment leaves the world's existing random stream intact.
Released ponies receive a different quest type and another road. Save validation
requires seven records, two distinct companions, valid roads, and bounded quest
and history values.

## Checks

`pony_tests` covers seeded assignment, quest costs, invalid and repeated actions,
both campaign save and journal recovery, release history, reunion, migration,
and missing save rows.

The client review commands use the real interface and renderer:

```sh
./build/crownless_carriage --capture-pony-encounter build/pony-encounter.png
./build/crownless_carriage --capture-pony-swap build/pony-swap.png
./build/crownless_carriage --capture-pony-book build/pony-book.png
```

The swap and book captures also exercise quest completion, both swap keys, and
opening and closing the book. CI saves these images as `pony-companion-review`.
