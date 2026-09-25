# Town refresh review frames

Native macOS captures from the #931 branch after it merged main 2b8562ab.
Frames are scaled to 960 px wide with a 128-colour palette.

Make them again with:

```sh
python3 tools/capture_town_refresh.py \
  out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage
out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage \
  --capture-town <0-5> <abs.png>
```

| Frame | What to check |
| --- | --- |
| `*-hall.png` | Default town view (`--capture-town N`). The grain merchant hall and its sign. |
| `*-carriage-court.png` | Carriage court (`--capture-town N 42.4 55.2`). The road reaches the parked carriage and leads out to the named lanes. |
| `rosespire-bakery-front.png` | The bakery door is open to the square and has a bread display. Shop cards page through all nine shops. |
| `rosespire-bakery-interior.png` | The bakery shelves show only bread. |
| `rosespire-stonecutter-front.png` | The stonecutter door is clear of the carriage court. |
| `silverwick-mine-supplier-trade.png` | The mine supply office sells raw stone and explains the stonecutter margin. |

Checks:

- All six towns render. Every carriage court has a road from the carriage to
  its lane exits.
- Shop fronts are not blocked. `town_shop_walks` walks from the carriage to all
  55 shop doors and back, in six towns and for two seeds (110 walks).
- Raw stone had no icon in the goods atlas, so the trade panel showed a blank.
  It now shows a darker cut stone icon.

Found, but the same on main (not caused by this branch):

- Gloamgate market circle: a dark smear crosses the road right of the fountain.
- Gloamgate coach court: the "To customs road" and "To trading court" labels
  overlap.
- Silverwick ore wagon yard and furnace alley: dark roof slabs float in the sky.
- Shop interiors: the player stands inside the keeper at the counter.
