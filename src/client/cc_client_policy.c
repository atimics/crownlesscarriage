#include "client/cc_client_policy.h"

#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CC_CLIENT_PREFERENCES_VERSION 5U

static bool ValidAvatar(uint32_t avatar)
{
    return avatar < 32768U && (avatar & 7U) < 6U &&
        ((avatar >> 3U) & 7U) < 6U && ((avatar >> 6U) & 7U) < 6U &&
        ((avatar >> 9U) & 7U) < 4U && ((avatar >> 12U) & 7U) < 6U;
}

static void SetPreferencesError(char *error, size_t capacity,
                                const char *message)
{
    if (error == NULL || capacity == 0U) return;
    (void)snprintf(error, capacity, "%s", message != NULL ? message : "");
}

void CcClientPreferencesDefault(CcClientPreferences *preferences)
{
    if (preferences == NULL) return;
    *preferences = (CcClientPreferences){.focus_hints = true, .voice_volume = 100,
        .player_voice = 5, .ambient_voices = true};
}

bool CcClientPreferencesLoad(const char *path,
                             CcClientPreferences *preferences,
                             char *error, size_t error_capacity)
{
    if (path == NULL || path[0] == '\0' || preferences == NULL) {
        SetPreferencesError(error, error_capacity,
                            "Preferences path or output is missing.");
        return false;
    }
    CcClientPreferencesDefault(preferences);
    errno = 0;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            SetPreferencesError(error, error_capacity, "");
            return true;
        }
        SetPreferencesError(error, error_capacity,
                            "Could not read client preferences.");
        return false;
    }
    char marker[32] = "";
    char setting[32] = "";
    unsigned int version = 0U;
    int reduced_motion = -1;
    bool valid = fscanf(file, "%31s %u", marker, &version) == 2 &&
        strcmp(marker, "CROWNLESS_PREFERENCES") == 0 &&
        (version >= 1U && version <= CC_CLIENT_PREFERENCES_VERSION) &&
        fscanf(file, "%31s %d", setting, &reduced_motion) == 2 &&
        strcmp(setting, "reduced_motion") == 0 &&
        (reduced_motion == 0 || reduced_motion == 1);
    int audio_mode = 0;
    if (valid && version >= 2U) {
        valid = fscanf(file, "%31s %d", setting, &audio_mode) == 2 &&
            strcmp(setting, "audio_mode") == 0 && audio_mode >= 0 && audio_mode <= 2;
    }
    int text_size = 0, focus_hints = 1;
    if (valid && version >= 3U) {
        valid = fscanf(file, "%31s %d", setting, &text_size) == 2 &&
            strcmp(setting, "text_size") == 0 && text_size >= 0 && text_size <= 2 &&
            fscanf(file, "%31s %d", setting, &focus_hints) == 2 &&
            strcmp(setting, "focus_hints") == 0 && (focus_hints == 0 || focus_hints == 1);
    }
    unsigned int avatar = 0U;
    if (valid && version >= 4U) {
        valid = fscanf(file, "%31s %u", setting, &avatar) == 2 &&
            strcmp(setting, "avatar") == 0 && ValidAvatar(avatar);
    }
    int voice_volume = 100, player_voice = 5, read_aloud = 0, ambient_voices = 1;
    if (valid && version >= 5U) {
        valid = fscanf(file, "%31s %d", setting, &voice_volume) == 2 &&
            strcmp(setting, "voice_volume") == 0 && voice_volume >= 0 && voice_volume <= 100 &&
            fscanf(file, "%31s %d", setting, &player_voice) == 2 &&
            strcmp(setting, "player_voice") == 0 && (player_voice == -1 || (player_voice >= 5 && player_voice <= 12)) &&
            fscanf(file, "%31s %d", setting, &read_aloud) == 2 &&
            strcmp(setting, "read_aloud") == 0 && (read_aloud == 0 || read_aloud == 1) &&
            fscanf(file, "%31s %d", setting, &ambient_voices) == 2 &&
            strcmp(setting, "ambient_voices") == 0 && (ambient_voices == 0 || ambient_voices == 1);
    }
    if (fclose(file) != 0) valid = false;
    if (!valid) {
        CcClientPreferencesDefault(preferences);
        SetPreferencesError(error, error_capacity,
                            "Client preferences are invalid.");
        return false;
    }
    preferences->reduced_motion = reduced_motion != 0;
    preferences->audio_mode = audio_mode;
    preferences->text_size = text_size;
    preferences->focus_hints = focus_hints != 0;
    preferences->avatar = avatar;
    preferences->voice_volume = voice_volume;
    preferences->player_voice = player_voice;
    preferences->read_aloud = read_aloud != 0;
    preferences->ambient_voices = ambient_voices != 0;
    SetPreferencesError(error, error_capacity, "");
    return true;
}

bool CcClientPreferencesSave(const char *path,
                             const CcClientPreferences *preferences,
                             char *error, size_t error_capacity)
{
    if (path == NULL || path[0] == '\0' || preferences == NULL ||
        preferences->audio_mode < 0 || preferences->audio_mode > 2 ||
        preferences->text_size < 0 || preferences->text_size > 2 ||
        preferences->voice_volume < 0 || preferences->voice_volume > 100 ||
        (preferences->player_voice != -1 && (preferences->player_voice < 5 || preferences->player_voice > 12)) ||
        !ValidAvatar(preferences->avatar)) {
        SetPreferencesError(error, error_capacity,
                            "Preferences path or state is invalid.");
        return false;
    }
    char temporary[768];
    int length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (length < 0 || (size_t)length >= sizeof(temporary)) {
        SetPreferencesError(error, error_capacity,
                            "Preferences path is too long.");
        return false;
    }
    FILE *file = fopen(temporary, "wb");
    if (file == NULL) {
        SetPreferencesError(error, error_capacity,
                            "Could not open client preferences for writing.");
        return false;
    }
    bool saved = fprintf(file, "CROWNLESS_PREFERENCES %u\n"
                               "reduced_motion %d\n"
                               "audio_mode %d\n"
                               "text_size %d\n"
                               "focus_hints %d\n"
                               "avatar %u\n"
                               "voice_volume %d\n"
                               "player_voice %d\n"
                               "read_aloud %d\n"
                               "ambient_voices %d\n",
                         CC_CLIENT_PREFERENCES_VERSION,
                         preferences->reduced_motion ? 1 : 0,
                         preferences->audio_mode, preferences->text_size,
                         preferences->focus_hints ? 1 : 0,
                         (unsigned int)preferences->avatar, preferences->voice_volume,
                         preferences->player_voice, preferences->read_aloud ? 1 : 0,
                         preferences->ambient_voices ? 1 : 0) > 0;
    saved = saved && fflush(file) == 0;
    if (fclose(file) != 0) saved = false;
    if (saved && rename(temporary, path) != 0) saved = false;
    if (!saved) {
        (void)remove(temporary);
        SetPreferencesError(error, error_capacity,
                            "Could not commit client preferences.");
        return false;
    }
    SetPreferencesError(error, error_capacity, "");
    return true;
}

bool CcClientHitEffectVisible(bool reduced_motion,
                              float hit_flash_seconds)
{
    return !reduced_motion && isfinite(hit_flash_seconds) &&
           hit_flash_seconds > 0.0f;
}

static float ClampPace(float pace)
{
    return fmaxf(0.0f, fminf(1.0f, pace));
}

float CcClientTravelBlendStep(float blend, bool fast_forward, float delta_time)
{
    if (!isfinite(blend)) blend = 0.0f;
    blend = ClampPace(blend);
    if (!isfinite(delta_time) || delta_time <= 0.0f) return blend;
    float step = fminf(delta_time, 0.1f) / 1.4f;
    return ClampPace(blend + (fast_forward ? step : -step));
}

float CcClientTravelTimeScale(float blend)
{
    if (!isfinite(blend)) return 1.0f;
    blend = ClampPace(blend);
    return 1.0f + 7.0f * blend * blend * (3.0f - 2.0f * blend);
}

static float ClampUnit(float value)
{
    return fmaxf(0.0f, fminf(1.0f, value));
}

void CcClientDepartureBegin(CcClientDepartureTransition *transition)
{
    if (transition == NULL) return;
    *transition = (CcClientDepartureTransition){
        .phase = CC_CLIENT_DEPARTURE_TOWN,
    };
}

void CcClientDepartureAdvance(CcClientDepartureTransition *transition,
                              float pace, float delta_time)
{
    if (transition == NULL || transition->phase == CC_CLIENT_DEPARTURE_READY) {
        return;
    }
    float step = isfinite(delta_time) ? fmaxf(0.0f, delta_time) : 0.0f;
    if (transition->phase == CC_CLIENT_DEPARTURE_TOWN) {
        transition->town_progress = ClampUnit(
            transition->town_progress + ClampPace(pace) * step * 0.11f);
        if (transition->town_progress >= 1.0f) {
            transition->phase = CC_CLIENT_DEPARTURE_ROAD_BOOK;
            transition->road_book_progress = 0.0f;
        }
        return;
    }
    transition->road_book_progress = ClampUnit(
        transition->road_book_progress + step * 1.35f);
    if (transition->road_book_progress >= 1.0f) {
        transition->phase = CC_CLIENT_DEPARTURE_READY;
    }
}

void CcClientArrivalBegin(CcClientArrivalTransition *transition)
{
    if (transition == NULL) return;
    *transition = (CcClientArrivalTransition){
        .phase = CC_CLIENT_ARRIVAL_ROAD_BOOK,
    };
}

void CcClientArrivalAdvance(CcClientArrivalTransition *transition,
                            float pace, float delta_time)
{
    if (transition == NULL || transition->phase == CC_CLIENT_ARRIVAL_PARKED) {
        return;
    }
    float step = isfinite(delta_time) ? fmaxf(0.0f, delta_time) : 0.0f;
    if (transition->phase == CC_CLIENT_ARRIVAL_ROAD_BOOK) {
        transition->road_book_progress = ClampUnit(
            transition->road_book_progress + step * 1.35f);
        if (transition->road_book_progress >= 1.0f) {
            transition->phase = CC_CLIENT_ARRIVAL_TOWN;
        }
        return;
    }
    transition->town_progress = ClampUnit(
        transition->town_progress + ClampPace(pace) * step * 0.13f);
    if (transition->town_progress >= 1.0f) {
        transition->phase = CC_CLIENT_ARRIVAL_PARKED;
    }
}

void CcClientArrivalComplete(CcClientArrivalTransition *transition)
{
    if (transition == NULL) return;
    *transition = (CcClientArrivalTransition){
        .phase = CC_CLIENT_ARRIVAL_PARKED,
        .road_book_progress = 1.0f,
        .town_progress = 1.0f,
    };
}

float CcClientArrivalCameraWeight(
    const CcClientArrivalTransition *transition)
{
    if (transition == NULL) return 0.0f;
    return 1.0f - ClampUnit(transition->road_book_progress);
}

float CcClientConvoyPaceStep(float pace, bool road_phase,
                             bool urge, bool rein_in, bool stopped,
                             float delta_time)
{
    float step = fmaxf(0.0f, delta_time);
    if (urge) pace += step * 0.72f;
    if (rein_in) pace -= step * 1.08f;
    if (!urge && !rein_in && !stopped) {
        float cruise = road_phase ? 0.72f : 1.0f;
        float correction = cruise - pace;
        float maximum_change = step * 0.55f;
        correction = fmaxf(-maximum_change,
                           fminf(maximum_change, correction));
        pace += correction;
    }
    if (stopped) pace = 0.0f;
    return ClampPace(pace);
}

float CcClientRoadApproachStep(float progress, float pace, float delta_time)
{
    float step = fmaxf(0.0f, delta_time);
    float next = progress + ClampPace(pace) * step * 0.24f;
    return fmaxf(0.0f, fminf(1.0f, next));
}

float CcClientConvoyPosturePace(int32_t posture)
{
    if (posture <= 0) return 0.52f;
    if (posture >= 2) return 1.0f;
    return 0.72f;
}

int32_t CcClientStepConvoyPosture(int32_t posture, int32_t direction)
{
    int32_t next = posture + (direction > 0 ? 1 : direction < 0 ? -1 : 0);
    if (next < 0) next = 0;
    if (next > 2) next = 2;
    return next;
}

CcClientConvoyGait CcClientConvoyGaitForPace(float pace)
{
    if (!isfinite(pace)) return CC_CLIENT_CONVOY_GAIT_HALT;
    pace = ClampPace(pace);
    if (pace <= 0.02f) return CC_CLIENT_CONVOY_GAIT_HALT;
    if (pace < 0.62f) return CC_CLIENT_CONVOY_GAIT_WALK;
    if (pace < 0.86f) return CC_CLIENT_CONVOY_GAIT_TROT;
    return CC_CLIENT_CONVOY_GAIT_CANTER;
}

float CcClientConvoyGaitCadence(CcClientConvoyGait gait)
{
    switch (gait) {
        case CC_CLIENT_CONVOY_GAIT_HALT: return 0.0f;
        case CC_CLIENT_CONVOY_GAIT_WALK: return 3.6f;
        case CC_CLIENT_CONVOY_GAIT_TROT: return 4.8f;
        case CC_CLIENT_CONVOY_GAIT_CANTER: return 6.0f;
        case CC_CLIENT_CONVOY_GAIT_COUNT:
        default:
            return 0.0f;
    }
}

const char *CcClientConvoyGaitName(CcClientConvoyGait gait)
{
    switch (gait) {
        case CC_CLIENT_CONVOY_GAIT_HALT: return "HALT";
        case CC_CLIENT_CONVOY_GAIT_WALK: return "WALK";
        case CC_CLIENT_CONVOY_GAIT_TROT: return "TROT";
        case CC_CLIENT_CONVOY_GAIT_CANTER: return "CANTER";
        case CC_CLIENT_CONVOY_GAIT_COUNT:
        default:
            return "UNKNOWN";
    }
}

bool CcClientRoadHasNextBranch(int32_t branch_ordinal,
                               int32_t branch_count)
{
    return branch_ordinal >= 0 && branch_ordinal + 1 < branch_count;
}

bool CcClientMapCommandEnabled(bool viewing_map, bool road_local,
                               bool market_interior,
                               float carriage_distance)
{
    return viewing_map ||
           (!road_local && !market_interior && carriage_distance < 1.35f);
}

bool CcClientInteractionActivated(bool requested, float distance,
                                  float maximum_distance)
{
    return requested && isfinite(distance) && isfinite(maximum_distance) &&
           maximum_distance >= 0.0f && distance < maximum_distance;
}

CcClientCampaignAccess CcClientCampaignAccessFor(bool normal_play,
                                                 bool journal_available)
{
    if (!normal_play) return CC_CLIENT_CAMPAIGN_EPHEMERAL;
    return journal_available ? CC_CLIENT_CAMPAIGN_PERSISTENT :
                               CC_CLIENT_CAMPAIGN_BLOCKED;
}
