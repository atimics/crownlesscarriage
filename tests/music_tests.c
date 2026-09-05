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
    /* Every town keeps its familiar tune through everyday, shortage and recovery. */
    static const int original_towns[] = {8, 12, 16, 20, 24, 28};
    static const int original_variants[] = {5, 5, 3, 1, 1, 1};
    static const CcMusicMood moods[] = {
        CC_MUSIC_MOOD_EVERYDAY, CC_MUSIC_MOOD_SHORTAGE, CC_MUSIC_MOOD_RECOVERY
    };
    for (int town = 0; town < 6; ++town) {
        CcSimInit(&sim, 17);
        CcSettlement *place = &sim.settlements[town];
        sim.player.location_id = place->id;
        sim.current_day = 10;
        sim.event_count = 0;
        sim.event_write_index = 0;
        CcMusicDirector town_music;
        CcMusicInit(&town_music, 17);
        int original = TakeFor(original_towns[town], original_variants[town]);
        CHECK(original >= 0 && CcMusicBundled(original));
        /* The currently exported town cue carries every mood before new exports arrive. */
        town_music.available[original] = true;
        for (int mood = 0; mood < 3; ++mood) {
            place->hunger = mood == 1 ? 40 : 39;
            sim.event_count = sim.event_write_index = mood == 2 ? 1 : 0;
            sim.events[0] = (CcEvent){.day = 9, .kind = CC_EVENT_RELIEF,
                .location_id = place->id, .magnitude = 1};
            CcMusicContext context = CcMusicContextFor(&sim, (CcMusicScene){0});
            CHECK(context.town_mood == moods[mood]);
            CHECK(CcMusicChoose(&town_music, &context) == original);
        }
        for (int mood = 0; mood < 3; ++mood) {
            int number = 65 + town * 3 + mood;
            town_music.available[TakeFor(number, 1)] = true;
            town_music.available[TakeFor(number, 2)] = true;
        }
        for (int mood = 0; mood < 3; ++mood) {
            place->hunger = mood == 1 ? 40 : 39;
            sim.event_count = sim.event_write_index = mood == 2 ? 1 : 0;
            CcMusicContext context = CcMusicContextFor(&sim, (CcMusicScene){0});
            int cue = 64 + town * 3 + mood;
            CHECK(CcMusicScore(&context, cue) >=
                  2.0f * CcMusicScore(&context, original_towns[town] - 1));
            for (int candidate = 64; candidate < CC_MUSIC_CUE_COUNT; ++candidate)
                if (candidate != cue) CHECK(CcMusicScore(&context, candidate) == 0.0f);
            int chosen_a = 0, chosen_b = 0, old = 0;
            for (int draw = 0; draw < 1000; ++draw) {
                int selected = CcMusicChoose(&town_music, &context);
                if (selected == original) old++;
                else if (selected == TakeFor(cue + 1, 1)) chosen_a++;
                else if (selected == TakeFor(cue + 1, 2)) chosen_b++;
                else CHECK(false);
            }
            CHECK(chosen_a > 300 && chosen_b > 300 && old > 100 && old < 300);
            town_music.recent_cue[0] = cue;
            old = 0;
            for (int draw = 0; draw < 1000; ++draw)
                old += CcMusicChoose(&town_music, &context) == original;
            CHECK(old > 350 && old < 650);
            town_music.recent_cue[0] = -1;
            int attack = TakeFor(59, 1);
            town_music.available[attack] = true;
            CcMusicContext combat = context;
            combat.combat = true;
            combat.theme[CC_MUSIC_BANDIT] = 1.0f;
            combat.theme[CC_MUSIC_COMBAT] = 1.0f;
            CHECK(CcMusicChoose(&town_music, &combat) == attack);
        }
        /* Recovery uses a recent positive delivery at this town, then expires. */
        place->hunger = 39;
        sim.events[0].day = 8;
        CHECK(CcMusicContextFor(&sim, (CcMusicScene){0}).town_mood == CC_MUSIC_MOOD_RECOVERY);
        sim.events[0].day = 7;
        CHECK(CcMusicContextFor(&sim, (CcMusicScene){0}).town_mood == CC_MUSIC_MOOD_EVERYDAY);
        sim.events[0].day = 11;
        CHECK(CcMusicContextFor(&sim, (CcMusicScene){0}).town_mood == CC_MUSIC_MOOD_EVERYDAY);
        sim.events[0].day = 10;
        sim.events[0].location_id = sim.settlements[(town + 1) % 6].id;
        CHECK(CcMusicContextFor(&sim, (CcMusicScene){0}).town_mood == CC_MUSIC_MOOD_EVERYDAY);
        sim.events[0].location_id = place->id;
        sim.events[0].magnitude = 0;
        CHECK(CcMusicContextFor(&sim, (CcMusicScene){0}).town_mood == CC_MUSIC_MOOD_EVERYDAY);
        sim.events[0].magnitude = 1;
        place->hunger = 40;
        uint64_t town_hash = CcSimHash(&sim);
        CHECK(CcMusicContextFor(&sim, (CcMusicScene){0}).town_mood == CC_MUSIC_MOOD_SHORTAGE);
        CHECK(CcSimHash(&sim) == town_hash);
        static const CcMusicScene outside_town[] = {
            {.road = true}, {.goblin_cave = true}, {.dragon_cave = true}, {.loss = true}
        };
        for (int site = 0; site < 4; ++site) {
            CcMusicContext context = CcMusicContextFor(&sim, outside_town[site]);
            CHECK(context.town_mood == CC_MUSIC_MOOD_ANY);
            for (int cue = 64; cue < CC_MUSIC_CUE_COUNT; ++cue)
                CHECK(CcMusicScore(&context, cue) == 0.0f);
        }
        sim.dungeon_expedition.active = true;
        CcMusicContext dungeon = CcMusicContextFor(&sim, (CcMusicScene){0});
        for (int cue = 64; cue < CC_MUSIC_CUE_COUNT; ++cue)
            CHECK(CcMusicScore(&dungeon, cue) == 0.0f);
    }

    /* Each underground scene draws its score from the site after leaving town. */
    static const CcMusicScene underground[] = {
        {.goblin_cave = true}, {.dragon_cave = true}, {0}
    };
    static const int site_cues[] = {46, 52, 39};
    for (int site = 0; site < 3; ++site) {
        CcSimInit(&sim, 17);
        sim.player.location_id = sim.settlements[3].id; /* Silverwick. */
        CcMusicDirector site_music;
        CcMusicInit(&site_music, 17);
        int town_take = TakeFor(20, 1);
        int site_take = TakeFor(site_cues[site], 1);
        CHECK(town_take >= 0 && site_take >= 0);
        site_music.available[town_take] = true;
        site_music.available[site_take] = true;
        CcMusicContext town = CcMusicContextFor(&sim, (CcMusicScene){0});
        CHECK(CcMusicChoose(&site_music, &town) == town_take);
        sim.dungeon_expedition.active = site == 2;
        CcMusicContext inside = CcMusicContextFor(&sim, underground[site]);
        CHECK(CcMusicScore(&inside, 19) == 0.0f);
        CHECK(CcMusicChoose(&site_music, &inside) == site_take);
        sim.dungeon_expedition.active = false;
        CcMusicContext returned = CcMusicContextFor(&sim, (CcMusicScene){0});
        CHECK(CcMusicChoose(&site_music, &returned) == town_take);
    }

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
    /* The whole expanded host catalog fits the parser and resolves each take. */
    unsigned char full_catalog[16384];
    size_t full_size = (size_t)snprintf((char *)full_catalog, sizeof(full_catalog),
                                      "CROWNLESS_MUSIC 1\n");
    for (int take = 0; take < CC_MUSIC_TAKE_COUNT; ++take) {
        int written = snprintf((char *)full_catalog + full_size,
            sizeof(full_catalog) - full_size,
            "%02d-%02d-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef.mp3\n",
            cc_music_takes[take].cue + 1, cc_music_takes[take].variant);
        CHECK(written == 75);
        full_size += (size_t)written;
    }
    CHECK(CcMusicLibraryParse(&library, full_catalog, full_size));
    for (int take = 0; take < CC_MUSIC_TAKE_COUNT; ++take)
        CHECK(library.file[take][0] != '\0');
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
    /* Prepare the next song while the current song still has minutes to play. */
    CcMusicDirector ahead;
    CcMusicInit(&ahead, 22);
    ahead.available[calm_a] = true;
    ahead.duration[calm_a] = 180.0f;
    Advance(&ahead, &context, 240);
    ahead.available[calm_b] = true;
    ahead.ready[calm_b] = false;
    Advance(&ahead, &context, 120);
    CHECK(ahead.requested_take == calm_b);
    CHECK(ahead.voice[ahead.target_voice].take == calm_a);
    uint32_t ahead_random = ahead.random_state;
    ahead.ready[calm_b] = true;
    Advance(&ahead, &context, 600);
    CHECK(ahead.requested_take == calm_b && ahead.random_state == ahead_random);
    CHECK(ahead.voice[ahead.target_voice].take == calm_a);
    ahead.voice[ahead.target_voice].age = 172.0f;
    CcMusicUpdate(&ahead, &context, 1.0f / 60.0f);
    CHECK(ahead.voice[ahead.target_voice].take == calm_b);

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
