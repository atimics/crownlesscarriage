# Custody save checks

Source: `27d34458b917099cdfc740a2416a88f6c5d0d9ef`.
Base: `0edd7347dc954ff3b5e0e0bc01b85888fdd3bab0` (#695).

All 140 strict headless tests pass. Repository static analysis passes. The custody save test checks binary and file
round trips with goods, a purse, a container, and a retired slot. It checks world
totals, damaged rows, unchanged destinations after failed reads, and schema 98
upgrade. The transfer tests cover each field in the custody hash.

`probe.c` was compiled against each build's simulation and persistence libraries,
with SQLite and libm. The compressed outputs retain all 3,193 version checks and
61 save/replay pairs. The only admission change is schema 99 with generator 25.
For the replay comparison, the decoded world's schema is set to 98 before hashing,
so the comparison checks existing fields under their existing hash contract.
The new schema's custody state is checked separately by `custody_save_tests`.

The WebAssembly compiler accepted `cc_sim.c` and `cc_sim_hash.c` with strict
warnings. The quick benchmark passed its 90,000 ns/day limit at 32,366.3 ns/day.

This stage persists goods, purses, and containers rooted in settlement stores.
Carrier adapters, named item migration, document work identity, creation costs,
and the player packing journey remain part of the open draft.

## Town transfer follow-up

Source `66a7eb4957b8da6cf8de04bdccfb152f288544b9` adds atomic packing and unpacking
of town stock through the shared transfer core. All 141 headless tests pass.
The store journey packs seven wheat, saves and reloads, unpacks three, then
unpacks the remaining four. Goods and coin totals match the starting values.
Duplicate requests leave the full simulation unchanged. Further checks cover
capacity, distant stores, ownership, exhausted pools, bounded town stock, and
reuse of retired slots with fresh IDs.

The new adapter passes Cppcheck. The adapter and transfer core compile under
WebAssembly with strict warnings. The focused store test passes with AddressSanitizer
and UndefinedBehaviorSanitizer applied to the test, transfer core, and adapter;
the remaining simulation and persistence libraries use the strict release build.

Commands: `cmake --preset release -DCC_BUILD_CLIENT=OFF`,
`cmake --build out/build/release -j 6`, and
`ctest --test-dir out/build/release --output-on-failure -j 6`.
The focused test selector is `-R '^custody_'`.

## Shared freight slots

Source `a72b4301a05cdb0c1fe0fe05af4c2ca72be2bdc4` combines the remote main
integration with shared freight conversion. All 144 headless tests pass.
A ten-slot container holds 100 wheat using `CcGoodsFreightCargoSlots`; one
more wheat is rejected. A core regression proves that splitting a stack can
consume an extra rounded slot at the same root holder. Full-capacity rejection
preserves the entire state.

The custody core and adapter pass Cppcheck and strict WebAssembly compilation.
The core passes AddressSanitizer and UndefinedBehaviorSanitizer. The store test
also passes with the core and adapter instrumented against merged release
libraries. The quick benchmark passes at 42,268.2 ns/day against a 90,000 limit.
Main integration preserves all three custody registrations in
`cmake/tests/94-custody.cmake`. The draft remains schema 99 / generator 25;
its base is now main, whose integrated schema is 98.

## Ordinary carrier journey

Source `5cabed0ac8d06cf87f3efd649d5ecf18322f9d6b` carries seven wheat in a container on an
ordinary royal carriage. The test uses real packing, the shared transfer call,
royal route planning, saved world state, daily movement, and arrival. Unloading
at the destination preserves container ID, goods ID, owner, and quantity.
The dedicated booking reserves two freight slots and yields the carriage back
to other work after unloading. Full routes and repeated departure requests
leave the simulation unchanged. Blocked-road cargo stays on the route under
the existing carriage waiting model.

All 144 headless tests pass. The final repeated-dispatch assertion also passes
in the focused store test. The changed adapter and archive planners pass
Cppcheck; all changed C sources pass strict WebAssembly compilation. The
journey passes AddressSanitizer and UndefinedBehaviorSanitizer with the custody
core, adapter, simulation source, and changed archive planners instrumented.
Remaining libraries use the strict release build.

Player commands, captor/character holders, named-item migration, physical document
work identity, and crate creation/repair remain open acceptance work.

## Booking limits

Source `357bf1c20a92fca334c5b13cf879f6c9fb19fc68` adds regressions for carriage availability
and transfer changes during travel. Both regressions reproduced failures before
their fixes. Dispatch now respects the next available day. Transfers into a
carriage or a carried container require the carriage to be idle.

The test carries 14 wheat in one of two containers. Splitting one wheat into
the second container would raise the load from four to five slots. That request
leaves the complete world unchanged during travel, then succeeds after arrival.
All 144 headless tests pass. Focused Cppcheck and strict WebAssembly checks pass.
The booking test passes sanitizers with the custody core, adapter and simulation
source instrumented; remaining libraries use the strict release build.
