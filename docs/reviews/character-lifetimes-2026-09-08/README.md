# Received accounts and character lifetimes

This delivery advances #459 and supplies the bounded lifetime store for #250.
It builds on #500 (schema 59 / generator 25).

## Contract

Schema 60 / generator 25, SQLite layout 30. Each received situation account
keeps the teller's lifetime ID and the name recorded when it was learned.
A death preserves surviving characters' held accounts. Recasting a situation
assigns its live roles while preserving the surviving accounts and the old
lead event. Later observations and exchanges can update knowledge through
ordinary paths.

Before slot reuse, the engine records the person's ID, name, birth and death
dates, generation, ancestor, home, role and importance. The newborn receives a
fresh ID. The history store holds 32 records. Once full, the new death replaces
the lowest-importance stored record, then the earliest death, then the lowest
ID. Importance is 2 for a current officeholder, 1 for another official, and 0
for other people. This retains a bounded set of detailed lives and always
admits the latest death.

A retired biography leaves its lifetime ID issued. Received accounts retain
their own source name and ID through that retirement. The engine lookup
`CcSimHistoricCharacter` is for engine/debug use. Actor text can use only its
held snapshot. Changing a biography changes engine history while the held
source name stays intact. Future letters, seals and custody records should
reuse this identity and carry their own attributed text.

The existing situation-account pool still governs retention when a situation
is reclaimed. The detailed-history bound governs biographies. Those two
lifetimes are separate. The change adds 8,712 bytes to `CcSim`, including 6,144
bytes for source-name snapshots and the bounded history store.

## Compatibility

Schema 59 and earlier journals replay their original source cleanup and cast
seeding rules. The loader checks their original hashes before upgrading. It
fills source-name snapshots only for accounts that survived that replay, using
the referenced living source (or the Company). Historical records start empty.
Future deaths populate the store. All new fields enter the schema-60 hash,
save paths and validation. SQLite layout 30 marks the added table and column.

## Verification

```sh
cmake -S . -B out/build/foundation -DCC_BUILD_CLIENT=OFF -DCC_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/foundation -j 4
ctest --test-dir out/build/foundation --output-on-failure -j 4
```

The focused test takes an actual told account from the seeded mine thread,
then kills its source through daily lifecycle processing. Every survivor keeps
their prior account set. The test covers 35 replacements, deterministic history
retirement, names after retirement, save round trips, and a hash mutation for
every new historical field and the received source name. Saved fields are
compared directly. The schema-59 journal test verifies old cleanup before the
new snapshot upgrade. The standard SQLite suite covers shipped fixtures.

## Follow-up deliveries

The wider #250 contract also needs campaign/deed attribution and surviving
physical letters, seals and items. Those consumers must distinguish historical
authorship from current office or custody. The engine history store is ready
for that work. #277/#485 cover richer exchanges and conflicting accounts;
#434 owns the shared physical transfer operation.
