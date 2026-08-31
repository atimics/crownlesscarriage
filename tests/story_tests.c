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
    static const char *forbidden_phrases[] = {
        "Something is",
        "road will remember",
        "road may decide",
        "mine lies",
        "mine sings",
        "Flour has never answered",
        "Both things are true",
        "That is the trouble",
        "seal",
        "guild",
        "levy",
        "props",
        "stonebacks",
        "charter",
        "spoiled",
        "black grain",
        "newest sack"
    };
    size_t count = CcStoryAuthoredLineCount();
    CC_CHECK(count >= 30U);
    for (size_t i = 0U; i < count; ++i) {
        const CcStoryLine *line = CcStoryAuthoredLineAt(i);
        CC_CHECK(line != NULL);
        CC_CHECK(line->id != NULL && line->id[0] != '\0');
        CC_CHECK(line->text != NULL && line->text[0] != '\0');
        CC_CHECK(strlen(line->text) <= 96U);
        CC_CHECK(strcmp(CcStoryBeatName(line->beat), "unknown") != 0);
        for (size_t phrase = 0U;
             phrase < sizeof(forbidden_phrases) /
                          sizeof(forbidden_phrases[0]); ++phrase) {
            CC_CHECK(strstr(line->text, forbidden_phrases[phrase]) == NULL);
        }
        for (size_t other = i + 1U; other < count; ++other) {
            const CcStoryLine *candidate = CcStoryAuthoredLineAt(other);
            CC_CHECK(candidate != NULL);
            CC_CHECK(strcmp(line->id, candidate->id) != 0);
        }
    }
    CC_CHECK(CcStoryAuthoredLineAt(count) == NULL);
}

static void ValidatePlayerChoices(void)
{
    for (int32_t kind = CC_SITUATION_RELIEF_DELIVERY;
         kind <= CC_SITUATION_COURIER_DELIVERY; ++kind) {
        const char *ask = CcStoryPlayerChoiceText(
            (CcSituationKind)kind, CC_STORY_PLAYER_ASK);
        const char *promise = CcStoryPlayerChoiceText(
            (CcSituationKind)kind, CC_STORY_PLAYER_PROMISE);
        const char *leave = CcStoryPlayerChoiceText(
            (CcSituationKind)kind, CC_STORY_PLAYER_LEAVE);
        CC_CHECK(ask != NULL && strncmp(ask, "1  ", 3U) == 0);
        CC_CHECK(promise != NULL && strncmp(promise, "2  ", 3U) == 0);
        CC_CHECK(leave != NULL && strcmp(leave, "Esc  Not now.") == 0);
        CC_CHECK(strlen(ask) <= 36U);
        CC_CHECK(strlen(promise) <= 36U);
    }
    const char *report = CcStoryPlayerChoiceText(
        CC_SITUATION_MONSTER_EXPEDITION, CC_STORY_PLAYER_REPORT);
    const char *confide = CcStoryPlayerChoiceText(
        CC_SITUATION_MONSTER_EXPEDITION,
        CC_STORY_PLAYER_KEEP_CONFIDENCE);
    CC_CHECK(report != NULL && strncmp(report, "1  ", 3U) == 0);
    CC_CHECK(confide != NULL && strncmp(confide, "2  ", 3U) == 0);
}

static void ValidateRoadCompanyVoices(void)
{
    static const char *names[] = {
        "The Unpaid Company",
        "The Tallow Knives",
        "The Broken Pennants",
        "The Ditch Parliament"
    };
    const char *lines[sizeof(names) / sizeof(names[0])] = {0};
    for (size_t i = 0U; i < sizeof(names) / sizeof(names[0]); ++i) {
        CcBanditGroup company = {0};
        (void)snprintf(company.name, sizeof(company.name), "%s", names[i]);
        lines[i] = CcStoryRoadCompanyLine(&company);
        CC_CHECK(lines[i] != NULL && lines[i][0] != '\0');
        CC_CHECK(strlen(lines[i]) <= 96U);
        for (size_t other = 0U; other < i; ++other) {
            CC_CHECK(strcmp(lines[i], lines[other]) != 0);
        }
    }
    CC_CHECK(strcmp(CcStoryRoadCompanyLine(NULL),
                    "Pay for the road or turn back.") == 0);
}

static void ValidateSituationCoverage(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    for (int32_t i = 0; i < sim.situation_count; ++i) {
        CcSituation *situation = &sim.situations[i];
        CcId conversation_place = CcSimSituationOfferSettlementId(
            &sim, situation);
        const CcCharacter *speaker = CcSimSituationConversationCharacter(
            &sim, situation, conversation_place);
        const CcCharacter *affected = CcSimSituationAffectedCharacter(
            &sim, situation);
        CC_CHECK(speaker != NULL);
        CC_CHECK(affected != NULL);
        CC_CHECK(CcStoryCharacterLine(&sim, situation, speaker) != NULL);
        char spoken[192];
        CC_CHECK(CcStoryCharacterText(
            &sim, situation, speaker, spoken, sizeof(spoken)));
        if (situation->kind != CC_SITUATION_MONSTER_EXPEDITION) {
            CC_CHECK(CcStoryCharacterLine(&sim, situation, affected) != NULL);
            CC_CHECK(CcStoryCharacterText(
                &sim, situation, affected, spoken, sizeof(spoken)));
        }
    }
}

static void ValidateMineSocialThread(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcSituation *mine = FindSituation(
        &sim, CC_SITUATION_MONSTER_EXPEDITION);
    CC_CHECK(mine != NULL);
    CC_CHECK(mine->discovery_stage == CC_DISCOVERY_RUMOR);
    CC_CHECK(!CcSimSituationCanAccept(&sim, mine));
    const CcCharacter *jory = CcSimSituationAffectedCharacter(&sim, mine);
    const CcCharacter *mara = CcSimSituationSponsorCharacter(&sim, mine);
    const CcCharacter *bren = CcSimSituationWitnessCharacter(&sim, mine);
    CC_CHECK(jory != NULL && mara != NULL && bren != NULL);
    const CcRelationship *jory_to_mara = CcSimRelationship(
        &sim, jory->id, mara->id);
    CC_CHECK(jory_to_mara != NULL);
    CC_CHECK(jory_to_mara->history >= CC_RELATIONSHIP_HISTORY_OLD_FRIENDS);
    CC_CHECK(CcCharacterKnows(
        jory, CC_KNOWLEDGE_PROBLEM_RUMOR, mine->id));
    CC_CHECK(CcCharacterKnows(
        bren, CC_KNOWLEDGE_WITNESS_ACCOUNT, mine->id));

    sim.player.location_id = jory->current_settlement_id;
    sim.carriage.location_id = sim.player.location_id;
    const CcStoryLine *line = CcStoryCharacterLine(&sim, mine, jory);
    CC_CHECK(line != NULL && line->beat == CC_STORY_BEAT_LEAD);
    CC_CHECK(strstr(line->text, "Bren") != NULL);
    CC_CHECK(strstr(line->text, "Picks") == NULL);
    char error[192];
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = mine->id
    };
    CC_CHECK(!CcSimApply(&sim, &accept, error, sizeof(error)));
    CC_CHECK(strstr(error, "job to accept") != NULL);

    CcCommand listen = {
        .kind = CC_COMMAND_CHARACTER_RESPONSE,
        .target_id = mine->id,
        .amount = CC_CHARACTER_RESPONSE_LISTEN
    };
    CC_CHECK(CcSimApply(&sim, &listen, error, sizeof(error)));
    CC_CHECK(mine->discovery_stage == CC_DISCOVERY_WITNESS);
    CC_CHECK(CcSimSituationConversationCharacter(
                 &sim, mine, sim.player.location_id) == bren);
    line = CcStoryCharacterLine(&sim, mine, bren);
    CC_CHECK(line != NULL && line->beat == CC_STORY_BEAT_WITNESS);
    CC_CHECK(strstr(line->text, "using a pick behind the old wall") != NULL);

    CC_CHECK(CcSimApply(&sim, &listen, error, sizeof(error)));
    CC_CHECK(mine->discovery_stage == CC_DISCOVERY_DECISION);
    line = CcStoryCharacterLine(&sim, mine, jory);
    CC_CHECK(line != NULL && line->beat == CC_STORY_BEAT_DECISION);
    CcSim report_path = sim;
    CcSim confidence_path = sim;

    CcSituation *reported = (CcSituation *)CcSimSituation(
        &report_path, mine->id);
    const CcCharacter *reported_jory = CcSimSituationAffectedCharacter(
        &report_path, reported);
    const CcCharacter *reported_mara = CcSimSituationSponsorCharacter(
        &report_path, reported);
    const CcRelationship *before_report = CcSimRelationship(
        &report_path, reported_jory->id, reported_mara->id);
    int32_t trust_before = before_report->trust;
    CcCommand report = {
        .kind = CC_COMMAND_CHARACTER_RESPONSE,
        .target_id = reported->id,
        .amount = CC_CHARACTER_RESPONSE_REPORT_EVIDENCE
    };
    CC_CHECK(CcSimApply(&report_path, &report, error, sizeof(error)));
    CC_CHECK(reported->discovery_stage == CC_DISCOVERY_AUTHORITY);
    CC_CHECK(!CcSimSituationCanAccept(&report_path, reported));
    report_path.player.location_id = reported_mara->current_settlement_id;
    report_path.carriage.location_id = report_path.player.location_id;
    line = CcStoryCharacterLine(&report_path, reported, reported_mara);
    CC_CHECK(line != NULL && line->beat == CC_STORY_BEAT_AUTHORITY);
    CC_CHECK(CcSimApply(&report_path, &listen, error, sizeof(error)));
    CC_CHECK(reported->discovery_stage == CC_DISCOVERY_OFFER);
    CC_CHECK(CcSimSituationCanAccept(&report_path, reported));
    CC_CHECK(CcSimRelationship(
                 &report_path, reported_jory->id, reported_mara->id)->trust !=
             trust_before);
    CcCommand pledge = listen;
    pledge.amount = CC_CHARACTER_RESPONSE_PLEDGE_HELP;
    CC_CHECK(CcSimApply(&report_path, &pledge, error, sizeof(error)));
    CC_CHECK(report_path.player.accepted_situation_id == reported->id);
    CC_CHECK(CcSimValidate(&report_path, error, sizeof(error)));

    CcSituation *private_lead = (CcSituation *)CcSimSituation(
        &confidence_path, mine->id);
    const CcCharacter *private_jory = CcSimSituationAffectedCharacter(
        &confidence_path, private_lead);
    CcCommand confide = report;
    confide.target_id = private_lead->id;
    confide.amount = CC_CHARACTER_RESPONSE_KEEP_CONFIDENCE;
    CC_CHECK(CcSimApply(&confidence_path, &confide,
                        error, sizeof(error)));
    CC_CHECK(private_lead->discovery_stage == CC_DISCOVERY_OFFER);
    CC_CHECK(private_lead->lead_path == CC_LEAD_PATH_CONFIDENCE);
    CC_CHECK(CcCharacterKnows(
        private_jory, CC_KNOWLEDGE_OFFER, private_lead->id));
    CC_CHECK(CcSimSituationCanAccept(&confidence_path, private_lead));
    CC_CHECK(CcSimCharacter(
                 &report_path, reported_jory->id)->player_disposition >
             private_jory->player_disposition);
    pledge.target_id = private_lead->id;
    CC_CHECK(CcSimApply(&confidence_path, &pledge,
                        error, sizeof(error)));
    CC_CHECK(CcSimValidate(&confidence_path, error, sizeof(error)));
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
    const CcSettlement *target = CcSimSettlement(&sim, relief->target_id);
    const CcEvent *cause = CcSimEvent(&sim, relief->cause_event_id);
    CC_CHECK(target != NULL);
    CC_CHECK(cause != NULL && cause->kind == CC_EVENT_SHORTAGE);
    CC_CHECK(CcCharacterKnows(
        sponsor, CC_KNOWLEDGE_OFFER, relief->id));
    CC_CHECK(CcCharacterKnows(
        affected, CC_KNOWLEDGE_IMMEDIATE_STAKE, relief->id));
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
    char spoken[192];
    CC_CHECK(CcStoryCharacterText(
        &sim, relief, sponsor, spoken, sizeof(spoken)));
    CC_CHECK(strstr(spoken, target->name) != NULL);
    CC_CHECK(strstr(spoken, "running out of food") != NULL);
    CC_CHECK(strstr(spoken, "spoiled") == NULL);

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
    CC_CHECK(CcStoryCharacterText(
        &sim, relief, sponsor, spoken, sizeof(spoken)));
    char quantity[32];
    (void)snprintf(quantity, sizeof(quantity), "%d sacks", relief->quantity);
    CC_CHECK(strstr(spoken, quantity) != NULL);

    CcCommand pledge = listen;
    pledge.amount = CC_CHARACTER_RESPONSE_PLEDGE_HELP;
    CC_CHECK(CcSimApply(&sim, &pledge, error, sizeof(error)));
    sponsor = CcSimSituationSponsorCharacter(&sim, relief);
    CC_CHECK(sponsor != NULL);
    const CcStoryLine *promised_line = CcStoryCharacterLine(
        &sim, relief, sponsor);
    CC_CHECK(promised_line != NULL);
    CC_CHECK(promised_line->beat == CC_STORY_BEAT_PROMISED);
    CC_CHECK(CcStoryCharacterText(
        &sim, relief, sponsor, spoken, sizeof(spoken)));
    CC_CHECK(strstr(spoken, affected->name) != NULL);
    CC_CHECK(strstr(spoken, target->name) != NULL);

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
    ValidatePlayerChoices();
    ValidateRoadCompanyVoices();
    ValidateSituationCoverage();
    ValidateConversationBeats();
    ValidateMineSocialThread();
    ValidateExpiredPromiseMemory();
    puts("Authored story beat tests passed");
    return 0;
}
