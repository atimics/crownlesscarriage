#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>

static void RemoveSave(const char *path)
{
    char sidecar[256];
    (void)remove(path);
    (void)snprintf(sidecar, sizeof(sidecar), "%s-wal", path);
    (void)remove(sidecar);
    (void)snprintf(sidecar, sizeof(sidecar), "%s-shm", path);
    (void)remove(sidecar);
}

static void ResolveEncounter(CcSim *sim, char *error, size_t error_capacity)
{
    if (sim->dungeon_expedition.encounter_kind ==
        CC_DUNGEON_ENCOUNTER_NONE) return;
    sim->dungeon_expedition.encounter_reaction = 12;
    CcCommand parley = {
        .kind = CC_COMMAND_RESOLVE_DUNGEON_ENCOUNTER,
        .amount = CC_DUNGEON_APPROACH_PARLEY
    };
    CC_CHECK(CcSimApply(sim, &parley, error, error_capacity));
    CC_CHECK(sim->dungeon_expedition.encounter_kind ==
             CC_DUNGEON_ENCOUNTER_NONE);
}

static void MoveTo(CcSim *sim, int32_t room, char *error,
                   size_t error_capacity)
{
    ResolveEncounter(sim, error, error_capacity);
    CcCommand move = {
        .kind = CC_COMMAND_MOVE_DUNGEON,
        .amount = room
    };
    CC_CHECK(CcSimApply(sim, &move, error, error_capacity));
}

static void Begin(CcSim *sim, char *error, size_t error_capacity)
{
    sim->player.location_id = sim->dungeons[0].settlement_id;
    sim->carriage.location_id = sim->player.location_id;
    sim->player.cargo_capacity = 32;
    sim->player.cargo[CC_GOOD_FOOD] = 18;
    sim->player.cargo[CC_GOOD_TOOLS] = 5;
    sim->player.cargo[CC_GOOD_WEAPONS] = 1;
    CcCommand begin = {
        .kind = CC_COMMAND_BEGIN_DUNGEON_EXPEDITION,
        .target_id = sim->dungeons[0].id
    };
    CC_CHECK(CcSimApply(sim, &begin, error, error_capacity));
}

int main(void)
{
    char error[256];
    CcSim first;
    CcSim second;
    CcSimInit(&first, UINT32_C(0x71a7e5));
    CcSimInit(&second, UINT32_C(0x71a7e5));
    CC_CHECK(first.dungeons[0].room_count == CC_MAX_DUNGEON_ROOMS);
    CC_CHECK(first.dungeons[0].link_count >= 30);
    CC_CHECK(CcSimHash(&first) == CcSimHash(&second));
    CC_CHECK(first.dungeons[0].layout_seed == second.dungeons[0].layout_seed);

    Begin(&second, error, sizeof(error));
    MoveTo(&second, 1, error, sizeof(error));
    MoveTo(&second, 2, error, sizeof(error));
    ResolveEncounter(&second, error, sizeof(error));
    int32_t goods_before_retreat[CC_GOOD_COUNT];
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        goods_before_retreat[good] = CcSimTrackedGood(&second, (CcGood)good);
    }
    second.dungeon_expedition.strain = 74;
    CcCommand forced_retreat = {.kind = CC_COMMAND_RETREAT_DUNGEON};
    CC_CHECK(CcSimApply(&second, &forced_retreat, error, sizeof(error)));
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        CC_CHECK(CcSimTrackedGood(&second, (CcGood)good) ==
                 goods_before_retreat[good]);
    }

    Begin(&first, error, sizeof(error));
    CC_CHECK(first.dungeon_expedition.current_room == 0);
    CC_CHECK(CcSimDungeonVisibleExitCount(&first) == 1);
    CcCommand travel_while_below = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = first.settlements[2].id
    };
    CC_CHECK(!CcSimApply(&first, &travel_while_below,
                         error, sizeof(error)));
    CC_CHECK(first.dungeon_expedition.active);
    CC_CHECK(first.dungeon_expedition.current_room == 0);
    MoveTo(&first, 1, error, sizeof(error));
    MoveTo(&first, 3, error, sizeof(error));
    MoveTo(&first, 6, error, sizeof(error));
    MoveTo(&first, 21, error, sizeof(error));
    MoveTo(&first, 22, error, sizeof(error));
    ResolveEncounter(&first, error, sizeof(error));

    CcCommand search = {.kind = CC_COMMAND_SEARCH_DUNGEON};
    int32_t iron_before = CcSimTrackedGood(&first, CC_GOOD_IRON);
    CC_CHECK(CcSimApply(&first, &search, error, sizeof(error)));
    ResolveEncounter(&first, error, sizeof(error));
    CC_CHECK(CcSimTrackedGood(&first, CC_GOOD_IRON) == iron_before);
    int32_t shortcut = CcSimDungeonOpenableShortcut(&first);
    CC_CHECK(shortcut >= 0);
    CcCommand open = {
        .kind = CC_COMMAND_OPEN_DUNGEON_SHORTCUT,
        .amount = shortcut
    };
    CC_CHECK(CcSimApply(&first, &open, error, sizeof(error)));
    ResolveEncounter(&first, error, sizeof(error));
    CC_CHECK((first.dungeons[0].links[shortcut].flags &
              CC_DUNGEON_LINK_OPEN) != 0U);

    const char *save_path = "underroad-round-trip.ccsave";
    RemoveSave(save_path);
    uint64_t saved_hash = CcSimHash(&first);
    CC_CHECK(CcSaveWrite(save_path, &first, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(save_path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == saved_hash);
    CC_CHECK(restored.dungeon_expedition.active);
    CC_CHECK(restored.dungeon_expedition.current_room == 22);
    CC_CHECK((restored.dungeons[0].links[shortcut].flags &
              CC_DUNGEON_LINK_OPEN) != 0U);

    MoveTo(&restored, 1, error, sizeof(error));
    MoveTo(&restored, 0, error, sizeof(error));
    CcCommand leave = {.kind = CC_COMMAND_RETREAT_DUNGEON};
    CC_CHECK(CcSimApply(&restored, &leave, error, sizeof(error)));
    CC_CHECK(!restored.dungeon_expedition.active);

    Begin(&restored, error, sizeof(error));
    const int32_t threshold_path[] = {1, 2, 4, 8, 9, 10, 13, 16, 17, 19};
    for (size_t i = 0;
         i < sizeof(threshold_path) / sizeof(threshold_path[0]); ++i) {
        MoveTo(&restored, threshold_path[i], error, sizeof(error));
    }
    ResolveEncounter(&restored, error, sizeof(error));
    CC_CHECK((restored.dungeons[0].rooms[19].state_flags &
              CC_DUNGEON_ROOM_OBJECTIVE_REACHED) != 0U);
    CC_CHECK(restored.dungeons[0].state == CC_DUNGEON_EXPLORED);
    CC_CHECK(CcSimApply(&restored, &leave, error, sizeof(error)));

    restored.player.cargo[CC_GOOD_TOOLS] = 2;
    restored.player.coins = 20;
    CcCommand public_road = {
        .kind = CC_COMMAND_CHANGE_DUNGEON,
        .target_id = restored.dungeons[0].id,
        .dungeon_state = CC_DUNGEON_PUBLIC_ROUTE
    };
    CC_CHECK(CcSimApply(&restored, &public_road, error, sizeof(error)));
    CC_CHECK(restored.dungeons[0].state == CC_DUNGEON_PUBLIC_ROUTE);
    CC_CHECK(!restored.routes[6].closed);
    CC_CHECK(CcSimValidate(&restored, error, sizeof(error)));

    const char *journal_path = "underroad-journal-replay.ccsave";
    RemoveSave(journal_path);
    CcSim journal_sim;
    CcSimInit(&journal_sim, UINT32_C(0x0ddba11));
    journal_sim.player.location_id = journal_sim.dungeons[0].settlement_id;
    journal_sim.carriage.location_id = journal_sim.player.location_id;
    journal_sim.player.cargo[CC_GOOD_FOOD] = 4;
    CcJournal *journal = CcJournalStart(
        journal_path, &journal_sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CcCommand journal_begin = {
        .kind = CC_COMMAND_BEGIN_DUNGEON_EXPEDITION,
        .target_id = journal_sim.dungeons[0].id
    };
    CC_CHECK(CcJournalApply(journal, &journal_sim, &journal_begin,
                            error, sizeof(error)));
    CcCommand journal_move = {
        .kind = CC_COMMAND_MOVE_DUNGEON,
        .amount = 1
    };
    CC_CHECK(CcJournalApply(journal, &journal_sim, &journal_move,
                            error, sizeof(error)));
    uint64_t journal_hash = CcSimHash(&journal_sim);
    CcJournalAbandon(&journal);
    CcSim replayed;
    journal = CcJournalResume(journal_path, &replayed,
                              error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcSimHash(&replayed) == journal_hash);
    CC_CHECK(replayed.dungeon_expedition.active);
    CC_CHECK(replayed.dungeon_expedition.current_room == 1);
    CC_CHECK(CcJournalClose(&journal, &replayed, error, sizeof(error)));

    RemoveSave(save_path);
    RemoveSave(journal_path);
    puts("Persistent Underroad expedition tests passed");
    return 0;
}
