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

static bool SessionFloatInRange(float value, float minimum, float maximum)
{
    return isfinite(value) && value >= minimum && value <= maximum;
}

static float AthleticExperienceLimit(int32_t level)
{
    return 30.0f + (float)level * 20.0f;
}

static void AthleticProfileSetDefault(CcClientAthleticProfile *profile)
{
    if (profile == NULL) return;
    *profile = (CcClientAthleticProfile){0};
    for (int32_t discipline = 0;
         discipline < CC_CLIENT_SESSION_ATHLETIC_COUNT; ++discipline) {
        profile->level[discipline] = 1;
    }
}

static bool AthleticProfileValidate(
    const CcClientAthleticProfile *profile)
{
    if (profile == NULL ||
        !isfinite(profile->travel_training_distance) ||
        profile->travel_training_distance < 0.0f ||
        profile->travel_training_distance >= 12.0f) {
        return false;
    }
    for (int32_t discipline = 0;
         discipline < CC_CLIENT_SESSION_ATHLETIC_COUNT; ++discipline) {
        int32_t level = profile->level[discipline];
        float experience = profile->experience[discipline];
        if (level < 1 || level > CC_CLIENT_SESSION_ATHLETIC_MAX_LEVEL ||
            !isfinite(experience) || experience < 0.0f ||
            (level == CC_CLIENT_SESSION_ATHLETIC_MAX_LEVEL &&
             experience != 0.0f) ||
            (level < CC_CLIENT_SESSION_ATHLETIC_MAX_LEVEL &&
             experience >= AthleticExperienceLimit(level))) {
            return false;
        }
    }
    return true;
}

static bool EncounterActorValidate(const CcClientEncounterActor *actor)
{
    if (actor == NULL ||
        !SessionFloatInRange(actor->position_x, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->position_y, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->position_z, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->velocity_x, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->velocity_y, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->velocity_z, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->facing_yaw, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->focus_x, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->focus_y, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->focus_z, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->knockback_x, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->knockback_y, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->knockback_z, -100000.0f, 100000.0f) ||
        !SessionFloatInRange(actor->health, 0.0f, 100.0f) ||
        !SessionFloatInRange(actor->posture, 0.0f, 100.0f) ||
        !SessionFloatInRange(actor->stagger_seconds, 0.0f, 100000.0f) ||
        !SessionFloatInRange(actor->hit_flash_seconds, 0.0f, 100000.0f) ||
        !SessionFloatInRange(actor->hitstop_seconds, 0.0f, 100000.0f) ||
        !SessionFloatInRange(actor->respawn_seconds, 0.0f, 100000.0f) ||
        !SessionFloatInRange(actor->auto_attack_cooldown,
                             0.0f, 100000.0f) ||
        actor->target_index < -1 || actor->target_index > 31 ||
        actor->queued_skill < -1 ||
        actor->queued_skill >= CC_CLIENT_SESSION_SKILL_COUNT ||
        actor->active_skill < -1 ||
        actor->active_skill >= CC_CLIENT_SESSION_SKILL_COUNT ||
        actor->life_state < 0 || actor->life_state > 3 ||
        actor->weapon_mode < 0 || actor->weapon_mode > 2) {
        return false;
    }
    for (int32_t skill = 0; skill < CC_CLIENT_SESSION_SKILL_COUNT; ++skill) {
        if (!SessionFloatInRange(actor->skill_cooldown[skill],
                                 0.0f, 100000.0f)) {
            return false;
        }
    }
    return true;
}

static bool RoadEncounterValidate(const CcClientSession *session)
{
    const CcClientRoadEncounter *encounter = &session->road_encounter;
    if (encounter->mode == CC_CLIENT_ROAD_ENCOUNTER_NONE) return true;
    if (encounter->mode < CC_CLIENT_ROAD_ENCOUNTER_FIGHT ||
        encounter->mode > CC_CLIENT_ROAD_ENCOUNTER_LOCAL ||
        (encounter->mode != CC_CLIENT_ROAD_ENCOUNTER_LOCAL &&
         (session->scene != CC_CLIENT_SESSION_STREET ||
          session->coordinate_space != CC_CLIENT_SESSION_LEGACY_LOCAL ||
          session->route_id != 0U)) ||
        !EncounterActorValidate(&encounter->player) ||
        !SessionFloatInRange(encounter->engagement_time,
                             0.0f, 100000.0f) ||
        !SessionFloatInRange(encounter->alarm_countdown,
                             0.0f, 100000.0f) ||
        !SessionFloatInRange(encounter->combat_event_seconds,
                             0.0f, 100000.0f) ||
        encounter->raider_initial_resolve < 0 ||
        encounter->raider_initial_resolve > 100000 ||
        encounter->raider_resolve < 0 ||
        encounter->raider_resolve > 100000 ||
        encounter->defenses_completed < 0 ||
        encounter->defenses_completed > 100000) {
        return false;
    }
    for (int32_t guard = 0;
         guard < CC_CLIENT_SESSION_GUARD_COUNT; ++guard) {
        if (!EncounterActorValidate(&encounter->guards[guard]) ||
            !SessionFloatInRange(encounter->guard_pause_seconds[guard],
                                 0.0f, 100000.0f) ||
            !SessionFloatInRange(encounter->guard_attack_cooldown[guard],
                                 0.0f, 100000.0f) ||
            encounter->guard_duty[guard] < 0 ||
            encounter->guard_duty[guard] > 3 ||
            encounter->guard_response_stage[guard] < 0 ||
            encounter->guard_response_stage[guard] > 100000) {
            return false;
        }
    }
    for (int32_t raider = 0;
         raider < CC_CLIENT_SESSION_RAIDER_COUNT; ++raider) {
        if (!EncounterActorValidate(&encounter->raiders[raider]) ||
            !SessionFloatInRange(encounter->raider_attack_cooldown[raider],
                                 0.0f, 100000.0f) ||
            encounter->raider_response_stage[raider] < 0 ||
            encounter->raider_response_stage[raider] > 100000) {
            return false;
        }
    }
    return true;
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
           session->opening_step <= 2U &&
           SessionFloatInRange(session->site_travel_progress, 0.0f, 1.0f);
}

bool CcClientSessionValidate(const CcClientSession *session)
{
    return ClientSessionValidateBase(session) &&
           (session->coordinate_space != CC_CLIENT_SESSION_WORLD ||
            session->route_id != 0U) &&
           AthleticProfileValidate(&session->athletics) &&
           RoadEncounterValidate(session);
}

static bool WriteAthleticProfile(
    FILE *file, const CcClientAthleticProfile *profile)
{
    return fprintf(
        file,
        "ATHLETICS %d %.9g %d %.9g %d %.9g %.9g\n",
        profile->level[0], (double)profile->experience[0],
        profile->level[1], (double)profile->experience[1],
        profile->level[2], (double)profile->experience[2],
        (double)profile->travel_training_distance) > 0;
}

static bool WriteEncounterActor(FILE *file,
                                const CcClientEncounterActor *actor)
{
    return fprintf(
        file,
        "ACTOR %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
        "%.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
        "%.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
        "%d %d %d %d %d %d %d\n",
        (double)actor->position_x, (double)actor->position_y,
        (double)actor->position_z, (double)actor->velocity_x,
        (double)actor->velocity_y, (double)actor->velocity_z,
        (double)actor->facing_yaw, (double)actor->focus_x,
        (double)actor->focus_y, (double)actor->focus_z,
        (double)actor->knockback_x, (double)actor->knockback_y,
        (double)actor->knockback_z, (double)actor->health,
        (double)actor->posture, (double)actor->stagger_seconds,
        (double)actor->hit_flash_seconds, (double)actor->hitstop_seconds,
        (double)actor->respawn_seconds,
        (double)actor->auto_attack_cooldown,
        (double)actor->skill_cooldown[0],
        (double)actor->skill_cooldown[1],
        (double)actor->skill_cooldown[2],
        actor->target_index, actor->queued_skill, actor->active_skill,
        actor->life_state, actor->weapon_mode,
        actor->focus_valid ? 1 : 0,
        actor->strike_resolved ? 1 : 0) > 0;
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
        "CROWNLESS_SESSION %u\n%u %llu %d %d %llu %.9g %.9g %.9g %u %d\n",
        session->version, session->world_seed,
        (unsigned long long)session->location_id, (int)session->scene,
        (int)session->coordinate_space,
        (unsigned long long)session->route_id,
        (double)session->position_x, (double)session->position_z,
        (double)session->facing_yaw, session->opening_step,
        (int)session->road_encounter.mode);
    const CcClientRoadEncounter *encounter = &session->road_encounter;
    bool ok = written > 0 &&
        WriteAthleticProfile(file, &session->athletics) &&
        fprintf(file, "ROAD %.9g %.9g %.9g %d %d %d %d %d\n",
                (double)encounter->engagement_time,
                (double)encounter->alarm_countdown,
                (double)encounter->combat_event_seconds,
                encounter->raider_initial_resolve,
                encounter->raider_resolve,
                encounter->defenses_completed,
                encounter->alarm_active ? 1 : 0,
                encounter->raiders_retreating ? 1 : 0) > 0;
    for (int32_t guard = 0;
         ok && guard < CC_CLIENT_SESSION_GUARD_COUNT; ++guard) {
        ok = fprintf(file, "GUARD %.9g %.9g %d %d\n",
                     (double)encounter->guard_pause_seconds[guard],
                     (double)encounter->guard_attack_cooldown[guard],
                     encounter->guard_duty[guard],
                     encounter->guard_response_stage[guard]) > 0;
    }
    for (int32_t raider = 0;
         ok && raider < CC_CLIENT_SESSION_RAIDER_COUNT; ++raider) {
        ok = fprintf(file, "RAIDER %.9g %d\n",
                     (double)encounter->raider_attack_cooldown[raider],
                     encounter->raider_response_stage[raider]) > 0;
    }
    if (ok) ok = WriteEncounterActor(file, &encounter->player);
    for (int32_t guard = 0;
         ok && guard < CC_CLIENT_SESSION_GUARD_COUNT; ++guard) {
        ok = WriteEncounterActor(file, &encounter->guards[guard]);
    }
    for (int32_t raider = 0;
         ok && raider < CC_CLIENT_SESSION_RAIDER_COUNT; ++raider) {
        ok = WriteEncounterActor(file, &encounter->raiders[raider]);
    }
    ok = ok && fprintf(file, "TRAVEL %d %d %.9g\n",
        session->site_travel_active ? 1 : 0, session->site_returning ? 1 : 0,
        (double)session->site_travel_progress) > 0;
    ok = ok && fflush(file) == 0;
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

static bool ReadEncounterActor(FILE *file, CcClientEncounterActor *actor)
{
    char marker[16] = "";
    int focus_valid = 0;
    int strike_resolved = 0;
    if (fscanf(file, "%15s", marker) != 1 ||
        strcmp(marker, "ACTOR") != 0) {
        return false;
    }
    int fields = fscanf(
        file,
        "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f "
        "%f %f %f %f %f %f %f %f %d %d %d %d %d %d %d",
        &actor->position_x, &actor->position_y, &actor->position_z,
        &actor->velocity_x, &actor->velocity_y, &actor->velocity_z,
        &actor->facing_yaw, &actor->focus_x, &actor->focus_y,
        &actor->focus_z, &actor->knockback_x, &actor->knockback_y,
        &actor->knockback_z, &actor->health, &actor->posture,
        &actor->stagger_seconds, &actor->hit_flash_seconds,
        &actor->hitstop_seconds, &actor->respawn_seconds,
        &actor->auto_attack_cooldown, &actor->skill_cooldown[0],
        &actor->skill_cooldown[1], &actor->skill_cooldown[2],
        &actor->target_index, &actor->queued_skill, &actor->active_skill,
        &actor->life_state, &actor->weapon_mode,
        &focus_valid, &strike_resolved);
    actor->focus_valid = focus_valid != 0;
    actor->strike_resolved = strike_resolved != 0;
    return fields == 30 && (focus_valid == 0 || focus_valid == 1) &&
           (strike_resolved == 0 || strike_resolved == 1);
}

static bool ReadRoadEncounter(FILE *file, CcClientRoadEncounter *encounter)
{
    char marker[16] = "";
    int alarm_active = 0;
    int raiders_retreating = 0;
    if (fscanf(file, "%15s", marker) != 1 ||
        strcmp(marker, "ROAD") != 0 ||
        fscanf(file, "%f %f %f %d %d %d %d %d",
               &encounter->engagement_time,
               &encounter->alarm_countdown,
               &encounter->combat_event_seconds,
               &encounter->raider_initial_resolve,
               &encounter->raider_resolve,
               &encounter->defenses_completed,
               &alarm_active, &raiders_retreating) != 8 ||
        (alarm_active != 0 && alarm_active != 1) ||
        (raiders_retreating != 0 && raiders_retreating != 1)) {
        return false;
    }
    encounter->alarm_active = alarm_active != 0;
    encounter->raiders_retreating = raiders_retreating != 0;
    for (int32_t guard = 0;
         guard < CC_CLIENT_SESSION_GUARD_COUNT; ++guard) {
        if (fscanf(file, "%15s", marker) != 1 ||
            strcmp(marker, "GUARD") != 0 ||
            fscanf(file, "%f %f %d %d",
                   &encounter->guard_pause_seconds[guard],
                   &encounter->guard_attack_cooldown[guard],
                   &encounter->guard_duty[guard],
                   &encounter->guard_response_stage[guard]) != 4) {
            return false;
        }
    }
    for (int32_t raider = 0;
         raider < CC_CLIENT_SESSION_RAIDER_COUNT; ++raider) {
        if (fscanf(file, "%15s", marker) != 1 ||
            strcmp(marker, "RAIDER") != 0 ||
            fscanf(file, "%f %d",
                   &encounter->raider_attack_cooldown[raider],
                   &encounter->raider_response_stage[raider]) != 2) {
            return false;
        }
    }
    if (!ReadEncounterActor(file, &encounter->player)) return false;
    for (int32_t guard = 0;
         guard < CC_CLIENT_SESSION_GUARD_COUNT; ++guard) {
        if (!ReadEncounterActor(file, &encounter->guards[guard])) {
            return false;
        }
    }
    for (int32_t raider = 0;
         raider < CC_CLIENT_SESSION_RAIDER_COUNT; ++raider) {
        if (!ReadEncounterActor(file, &encounter->raiders[raider])) {
            return false;
        }
    }
    return true;
}

static bool ReadAthleticProfile(
    FILE *file, CcClientAthleticProfile *profile)
{
    char marker[16] = "";
    return fscanf(file, "%15s", marker) == 1 &&
           strcmp(marker, "ATHLETICS") == 0 &&
           fscanf(file, "%d %f %d %f %d %f %f",
                  &profile->level[0], &profile->experience[0],
                  &profile->level[1], &profile->experience[1],
                  &profile->level[2], &profile->experience[2],
                  &profile->travel_training_distance) == 7;
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
    int road_encounter_mode = CC_CLIENT_ROAD_ENCOUNTER_NONE;
    int header_fields = fscanf(file, "%31s %u", marker, &version);
    int body_fields = 0;
    if (header_fields == 2 &&
        (version == CC_CLIENT_SESSION_VERSION || version == 6U || version == 5U)) {
        body_fields = fscanf(file, "%u %llu %d %d %llu %f %f %f %u %d",
                             &world_seed, &location_id, &scene,
                             &coordinate_space, &route_id, &position_x,
                             &position_z, &facing_yaw, &opening_step,
                             &road_encounter_mode);
    } else if (header_fields == 2 && version == 4U) {
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
        .opening_step = (uint32_t)opening_step,
        .road_encounter.mode =
            (CcClientRoadEncounterMode)road_encounter_mode
    };
    bool current_payload = (version == CC_CLIENT_SESSION_VERSION || version == 6U) &&
                           body_fields == 10 &&
                           ReadAthleticProfile(file, &loaded.athletics) &&
                           ReadRoadEncounter(file, &loaded.road_encounter);
    if (current_payload && version == CC_CLIENT_SESSION_VERSION) {
        char travel_marker[16] = "";
        int active = 0, returning = 0;
        current_payload = fscanf(file, "%15s %d %d %f", travel_marker,
            &active, &returning, &loaded.site_travel_progress) == 4 &&
            strcmp(travel_marker, "TRAVEL") == 0 &&
            (active == 0 || active == 1) && (returning == 0 || returning == 1);
        loaded.site_travel_active = active == 1;
        loaded.site_returning = returning == 1;
    }
    if (current_payload) loaded.version = CC_CLIENT_SESSION_VERSION;
    bool version_five_payload = version == 5U && body_fields == 10 &&
        ReadRoadEncounter(file, &loaded.road_encounter);
    bool closed = fclose(file) == 0;
    bool version_one = version == 1U && body_fields == 6;
    bool version_two = version == 2U && body_fields == 7;
    bool version_three = version == 3U && body_fields == 8;
    bool version_four = version == 4U && body_fields == 9;
    if (version_one || version_two || version_three || version_four ||
        version_five_payload) {
        loaded.version = CC_CLIENT_SESSION_VERSION;
        AthleticProfileSetDefault(&loaded.athletics);
        if (version_one || version_two) {
            loaded.coordinate_space = CC_CLIENT_SESSION_LEGACY_LOCAL;
        }
        if (version_one || version_two || version_three) {
            loaded.route_id = 0U;
        }
        if (version_one) loaded.opening_step = 2U;
    }
    bool valid = current_payload || version_five_payload || version_four ?
        CcClientSessionValidate(&loaded) : ClientSessionValidateBase(&loaded);
    if ((!version_one && !version_two && !version_three && !version_four &&
         !version_five_payload && !current_payload) ||
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
