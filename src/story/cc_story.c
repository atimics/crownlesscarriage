#include "story/cc_story.h"

#include <string.h>

typedef struct CcStoryLineTemplate {
    CcStoryLine line;
    int32_t situation_kind;
    const char *character_name;
    int32_t relationship_history;
} CcStoryLineTemplate;

enum {
    CC_STORY_ANY_SITUATION = -1,
    CC_STORY_ANY_RELATIONSHIP = -1
};

#define STORY_LINE(line_id, spoken_text, story_beat, speaker, kind, name) \
    {{line_id, spoken_text, story_beat, speaker}, kind, name, \
     CC_STORY_ANY_RELATIONSHIP}
#define STORY_RELATIONSHIP_LINE(line_id, spoken_text, story_beat, speaker, \
                                kind, name, history) \
    {{line_id, spoken_text, story_beat, speaker}, kind, name, history}

static const CcStoryLineTemplate STORY_LINES[] = {
    STORY_LINE(
        "empty_granary.mara.offer",
        "I can spare eight sacks of flour for Silverwick. Deliver them to the merchants' storehouse.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.heard",
        "Alderwatch closed the bridge. I sent three letters. They never answered.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.promised",
        "Good. Count all eight sacks before you leave. I will prepare the delivery papers.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.helped",
        "Silverwick sent back the receipt. All eight sacks reached the merchants. The bakery got none.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.tomas.offer",
        "Take eight sacks to the hungry miners by the old road. No soldiers. No inspections. Better pay.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, "Tomas Rill"),
    STORY_LINE(
        "empty_granary.tomas.heard",
        "Leave after dark. Cover the lamps when you pass the tax patrol.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, "Tomas Rill"),
    STORY_LINE(
        "empty_granary.tomas.promised",
        "No passengers. No stops. My sister will meet you under the fox lanterns.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, "Tomas Rill"),
    STORY_LINE(
        "empty_granary.tomas.helped",
        "The flour arrived. My sister says you drive like a thief. She meant it kindly.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, "Tomas Rill"),
    STORY_LINE(
        "treaty_bridge.ilyra.offer",
        "The bridge works. I was ordered to keep the gate closed.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.heard",
        "Alderwatch is hungry too. If I open the gate, I am responsible for every sack that crosses.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.promised",
        "Bring two crates of tools. My crew will pretend the bridge needs repairs.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.helped",
        "I cut the gate chain myself. My report says the bridge machinery failed.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "lower_silverworks.jory.lead",
        "Bren ran out of the west gallery and left his lamp behind. He will not tell me why.",
        CC_STORY_BEAT_LEAD, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "lower_silverworks.bren.witness",
        "I heard someone using a pick behind the old wall. Cera was still down there, so I ran.",
        CC_STORY_BEAT_WITNESS, CC_STORY_SPEAKER_WITNESS,
        CC_SITUATION_MONSTER_EXPEDITION, "Bren Alder"),
    STORY_RELATIONSHIP_LINE(
        "lower_silverworks.jory.decision.friend",
        "Tell Mara what Bren heard. She knows the mine, and she will listen.",
        CC_STORY_BEAT_DECISION, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen",
        CC_RELATIONSHIP_HISTORY_OLD_FRIENDS),
    STORY_RELATIONSHIP_LINE(
        "lower_silverworks.jory.decision.former",
        "Tell Mara what Bren heard. Do not tell her it came from me.",
        CC_STORY_BEAT_DECISION, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen",
        CC_RELATIONSHIP_HISTORY_FORMER_PARTNERS),
    STORY_RELATIONSHIP_LINE(
        "lower_silverworks.jory.decision.rival",
        "Do not tell Mara yet. She will close the mine before we find Cera.",
        CC_STORY_BEAT_DECISION, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen",
        CC_RELATIONSHIP_HISTORY_PROFESSIONAL_RIVALS),
    STORY_LINE(
        "lower_silverworks.mara.authority",
        "I believe Bren. Take me to the wall. We need to find Cera first.",
        CC_STORY_BEAT_AUTHORITY, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_MONSTER_EXPEDITION, "Mara Venn"),
    STORY_LINE(
        "lower_silverworks.jory.offer",
        "Help me find Cera. We can deal with the old wall after she is safe.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "lower_silverworks.mara.offer",
        "I will pay you to find Cera. Do not open the old wall until she is safe.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_MONSTER_EXPEDITION, "Mara Venn"),
    STORY_LINE(
        "lower_silverworks.jory.promised",
        "Stay close to me. Keep your lamp up and watch the roof.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "lower_silverworks.jory.helped",
        "Cera came up with the morning shift. She won't work the west gallery again.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.offer",
        "The mine owners stopped selling flour yesterday. My mother went to work without breakfast.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.heard",
        "The foreman promises a food wagon every morning. By supper, he calls it tomorrow's wagon.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.promised",
        "Take the flour to the town ovens. Feed people before the merchants count the sacks.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.helped",
        "Mam took bread to the morning shift. First time this week.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.nell.offer",
        "I'm Nell. My mother is on the morning shift. I came for her bread, but the line has stopped.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Nell Varo"),
    STORY_LINE(
        "empty_granary.nell.heard",
        "Jory works at the bakery. He says the newest grain was spoiled.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Nell Varo"),
    STORY_LINE(
        "empty_granary.nell.promised",
        "I'll wait outside the bakery. The first bell rings before my mother's shift.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Nell Varo"),
    STORY_LINE(
        "empty_granary.nell.helped",
        "Mama ate bread this morning. I made her eat her half first.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Nell Varo"),

    STORY_LINE(
        "situation.relief.offer.sponsor",
        "The town has less than a week's food left. I can pay for one full wagon.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, NULL),
    STORY_LINE(
        "situation.relief.offer.affected",
        "The market ran out of food yesterday. Bring it to us before the merchants buy it all.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, NULL),
    STORY_LINE(
        "situation.repair.offer.sponsor",
        "The road crew can open the gate. They need tools for the bridge, or coin for the guards.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, NULL),
    STORY_LINE(
        "situation.repair.offer.affected",
        "We used to get two carts a week on that road. We have had none since it closed.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_ROUTE_REPAIR, NULL),
    STORY_LINE(
        "situation.monster.offer.sponsor",
        "The workers will not pass the bricked-up tunnel. Clear the lower road and they can work again.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_MONSTER_EXPEDITION, NULL),
    STORY_LINE(
        "situation.monster.offer.affected",
        "We heard movement behind the bricked-up tunnel. The foreman sent us home and locked the gate.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, NULL),
    STORY_LINE(
        "situation.night_road.offer.sponsor",
        "The tax patrol is turning away hungry families. I need you to take food past the guards.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, NULL),
    STORY_LINE(
        "situation.night_road.offer.affected",
        "We cannot use the town store. The tax collector waits there with two guards.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_BLACK_MARKET_DELIVERY, NULL),
    STORY_LINE(
        "situation.courier.offer.sponsor",
        "This letter is for the king's court. If anyone opens it, the court can refuse it.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_COURIER_DELIVERY, NULL),
    STORY_LINE(
        "situation.courier.offer.affected",
        "I should have reached the king's court yesterday. My horse cannot cross another mountain.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_COURIER_DELIVERY, NULL),

    STORY_LINE(
        "character.heard.sponsor",
        "That is the whole of it. Will you take the work?",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.heard.affected",
        "That is what happened. I do not know what else to tell you.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.promised.sponsor",
        "All right. I will put your company name on the job papers.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.promised.affected",
        "Thank you. I will tell the others you are coming.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.helped.sponsor",
        "I received the report this morning. You did what you promised.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.helped.affected",
        "You came back. Thank you.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.withdrew.sponsor",
        "I put your name on the job. Now I have to find another carrier.",
        CC_STORY_BEAT_WITHDREW, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.withdrew.affected",
        "We waited until dark. No one came.",
        CC_STORY_BEAT_WITHDREW, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.resolved.sponsor",
        "Another crew finished it this morning. The offer is closed.",
        CC_STORY_BEAT_RESOLVED, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.resolved.affected",
        "They settled it without us. I do not know yet if that is good news.",
        CC_STORY_BEAT_RESOLVED, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.failed.sponsor",
        "The deadline passed. I cannot hold the offer open.",
        CC_STORY_BEAT_FAILED, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.failed.affected",
        "It is too late for that plan now. We have to manage what is left.",
        CC_STORY_BEAT_FAILED, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL)
};

#undef STORY_LINE
#undef STORY_RELATIONSHIP_LINE

static CcStorySpeakerRole CharacterRole(const CcSituation *situation,
                                        const CcCharacter *character)
{
    if (situation == NULL || character == NULL) return CC_STORY_SPEAKER_ANY;
    if (situation->sponsor_character_id == character->id) {
        return CC_STORY_SPEAKER_SPONSOR;
    }
    if (situation->affected_character_id == character->id) {
        return CC_STORY_SPEAKER_AFFECTED;
    }
    if (situation->witness_character_id == character->id) {
        return CC_STORY_SPEAKER_WITNESS;
    }
    return CC_STORY_SPEAKER_ANY;
}

static CcStoryBeat CharacterBeat(const CcSituation *situation,
                                 const CcCharacter *character)
{
    if (situation == NULL) return CC_STORY_BEAT_FAILED;
    if (character != NULL && CcCharacterRemembers(
            character, CC_CHARACTER_MEMORY_PLAYER_HELPED, situation->id)) {
        return CC_STORY_BEAT_HELPED;
    }
    if (character != NULL && CcCharacterRemembers(
            character, CC_CHARACTER_MEMORY_PLAYER_WITHDREW, situation->id)) {
        return CC_STORY_BEAT_WITHDREW;
    }
    if (situation->status == CC_SITUATION_RESOLVED) {
        return CC_STORY_BEAT_RESOLVED;
    }
    if (situation->status == CC_SITUATION_FAILED) {
        return CC_STORY_BEAT_FAILED;
    }
    if (character != NULL && CcCharacterRemembers(
            character, CC_CHARACTER_MEMORY_PLAYER_PROMISED, situation->id)) {
        return CC_STORY_BEAT_PROMISED;
    }
    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
        switch (situation->discovery_stage) {
            case CC_DISCOVERY_RUMOR: return CC_STORY_BEAT_LEAD;
            case CC_DISCOVERY_WITNESS: return CC_STORY_BEAT_WITNESS;
            case CC_DISCOVERY_DECISION: return CC_STORY_BEAT_DECISION;
            case CC_DISCOVERY_AUTHORITY: return CC_STORY_BEAT_AUTHORITY;
            case CC_DISCOVERY_OFFER: return CC_STORY_BEAT_OFFER;
        }
    }
    if (character != NULL && CcCharacterRemembers(
            character, CC_CHARACTER_MEMORY_MET_PLAYER, situation->id)) {
        return CC_STORY_BEAT_HEARD;
    }
    return CC_STORY_BEAT_OFFER;
}

static bool TemplateMatches(const CcSim *sim,
                            const CcStoryLineTemplate *entry,
                            const CcSituation *situation,
                            const CcCharacter *character,
                            CcStoryBeat beat,
                            CcStorySpeakerRole role)
{
    if (entry == NULL || situation == NULL || character == NULL ||
        entry->line.beat != beat) return false;
    if (entry->line.speaker_role != CC_STORY_SPEAKER_ANY &&
        entry->line.speaker_role != role) return false;
    if (entry->situation_kind != CC_STORY_ANY_SITUATION &&
        entry->situation_kind != (int32_t)situation->kind) return false;
    if (entry->character_name != NULL &&
        strcmp(entry->character_name, character->name) != 0) return false;
    if (entry->relationship_history != CC_STORY_ANY_RELATIONSHIP) {
        const CcRelationship *relationship = CcSimRelationship(
            sim, situation->affected_character_id,
            situation->sponsor_character_id);
        if (relationship == NULL ||
            (int32_t)relationship->history !=
                entry->relationship_history) {
            return false;
        }
    }
    return true;
}

const CcStoryLine *CcStoryCharacterLine(
    const CcSim *sim, const CcSituation *situation,
    const CcCharacter *character)
{
    if (situation == NULL || character == NULL) return NULL;
    CcStoryBeat beat = CharacterBeat(situation, character);
    CcStorySpeakerRole role = CharacterRole(situation, character);
    size_t count = sizeof(STORY_LINES) / sizeof(STORY_LINES[0]);
    for (size_t i = 0U; i < count; ++i) {
        if (TemplateMatches(sim, &STORY_LINES[i], situation, character,
                            beat, role)) return &STORY_LINES[i].line;
    }
    return NULL;
}

size_t CcStoryAuthoredLineCount(void)
{
    return sizeof(STORY_LINES) / sizeof(STORY_LINES[0]);
}

const CcStoryLine *CcStoryAuthoredLineAt(size_t index)
{
    return index < CcStoryAuthoredLineCount() ? &STORY_LINES[index].line : NULL;
}

const char *CcStoryBeatName(CcStoryBeat beat)
{
    switch (beat) {
        case CC_STORY_BEAT_OFFER: return "offer";
        case CC_STORY_BEAT_LEAD: return "lead";
        case CC_STORY_BEAT_WITNESS: return "witness";
        case CC_STORY_BEAT_DECISION: return "decision";
        case CC_STORY_BEAT_AUTHORITY: return "authority";
        case CC_STORY_BEAT_HEARD: return "heard";
        case CC_STORY_BEAT_PROMISED: return "promised";
        case CC_STORY_BEAT_HELPED: return "helped";
        case CC_STORY_BEAT_WITHDREW: return "withdrew";
        case CC_STORY_BEAT_RESOLVED: return "resolved";
        case CC_STORY_BEAT_FAILED: return "failed";
    }
    return "unknown";
}

const char *CcStoryPlayerChoiceText(CcSituationKind kind,
                                    CcStoryPlayerChoice choice)
{
    if (choice == CC_STORY_PLAYER_LEAVE) return "Esc  Not now.";
    if (choice == CC_STORY_PLAYER_REPORT) return "1  Tell Mara.";
    if (choice == CC_STORY_PLAYER_KEEP_CONFIDENCE) {
        return "2  Keep it between us.";
    }
    if (choice == CC_STORY_PLAYER_ASK) {
        switch (kind) {
            case CC_SITUATION_RELIEF_DELIVERY:
                return "1  Where does the food go?";
            case CC_SITUATION_ROUTE_REPAIR:
                return "1  Why is the bridge closed?";
            case CC_SITUATION_MONSTER_EXPEDITION:
                return "1  What did the miners hear?";
            case CC_SITUATION_BLACK_MARKET_DELIVERY:
                return "1  Who gets the food?";
            case CC_SITUATION_COURIER_DELIVERY:
                return "1  Who needs the letter?";
        }
    }
    if (choice == CC_STORY_PLAYER_PROMISE) {
        switch (kind) {
            case CC_SITUATION_RELIEF_DELIVERY:
                return "2  I will bring the food.";
            case CC_SITUATION_ROUTE_REPAIR:
                return "2  I will open the road.";
            case CC_SITUATION_MONSTER_EXPEDITION:
                return "2  I will go below.";
            case CC_SITUATION_BLACK_MARKET_DELIVERY:
                return "2  I will make the delivery.";
            case CC_SITUATION_COURIER_DELIVERY:
                return "2  I will carry the letter.";
        }
    }
    return "Continue.";
}

const char *CcStoryRoadCompanyLine(const CcBanditGroup *company)
{
    if (company == NULL) return "Pay for the road or turn back.";
    if (strcmp(company->name, "The Unpaid Company") == 0) {
        return "The crown owes us six months' pay. Until it pays, travellers pay us.";
    }
    if (strcmp(company->name, "The Tallow Knives") == 0) {
        return "Food or coin. We have no use for speeches.";
    }
    if (strcmp(company->name, "The Broken Pennants") == 0) {
        return "We guarded this road until the crown dismissed us. We still collect.";
    }
    if (strcmp(company->name, "The Ditch Parliament") == 0) {
        return "We counted your guards. We counted your wheels. Now we're counting your coin.";
    }
    return "Pay for the road or turn back.";
}
