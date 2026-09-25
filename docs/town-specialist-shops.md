# Town shops

Every town has nine shops. Look for the painted sign above each door. Walk to
the door and press **F** to enter. Each shop uses the town's current stock and
prices.
Silverwick also has a mine supply office near the quarry.

| Shop | Goods |
| --- | --- |
| Bakery | Bread |
| Butcher | Meat |
| Grain merchant | Wheat |
| Smith | Iron, tools, weapons |
| Jeweler | Gold, gems |
| Timber merchant | Wood |
| Clothier | Wool |
| Stonecutter | Stone |
| Stationer | Paper |
| Silverwick mine supply office | Raw stone |

The main hall's grain merchant also receives town promises. Bread belongs at
the bakery; cut stone belongs at the stonecutter. The grain merchant sells wheat
to carry to a bakery. Silverwick's mine supply office sells raw stone for a
stonecutter. Both workshops buy their materials for the town's current orders.
Bring spoiled meat to the butcher
or spoiled grain to the grain merchant for the existing sale and disposal terms.

## Screenshot review

Build the native game, then capture the Rosespire bakery and stonecutter from
the street. Run the commands from the repository root. The frame paths are
relative to that root.

```sh
cmake --build out/build/play --target crownless_carriage -j 8
out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage \
  --capture-town-state 4 60 30 out/rosespire-bakery.png peaceful
out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage \
  --capture-town-state 4 22 43 out/rosespire-stonecutter.png peaceful
```

For an interior or trade panel, enter the shop in the native game and capture
the visible window:

```sh
screencapture -i out/rosespire-shop-interior.png
```
