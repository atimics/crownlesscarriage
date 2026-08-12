#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"
#include <sqlite3.h>
#include <stdio.h>

static void RequireSuccess(bool succeeded, const char *operation,
                           const char *error)
{
    if (succeeded) return;
    (void)fprintf(stderr, "%s failed: %s\n", operation,
                  error != NULL ? error : "unknown error");
    exit(EXIT_FAILURE);
}

int main(void)
{
    const char *path = "persistence-test.ccsave";
    const char *missing_path = "missing-persistence-test.ccsave";
    (void)remove(path);
    (void)remove(missing_path);

    CcSim original;
    CcSimInit(&original, UINT32_C(0xa11ce5ed));
    CcSimAdvanceDays(&original, 23);
    char error[256];
    CcCommand command = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = original.settlements[1].id
    };
    RequireSuccess(CcSimApply(&original, &command, error, sizeof(error)),
                   "simulation command", error);
    original.player.athletics.level[CC_ATHLETIC_MOBILITY] = 2;
    original.player.athletics.experience[CC_ATHLETIC_MOBILITY] = 17.25f;
    original.player.athletics.level[CC_ATHLETIC_GRIP] = 3;
    original.player.athletics.experience[CC_ATHLETIC_GRIP] = 54.5f;
    original.player.athletics.level[CC_ATHLETIC_POWER] = 5;
    original.player.athletics.travel_training_distance = 7.75f;
    uint64_t expected = CcSimHash(&original);
    RequireSuccess(CcSaveWrite(path, &original, error, sizeof(error)),
                   "save", error);

    CcSim restored;
    RequireSuccess(CcSaveRead(path, &restored, error, sizeof(error)),
                   "load", error);
    CC_CHECK(CcSimHash(&restored) == expected);
    CC_CHECK(restored.current_day == original.current_day);
    CC_CHECK(restored.player.location_id == original.player.location_id);
    CC_CHECK(restored.player.map_capacity == original.player.map_capacity);
    for (int32_t discipline = 0;
         discipline < CC_ATHLETIC_DISCIPLINE_COUNT; ++discipline) {
        CC_CHECK(restored.player.athletics.level[discipline] ==
                 original.player.athletics.level[discipline]);
        CC_CHECK(restored.player.athletics.experience[discipline] ==
                 original.player.athletics.experience[discipline]);
    }
    CC_CHECK(restored.player.athletics.travel_training_distance ==
             original.player.athletics.travel_training_distance);
    CC_CHECK(restored.map_count == original.map_count);
    CC_CHECK(restored.maps[0].owner_id == original.maps[0].owner_id);
    CC_CHECK(restored.maps[0].recorded_danger ==
             original.maps[0].recorded_danger);
    CC_CHECK(restored.event_count == original.event_count);

    CcSim unchanged = original;
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(database,
                          "UPDATE player_company SET mobility_level=0;",
                          NULL, NULL, NULL) == SQLITE_OK);
    CC_CHECK(sqlite3_close(database) == SQLITE_OK);
    CC_CHECK(!CcSaveRead(path, &unchanged, error, sizeof(error)));
    CC_CHECK(CcSimHash(&unchanged) == expected);

    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(database,
                          "UPDATE player_company SET mobility_level=2;"
                          "UPDATE meta SET state_hash='0000000000000000';",
                          NULL, NULL, NULL) == SQLITE_OK);
    CC_CHECK(sqlite3_close(database) == SQLITE_OK);
    CC_CHECK(!CcSaveRead(path, &unchanged, error, sizeof(error)));
    CC_CHECK(CcSimHash(&unchanged) == expected);

    CC_CHECK(!CcSaveRead(missing_path, &unchanged, error, sizeof(error)));
    CC_CHECK(CcSimHash(&unchanged) == expected);
    FILE *missing_file = fopen(missing_path, "rb");
    CC_CHECK(missing_file == NULL);
    if (missing_file != NULL) (void)fclose(missing_file);

    (void)remove(path);
    (void)remove(missing_path);
    puts("SQLite persistence tests passed");
    return 0;
}
