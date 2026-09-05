#include "story/cc_speech.h"
#include "test_support.h"

#include <stdio.h>
#include <string.h>

static CcSim sim, before;

static void CharacterIdentity(void)
{
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    before = sim;
    for (int32_t i = 0; i < sim.character_count; ++i) {
        const CcCharacter *character = &sim.characters[i];
        uint32_t voice = CcSpeechCharacterVoice(&sim, character);
        CC_CHECK(CcSpeechVoiceAt(voice) != NULL);
        CcCharacter moved = *character;
        moved.current_settlement_id += 100;
        moved.stress = 95;
        moved.appearance_seed ^= 123U;
        moved.player_disposition = -100;
        CC_CHECK(CcSpeechCharacterVoice(&sim, &moved) == voice);
        if (strcmp(character->name, "Mara Venn") == 0) {
            CC_CHECK(strcmp(CcSpeechVoiceAt(voice)->id, "mara-v1") == 0);
            moved.generation = 1;
            moved.id += 1000;
            CC_CHECK(CcSpeechCharacterVoice(&sim, &moved) >= 5U);
        }
    }
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CC_CHECK(CcSpeechVoiceAt(CcSpeechVoiceCount()) == NULL);
}

static void ExactWords(void)
{
    CcSpeech first, next;
    CC_CHECK(CcSpeechCompose(&first, "test", 1, "Mara", 0, "Eight boxes.",
        CC_SPEECH_PLAIN, CC_SPEECH_CONVERSATION, 2));
    CC_CHECK(CcSpeechCompose(&next, "other", 2, "Another name", 0, "Eight boxes.",
        CC_SPEECH_PLAIN, CC_SPEECH_FEEDBACK, 3));
    CC_CHECK(first.audio_key == next.audio_key);
    CC_CHECK(CcSpeechCompose(&next, "test", 1, "Mara", 0, "Nine boxes.",
        CC_SPEECH_PLAIN, CC_SPEECH_CONVERSATION, 2));
    CC_CHECK(first.audio_key != next.audio_key);
    CC_CHECK(CcSpeechCompose(&next, "test", 1, "Mara", 0, "Eight boxes.",
        CC_SPEECH_URGENT, CC_SPEECH_CONVERSATION, 2));
    CC_CHECK(first.audio_key != next.audio_key);
    CC_CHECK(CcSpeechCompose(&next, "test", 1, "Mara", 1, "Eight boxes.",
        CC_SPEECH_PLAIN, CC_SPEECH_CONVERSATION, 2));
    CC_CHECK(first.audio_key != next.audio_key);
    char path[128];
    CC_CHECK(CcSpeechPath(&first, path, sizeof(path)));
    CC_CHECK(strncmp(path, "assets/audio/speech/", 20) == 0);
    CC_CHECK(!CcSpeechPath(&first, path, 5));
    CC_CHECK(path[0] == '\0');
    char oversized[CC_SPEECH_TEXT_CAPACITY + 1];
    memset(oversized, 'a', sizeof(oversized) - 1);
    oversized[sizeof(oversized) - 1] = '\0';
    CC_CHECK(!CcSpeechCompose(&next, "test", 1, "Mara", 0, oversized,
        CC_SPEECH_PLAIN, CC_SPEECH_CONVERSATION, 2));
    CC_CHECK(next.text[0] == '\0');
}

static void CampaignSpeech(void)
{
    for (uint32_t seed = 1; seed <= 16; ++seed) {
        CcSimInit(&sim, seed);
        before = sim;
        for (int32_t i = 0; i < sim.situation_count; ++i) {
            const CcSituation *situation = &sim.situations[i];
            const CcCharacter *character = CcSimSituationConversationCharacter(
                &sim, situation, CcSimSituationOfferSettlementId(&sim, situation));
            CcSpeech speech;
            CC_CHECK(CcSpeechCharacter(&sim, situation, character, &speech));
            char expected[CC_SPEECH_TEXT_CAPACITY];
            CC_CHECK(CcStoryCharacterText(&sim, situation, character, expected, sizeof(expected)));
            CC_CHECK(strcmp(speech.text, expected) == 0);
            CC_CHECK(speech.speaker_id == character->id);
            if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
                CcCharacter successor = *character;
                (void)snprintf(successor.name, sizeof(successor.name), "New Miner");
                successor.generation = 1;
                CC_CHECK(CcSpeechCharacter(&sim, situation, &successor, &speech));
                CC_CHECK(strstr(speech.text, "fled the west gallery") != NULL);
                CC_CHECK(strncmp(speech.line_id, "situation.", 10) == 0);
            }
        }
        CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    }
}

static void EverydaySpeech(void)
{
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    before = sim;
    CcSpeech speech;
    CcId place_id = sim.player.location_id;
    CC_CHECK(CcSpeechGreeting(&sim, place_id, 15, "Town guard", "hall", &speech));
    CC_CHECK(strstr(speech.text, CcSimSettlement(&sim, place_id)->name) != NULL);
    uint32_t voice = speech.voice_index;
    CC_CHECK(CcSpeechGreeting(&sim, place_id, 15, "Town worker", "hall", &speech));
    CC_CHECK(speech.voice_index == voice);
    CC_CHECK(CcSpeechTrade(&sim, "Keeper", CC_GOOD_FOOD, 8, 123, 0, "", &speech));
    CC_CHECK(strstr(speech.text, "123 crowns") != NULL && strstr(speech.text, "8 ") != NULL);
    CC_CHECK(CcSpeechTrade(&sim, "Keeper", CC_GOOD_FOOD, 7, 321, 1, "", &speech));
    CC_CHECK(strstr(speech.text, "321 crowns") != NULL && strstr(speech.text, "7 ") != NULL);
    CC_CHECK(CcSpeechTrade(&sim, "Keeper", CC_GOOD_FOOD, 7, 321, 1, "Bring the full quantity.", &speech));
    CC_CHECK(strcmp(speech.text, "Bring the full quantity.") == 0);
    for (int choice = CC_STORY_PLAYER_ASK; choice <= CC_STORY_PLAYER_LEAVE; ++choice) {
        CC_CHECK(CcSpeechPlayerChoice(&sim, &sim.situations[0], (CcStoryPlayerChoice)choice, 5, &speech));
        CC_CHECK(strncmp(speech.text, "1 ", 2) != 0 && strncmp(speech.text, "2 ", 2) != 0);
        CC_CHECK(strstr(speech.text, "Esc") == NULL);
    }
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CC_CHECK(!CcSpeechRoad(&sim, &speech));
    sim.journey.active = true;
    sim.journey.phase = CC_JOURNEY_PHASE_BLOCKED;
    sim.journey.route_id = sim.bandits[0].route_id;
    sim.journey.bargain_cost = 37;
    CC_CHECK(CcSpeechRoad(&sim, &speech));
    CC_CHECK(strstr(speech.text, "37 crowns") != NULL);
    CC_CHECK(speech.speaker_id == sim.bandits[0].id);
}

int main(void)
{
    CharacterIdentity();
    ExactWords();
    CampaignSpeech();
    EverydaySpeech();
    return 0;
}
