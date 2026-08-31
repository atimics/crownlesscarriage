#include "story/cc_story.h"

#include "test_support.h"

#include <stdio.h>
#include <string.h>

static CcSituation *FindSituation(CcSim *sim, CcSituationKind kind)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].kind == kind) return &sim->situations[i];
    }
    return NULL;
}

static void ValidateCatalogue(void)
{
    size_t count = CcStoryAuthoredLineCount();
    CC_CHECK(count >= 30U);
    for (size_t i = 0U; i < count; ++i) {
        const CcStoryLine *line = CcStoryAuthoredLineAt(i);
        CC_CHECK(line != NULL);
        CC_CHECK(line->id != NULL && line->id[0] != '\0');
        CC_CHECK(line->text != NULL && line->text[0] != '\0');
        CC_CHECK(strlen(line->text) <= 96U);
        CC_CHECK(strcmp(CcStoryBeatName(line->beat), "unknown") != 0);
        for (size_t other = i + 1U; other < count; ++other) {
            const CcStoryLine *candidate = CcStoryAuthoredLineAt(other);
            CC_CHECK(candidate != NULL);
            CC_CHECK(strcmp(line->id, candidate->id) != 0);
        }
    }
    CC_CHECK(CcStoryAuthoredLineAt(count) == NULL);
}

static void ValidateSituationCoverage(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    for (int32_t i = 0; i < sim.situation_count; ++i) {
        CcSituation *situation = &sim.situations[i];
        const CcCharacter *sponsor = CcSimSituationSponsorCharacter(
            &sim, situation);
        const CcCharacter *affected = CcSimSituationAffectedCharacter(
            &sim, situation);
        CC_CHECK(sponsor != NULL);
        CC_CHECK(affected != NULL);
        CC_CHECK(CcStoryCharacterLine(&sim, situation, sponsor) != NULL);
        CC_CHECK(CcStoryCharacterLine(&sim, situation, affected) != NULL);
    }
}

static void ValidateConversationBeats(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcSituation *relief = FindSituation(
        &sim, CC_SITUATION_RELIEF_DELIVERY);
    CC_CHECK(relief != NULL);
    CcId offer = CcSimSituationOfferSettlementId(&sim, relief);
    const CcCharacter *sponsor = CcSimSituationSponsorCharacter(
        &sim, relief);
    const CcCharacter *affected = CcSimSituationAffectedCharacter(
        &sim, relief);
    CC_CHECK(sponsor != NULL);
    CC_CHECK(affected != NULL);
    CC_CHECK(CcSimSituationConversationCharacter(&sim, relief, offer) ==
             sponsor);
    if (affected->current_settlement_id != offer) {
        CC_CHECK(CcSimSituationConversationCharacter(
                     &sim, relief, affected->current_settlement_id) ==
                 affected);
    }

    const CcStoryLine *offer_line = CcStoryCharacterLine(
        &sim, relief, sponsor);
    CC_CHECK(offer_line != NULL);
    CC_CHECK(offer_line->beat == CC_STORY_BEAT_OFFER);
    CC_CHECK(strcmp(offer_line->id, "empty_granary.mara.offer") == 0);

    sim.player.location_id = offer;
    sim.carriage.location_id = offer;
    char error[192];
    CcCommand listen = {
        .kind = CC_COMMAND_CHARACTER_RESPONSE,
        .target_id = relief->id,
        .amount = CC_CHARACTER_RESPONSE_LISTEN
    };
    CC_CHECK(CcSimApply(&sim, &listen, error, sizeof(error)));
    sponsor = CcSimSituationSponsorCharacter(&sim, relief);
    affected = CcSimSituationAffectedCharacter(&sim, relief);
    CC_CHECK(sponsor != NULL);
    CC_CHECK(affected != NULL);
    CC_CHECK(CcCharacterRemembers(
        sponsor, CC_CHARACTER_MEMORY_MET_PLAYER, relief->id));
    CC_CHECK(!CcCharacterRemembers(
        affected, CC_CHARACTER_MEMORY_MET_PLAYER, relief->id));
    const CcStoryLine *heard_line = CcStoryCharacterLine(
        &sim, relief, sponsor);
    CC_CHECK(heard_line != NULL);
    CC_CHECK(heard_line->beat == CC_STORY_BEAT_HEARD);
    CC_CHECK(strcmp(offer_line->id, heard_line->id) != 0);

    CcCommand pledge = listen;
    pledge.amount = CC_CHARACTER_RESPONSE_PLEDGE_HELP;
    CC_CHECK(CcSimApply(&sim, &pledge, error, sizeof(error)));
    sponsor = CcSimSituationSponsorCharacter(&sim, relief);
    CC_CHECK(sponsor != NULL);
    const CcStoryLine *promised_line = CcStoryCharacterLine(
        &sim, relief, sponsor);
    CC_CHECK(promised_line != NULL);
    CC_CHECK(promised_line->beat == CC_STORY_BEAT_PROMISED);

    CcCommand abandon = {
        .kind = CC_COMMAND_ABANDON_SITUATION,
        .target_id = relief->id
    };
    CC_CHECK(CcSimApply(&sim, &abandon, error, sizeof(error)));
    sponsor = CcSimSituationSponsorCharacter(&sim, relief);
    affected = CcSimSituationAffectedCharacter(&sim, relief);
    CC_CHECK(sponsor != NULL);
    CC_CHECK(affected != NULL);
    CC_CHECK(CcCharacterRemembers(
        sponsor, CC_CHARACTER_MEMORY_PLAYER_WITHDREW, relief->id));
    CC_CHECK(CcCharacterRemembers(
        affected, CC_CHARACTER_MEMORY_PLAYER_WITHDREW, relief->id));
    const CcStoryLine *withdrew_line = CcStoryCharacterLine(
        &sim, relief, sponsor);
    CC_CHECK(withdrew_line != NULL);
    CC_CHECK(withdrew_line->beat == CC_STORY_BEAT_WITHDREW);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void ValidateFrontUrgencyBeats(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcSituation *relief = FindSituation(
        &sim, CC_SITUATION_RELIEF_DELIVERY);
    CC_CHECK(relief != NULL);
    const CcCharacter *sponsor = CcSimSituationSponsorCharacter(
        &sim, relief);
    CC_CHECK(sponsor != NULL);
    CcFront *front = NULL;
    for (int32_t i = 0; i < sim.front_count; ++i) {
        if (sim.fronts[i].id == relief->front_id) {
            front = &sim.fronts[i];
            break;
        }
    }
    CC_CHECK(front != NULL);

    front->portent.value = (front->portent.limit * 40) / 100;
    const CcStoryLine *pressing = CcStoryCharacterLine(
        &sim, relief, sponsor);
    CC_CHECK(pressing != NULL);
    CC_CHECK(pressing->beat == CC_STORY_BEAT_PRESSING);
    CC_CHECK(strcmp(pressing->id, "empty_granary.mara.pressing") == 0);

    front->portent.value = (front->portent.limit * 70) / 100;
    const CcStoryLine *breaking = CcStoryCharacterLine(
        &sim, relief, sponsor);
    CC_CHECK(breaking != NULL);
    CC_CHECK(breaking->beat == CC_STORY_BEAT_BREAKING);
    CC_CHECK(strcmp(breaking->id, "empty_granary.mara.breaking") == 0);
}

static void ValidateExpiredPromiseMemory(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x5e71a1));
    CcSituation *relief = FindSituation(
        &sim, CC_SITUATION_RELIEF_DELIVERY);
    CC_CHECK(relief != NULL);
    sim.player.location_id = CcSimSituationOfferSettlementId(&sim, relief);
    sim.carriage.location_id = sim.player.location_id;
    char error[192];
    CcCommand pledge = {
        .kind = CC_COMMAND_CHARACTER_RESPONSE,
        .target_id = relief->id,
        .amount = CC_CHARACTER_RESPONSE_PLEDGE_HELP
    };
    CC_CHECK(CcSimApply(&sim, &pledge, error, sizeof(error)));
    CcId situation_id = relief->id;
    CcId sponsor_id = relief->sponsor_character_id;
    CcId affected_id = relief->affected_character_id;
    int32_t days = relief->deadline_day - sim.current_day + 1;
    CcSimAdvanceDays(&sim, days);
    relief = (CcSituation *)CcSimSituation(&sim, situation_id);
    const CcCharacter *sponsor = CcSimCharacter(&sim, sponsor_id);
    const CcCharacter *affected = CcSimCharacter(&sim, affected_id);
    CC_CHECK(relief != NULL);
    CC_CHECK(relief->status == CC_SITUATION_FAILED);
    CC_CHECK(sponsor != NULL && affected != NULL);
    CC_CHECK(CcCharacterRemembers(
        sponsor, CC_CHARACTER_MEMORY_PLAYER_WITHDREW, situation_id));
    CC_CHECK(CcCharacterRemembers(
        affected, CC_CHARACTER_MEMORY_PLAYER_WITHDREW, situation_id));
    const CcStoryLine *sponsor_line = CcStoryCharacterLine(
        &sim, relief, sponsor);
    const CcStoryLine *affected_line = CcStoryCharacterLine(
        &sim, relief, affected);
    CC_CHECK(sponsor_line != NULL);
    CC_CHECK(affected_line != NULL);
    CC_CHECK(sponsor_line->beat == CC_STORY_BEAT_WITHDREW);
    CC_CHECK(affected_line->beat == CC_STORY_BEAT_WITHDREW);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

int main(void)
{
    ValidateCatalogue();
    ValidateSituationCoverage();
    ValidateConversationBeats();
    ValidateFrontUrgencyBeats();
    ValidateExpiredPromiseMemory();
    puts("Authored story beat tests passed");
    return 0;
}
