#define _POSIX_C_SOURCE 200809L

#include "client/cc_client_session.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if !defined(__EMSCRIPTEN__) && \
    (defined(__APPLE__) || defined(__linux__) || defined(__unix__))
#include <fcntl.h>
#include <unistd.h>
#endif

static void SetSessionError(char *error, size_t capacity,
                            const char *message)
{
    if (error == NULL || capacity == 0U) return;
    (void)snprintf(error, capacity, "%s", message != NULL ? message : "");
}

static bool ClientSessionValidateBase(const CcClientSession *session)
{
    return session != NULL &&
           session->version == CC_CLIENT_SESSION_VERSION &&
           session->location_id != 0U &&
           session->scene >= CC_CLIENT_SESSION_STREET &&
           session->scene <= CC_CLIENT_SESSION_DRAGON_SITE &&
           session->coordinate_space >= CC_CLIENT_SESSION_LEGACY_LOCAL &&
           session->coordinate_space <= CC_CLIENT_SESSION_WORLD &&
           isfinite(session->position_x) && isfinite(session->position_z) &&
           isfinite(session->facing_yaw) &&
           fabsf(session->position_x) <= 100000.0f &&
           fabsf(session->position_z) <= 100000.0f &&
           fabsf(session->facing_yaw) <= 100000.0f &&
           session->opening_step <= 2U;
}

bool CcClientSessionValidate(const CcClientSession *session)
{
    return ClientSessionValidateBase(session) &&
           (session->coordinate_space != CC_CLIENT_SESSION_WORLD ||
            session->route_id != 0U);
}

bool CcClientSessionWrite(const char *path, const CcClientSession *session,
                          char *error, size_t error_capacity)
{
    if (path == NULL || path[0] == '\0' || !CcClientSessionValidate(session)) {
        SetSessionError(error, error_capacity,
                        "Local session path or state is invalid.");
        return false;
    }
    char temporary[768];
    int length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (length < 0 || (size_t)length >= sizeof(temporary)) {
        SetSessionError(error, error_capacity,
                        "Local session path is too long.");
        return false;
    }
    FILE *file = fopen(temporary, "wb");
    if (file == NULL) {
        SetSessionError(error, error_capacity,
                        "Could not open the local session for writing.");
        return false;
    }
    int written = fprintf(
        file,
        "CROWNLESS_SESSION %u\n%u %llu %d %d %llu %.9g %.9g %.9g %u\n",
        session->version, session->world_seed,
        (unsigned long long)session->location_id, (int)session->scene,
        (int)session->coordinate_space,
        (unsigned long long)session->route_id,
        (double)session->position_x, (double)session->position_z,
        (double)session->facing_yaw, session->opening_step);
    bool ok = written > 0 && fflush(file) == 0;
#if !defined(__EMSCRIPTEN__) && \
    (defined(__APPLE__) || defined(__linux__) || defined(__unix__))
    if (ok) ok = fsync(fileno(file)) == 0;
#endif
    if (fclose(file) != 0) ok = false;
    if (ok && rename(temporary, path) != 0) ok = false;
    if (!ok) {
        (void)remove(temporary);
        SetSessionError(error, error_capacity,
                        "Could not commit the local session.");
        return false;
    }
    SetSessionError(error, error_capacity, "");
    return true;
}

bool CcClientSessionRead(const char *path, CcClientSession *session,
                         char *error, size_t error_capacity)
{
    if (path == NULL || path[0] == '\0' || session == NULL) {
        SetSessionError(error, error_capacity,
                        "Local session path or output is missing.");
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        SetSessionError(error, error_capacity,
                        "No saved local session was found.");
        return false;
    }
    char marker[32] = "";
    unsigned int version = 0U;
    unsigned int world_seed = 0U;
    unsigned long long location_id = 0U;
    int scene = -1;
    int coordinate_space = CC_CLIENT_SESSION_LEGACY_LOCAL;
    unsigned long long route_id = 0U;
    float position_x = 0.0f;
    float position_z = 0.0f;
    float facing_yaw = 0.0f;
    unsigned int opening_step = 2U;
    int header_fields = fscanf(file, "%31s %u", marker, &version);
    int body_fields = 0;
    if (header_fields == 2 && version == CC_CLIENT_SESSION_VERSION) {
        body_fields = fscanf(file, "%u %llu %d %d %llu %f %f %f %u",
                             &world_seed, &location_id, &scene,
                             &coordinate_space, &route_id, &position_x,
                             &position_z, &facing_yaw, &opening_step);
    } else if (header_fields == 2 && version == 3U) {
        body_fields = fscanf(file, "%u %llu %d %d %f %f %f %u",
                             &world_seed, &location_id, &scene,
                             &coordinate_space, &position_x, &position_z,
                             &facing_yaw, &opening_step);
    } else if (header_fields == 2 && (version == 1U || version == 2U)) {
        body_fields = fscanf(file, "%u %llu %d %f %f %f %u",
                             &world_seed, &location_id, &scene,
                             &position_x, &position_z, &facing_yaw,
                             &opening_step);
    }
    bool closed = fclose(file) == 0;
    CcClientSession loaded = {
        .version = (uint32_t)version,
        .world_seed = (uint32_t)world_seed,
        .location_id = (uint64_t)location_id,
        .scene = (CcClientSessionScene)scene,
        .coordinate_space = (CcClientSessionCoordinateSpace)coordinate_space,
        .route_id = (uint64_t)route_id,
        .position_x = position_x,
        .position_z = position_z,
        .facing_yaw = facing_yaw,
        .opening_step = (uint32_t)opening_step
    };
    bool version_one = version == 1U && body_fields == 6;
    bool version_two = version == 2U && body_fields == 7;
    bool version_three = version == 3U && body_fields == 8;
    bool current = version == CC_CLIENT_SESSION_VERSION && body_fields == 9;
    if (version_one || version_two || version_three) {
        loaded.version = CC_CLIENT_SESSION_VERSION;
        if (version_one || version_two) {
            loaded.coordinate_space = CC_CLIENT_SESSION_LEGACY_LOCAL;
        }
        loaded.route_id = 0U;
        if (version_one) loaded.opening_step = 2U;
    }
    bool valid = current ? CcClientSessionValidate(&loaded) :
                           ClientSessionValidateBase(&loaded);
    if ((!version_one && !version_two && !version_three && !current) ||
        !closed ||
        strcmp(marker, "CROWNLESS_SESSION") != 0 ||
        !valid) {
        SetSessionError(error, error_capacity,
                        "Saved local session is invalid or unsupported.");
        return false;
    }
    *session = loaded;
    SetSessionError(error, error_capacity, "");
    return true;
}

bool CcClientInstanceLockAcquire(const char *path,
                                 CcClientInstanceLock *lock,
                                 char *error, size_t error_capacity)
{
    if (path == NULL || path[0] == '\0' || lock == NULL) {
        SetSessionError(error, error_capacity,
                        "Campaign lock path or output is missing.");
        return false;
    }
    lock->descriptor = -1;
#if !defined(__EMSCRIPTEN__) && \
    (defined(__APPLE__) || defined(__linux__) || defined(__unix__))
    int descriptor = open(path, O_CREAT | O_RDWR, 0600);
    if (descriptor < 0) {
        SetSessionError(error, error_capacity,
                        "Could not open the campaign lock.");
        return false;
    }
    struct flock campaign_lock = {
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0
    };
    if (fcntl(descriptor, F_SETLK, &campaign_lock) != 0) {
        (void)close(descriptor);
        SetSessionError(error, error_capacity,
                        errno == EACCES || errno == EAGAIN ?
                            "This campaign is already open." :
                            "Could not lock this campaign.");
        return false;
    }
    lock->descriptor = descriptor;
#else
    (void)path;
#endif
    SetSessionError(error, error_capacity, "");
    return true;
}

void CcClientInstanceLockRelease(CcClientInstanceLock *lock)
{
    if (lock == NULL || lock->descriptor < 0) return;
#if !defined(__EMSCRIPTEN__) && \
    (defined(__APPLE__) || defined(__linux__) || defined(__unix__))
    struct flock campaign_unlock = {
        .l_type = F_UNLCK,
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0
    };
    (void)fcntl(lock->descriptor, F_SETLK, &campaign_unlock);
    (void)close(lock->descriptor);
#endif
    lock->descriptor = -1;
}
