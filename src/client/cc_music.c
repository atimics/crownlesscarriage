#include "client/cc_music.h"

#include <math.h>
#include <ctype.h>
#include <string.h>

#include "client/cc_music_catalog.inc"

static float Unit(float value)
{
    return isfinite(value) ? fminf(1.0f, fmaxf(0.0f, value)) : 0.0f;
}

static bool SamePlace(const char *left, const char *right)
{
    if (strncmp(left, "The ", 4) == 0) left += 4;
    if (strncmp(right, "The ", 4) == 0) right += 4;
    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left++) != tolower((unsigned char)*right++))
            return false;
    }
    return *left == *right;
}

void CcMusicAttractPlace(CcMusicContext *context, const char *name, float weight)
{
    if (context == NULL || name == NULL || name[0] == '\0') return;
    for (int i = 0; i < CC_MUSIC_CUE_COUNT; ++i) {
        const char *place = cc_music_cues[i].place;
        if (place[0] != '\0' && SamePlace(place, name)) {
            context->cue[i] = fmaxf(context->cue[i], Unit(weight));
        }
    }
}

static void AttractSettlement(CcMusicContext *context,
                              const CcSettlement *place, float weight)
{
    if (place == NULL || (int)place->function < 0 ||
        (int)place->function >= CC_MUSIC_REGION_COUNT) return;
    context->region[place->function] += Unit(weight);
    context->theme[CC_MUSIC_HUNGER] +=
        weight * Unit(((float)place->hunger - 25.0f) / 60.0f);
}

CcMusicContext CcMusicContextFor(const CcSim *sim, CcMusicScene scene)
{
    CcMusicContext context = {0};
    if (sim == NULL) return context;
    const CcSettlement *here = CcSimSettlement(sim, sim->player.location_id);
    bool road = scene.road && sim->journey.active;
    context.theme[CC_MUSIC_TRAVEL] = scene.road ? 1.0f : 0.06f;
    context.theme[CC_MUSIC_TOWN] = scene.road ? 0.18f : 1.0f;
    if (road) {
        float progress = Unit((float)sim->carriage.progress_milli / 1000.0f);
        AttractSettlement(&context, CcSimSettlement(sim, sim->journey.origin_id),
                          1.0f - progress);
        AttractSettlement(&context, CcSimSettlement(sim, sim->journey.destination_id),
                          progress);
        context.theme[CC_MUSIC_DANGER] =
            Unit((float)sim->journey.danger / 100.0f) * 0.55f;
        if (sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
            context.theme[CC_MUSIC_BANDIT] = 1.0f;
            context.theme[CC_MUSIC_DANGER] = 0.8f;
        }
        if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
            context.theme[CC_MUSIC_CAMP] = 1.0f;
            context.theme[CC_MUSIC_TRAVEL] = 0.08f;
        }
    } else {
        AttractSettlement(&context, here, 1.0f);
        if (here != NULL && !scene.road)
            CcMusicAttractPlace(&context, here->name, 0.35f);
    }
    context.theme[CC_MUSIC_NIGHT] = scene.night ? 0.75f : 0.0f;
    context.theme[CC_MUSIC_RAIN] = scene.rain ? 0.75f : 0.0f;
    context.theme[CC_MUSIC_MARKET] = scene.market ? 1.0f : 0.0f;
    if (sim->dungeon_expedition.active) {
        context.theme[CC_MUSIC_TOWN] = 0.0f;
        context.theme[CC_MUSIC_TRAVEL] = 0.0f;
        context.theme[CC_MUSIC_DUNGEON] = 1.0f;
        const CcDungeonExpedition *expedition = &sim->dungeon_expedition;
        for (int i = 0; i < sim->dungeon_count; ++i) {
            const CcDungeon *dungeon = &sim->dungeons[i];
            if (dungeon->id != expedition->dungeon_id ||
                expedition->current_room < 0 ||
                expedition->current_room >= dungeon->room_count) continue;
            const CcDungeonRoom *room = &dungeon->rooms[expedition->current_room];
            static const CcMusicTheme room_themes[] = {
                CC_MUSIC_MINE, CC_MUSIC_MINE, CC_MUSIC_FLOOD, CC_MUSIC_FORGE,
                CC_MUSIC_BRIDGE, CC_MUSIC_DANGER, CC_MUSIC_ARCHIVE,
                CC_MUSIC_MARKET, CC_MUSIC_GOBLIN, CC_MUSIC_CLOISTER,
                CC_MUSIC_ARCHIVE, CC_MUSIC_DRAGON
            };
            if ((unsigned int)room->kind < sizeof(room_themes) / sizeof(room_themes[0])) {
                context.theme[room_themes[room->kind]] = 0.85f;
            }
            if ((room->flags & CC_DUNGEON_ROOM_GOBLIN) != 0U)
                context.theme[CC_MUSIC_GOBLIN] = 0.8f;
            if ((room->flags & CC_DUNGEON_ROOM_DRAGON_SIGN) != 0U)
                context.theme[CC_MUSIC_DRAGON] = 0.65f;
            CcMusicAttractPlace(&context, room->name, 1.0f);
        }
        if (expedition->encounter_kind != CC_DUNGEON_ENCOUNTER_NONE) {
            context.theme[CC_MUSIC_DANGER] = 1.0f;
            context.combat = expedition->encounter_reaction <= 5;
            context.theme[CC_MUSIC_COMBAT] = context.combat ? 1.0f : 0.0f;
        }
    }
    if (scene.goblin_cave || scene.dragon_cave) {
        context.theme[CC_MUSIC_TOWN] = 0.0f;
        context.theme[CC_MUSIC_TRAVEL] = 0.0f;
        context.theme[scene.goblin_cave ? CC_MUSIC_GOBLIN : CC_MUSIC_DRAGON] = 1.0f;
        if (scene.goblin_cave) CcMusicAttractPlace(&context, sim->goblins.name, 0.5f);
        if (scene.dragon_cave && !sim->dragon.slain &&
            sim->dragon.activity == CC_DRAGON_ACTIVITY_RETALIATING) {
            context.combat = true;
            context.theme[CC_MUSIC_COMBAT] = 1.0f;
        }
    }
    if ((int)scene.nearby_theme > CC_MUSIC_NONE &&
        scene.nearby_theme < CC_MUSIC_THEME_COUNT) {
        context.theme[scene.nearby_theme] = Unit(scene.nearby_weight);
        CcMusicAttractPlace(&context, scene.nearby_name, scene.nearby_weight);
    }
    if (scene.bandit_attack || scene.town_attack) {
        context.combat = true;
        context.theme[CC_MUSIC_COMBAT] = 1.0f;
        context.theme[scene.bandit_attack ? CC_MUSIC_BANDIT : CC_MUSIC_SIEGE] = 1.0f;
    }
    if (scene.loss) {
        memset(context.theme, 0, sizeof(context.theme));
        memset(context.cue, 0, sizeof(context.cue));
        context.theme[CC_MUSIC_LOSS] = 1.0f;
        context.combat = false;
    }
    return context;
}

float CcMusicScore(const CcMusicContext *context, int cue)
{
    if (context == NULL || cue < 0 || cue >= CC_MUSIC_CUE_COUNT) return 0.0f;
    const CcMusicCue *track = &cc_music_cues[cue];
    if (track->combat && !context->combat) return 0.0f;
    if (track->combat && Unit(context->theme[track->theme]) == 0.0f) return 0.0f;
    if ((track->theme == CC_MUSIC_NIGHT || track->theme == CC_MUSIC_RAIN ||
         track->theme == CC_MUSIC_LOSS || track->theme == CC_MUSIC_ENDING ||
         track->theme == CC_MUSIC_GOBLIN || track->theme == CC_MUSIC_DRAGON) &&
        Unit(context->theme[track->theme]) == 0.0f && Unit(context->cue[cue]) == 0.0f)
        return 0.0f;
    float score = Unit(context->theme[track->theme]) +
        0.22f * Unit(context->theme[track->secondary]) +
        3.0f * Unit(context->cue[cue]);
    if (track->region >= 0) score *= 0.03f + 2.0f * Unit(context->region[track->region]);
    if (context->combat && !track->combat) score *= 0.015f;
    return score;
}

void CcMusicInit(CcMusicDirector *director, uint32_t seed)
{
    if (director == NULL) return;
    memset(director, 0, sizeof(*director));
    director->random_state = seed != 0U ? seed : UINT32_C(0x6d757369);
    director->target_voice = -1;
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) director->voice[i].take = -1;
    for (int i = 0; i < 4; ++i) director->recent_cue[i] = -1;
    for (int i = 0; i < CC_MUSIC_CUE_COUNT; ++i) director->last_take[i] = -1;
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i)
        director->duration[i] = cc_music_takes[i].duration;
}

static float RandomUnit(CcMusicDirector *director)
{
    uint32_t x = director->random_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    director->random_state = x;
    return (float)(x >> 8) / 16777216.0f;
}

int CcMusicChoose(CcMusicDirector *director, const CcMusicContext *context)
{
    if (director == NULL || context == NULL) return -1;
    bool available[CC_MUSIC_CUE_COUNT] = {false};
    bool combat_available = false;
    int variants[CC_MUSIC_CUE_COUNT] = {0};
    float weights[CC_MUSIC_CUE_COUNT] = {0};
    float total = 0.0f;
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i) {
        if (!director->available[i]) continue;
        int cue = cc_music_takes[i].cue;
        available[cue] = true;
        variants[cue] += 1;
        if (cc_music_cues[cue].combat && CcMusicScore(context, cue) > 0.0f)
            combat_available = true;
    }
    for (int cue = 0; cue < CC_MUSIC_CUE_COUNT; ++cue) {
        if (!available[cue]) continue;
        if (context->combat && combat_available && !cc_music_cues[cue].combat) continue;
        float score = CcMusicScore(context, cue);
        weights[cue] = score * score;
        for (int recent = 0; recent < 4; ++recent)
            if (director->recent_cue[recent] == cue) weights[cue] *= 0.25f;
        total += weights[cue];
    }
    if (total <= 0.0f) return -1;
    float draw = RandomUnit(director) * total;
    int selected = -1;
    for (int cue = 0; cue < CC_MUSIC_CUE_COUNT; ++cue) {
        if (weights[cue] <= 0.0f) continue;
        selected = cue;
        draw -= weights[cue];
        if (draw < 0.0f) break;
    }
    /* Pick the title first so extra remixes do not inflate its chance. */
    total = 0.0f;
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i) {
        if (!director->available[i] || cc_music_takes[i].cue != selected) continue;
        if (variants[selected] > 1 && director->last_take[selected] == i) continue;
        total += cc_music_takes[i].weight;
    }
    draw = RandomUnit(director) * total;
    int take = -1;
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i) {
        if (!director->available[i] || cc_music_takes[i].cue != selected) continue;
        if (variants[selected] > 1 && director->last_take[selected] == i) continue;
        take = i;
        draw -= cc_music_takes[i].weight;
        if (draw < 0.0f) break;
    }
    return take;
}

static bool StartFade(CcMusicDirector *director, int take, float seconds)
{
    int slot = -1;
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) {
        if (director->voice[i].take == take) { slot = i; break; }
        if (director->voice[i].take < 0) slot = i;
    }
    if (slot < 0) return false;
    if (director->voice[slot].take < 0)
        director->voice[slot] = (CcMusicVoice){.take = take};
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) {
        CcMusicVoice *voice = &director->voice[i];
        voice->from_power = voice->gain * voice->gain;
        voice->target_power = i == slot ? 1.0f : 0.0f;
    }
    director->target_voice = slot;
    director->fade_age = 0.0f;
    director->fade_seconds = seconds;
    int cue = cc_music_takes[take].cue;
    for (int i = 3; i > 0; --i) director->recent_cue[i] = director->recent_cue[i - 1];
    director->recent_cue[0] = cue;
    director->last_take[cue] = take;
    return true;
}

void CcMusicUpdate(CcMusicDirector *director, const CcMusicContext *context,
                   float delta_seconds)
{
    if (director == NULL || context == NULL || !isfinite(delta_seconds) ||
        delta_seconds <= 0.0f) return;
    float dt = fminf(delta_seconds, 1.0f);
    CcMusicContext effective = *context;
    if (context->combat) {
        director->combat_context = *context;
        director->combat_release = 8.0f;
    } else {
        director->combat_release = fmaxf(0.0f, director->combat_release - dt);
        if (director->combat_release > 0.0f && context->theme[CC_MUSIC_LOSS] < 0.5f)
            effective = director->combat_context;
    }
    director->fade_age += dt;
    float progress = director->fade_seconds > 0.0f ?
        Unit(director->fade_age / director->fade_seconds) : 1.0f;
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) {
        CcMusicVoice *voice = &director->voice[i];
        if (voice->take < 0) continue;
        voice->age += dt;
        voice->gain = sqrtf(fmaxf(0.0f, voice->from_power * (1.0f - progress) +
                                 voice->target_power * progress));
        if (progress >= 1.0f && voice->target_power == 0.0f)
            *voice = (CcMusicVoice){.take = -1};
    }
    CcMusicVoice *current = director->target_voice >= 0 ?
        &director->voice[director->target_voice] : NULL;
    bool change = current == NULL;
    float fade = 12.0f;
    if (current != NULL && current->take >= 0) {
        int cue = cc_music_takes[current->take].cue;
        float score = CcMusicScore(&effective, cue);
        float best = 0.0f;
        bool combat_available = false;
        for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i) {
            if (!director->available[i]) continue;
            int candidate = cc_music_takes[i].cue;
            float candidate_score = CcMusicScore(&effective, candidate);
            best = fmaxf(best, candidate_score);
            if (cc_music_cues[candidate].combat && candidate_score > 0.0f)
                combat_available = true;
        }
        bool combat_change = (effective.combat && combat_available) !=
            cc_music_cues[cue].combat;
        bool near_end = current->age >= director->duration[current->take] - 8.0f;
        change = combat_change || !director->available[current->take] ||
            near_end || (current->age >= 25.0f && score < best * 0.55f);
        fade = combat_change ? (effective.combat ? 1.2f : 8.0f) : near_end ? 8.0f : 12.0f;
    } else {
        fade = effective.combat ? 1.2f : 4.0f;
    }
    if (!change) return;
    int take = CcMusicChoose(director, &effective);
    if (take < 0) {
        if (current != NULL) {
            for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) {
                CcMusicVoice *voice = &director->voice[i];
                voice->from_power = voice->gain * voice->gain;
                voice->target_power = 0.0f;
            }
            director->target_voice = -1;
            director->fade_age = 0.0f;
            director->fade_seconds = 4.0f;
        }
        return;
    }
    if (current != NULL && current->take == take) {
        if (current->age >= director->duration[take] - 8.0f)
            current->age = 0.0f; /* A single installed take can loop. */
        return;
    }
    (void)StartFade(director, take, fade);
}
