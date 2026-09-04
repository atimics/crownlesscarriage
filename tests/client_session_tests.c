#include "client/cc_client_session.h"
#include "test_support.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#include <sys/wait.h>
#include <unistd.h>
#endif

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
        .opening_step = 1U
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
