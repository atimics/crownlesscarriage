#include "client/cc_music.h"
#include "client/cc_music_library.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #test); return 1; \
} } while (0)

static CcSim sim;

static int TakeFor(int number, int variant)
{
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i)
        if (cc_music_takes[i].cue == number - 1 && cc_music_takes[i].variant == variant)
            return i;
    return -1;
}

static void Advance(CcMusicDirector *director, const CcMusicContext *context, int frames)
{
    for (int i = 0; i < frames; ++i) CcMusicUpdate(director, context, 1.0f / 60.0f);
}

int main(void)
{
    CcSimInit(&sim, 17);
    CcId from_id = sim.settlements[0].id;
    CcId to_id = sim.settlements[1].id;
    sim.player.location_id = from_id;
    sim.journey = (CcJourneyEncounter){.active = true, .origin_id = from_id,
        .destination_id = to_id, .phase = CC_JOURNEY_PHASE_TRAVELLING};
    CcMusicScene road = {.road = true};
    float previous_from = 10.0f;
    float previous_to = -1.0f;
    for (int progress = 0; progress <= 1000; progress += 100) {
        sim.carriage.progress_milli = progress;
        CcMusicContext context = CcMusicContextFor(&sim, road);
        float from = CcMusicScore(&context, 6);
        float to = CcMusicScore(&context, 10);
        CHECK(from <= previous_from && to >= previous_to);
        CHECK(fabsf(context.region[0] + context.region[2] - 1.0f) < 0.00001f);
        previous_from = from; previous_to = to;
    }
    sim.journey.origin_id = to_id; sim.journey.destination_id = from_id;
    sim.carriage.progress_milli = 250;
    CcMusicContext reversed = CcMusicContextFor(&sim, road);
    CHECK(fabsf(reversed.region[2] - 0.75f) < 0.00001f);
    CHECK(fabsf(reversed.region[0] - 0.25f) < 0.00001f);
    sim.carriage.progress_milli = -200;
    CHECK(CcMusicContextFor(&sim, road).region[2] == 1.0f);
    sim.carriage.progress_milli = 1200;
    CHECK(CcMusicContextFor(&sim, road).region[0] == 1.0f);

    CcMusicContext context = {.theme = {[CC_MUSIC_TRAVEL] = 1.0f}};
    CcMusicContext named = context;
    CcMusicAttractPlace(&named, "Stag's Mill", 1.0f);
    CHECK(CcMusicScore(&named, 8) > CcMusicScore(&context, 8));
    CcMusicAttractPlace(&named, "Flooded Turntable", 1.0f);
    CHECK(named.cue[39] == 1.0f);
    CHECK(CcMusicScore(&context, 52) == 0.0f); /* Dragon combat waits for its context. */
    CHECK(CcMusicScore(&context, 62) == 0.0f);
    context.theme[CC_MUSIC_TRAVEL] = NAN;
    CHECK(isfinite(CcMusicScore(&context, 2)));
    context.theme[CC_MUSIC_TRAVEL] = 1.0f;

    CcMusicDirector director;
    CcMusicInit(&director, 17);
    CHECK(CcMusicChoose(&director, &context) == -1);
    int calm_a = TakeFor(3, 1), calm_b = TakeFor(3, 2), attack = TakeFor(59, 1);
    CHECK(calm_a >= 0 && calm_b >= 0 && attack >= 0);
    CcMusicLibrary library = {0};
    const unsigned char manifest[] = "CROWNLESS_MUSIC 1\n03-01-"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef.mp3\n";
    CHECK(CcMusicLibraryParse(&library, manifest, sizeof(manifest) - 1));
    CHECK(strncmp(library.file[calm_a], "03-01-", 6) == 0);
    CcMusicLibrary saved = library;
    CHECK(!CcMusicLibraryParse(&library, manifest, sizeof(manifest) - 2));
    CHECK(memcmp(&saved, &library, sizeof(library)) == 0);
    unsigned char invalid[sizeof(manifest)];
    memcpy(invalid, manifest, sizeof(invalid));
    invalid[17] = '/';
    CHECK(!CcMusicLibraryParse(&library, invalid, sizeof(invalid) - 1));
    CHECK(!CcMusicLibraryParse(&library, (const unsigned char *)"<html>", 6));
    int bundled_count = 0;
    for (int i = 0; i < CC_MUSIC_TAKE_COUNT; ++i) bundled_count += CcMusicBundled(i) ? 1 : 0;
    CHECK(bundled_count == 27);
    director.available[calm_a] = true;
    director.available[calm_b] = true;
    director.available[attack] = true;
    Advance(&director, &context, 300);
    CHECK(director.target_voice >= 0);
    int first = director.voice[director.target_voice].take;
    CHECK(first == calm_a || first == calm_b);
    for (int i = 0; i < 100; ++i) CHECK(CcMusicChoose(&director, &context) != first);

    CcMusicContext battle = {.combat = true, .theme = {
        [CC_MUSIC_BANDIT] = 1.0f, [CC_MUSIC_COMBAT] = 1.0f}};
    /* A selected download holds the existing score until its bytes are ready. */
    CcMusicDirector waiting;
    CcMusicInit(&waiting, 22);
    waiting.available[calm_a] = true;
    Advance(&waiting, &context, 300);
    waiting.available[calm_b] = true;
    waiting.ready[calm_b] = false;
    waiting.duration[calm_a] = 10.0f;
    Advance(&waiting, &context, 300);
    CHECK(waiting.requested_take == calm_b);
    CHECK(waiting.voice[waiting.target_voice].take == calm_a);
    CHECK(waiting.voice[waiting.target_voice].gain > 0.99f);
    uint32_t held_random = waiting.random_state;
    Advance(&waiting, &context, 120);
    CHECK(waiting.random_state == held_random);
    waiting.ready[calm_b] = true;
    Advance(&waiting, &context, 600);
    CHECK(waiting.voice[waiting.target_voice].take == calm_b);
    CHECK(waiting.voice[waiting.target_voice].gain > 0.99f);
    /* A bandit attack replaces a pending calm download with a ready combat take. */
    waiting.ready[calm_a] = false;
    waiting.duration[calm_b] = 10.0f;
    Advance(&waiting, &context, 60);
    CHECK(waiting.requested_take == calm_a);
    waiting.available[attack] = true;
    Advance(&waiting, &battle, 90);
    CHECK(waiting.voice[waiting.target_voice].take == attack);
    CHECK(waiting.requested_take == -1);
    /* Losing the remote choice leaves the bundled score playable. */
    CcMusicInit(&waiting, 22);
    waiting.available[calm_a] = true;
    Advance(&waiting, &context, 300);
    waiting.available[calm_b] = true;
    waiting.ready[calm_b] = false;
    waiting.duration[calm_a] = 10.0f;
    Advance(&waiting, &context, 60);
    CHECK(waiting.requested_take == calm_b);
    waiting.available[calm_b] = false;
    Advance(&waiting, &context, 600);
    CHECK(waiting.voice[waiting.target_voice].take == calm_a);
    CHECK(waiting.voice[waiting.target_voice].gain > 0.99f);
    for (int i = 0; i < 100; ++i) CHECK(CcMusicChoose(&director, &battle) == attack);
    CcMusicUpdate(&director, &battle, 1.0f / 60.0f);
    CHECK(director.voice[director.target_voice].take == attack);
    CHECK(director.fade_seconds == 1.2f);
    for (int i = 0; i < 90; ++i) {
        CcMusicUpdate(&director, &battle, 1.0f / 60.0f);
        float power = 0.0f;
        for (int voice = 0; voice < CC_MUSIC_VOICE_COUNT; ++voice) {
            CHECK(isfinite(director.voice[voice].gain));
            CHECK(director.voice[voice].gain >= 0.0f && director.voice[voice].gain <= 1.0f);
            power += director.voice[voice].gain * director.voice[voice].gain;
        }
        CHECK(fabsf(power - 1.0f) < 0.0001f);
    }
    Advance(&director, &context, 300);
    CHECK(director.voice[director.target_voice].take == attack);
    Advance(&director, &context, 190);
    CHECK(director.voice[director.target_voice].take != attack);
    CHECK(director.fade_seconds == 8.0f);
    Advance(&director, &context, 500);
    int voices = 0;
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i)
        if (director.voice[i].take >= 0) voices += 1;
    CHECK(voices == 1);

    /* A fight can interrupt a regional fade while every gain stays continuous. */
    int region_take = TakeFor(11, 1);
    director.available[region_take] = true;
    director.voice[director.target_voice].age = 40.0f;
    CcMusicContext arrival = context;
    arrival.cue[10] = 1.0f; arrival.region[2] = 1.0f;
    CcMusicUpdate(&director, &arrival, 1.0f / 60.0f);
    Advance(&director, &arrival, 120);
    float gains[CC_MUSIC_VOICE_COUNT];
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) gains[i] = director.voice[i].gain;
    CcMusicUpdate(&director, &battle, 1.0f / 60.0f);
    CHECK(director.voice[director.target_voice].take == attack);
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i)
        CHECK(fabsf(gains[i] - director.voice[i].gain) < 0.02f);
    CcMusicDirector before = director;
    CcMusicUpdate(&director, &context, NAN);
    CHECK(memcmp(&before, &director, sizeof(director)) == 0);
    CcMusicUpdate(&director, &context, 0.0f);
    CHECK(memcmp(&before, &director, sizeof(director)) == 0);

    /* Regional draw probabilities follow travel position. */
    sim.journey.origin_id = from_id; sim.journey.destination_id = to_id;
    for (int stage = 0; stage < 3; ++stage) {
        sim.carriage.progress_milli = stage * 500;
        CcMusicContext travel = CcMusicContextFor(&sim, road);
        CcMusicDirector sample;
        CcMusicInit(&sample, 91);
        sample.available[TakeFor(7, 1)] = true;
        sample.available[TakeFor(11, 1)] = true;
        int farming = 0;
        for (int draw = 0; draw < 2000; ++draw) {
            int choice = CcMusicChoose(&sample, &travel);
            CHECK(choice >= 0);
            if (cc_music_takes[choice].cue == 6) farming += 1;
        }
        if (stage == 0) CHECK(farming > 1950);
        if (stage == 1) CHECK(farming > 900 && farming < 1100);
        if (stage == 2) CHECK(farming < 50);
    }

    /* Extra alternates keep the title's probability stable. */
    CcMusicDirector few, many;
    CcMusicInit(&few, 212); CcMusicInit(&many, 212);
    few.available[TakeFor(2, 1)] = many.available[TakeFor(2, 1)] = true;
    few.available[TakeFor(3, 1)] = true;
    for (int variant = 1; variant <= 6; ++variant)
        many.available[TakeFor(3, variant)] = true;
    for (int draw = 0; draw < 2000; ++draw) {
        int first_choice = CcMusicChoose(&few, &context);
        int second_choice = CcMusicChoose(&many, &context);
        CHECK(first_choice >= 0 && second_choice >= 0);
        CHECK(cc_music_takes[first_choice].cue == cc_music_takes[second_choice].cue);
    }

    CcMusicInit(&director, 17);
    director.available[calm_a] = true;
    Advance(&director, &context, 300);
    CcMusicContext battle_with_travel = battle;
    battle_with_travel.theme[CC_MUSIC_TRAVEL] = 1.0f;
    Advance(&director, &battle_with_travel, 300);
    CHECK(director.voice[director.target_voice].take == calm_a);
    CHECK(director.voice[director.target_voice].age > 9.0f);
    director.available[calm_a] = false;
    Advance(&director, &battle_with_travel, 300);
    for (int i = 0; i < CC_MUSIC_VOICE_COUNT; ++i) CHECK(director.voice[i].take == -1);

    sim.dungeon_expedition = (CcDungeonExpedition){.active = true,
        .dungeon_id = sim.dungeons[0].id, .current_room = 6};
    uint64_t sim_hash = CcSimHash(&sim);
    CcMusicContext room = CcMusicContextFor(&sim, (CcMusicScene){0});
    CHECK(room.cue[39] == 1.0f && room.theme[CC_MUSIC_FLOOD] > 0.8f);
    CHECK(CcSimHash(&sim) == sim_hash);

    puts("Music: regional blends, named places, shuffle, combat and interrupted fades passed.");
    return 0;
}
