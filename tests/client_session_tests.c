#include "client/cc_client_session.h"
#include "test_support.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#endif

static bool AthleticProfilesMatch(const CcClientAthleticProfile *first,
                                  const CcClientAthleticProfile *second)
{
    if (fabsf(first->travel_training_distance -
              second->travel_training_distance) >= 0.0001f) {
        return false;
    }
    for (int32_t discipline = 0;
         discipline < CC_CLIENT_SESSION_ATHLETIC_COUNT; ++discipline) {
        if (first->level[discipline] != second->level[discipline] ||
            fabsf(first->experience[discipline] -
                  second->experience[discipline]) >= 0.0001f) {
            return false;
        }
    }
    return true;
}

static bool AthleticProfileIsDefault(
    const CcClientAthleticProfile *profile)
{
    CcClientAthleticProfile expected = {0};
    for (int32_t discipline = 0;
         discipline < CC_CLIENT_SESSION_ATHLETIC_COUNT; ++discipline) {
        expected.level[discipline] = 1;
    }
    return AthleticProfilesMatch(profile, &expected);
}

static bool RewriteSessionAsLegacy(const char *path, int version)
{
    const char *temporary = "client-session-version-five.tmp";
    FILE *source = fopen(path, "rb");
    FILE *target = fopen(temporary, "wb");
    char line[2048];
    bool ok = source != NULL && target != NULL &&
        fgets(line, sizeof(line), source) != NULL &&
        fprintf(target, "CROWNLESS_SESSION %d\n", version) >= 0 &&
        fgets(line, sizeof(line), source) != NULL &&
        fputs(line, target) >= 0 &&
        fgets(line, sizeof(line), source) != NULL &&
        strncmp(line, "ATHLETICS ", 10U) == 0;
    if (ok && version == 6) ok = fputs(line, target) >= 0;
    while (ok && fgets(line, sizeof(line), source) != NULL) {
        ok = fputs(line, target) >= 0;
    }
    if (source != NULL && fclose(source) != 0) ok = false;
    if (target != NULL && fclose(target) != 0) ok = false;
    if (ok) ok = remove(path) == 0 && rename(temporary, path) == 0;
    if (!ok) (void)remove(temporary);
    return ok;
}

int main(void)
{
    const char *session_path = "client-session-test.state";
    const char *lock_path = "client-session-test.lock";
    (void)remove(session_path);
    (void)remove(lock_path);

    CcClientSession original = {
        .version = CC_CLIENT_SESSION_VERSION,
        .world_seed = UINT32_C(0xc0a71a9e),
        .location_id = UINT64_C(42),
        .scene = CC_CLIENT_SESSION_MARKET,
        .coordinate_space = CC_CLIENT_SESSION_WORLD,
        .route_id = UINT64_C(77),
        .position_x = 6.25f,
        .position_z = 2.75f,
        .facing_yaw = -0.35f,
        .opening_step = 1U,
        .athletics = {
            .experience = {12.5f, 23.5f, 34.5f},
            .travel_training_distance = 8.25f,
            .level = {2, 3, 4}
        }
    };
    char error[256];
    CC_CHECK(CcClientSessionValidate(&original));
    CC_CHECK(CcClientSessionWrite(session_path, &original,
                                  error, sizeof(error)));

    CcClientSession restored = {0};
    CC_CHECK(CcClientSessionRead(session_path, &restored,
                                 error, sizeof(error)));
    CC_CHECK(restored.version == original.version);
    CC_CHECK(restored.world_seed == original.world_seed);
    CC_CHECK(restored.location_id == original.location_id);
    CC_CHECK(restored.scene == original.scene);
    CC_CHECK(restored.coordinate_space == original.coordinate_space);
    CC_CHECK(restored.route_id == original.route_id);
    CC_CHECK(fabsf(restored.position_x - original.position_x) < 0.0001f);
    CC_CHECK(fabsf(restored.position_z - original.position_z) < 0.0001f);
    CC_CHECK(fabsf(restored.facing_yaw - original.facing_yaw) < 0.0001f);
    CC_CHECK(restored.opening_step == original.opening_step);
    CC_CHECK(AthleticProfilesMatch(&restored.athletics,
                                   &original.athletics));

    CC_CHECK(RewriteSessionAsLegacy(session_path, 6));
    CC_CHECK(CcClientSessionRead(session_path, &restored,
                                 error, sizeof(error)));
    CC_CHECK(restored.version == CC_CLIENT_SESSION_VERSION);
    CC_CHECK(AthleticProfilesMatch(&restored.athletics,
                                   &original.athletics));
    CC_CHECK(RewriteSessionAsLegacy(session_path, 5));
    CC_CHECK(CcClientSessionRead(session_path, &restored,
                                 error, sizeof(error)));
    CC_CHECK(restored.version == CC_CLIENT_SESSION_VERSION);
    CC_CHECK(AthleticProfileIsDefault(&restored.athletics));

    CcClientSession encounter = original;
    encounter.scene = CC_CLIENT_SESSION_STREET;
    encounter.coordinate_space = CC_CLIENT_SESSION_LEGACY_LOCAL;
    encounter.route_id = 0U;
    encounter.road_encounter.mode = CC_CLIENT_ROAD_ENCOUNTER_FIGHT;
    encounter.road_encounter.player.position_x = 46.25f;
    encounter.road_encounter.player.position_z = 40.5f;
    encounter.road_encounter.player.health = 62.0f;
    encounter.road_encounter.player.posture = 38.0f;
    encounter.road_encounter.player.target_index = -1;
    encounter.road_encounter.player.queued_skill = -1;
    encounter.road_encounter.player.active_skill = -1;
    encounter.road_encounter.player.life_state = 0;
    encounter.road_encounter.player.weapon_mode = 1;
    for (int actor = 0; actor < CC_CLIENT_SESSION_GUARD_COUNT; ++actor) {
        encounter.road_encounter.guards[actor] =
            encounter.road_encounter.player;
        encounter.road_encounter.guards[actor].position_x += (float)actor;
    }
    for (int actor = 0; actor < CC_CLIENT_SESSION_RAIDER_COUNT; ++actor) {
        encounter.road_encounter.raiders[actor] =
            encounter.road_encounter.player;
        encounter.road_encounter.raiders[actor].position_z += (float)actor;
    }
    encounter.road_encounter.engagement_time = 4.5f;
    encounter.road_encounter.alarm_countdown = 1.25f;
    encounter.road_encounter.raider_initial_resolve = 80;
    encounter.road_encounter.raider_resolve = 51;
    encounter.road_encounter.alarm_active = true;
    CC_CHECK(CcClientSessionValidate(&encounter));
    CC_CHECK(CcClientSessionWrite(session_path, &encounter,
                                  error, sizeof(error)));
    CC_CHECK(CcClientSessionRead(session_path, &restored,
                                 error, sizeof(error)));
    CC_CHECK(restored.road_encounter.mode ==
             CC_CLIENT_ROAD_ENCOUNTER_FIGHT);
    CC_CHECK(fabsf(restored.road_encounter.player.health - 62.0f) < 0.0001f);
    CC_CHECK(fabsf(restored.road_encounter.player.posture - 38.0f) < 0.0001f);
    CC_CHECK(restored.road_encounter.raider_resolve == 51);
    CC_CHECK(restored.road_encounter.alarm_active);

    CcClientSession invalid = original;
    invalid.version += 1U;
    CC_CHECK(!CcClientSessionValidate(&invalid));
    invalid = original;
    invalid.coordinate_space = (CcClientSessionCoordinateSpace)99;
    CC_CHECK(!CcClientSessionValidate(&invalid));
    invalid = original;
    invalid.position_x = INFINITY;
    CC_CHECK(!CcClientSessionValidate(&invalid));
    invalid = original;
    invalid.scene = CC_CLIENT_SESSION_DRAGON_SITE;
    CC_CHECK(CcClientSessionValidate(&invalid));
    invalid.scene = (CcClientSessionScene)99;
    CC_CHECK(!CcClientSessionValidate(&invalid));
    invalid = original;
    invalid.opening_step = 3U;
    CC_CHECK(!CcClientSessionValidate(&invalid));
    invalid = original;
    invalid.route_id = 0U;
    CC_CHECK(!CcClientSessionValidate(&invalid));
    invalid = original;
    invalid.athletics.level[0] = 0;
    CC_CHECK(!CcClientSessionValidate(&invalid));
    invalid = original;
    invalid.athletics.experience[1] = INFINITY;
    CC_CHECK(!CcClientSessionValidate(&invalid));
    invalid = original;
    invalid.athletics.travel_training_distance = 12.0f;
    CC_CHECK(!CcClientSessionValidate(&invalid));
    CC_CHECK(!CcClientSessionWrite(session_path, &invalid,
                                   error, sizeof(error)));

    FILE *version_four_without_route = fopen(session_path, "wb");
    CC_CHECK(version_four_without_route != NULL);
    CC_CHECK(fputs(
        "CROWNLESS_SESSION 4\n3232176798 42 0 1 0 17.5 12.5 -0.5 2\n",
        version_four_without_route) >= 0);
    CC_CHECK(fclose(version_four_without_route) == 0);
    CC_CHECK(!CcClientSessionRead(session_path, &restored,
                                  error, sizeof(error)));

    FILE *version_four = fopen(session_path, "wb");
    CC_CHECK(version_four != NULL);
    CC_CHECK(fputs(
        "CROWNLESS_SESSION 4\n3232176798 42 0 1 77 17.5 12.5 -0.5 2\n",
        version_four) >= 0);
    CC_CHECK(fclose(version_four) == 0);
    CC_CHECK(CcClientSessionRead(session_path, &restored,
                                 error, sizeof(error)));
    CC_CHECK(restored.version == CC_CLIENT_SESSION_VERSION);
    CC_CHECK(restored.route_id == 77U);
    CC_CHECK(restored.road_encounter.mode ==
             CC_CLIENT_ROAD_ENCOUNTER_NONE);
    CC_CHECK(AthleticProfileIsDefault(&restored.athletics));

    FILE *legacy = fopen(session_path, "wb");
    CC_CHECK(legacy != NULL);
    CC_CHECK(fputs(
        "CROWNLESS_SESSION 1\n3232176798 42 0 3.5 4.5 -0.25\n",
        legacy) >= 0);
    CC_CHECK(fclose(legacy) == 0);
    CC_CHECK(CcClientSessionRead(session_path, &restored,
                                 error, sizeof(error)));
    CC_CHECK(restored.version == CC_CLIENT_SESSION_VERSION);
    CC_CHECK(restored.opening_step == 2U);
    CC_CHECK(restored.coordinate_space == CC_CLIENT_SESSION_LEGACY_LOCAL);
    CC_CHECK(restored.route_id == 0U);
    CC_CHECK(AthleticProfileIsDefault(&restored.athletics));

    FILE *version_two = fopen(session_path, "wb");
    CC_CHECK(version_two != NULL);
    CC_CHECK(fputs(
        "CROWNLESS_SESSION 2\n3232176798 42 0 8.5 9.5 0.25 1\n",
        version_two) >= 0);
    CC_CHECK(fclose(version_two) == 0);
    CC_CHECK(CcClientSessionRead(session_path, &restored,
                                 error, sizeof(error)));
    CC_CHECK(restored.version == CC_CLIENT_SESSION_VERSION);
    CC_CHECK(restored.opening_step == 1U);
    CC_CHECK(restored.coordinate_space == CC_CLIENT_SESSION_LEGACY_LOCAL);
    CC_CHECK(restored.route_id == 0U);

    FILE *version_three = fopen(session_path, "wb");
    CC_CHECK(version_three != NULL);
    CC_CHECK(fputs(
        "CROWNLESS_SESSION 3\n3232176798 42 0 1 17.5 12.5 -0.5 2\n",
        version_three) >= 0);
    CC_CHECK(fclose(version_three) == 0);
    CC_CHECK(CcClientSessionRead(session_path, &restored,
                                 error, sizeof(error)));
    CC_CHECK(restored.version == CC_CLIENT_SESSION_VERSION);
    CC_CHECK(restored.coordinate_space == CC_CLIENT_SESSION_WORLD);
    CC_CHECK(restored.route_id == 0U);

    FILE *corrupt = fopen(session_path, "wb");
    CC_CHECK(corrupt != NULL);
    CC_CHECK(fputs("not a session\n", corrupt) >= 0);
    CC_CHECK(fclose(corrupt) == 0);
    CC_CHECK(!CcClientSessionRead(session_path, &restored,
                                  error, sizeof(error)));

    CcClientInstanceLock lock = {.descriptor = -1};
    CC_CHECK(CcClientInstanceLockAcquire(lock_path, &lock,
                                         error, sizeof(error)));
#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
    CC_CHECK((fcntl(lock.descriptor, F_GETFD) & FD_CLOEXEC) != 0);
    pid_t child = fork();
    CC_CHECK(child >= 0);
    if (child == 0) {
        CcClientInstanceLock duplicate = {.descriptor = -1};
        char duplicate_error[128];
        bool acquired = CcClientInstanceLockAcquire(
            lock_path, &duplicate, duplicate_error, sizeof(duplicate_error));
        if (acquired) CcClientInstanceLockRelease(&duplicate);
        _exit(!acquired && strstr(duplicate_error, "already open") != NULL ?
                  0 : 1);
    }
    int child_status = 0;
    CC_CHECK(waitpid(child, &child_status, 0) == child);
    CC_CHECK(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
#endif
    CcClientInstanceLockRelease(&lock);
    CC_CHECK(lock.descriptor == -1);

    (void)remove(session_path);
    (void)remove(lock_path);
    puts("Client session tests passed");
    return 0;
}
