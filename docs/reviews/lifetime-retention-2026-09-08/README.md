# Lifetime retention order

This strengthens bounded-history acceptance in #250 and retained-source acceptance in #459. Base: PR #579 at ce1652c. Simulation schema 73, generator 25.

The existing lifecycle fixture fills the history pool through ordinary deaths. Four new controlled cases then exercise the current retirement rule:

1. A lower-importance record is chosen even when its death is more recent.
2. Among equal low-importance records, the older death is chosen.
3. Equal importance and death dates select the smallest stable ID.
4. Reversing history storage order preserves that stable-ID choice.

Each case advances one normal lifecycle death. It checks the selected retired identity, the incoming lifetime, the successor's distinct ID, pool capacity, and byte-for-byte preservation of every retained historical record. The listener's received account remains intact. A second simulation produces the same hash. Save/load preserves every active historical record.

Fixture dates and importance weights are controlled inputs to isolate the ordering rule. This evidence covers the current retention policy. Broader seal, letter, campaign, and relic attribution remains acceptance work in #250.

## Reproduction

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -j4
out/build/headless/character_lifetime_tests --save-only
CC_CPPCHECK_JOBS=4 python3 tools/static_analysis.py
```

## Results

Strict release headless build, all 109 CTest tests, and the save-only lifetime test passed. Static analysis passed with one reviewed baseline item. The four retention cases and the existing 12 independent saved-field checks passed together.
