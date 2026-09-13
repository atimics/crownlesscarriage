# Independent lifetime save fields

This strengthens the save evidence for #459 and the shared lifetime identity work in #250. Base: PR #578, 640b911. Schema 73 and generator 25.

The existing lifecycle test proves that an account survives its source's death, repeated cast-slot reuse, and eventual biography retirement. This change adds 12 independent valid mutations after that lifecycle fixture:

- historical record count;
- lifetime ID, name, ancestor, home, birth day, death day, generation, role, and importance;
- received source ID and name.

Each mutation starts from one baseline, verifies a changed hash in normal mode, saves and decodes the state, and compares the changed field directly. Dates use a valid interval with room to vary each date independently. The retired original lifetime provides an issued identity for the ID and ancestry checks.

The test accepts `--save-only` for fault injection. That mode skips field hash assertions and round-trip hash equality in the test; direct decoded-value checks remain active. Normal CTest keeps the hash assertions.

## Validation

- Strict release headless build passed.
- All 109 tests passed after correcting the date fixture. The affected `received_account_lifetimes` test passed again after the final test-mode adjustment and source restoration.
- All 12 checks passed in normal mode and `--save-only` mode.
- Static analysis passed with one reviewed baseline item.
- A combined injected fault removed source names from hashing and replaced a changed source name during save. The direct comparison failed on the source-name field. `fault-injection.json` records the result and original source digests. Both production files were restored byte for byte.

An earlier save-only injection was caught by the decoder's checksum, which motivated the combined fault above. The final patch changes tests and evidence only. Wider document, seal, campaign, and relic attribution acceptance remains in #250.

## Reproduction

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -j4
out/build/headless/character_lifetime_tests --save-only
CC_CPPCHECK_JOBS=4 python3 tools/static_analysis.py
```
