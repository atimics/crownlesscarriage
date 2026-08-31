#include "story/cc_story.h"

#include <string.h>

typedef struct CcStoryLineTemplate {
    CcStoryLine line;
    int32_t situation_kind;
    const char *character_name;
} CcStoryLineTemplate;

enum {
    CC_STORY_ANY_SITUATION = -1
};

#define STORY_LINE(line_id, spoken_text, story_beat, speaker, kind, name) \
    {{line_id, spoken_text, story_beat, speaker}, kind, name}

static const CcStoryLineTemplate STORY_LINES[] = {
    STORY_LINE(
        "empty_granary.mara.offer",
        "Eight sacks for Silverwick. The guild store gets every one. Eighteen crowns on delivery.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.heard",
        "Alderwatch closed the bridge. I wrote three letters. The chain is still there.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.promised",
        "All right. Buy the food here and check the count before you leave.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.helped",
        "Silverwick returned my receipt. Eight seals. None from the public ovens.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.tomas.offer",
        "Ask for the foxfire supper. No soldiers, no inspection. Eight sacks. Better pay.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, "Tomas Rill"),
    STORY_LINE(
        "empty_granary.tomas.heard",
        "Take the old mine road after dark. Hood the lamps when you see the levy post.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, "Tomas Rill"),
    STORY_LINE(
        "empty_granary.tomas.promised",
        "No passengers. No stops. At the fox lanterns, ask for the foxfire supper.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, "Tomas Rill"),
    STORY_LINE(
        "empty_granary.tomas.helped",
        "The flour arrived. My sister says you drive like a thief. From her, that is praise.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, "Tomas Rill"),
    STORY_LINE(
        "treaty_bridge.ilyra.offer",
        "The winch works. My orders say the chain stays up.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.heard",
        "Alderwatch is hungry too. Open that gate and I answer for every sack that crosses.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.promised",
        "Bring two tool crates. My bridge keeper can make a very loud repair.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.helped",
        "The bridge defeated me in honourable combat. That is what my report says.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "lower_silverworks.jory.offer",
        "We heard three knocks below the old seal. Then all twelve pit lamps went out.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "lower_silverworks.jory.heard",
        "The night crew found fresh cuts in the props. No tool marks. We locked the lower gate.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "lower_silverworks.jory.promised",
        "I'll mark the safe turns. If you hear singing, don't answer with your own name.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "lower_silverworks.jory.helped",
        "Morning shift brought up a full cart. The stonebacks returned three lost wedding rings.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.offer",
        "The company store stopped selling flour yesterday. My mother is on the morning shift.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.heard",
        "The foreman promises a convoy every morning. By supper, he calls it tomorrow's convoy.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.promised",
        "Bring it to the public ovens. The guild can count sacks after people eat.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.helped",
        "Mam took bread to the morning shift. First time this week.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.nell.offer",
        "I have three grains. Not even enough for a mouse's sandwich. I counted.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Nell Varo"),
    STORY_LINE(
        "empty_granary.nell.heard",
        "They fell from a king's wagon. It went east full and came back empty.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Nell Varo"),
    STORY_LINE(
        "empty_granary.nell.promised",
        "I'll wait by the ovens. I know which bell rings before the morning shift.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Nell Varo"),
    STORY_LINE(
        "empty_granary.nell.helped",
        "Mama had bread this morning. I made her eat her half first.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Nell Varo"),

    STORY_LINE(
        "situation.relief.offer.sponsor",
        "The town on the notice is below reserve. I can pay for one full relief load.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, NULL),
    STORY_LINE(
        "situation.relief.offer.affected",
        "The market ran out yesterday. If you bring food, find us before the guild buyers do.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, NULL),
    STORY_LINE(
        "situation.repair.offer.sponsor",
        "The gate crew can reopen the road. They need tools for the winch, or coin for the men.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, NULL),
    STORY_LINE(
        "situation.repair.offer.affected",
        "We used to get two carts a week on that road. We have had none since it closed.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_ROUTE_REPAIR, NULL),
    STORY_LINE(
        "situation.monster.offer.sponsor",
        "The crews will not pass the old seal. Clear the lower route and I can put them back to work.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_MONSTER_EXPEDITION, NULL),
    STORY_LINE(
        "situation.monster.offer.affected",
        "We heard movement behind the old seal. The shift boss sent us home and locked the gate.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, NULL),
    STORY_LINE(
        "situation.night_road.offer.sponsor",
        "The patrol is turning away everyone named in the levy rolls. I need the food taken past them.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, NULL),
    STORY_LINE(
        "situation.night_road.offer.affected",
        "We cannot use the public store. The levy clerk waits there with two guards.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_BLACK_MARKET_DELIVERY, NULL),
    STORY_LINE(
        "situation.courier.offer.sponsor",
        "These orders are sealed for the other court. If the wax is broken, they can refuse them.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_COURIER_DELIVERY, NULL),
    STORY_LINE(
        "situation.courier.offer.affected",
        "I was due at the next court yesterday. My horse will not make another crossing.",
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
        "All right. I will put your company name on the charter.",
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
        "I had your name on the charter. Now I have to find another carrier.",
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
    if (character != NULL && CcCharacterRemembers(
            character, CC_CHARACTER_MEMORY_MET_PLAYER, situation->id)) {
        return CC_STORY_BEAT_HEARD;
    }
    return CC_STORY_BEAT_OFFER;
}

static bool TemplateMatches(const CcStoryLineTemplate *entry,
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
    return entry->character_name == NULL ||
           strcmp(entry->character_name, character->name) == 0;
}

const CcStoryLine *CcStoryCharacterLine(
    const CcSim *sim, const CcSituation *situation,
    const CcCharacter *character)
{
    (void)sim;
    if (situation == NULL || character == NULL) return NULL;
    CcStoryBeat beat = CharacterBeat(situation, character);
    CcStorySpeakerRole role = CharacterRole(situation, character);
    size_t count = sizeof(STORY_LINES) / sizeof(STORY_LINES[0]);
    for (size_t i = 0U; i < count; ++i) {
        if (TemplateMatches(&STORY_LINES[i], situation, character,
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
                return "2  I will carry the dispatch.";
        }
    }
    return "Continue.";
}

const char *CcStoryRoadCompanyLine(const CcBanditGroup *company)
{
    if (company == NULL) return "Pay for the road or turn back.";
    if (strcmp(company->name, "The Unpaid Company") == 0) {
        return "The crown owes us six months' pay. Today, you settle one day of it.";
    }
    if (strcmp(company->name, "The Tallow Knives") == 0) {
        return "Food or coin. Keep the speech.";
    }
    if (strcmp(company->name, "The Broken Pennants") == 0) {
        return "We guarded this road before they struck our colours. We still collect.";
    }
    if (strcmp(company->name, "The Ditch Parliament") == 0) {
        return "We counted your guards. We counted your wheels. Now we're counting your coin.";
    }
    return "Pay for the road or turn back.";
}
