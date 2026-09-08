# Character names from local roots

New residents draw names from the work and landscape of their home settlement. A mining town favours stone, lamps, silver, and forge roots. A farming settlement favours mills, fields, crossings, and carts. Markets, fortresses, capitals, and frontier towns have their own pools.

Each given name and family name draws from the local pool three quarters of the time. The remaining draws use the whole world pool, representing families who have travelled. Each root has four authored spoken forms. Examples include Aldwyn Millward, Silven Lampwright, and Talla Bookbinder.

`CcGenerateSettlementCharacterName` takes the world seed, settlement identity, settlement function, generation, and resident ordinal. It uses a separate deterministic mix. The world random stream stays at the same position. Unknown settlement functions use the whole pool. Names fit `CC_NAME_CAPACITY`, including their final terminator.

Initial generated residents use their home settlement's function. Descendants draw a new given name from their home and inherit their ancestor's family name. Existing collision checks choose another candidate when a name is already in use. Authored story names retain their spelling.

Schema 58 enables this rule for resident creation and succession. Generator 25 remains the map generator. Older saved names retain their spelling when upgraded. Historical journals run their original naming rule before upgrading. The original `CcGenerateCharacterName` function remains available for those older rules and migrations.

## Validation

The naming test samples 4,096 names for each of the six settlement functions. It checks repeatability, local family bias, wider-world variety, complete names, and buffer bounds. Eight seeded worlds check the full 24-person cast, simultaneous succession, family inheritance, save reloads, and ten further years of repeatable history.

Three frozen saves preserve older checks:

- `schema-52-generator-25-raid.ccsave`: the returning-raid setup from `raid_food_tests.c`, seed `0x460f00d`, before its final return day. The final hash remains `0x26bed631bd4afed2`.
- `schema-53-generator-25-nutrition.ccsave`: initial seed `0x5EED0001`. The uninstrumented 40-year hash remains `0x50d8f654db1ed6f8`.
- `schema-57-generator-25-names-journal.ccsave`: initial seed 42 with all 24 death dates set to day 2, followed by one flushed journal day. Replay creates 24 successors and has hash `9371884615631308597` before upgrade.

These fixtures were written with the simulation and persistence libraries at `7bccd65c33ee1db555bf9eab7f8d02fe086af8ae`. The schema 52 and 53 fixtures select their recorded schema before writing. The naming test checks the full historical replay hash after restoring schema 57 on the upgraded result.
