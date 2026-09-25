# Simulation and rendered world review

24 September 2026. Audited revision:
[`0f196d031dc65d5a8beca2938e43ec62e07c4392`](https://github.com/atimics/crownlesscarriage/commit/0f196d031dc65d5a8beca2938e43ec62e07c4392).
Schema 113, generator 25. The earlier measurement revision, `6173f617`, has the
same simulation and world code; the intervening changes affect browser tests.

The [design proposal](../../design/living-world.md) turns this audit and the game
research into a staged plan. This directory records the current implementation.
The proposal describes future work.

## Scope and method

Three research agents reviewed MUDs and people, city builders and 4X games, and
economic games. They then traced those subjects through the current code. The
main review checked the spatial conversions, population and food measurements,
and existing visual evidence, then combined the findings into one design.

The [probe](scale_probe.c) starts one new world with seed `0x3235a7ed`. Its
[saved output](scale-snapshot.json) contains counts, building profiles, food use,
route lengths, and record sizes. It reads the simulation without advancing time.
The counts describe this seed and starting state.

The scene review used the repository's
[24 September captures](../../screenshots/2026-09-24/README.md), including
Thornford and the road near Low Silver Pit. Those are staged captures with their
own build and capture records. They show the current street framing and place
links. Runtime proof of the proposed household and district model belongs to the
implementation milestones.

## Population and visible places

| Settlement | Reported population | Named home records | Main building records | Compound structures | Civilian rations per week |
|---|---:|---:|---:|---:|---:|
| Thornford | 1,463 | 12 | 8 | 9 | 5 |
| Gloamgate | 2,282 | 18 | 10 | 12 | 7 |
| Alderwatch | 1,606 | 13 | 9 | 12 | 6 |
| Silverwick | 2,281 | 18 | 11 | 10 | 7 |
| Rosespire | 3,233 | 24 | 12 | 11 | 8 |
| Hollowbarrow | 1,458 | 12 | 7 | 9 | 5 |
| **Total** | **12,323** | **97** | **57** | **63** | **38** |

Main buildings and compound structures are authored visual records. Compounds
include walls, halls, towers, stores, and silos. These counts measure the profile,
while housing requires separate room and occupancy measurements.

The record cap is 256 named people worldwide, with up to 128 in active detail.
The ordinary resident target uses `3 + population / 150`, capped at 24. Visiting
a settlement can expand its named cast toward 32. The initial 97 people are a
sample of the reported population. Each has a settlement home reference.
[Limits and person fields][people-types], [resident targets and visit growth][resident-target]

Building profiles store names, positions, dimensions, styles, and doors. Their
function selects a fixed layout. The wider census needs saved homes, households,
workplaces, and district membership linked to those placed buildings.
[Building records][buildings], [profile selection][profiles]

Population capacity currently comes from settlement class, with farm and market
bonuses. Every 28 days, hunger and prosperity can change scalar population.
Refugee transfers move that scalar and named people through separate steps.
[Class capacity][capacity], [population changes][population], [refugee moves][refugees]

Named deaths follow a separate replacement lifecycle. A newborn takes the previous
slot and receives its role, occupation, goal, and some titles. Validation requires
equal character births and deaths. Actual households require independent life
events, dependent children, vacancies, and succession.
[Replacement lifecycle][lifecycle], [lifecycle invariant][lifecycle-check]

## Daily life and rendering

The routine helper selects home before 07:00 and after 21:00, work until 17:00,
and the inn in the evening. It serves descriptive text. Occupations derive from
town services and resident order. The next step is to assign those routines to
specific destinations, job slots, and time budgets.
[Routine][routine], [occupations][occupations]

Away travellers buy food and lodging from town funds and services. Home arrival
resets their hunger and shelter counters. The traveller path already connects
people with prices and money; households need an equivalent pantry, purse, and
bed capacity at home. Named archive staff already provide a useful labour check
based on identity, occupation, activity, and location.
[Traveller needs][travellers], [archive staff][archive-staff]

The town scene draws figures in fixed roles and ambient paths. Interaction then
binds available named people to those roles. A person-driven scene should instead
read each person's actual current place and task. Keep the existing name,
appearance, conversation target, introduction, and history links.
[Scene figures][figures], [identity binding][binding]

The measured `CcSim` is 656,024 bytes and `CcCharacter` is 952 bytes on the review
machine's arm64 C build. A compact census plus optional rich detail is a useful
design direction. CPU and save costs still require measured trials at the target
population. Current active-detail selection already prioritises important people
and commitments; long-term routine work needs to continue for the whole census.
[Active detail][active-cast], [saved people][saved-people]

## Food, work, capacity, and money

| Finding | Current evidence | Consequence for the design |
|---|---|---|
| Town food use and personal food use have different scales | Civilian food becomes the authored town consumption at population 600; an adult away traveller buys one ration daily | Thornford uses five civilian rations per week; one traveller uses seven. Establish one food quantity model across town and person. [Food use][food], [traveller meals][travellers] |
| Work usually comes from configured budgets | Bakery work equals capacity; smithy and paper budgets use `INT32_MAX`; roadside works receive two units per week, or six for road crews | Connect existing recipe checks to actual worker time. Farm labour reaches its maximum factor at 1,300 people. [Town work][production], [site work][site-work] |
| Towns pool their stores | Primary output enters settlement stock; roadside sites hold local stock and use carriage delivery | Extend placed stores across town districts. Derive town stock summaries from those stores. [Town output][town-output], [site delivery][site-delivery] |
| Cargo slots use two quantity rules | One player slot holds one unit; freight holds eight bread, ten wheat, six wood, or two tools; both carriage constants are twelve slots | A wheat load can reach 120 in one carrier and twelve in another before other limits. Define mass, volume, packages, and actual vehicle capacity. [Goods definitions][goods], [capacity constants][constants] |
| Current freight has real transfer and payment steps | Carrier booking, source removal, buyer payment, seller receipt, departure, arrival, incoming reservations, and partial unloading exist | Reuse these mechanics for local and regional logistics. [Dispatch][dispatch], [reservations][reservations], [unloading][site-delivery] |
| Household money is partly pooled | Trade, tax, relief, wages, and casual work transfer real funds; several wages credit the town market | Allocate market funds to named public, workplace, and household accounts during migration. Preserve the total through account transfers. [Money total][money], [tax and wages][tax-wages] |
| A food sale changes hunger immediately | Selling food lowers town hunger while food remains in stock; scheduled eating happens weekly | Delivery should change stock coverage; eating should satisfy people's hunger. [Sale][sale], [weekly use][town-output] |
| The goods-total helper has a coverage gap | `CcSimTrackedGood` counts town stores, shipments, custody, dungeon and creature stores; road-site stock sits outside that helper | Include all stores before using the helper to prove migration conservation. Site tests already reconcile local stock. [Tracked goods][tracked-goods], [site tests][site-tests] |

The food multiplication in the proposal is a balance illustration: `1463 * 7 =
10241` rations per week at one ration per person per day, compared with five in
the current civilian rule. It is a proposed shared-unit baseline. Age, activity,
animal feed, reserves, yields, and prices require joint tuning.

## Space and time

The authored local patch is 96 by 72 units. World settlement placement uses
radius 38, 42, or 46 and derives `profile_scale = (radius - 3) / 60`.

| Class | Profile scale | Mapped local patch |
|---|---:|---:|
| Village | 0.583333 | 56 × 42 world units |
| Town | 0.65 | 62.4 × 46.8 world units |
| Capital | 0.716667 | 68.8 × 51.6 world units |

The world renderer applies this scale to buildings. Carriage scale also changes
with the arrival camera. A metre-based design should move the camera while keeping
physical dimensions stable. [Local patch][patch], [world placement][placement],
[world drawing][world-draw], [arrival scale][arrival-scale]

| Route | Days | Full path, world units | Active journey, world units | Derived road-house route miles |
|---|---:|---:|---:|---:|
| Thornford → Gloamgate | 2 | 261.356 | 204.556 | 43 |
| Gloamgate → Alderwatch | 2 | 238.014 | 180.414 | 44 |
| Alderwatch → Silverwick | 2 | 120.160 | 62.560 | 45 |
| Silverwick → Rosespire | 3 | 280.958 | 222.558 | 61 |
| Rosespire → Hollowbarrow | 3 | 343.246 | 285.647 | 62 |
| Hollowbarrow → Gloamgate | 3 | 222.619 | 165.820 | 66 |
| Gloamgate → Silverwick | 4 | 316.703 | 259.103 | 79 |
| Alderwatch → Rosespire | 3 | 237.962 | 179.562 | 64 |

The last column reproduces an intermediate formula used for road-house directions:
`days * 18 + 4 + seed % 9`. The final road-house distance uses a fraction of that
value. Treat it as a derived narrative quantity. The road geometry stores both
full and junction-to-junction journey lengths. [Narrative distance][journey],
[geometry lengths][geometry]

There are two distinct distance issues:

1. Saved `speed_milli_per_second` still derives from a fixed 52-metre represented
   route. Its consumers found in this review assign, save, restore, hash, or
   validate it. Actual position uses journey progress or saved leg distance and
   elapsed time. The finding is stale speed metadata alongside separate movement
   rules. [Speed calculation][journey], [movement][journey-runtime]
2. Road-view wheel travel uses full route length times progress. Active world
   travel maps progress between junction samples. For this seed, full distance
   is about 28% longer than the active Thornford–Gloamgate interval and 92% longer
   for Alderwatch–Silverwick. Use the same travelled interval in both views.
   [Road-view distance][road-distance], [world journey mapping][world-journey]

Clock rules also depend on activity. Idle campaign time has rate zero. Active
travel uses 30 game minutes per real second. Mine yard steps spend one minute;
underground movement spends five minutes per six steps, or per three steps while
carrying a crate. Mine cells render at one world unit each. The proposal separates
walking time from searching and work, then schedules all activity on one clock.
[Clock constants][constants], [mine time][mine-time], [mine scale][mine-scale]

Existing segment and junction IDs, saved road distance, and Stag's Mill spur offer
a base for district roads. Generalise the place graph and replace name-based
topology special cases as the world expands. [Road topology][topology]

## Reproduction and verification

From the repository root, use an external build directory:

```sh
cmake -S . -B /private/tmp/crownless-simulation-scale-build \
  -DCC_BUILD_CLIENT=OFF -DCC_BUILD_BENCHMARKS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build /private/tmp/crownless-simulation-scale-build \
  --target crownless_local_place crownless_world \
  world_stream_tests road_position_tests sim_tests local_place_tests --parallel 4
cc -std=c11 -Wall -Wextra -Werror -Isrc \
  docs/reviews/living-world-2026-09-24/scale_probe.c \
  /private/tmp/crownless-simulation-scale-build/libcrownless_local_place.a \
  /private/tmp/crownless-simulation-scale-build/libcrownless_world.a \
  /private/tmp/crownless-simulation-scale-build/libcrownless_sim.a \
  -lm -o /private/tmp/crownless-living-world-probe
/private/tmp/crownless-living-world-probe
ctest --test-dir /private/tmp/crownless-simulation-scale-build \
  -R '^(deterministic_simulation|saved_road_position|distinct_local_places|finite_world_streaming)$' \
  --output-on-failure
```

The probe compiled with warnings treated as errors. The four focused baseline
tests passed again at the audited revision: deterministic simulation (8.89 s),
saved road position (0.01 s), distinct local places (0.01 s), and finite world
streaming (0.49 s). The test runner reported 9.40 seconds in total. The committed
probe and snapshot make the measured scale claims reproducible. Gameplay
implementation and balance trials follow the milestones in the design proposal.

[people-types]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.h#L1743
[resident-target]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L12144
[buildings]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/client/cc_local_place.h#L94
[profiles]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/client/cc_local_place.c#L740
[capacity]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L2524
[population]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L6003
[refugees]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L5703
[lifecycle]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L12476
[lifecycle-check]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L20427
[routine]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L1393
[occupations]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_occupations.c#L48
[travellers]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L16455
[archive-staff]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_archive_staff.c#L12
[figures]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/client/local3d/road_book.inc#L2328
[binding]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/client/cc_adventure.inc#L177
[active-cast]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_active_cast.c#L80
[saved-people]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/persistence/cc_save.c#L3180
[food]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_food_economy.c#L48
[production]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_production.c#L237
[site-work]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_road_production.inc#L50
[town-output]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L5831
[site-delivery]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_site_carriages.inc#L54
[goods]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_goods.c#L8
[constants]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.h#L35
[dispatch]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L11044
[reservations]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_site_freight_plan.inc#L13
[money]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L2685
[tax-wages]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L15500
[sale]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L17107
[tracked-goods]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_sim.c#L2728
[site-tests]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/tests/road_production_tests.c#L57
[patch]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/client/cc_local3d.h#L26
[placement]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/world/cc_world.c#L449
[world-draw]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/client/local3d/open_world.inc#L269
[arrival-scale]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/client/local3d/open_world.inc#L1256
[journey]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_journey.c#L12
[geometry]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_road_position.c#L265
[journey-runtime]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_journey_runtime.c#L276
[road-distance]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/client/local3d/road_book.inc#L1055
[world-journey]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/world/cc_world.c#L207
[mine-time]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_mine.c#L473
[mine-scale]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/client/local3d/mine_scene.inc#L73
[topology]: https://github.com/atimics/crownlesscarriage/blob/0f196d031dc65d5a8beca2938e43ec62e07c4392/src/sim/cc_road_position.c#L451
