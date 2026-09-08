# Archive gossip fixture contract

This supports #240 and its mill-selection draft #453. Base: PR #580 at 543233c, schema 73, generator 25.

The older draft's seven failed build/test jobs all reported the same gossip assertion: the supposed remote account was already recorded. Its mill selector placed the archive at a different settlement, while the test treated settlement 1 as the archive without preparing that location.

Evidence: CI run 34148801799, including the Headless Debug Ubuntu job at https://github.com/atimics/crownlesscarriage/actions/runs/34148801799/job/101826474440.

The shared fixture now makes settlement 1 its sole mill and asserts that the material-chain query selects it as the archive. The late-recording case retains the mill service and sets paper stock and production capacity to zero. It checks that the archive stays at the supplied fixture location. Restoring paper then allows the received account to be recorded exactly once, with its original receipt ancestry and location.

This keeps the gossip test focused on evidence delivery and material supply. Seat scoring, stable-seat policy, saved relocation, physical book journeys, and rival sponsorship remain acceptance work in #240.

## Validation

- Strict release headless build and all 109 CTest tests passed.
- Static analysis passed with one reviewed baseline item.
- A controlled probe applied the exact mill-selection line from #453 to this base. The gossip suite passed, including the paper-shortage and restoration case. `mill-selection-probe.json` records the result.
- The engine source was restored byte for byte. The restored `traveler_gossip_network` test passed again.

## Reproduction

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -j4
CC_CPPCHECK_JOBS=4 python3 tools/static_analysis.py
```
