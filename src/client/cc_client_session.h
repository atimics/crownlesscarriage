#ifndef CROWNLESS_CLIENT_SESSION_H
#define CROWNLESS_CLIENT_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CC_CLIENT_SESSION_VERSION UINT32_C(5)
#define CC_CLIENT_SESSION_GUARD_COUNT 3
#define CC_CLIENT_SESSION_RAIDER_COUNT 2
#define CC_CLIENT_SESSION_SKILL_COUNT 3

typedef enum CcClientSessionScene {
    CC_CLIENT_SESSION_STREET = 0,
    CC_CLIENT_SESSION_MARKET = 1,
    CC_CLIENT_SESSION_DUNGEON_SITE = 2,
    CC_CLIENT_SESSION_GOBLIN_SITE = 3,
    CC_CLIENT_SESSION_DRAGON_SITE = 4
} CcClientSessionScene;

typedef enum CcClientSessionCoordinateSpace {
    CC_CLIENT_SESSION_LEGACY_LOCAL = 0,
    CC_CLIENT_SESSION_WORLD = 1
} CcClientSessionCoordinateSpace;

typedef enum CcClientRoadEncounterMode {
    CC_CLIENT_ROAD_ENCOUNTER_NONE = 0,
    CC_CLIENT_ROAD_ENCOUNTER_FIGHT = 1,
    CC_CLIENT_ROAD_ENCOUNTER_PARLEY = 2
} CcClientRoadEncounterMode;

typedef struct CcClientEncounterActor {
    float position_x;
    float position_y;
    float position_z;
    float velocity_x;
    float velocity_y;
    float velocity_z;
    float facing_yaw;
    float focus_x;
    float focus_y;
    float focus_z;
    float knockback_x;
    float knockback_y;
    float knockback_z;
    float health;
    float posture;
    float stagger_seconds;
    float hit_flash_seconds;
    float hitstop_seconds;
    float respawn_seconds;
    float auto_attack_cooldown;
    float skill_cooldown[CC_CLIENT_SESSION_SKILL_COUNT];
    int32_t target_index;
    int32_t queued_skill;
    int32_t active_skill;
    int32_t life_state;
    int32_t weapon_mode;
    bool focus_valid;
    bool strike_resolved;
} CcClientEncounterActor;

typedef struct CcClientRoadEncounter {
    CcClientRoadEncounterMode mode;
    CcClientEncounterActor player;
    CcClientEncounterActor guards[CC_CLIENT_SESSION_GUARD_COUNT];
    CcClientEncounterActor raiders[CC_CLIENT_SESSION_RAIDER_COUNT];
    float engagement_time;
    float alarm_countdown;
    float combat_event_seconds;
    float guard_pause_seconds[CC_CLIENT_SESSION_GUARD_COUNT];
    float guard_attack_cooldown[CC_CLIENT_SESSION_GUARD_COUNT];
    float raider_attack_cooldown[CC_CLIENT_SESSION_RAIDER_COUNT];
    int32_t guard_duty[CC_CLIENT_SESSION_GUARD_COUNT];
    int32_t guard_response_stage[CC_CLIENT_SESSION_GUARD_COUNT];
    int32_t raider_response_stage[CC_CLIENT_SESSION_RAIDER_COUNT];
    int32_t raider_initial_resolve;
    int32_t raider_resolve;
    int32_t defenses_completed;
    bool alarm_active;
    bool raiders_retreating;
} CcClientRoadEncounter;

typedef struct CcClientSession {
    uint32_t version;
    uint32_t world_seed;
    uint64_t location_id;
    CcClientSessionScene scene;
    CcClientSessionCoordinateSpace coordinate_space;
    uint64_t route_id;
    float position_x;
    float position_z;
    float facing_yaw;
    uint32_t opening_step;
    CcClientRoadEncounter road_encounter;
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
