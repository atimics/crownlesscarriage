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
        .position_x = 6.25f,
        .position_z = 2.75f,
        .facing_yaw = -0.35f
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
    CC_CHECK(fabsf(restored.position_x - original.position_x) < 0.0001f);
    CC_CHECK(fabsf(restored.position_z - original.position_z) < 0.0001f);
    CC_CHECK(fabsf(restored.facing_yaw - original.facing_yaw) < 0.0001f);

    CcClientSession invalid = original;
    invalid.version += 1U;
    CC_CHECK(!CcClientSessionValidate(&invalid));
    invalid = original;
    invalid.position_x = INFINITY;
    CC_CHECK(!CcClientSessionValidate(&invalid));

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
