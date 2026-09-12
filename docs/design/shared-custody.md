# Shared custody transfers

Implementation draft for #434. The starting branch is #695 at `0edd7347`,
with schema 98 and generator 25. This contract keeps the full issue scope open
until the integration and acceptance checks below pass.

## Existing state to use

`CcTreasure` already provides stable IDs and physical material content. Its
`owner_id` and `location_id` currently serve several treasure and archive flows.
`CcSimTrackedGold` and material accounting already count those contents.
`CcFreightLeg` provides route geometry, while ordinary carrier state supplies
movement and capacity. The new transfer code will use those sources directly.

## One physical holder

A custody entry has a stable ID, a revision, a holder kind and ID, an owner ID,
a condition, and the ID of its last transfer event. Holders are town stores,
road-site stores, ordinary carriers, the player company, captors, and containers.
Character references use the full lifetime ID. A replaced character slot is a
different holder.

A treasure entry refers to an existing treasure ID. Its material content stays
in `CcTreasure`. A document copy has its own physical ID and a separate work ID;
several copies can refer to one work. A goods entry holds a positive quantity
of one good. A purse holds one scalar balance.

A container has its own ID and bounded manifest capacity. Its holder determines
its physical position. Its owner may differ from the carrier that holds it.
The first implementation permits one container level: containers hold goods,
purses, treasure references, and document copies. Container placement inside
another container is rejected before any state changes.

## Transfer contract

One planner validates the source holder, destination holder, expected entry
revision, quantity, co-location, capacity, and permission for the requested
operation. Player commands, ordinary carrier dispatch, and recovery use that
same planner and apply function. A plan is a read-only result.

Apply revalidates the plan against current state. It advances the entry revision
and records provenance only after all checks pass. Repeating an old revision
returns a stale-request result and leaves quantities and balances unchanged.
A split creates a fresh stack ID and reduces the source quantity exactly once.
An exact transfer retains the entry ID. Full pools reject the complete operation.
Only resolved, empty entries may be reclaimed.

Packing moves existing stock into the manifest. Unpacking moves it back into
an existing stock balance. Crate creation and repair consume the shared economy's
materials and work. Packing and sealing are separate recorded operations.
The recipe interface leaves room for wax as the economy gains that good.

## Placement and accounting

Actual placement resolves through the current holder. A travelling carrier has
a route and progress; arrival moves its manifest with it. Interception transfers
the manifest to the captor at the interception place. Recovery uses the same
transfer path. An owner change alone preserves physical placement.

Last-reported placement is separate knowledge with its own observation time.
Catalogues and previews use available reports. They leave physical custody
unchanged. Player-facing manifest reads require local access or an available
record of those contents.

Each scalar stock is counted once. Packing subtracts from the source balance;
unpacking adds to the destination. Material accounting counts manifest goods
and purse balances, and counts referenced treasure through the existing treasure
account only. Container and carrier capacity use the manifest's total load once.

## Persistence and rollout

Introduce the new saved fields at the next schema after the actual implementation
base. Keep earlier schema simulations on their existing paths. Migration creates
custody references for live named objects using their current owner and placement;
it preserves anonymous bulk stock balances. Preserve active archive book journeys
when deriving the holder. Validation rejects unresolved live references, cycles,
invalid quantities, excess capacity, duplicate physical references, and stale
character lifetimes.

Every saved field needs binary and SQLite round trips and a field-sensitive hash
check. Extend the shared legacy boundary when the schema advances. Replay old
worlds against the parent and compare their hashes before accepting migration.

## Acceptance work

- Exercise store → container → ordinary carrier → destination store.
- Exercise interception → captor → recovery on the same manifest.
- Check item identity, stock and coin totals after success, rejection, and retries.
- Check partial stacks, full pools, stale revisions, character replacement,
  container capacity, and rejected nested placement.
- Check physical progress, arrival, owner changes, and last-reported placement.
- Use the shared transfer API from player commands and ordinary carriers.
- Verify crate material/work costs through common economy accounting.
- Verify every new field in save/load, hashing, validation, and legacy replay.
- Capture a player journey that packs, loads, travels, and unloads the same items.

The transfer core now has `CcCustodyState`, a read-only planner, and a revalidating
apply function in `src/sim/cc_custody.c`. Its tests use explicit physical-holder
and permission callbacks. They cover goods packing, container carriage,
interception, recovery, unloading, purse splits, separate copies of one work,
full pools, retired slots, stale requests, and movement between plan and apply.
Packing and unpacking also advance the container revision.

The strict native target, AddressSanitizer/UndefinedBehaviorSanitizer run,
Cppcheck, and WebAssembly compile check passed. This is isolated core evidence.
The core now validates every entry field, holder resolution, unique treasure
references, bounded manifests, and monotonically allocated IDs. Its portable
field hash includes active and retired entries. Mutation tests cover every
field, including fields with the same storage size.

Schema 99 adds custody state to the world hash and both save formats. The first
world adapter accepts goods, purses, and containers rooted in settlement stores,
with a player or settlement owner. World totals count stored goods and coins once.
The reader checks every slot and integer column, including retired records.
Earlier saves gain an empty custody pool during upgrade; existing item records
keep their current storage while the migration adapters are built.

Town store adapters now pack real bulk goods into an owned container and unpack
all or part of a stack through the shared transfer core. Packing binds the
request to the allocation counter and container revision. Unpacking binds it
to the stack revision. Successful unpacking retires the resolved record, and
later packing allocates a fresh ID. Both operations commit stock and custody
together after all checks pass. These internal adapters act for the town;
player commands will supply their own access and ownership rules.

World container capacity now uses `CcGoodsFreightCargoSlots`, the same goods
conversion as ordinary freight. Each physical stack rounds to whole slots.
Splitting a stack can add one occupied slot even when both pieces stay with
the same root holder; planning checks that extra load. Purses and containers
each occupy one slot. The core accepts a read-only load callback, while its
default test model counts goods units directly.

Town-owned custody can now load onto an ordinary royal carriage through the
shared transfer function. A loaded carriage has a dedicated booking. Trade,
site/grain supply, archive supply, and archive relocation yield that carriage
until its custody cargo is unloaded. Dispatch uses the existing royal path and
movement code, reserves the actual freight slots in the weekly route budget,
and records a cargo journey event. Dispatch respects the carriage's next
available day. Transfers into the carriage or its containers require an idle
carriage, so the booked route load stays fixed during travel. Arrival updates the holder's settlement;
unloading checks that physical placement. Blocked carriage placement follows
the existing route-origin waiting model and keeps transfers on the route.

A real-world test packs town stock, loads a carriage, saves during travel,
advances the simulation to arrival, and unloads the same container and contents
at the destination store. It also checks exhausted route capacity, duplicate
requests, and blocked-road unloading. Player cargo still needs its existing
player slot conversion and access rules at the player boundary.

Player, character and captor holders, named treasure migration, document work identity, object creation,
common economy costs, transfer commands, and the player journey remain open.
The draft is complete when the implementation and evidence satisfy the whole
acceptance list.
