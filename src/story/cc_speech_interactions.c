#include "story/cc_speech.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

bool CcSpeechGreeting(const CcSim *sim, CcId place_id, CcId object_id,
                       const char *speaker, const char *service, CcSpeech *speech)
{
    if (speech == NULL) return false;
    *speech = (CcSpeech){0};
    if (sim == NULL) return false;
    const CcSettlement *place = CcSimSettlement(sim, place_id);
    if (place == NULL || speaker == NULL || service == NULL) return false;
    char text[CC_SPEECH_TEXT_CAPACITY];
    if (strstr(speaker, "guard") != NULL) {
        (void)snprintf(text, sizeof(text), "Welcome to %s. Keep the gate clear for the wagons.", place->name);
    } else if (strcmp(speaker, "Traveller") == 0) {
        (void)snprintf(text, sizeof(text), "I am stopping in %s. Try the notice board for word from the other towns.", place->name);
    } else if (strstr(speaker, "trader") != NULL) {
        (void)snprintf(text, sizeof(text), "Come into the %s. We can settle a price at the counter.", service);
    } else if (place->hunger >= 50) {
        (void)snprintf(text, sizeof(text), "Food is short here in %s. Ask at the %s if you can help.", place->name, service);
    } else {
        (void)snprintf(text, sizeof(text), "Welcome to %s. The %s has work and supplies.", place->name, service);
    }
    return CcSpeechCompose(speech, "town.greeting", object_id, speaker,
        CcSpeechLocalVoice(sim->world_seed, place_id, object_id), text,
        CC_SPEECH_PLAIN, CC_SPEECH_CONVERSATION, 0);
}

bool CcSpeechRoad(const CcSim *sim, CcSpeech *speech)
{
    if (speech == NULL) return false;
    *speech = (CcSpeech){0};
    if (sim == NULL || !sim->journey.active || sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED) return false;
    const CcBanditGroup *company = CcSimBanditGroupOnRoute(sim, sim->journey.route_id);
    if (company == NULL) return false;
    CcGood good = CC_GOOD_FOOD;
    int32_t quantity = 0;
    char text[CC_SPEECH_TEXT_CAPACITY];
    if (CcSimBanditProvisionDemand(sim, sim->journey.route_id, &good, &quantity)) {
        (void)snprintf(text, sizeof(text), "%s Give us %d %s or %d crowns, and you may pass.",
            CcStoryRoadCompanyLine(company), quantity, CcGoodName(good), sim->journey.bargain_cost);
    } else {
        (void)snprintf(text, sizeof(text), "%s Pay %d crowns to pass.",
            CcStoryRoadCompanyLine(company), sim->journey.bargain_cost);
    }
    return CcSpeechCompose(speech, "road.demand", company->id, company->name,
        CcSpeechLocalVoice(sim->world_seed, company->id, company->id), text,
        CC_SPEECH_FIRM, CC_SPEECH_CONVERSATION, 0);
}

bool CcSpeechPlayerChoice(const CcSim *sim, const CcSituation *situation,
                           CcStoryPlayerChoice choice, uint32_t voice_index,
                           CcSpeech *speech)
{
    if (speech == NULL) return false;
    *speech = (CcSpeech){0};
    if (sim == NULL || situation == NULL) return false;
    const char *text = CcStoryPlayerChoiceText(situation->kind, choice);
    const char *words = strstr(text, "  ");
    words = words != NULL ? words + 2 : text;
    char report[CC_SPEECH_TEXT_CAPACITY];
    if (choice == CC_STORY_PLAYER_REPORT) {
        const CcCharacter *sponsor = CcSimSituationSponsorCharacter(sim, situation);
        (void)snprintf(report, sizeof(report), "I will tell %s.", sponsor != NULL ? sponsor->name : "the official");
        words = report;
    }
    char line_id[64];
    (void)snprintf(line_id, sizeof(line_id), "player.choice.%d.%d", (int)situation->kind, (int)choice);
    return CcSpeechCompose(speech, line_id, sim->player.id, "You", voice_index,
        words, CC_SPEECH_PLAIN, CC_SPEECH_CONVERSATION, situation->cause_event_id);
}

bool CcSpeechTrade(const CcSim *sim, const char *keeper, CcGood good,
                    int32_t quantity, CcMoney total, int mode,
                    const char *response, CcSpeech *speech)
{
    if (speech == NULL) return false;
    *speech = (CcSpeech){0};
    if (sim == NULL || keeper == NULL || good < 0 || good >= CC_GOOD_COUNT ||
        quantity < 1 || quantity > CC_SIM_MAX_UNITS || total < 0 || mode < 0 || mode > 2) return false;
    char text[CC_SPEECH_TEXT_CAPACITY];
    if (response != NULL && response[0] != '\0') {
        if (strlen(response) >= sizeof(text)) return false;
        (void)snprintf(text, sizeof(text), "%s", response);
    } else if (mode == 2) {
        (void)snprintf(text, sizeof(text), "You have brought %d %s for the delivery. Let us settle it.", quantity, CcGoodName(good));
    } else if (mode == 0) {
        (void)snprintf(text, sizeof(text), "For %d %s, the price is %" PRId64 " crowns.",
            quantity, CcGoodName(good), total);
    } else {
        (void)snprintf(text, sizeof(text), "I can pay %" PRId64 " crowns for %d %s.",
            total, quantity, CcGoodName(good));
    }
    return CcSpeechCompose(speech, "trade.counter", sim->player.location_id, keeper,
        CcSpeechLocalVoice(sim->world_seed, sim->player.location_id, UINT64_C(0x400000000)),
        text, CC_SPEECH_PLAIN, CC_SPEECH_CONVERSATION, 0);
}
