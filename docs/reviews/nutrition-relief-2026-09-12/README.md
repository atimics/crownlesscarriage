# Nutrition relief reserves

Base: ccde43771ea1ae71546c0930442d392945394048. Simulation schema 100 extends famine reserve relief to goods with civilian nutrition: bread, wheat and meat. Schema 99 and earlier retain the bread-only rule. The save layout and generator version stay at their current versions.

The destination must be inhabited and have hunger at least 65; the supplier must be inhabited and below 35. Relief can draw down to the greater of half the original reserve and six weeks of civilian nutrition, measured in units of the selected good. Wheat also retains one cycle of bakery, cattle, flock and pony inputs. The original reserve remains the upper bound on protected stock, matching the previous reserve policy when a town already sets a smaller target.

Existing trade owns the payer, credit, carrier, route, freight size and arrival. A closed route, missing funds or unavailable carrier retains its original gate. Abandoned towns use their separate resettlement and company-recovery rules (#722).

`fixtures.txt` records the integration test. Its controlled supplier has wheat equal to its ordinary reserve, so the earlier rule cannot dispatch any. Under the new rule a funded carriage departs, the source retains its reserve, and the wheat reaches the recipient after time advances. Save/reload during the shipment produces the same final hash as uninterrupted play. Paired cases cover schema 99, a closed road, no buyer funds and an unavailable carrier. Direct checks cover the nutrition conversion, herd input reserve, hunger thresholds, abandonment, ordinary goods and read-only queries.

Validation: strict Release headless build; all 152 checks passed across the full run and focused reruns. The full run first caught two version expectations that still named schema 99 as current or excluded it from legacy admission. Those expectations now cover the new boundary. The speech test passed with permission for its temporary loopback server.

Run from a clean checkout:

```
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCC_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -j4
```

These fixtures establish the reserve and delivery contract. Broader food-distribution measurements remain #400/#266.
