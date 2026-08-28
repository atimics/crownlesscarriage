#ifndef CROWNLESS_CLIENT_SESSION_H
#define CROWNLESS_CLIENT_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CC_CLIENT_SESSION_VERSION UINT32_C(1)

typedef enum CcClientSessionScene {
    CC_CLIENT_SESSION_STREET = 0,
    CC_CLIENT_SESSION_MARKET = 1,
    CC_CLIENT_SESSION_DUNGEON_SITE = 2,
    CC_CLIENT_SESSION_GOBLIN_SITE = 3,
    CC_CLIENT_SESSION_DRAGON_SITE = 4
} CcClientSessionScene;

typedef struct CcClientSession {
    uint32_t version;
    uint32_t world_seed;
    uint64_t location_id;
    CcClientSessionScene scene;
    float position_x;
    float position_z;
    float facing_yaw;
} CcClientSession;

typedef struct CcClientInstanceLock {
    int descriptor;
} CcClientInstanceLock;

bool CcClientSessionValidate(const CcClientSession *session);
bool CcClientSessionWrite(const char *path, const CcClientSession *session,
                          char *error, size_t error_capacity);
bool CcClientSessionRead(const char *path, CcClientSession *session,
                         char *error, size_t error_capacity);

bool CcClientInstanceLockAcquire(const char *path,
                                 CcClientInstanceLock *lock,
                                 char *error, size_t error_capacity);
void CcClientInstanceLockRelease(CcClientInstanceLock *lock);

#endif
