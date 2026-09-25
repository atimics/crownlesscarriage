# Town shops

Every town has nine shops. Look for the painted sign above each door. Walk to
the door and press **F** to enter. Each shop uses the town's current stock and
prices.

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

The main hall's grain merchant also receives town promises. Bread belongs at
the bakery; stone belongs at the stonecutter. Bring spoiled meat to the butcher
or spoiled grain to the grain merchant for the existing sale and disposal terms.

Each town has its own streets around these shops.

| Town | Street layout |
| --- | --- |
| Thornford | Winding farm lanes |
| Gloamgate | Fountain ring |
| Silverwick | Terraced work roads |
| Alderwatch | Bridge and muster spine |
| Rosespire | Rose avenue and court |
| Hollowbarrow | Lantern crescent and expedition yard |

## Screenshot review

Build the native game, then capture the Rosespire bakery and stonecutter. Run
these commands from the repository root. Town 4 is Rosespire. Goods 0 and 10
are bread and stone. Frame paths are relative to the repository root.

```sh
cmake --build out/build/play --target crownless_carriage -j 8
out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage \
  --capture-shop-front 4 0 out/rosespire-bakery-front.png
out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage \
  --capture-shop-interior 4 0 out/rosespire-bakery-interior.png
out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage \
  --capture-shop 4 0 out/rosespire-bakery-trade.png
out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage \
  --capture-shop-front 4 10 out/rosespire-stonecutter-front.png
out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage \
  --capture-shop-interior 4 10 out/rosespire-stonecutter-interior.png
out/build/play/crownless_carriage.app/Contents/MacOS/crownless_carriage \
  --capture-shop 4 10 out/rosespire-stonecutter-trade.png
```
