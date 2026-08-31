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
        "Every sack goes to the guild store. I will not pretend the road is open.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.heard",
        "You heard me. Good. Now decide what your carriage is for.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.promised",
        "Then I will seal the load. Bring back every receipt, and everyone you can.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.helped",
        "The receipt is complete. The bread line is not. Still, thank you.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.pressing",
        "The flour ledger lost another page this week. The bread line did not.",
        CC_STORY_BEAT_PRESSING, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.breaking",
        "There is seed grain left. If they eat it, spring becomes the next famine.",
        CC_STORY_BEAT_BREAKING, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.tomas.offer",
        "Ask for the foxfire supper. No soldiers. No inspection. Better pay.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, "Tomas Rill"),
    STORY_LINE(
        "empty_granary.tomas.heard",
        "The road is secret because the need is not. Now you understand.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, "Tomas Rill"),
    STORY_LINE(
        "empty_granary.tomas.promised",
        "Good. Follow the fox lanterns, and do not stop for anyone wearing a seal.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, "Tomas Rill"),
    STORY_LINE(
        "empty_granary.tomas.helped",
        "They ate first. The collectors came later. Both things are true.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, "Tomas Rill"),
    STORY_LINE(
        "treaty_bridge.ilyra.offer",
        "The bridge is not broken. That is the trouble.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.heard",
        "Then you know the chain is an order, not a broken machine.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.promised",
        "Bring tools or coin. I will make certain the winch remembers how to move.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.helped",
        "The chain is down. My orders still have not forgiven me.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.pressing",
        "The chain still holds. The wagons behind it have started turning back.",
        CC_STORY_BEAT_PRESSING, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.breaking",
        "Open the bridge now, or the road will belong to whoever feeds it at night.",
        CC_STORY_BEAT_BREAKING, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "lower_silverworks.jory.offer",
        "If the mine sings your name, do not answer with it.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "lower_silverworks.jory.heard",
        "Good. Keep the whistle close, and listen for breathing under the wagons.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "lower_silverworks.jory.promised",
        "Then I will mark the safe turns. The mine lies about the others.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "lower_silverworks.jory.helped",
        "The mine is quiet tonight. I still carry the whistle.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "lower_silverworks.jory.pressing",
        "The breathing is closer to the lifts now. Even the foreman heard it.",
        CC_STORY_BEAT_PRESSING, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "lower_silverworks.jory.breaking",
        "Nobody whistles below now. Something learned to whistle back.",
        CC_STORY_BEAT_BREAKING, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.offer",
        "People keep asking which store owns the flour. Flour has never answered.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.heard",
        "If you are still listening, look at the children, not the clerk.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.promised",
        "Then bring the flour to the ovens. The papers can follow.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.helped",
        "The first oven is warm. The clock still says breakfast.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.nell.offer",
        "Three grains. One of them burnt. A full wagon went east before sunrise.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Nell Varo"),
    STORY_LINE(
        "empty_granary.nell.heard",
        "Thank you for stopping. Grown people walk faster when they are ashamed.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Nell Varo"),
    STORY_LINE(
        "empty_granary.nell.promised",
        "I tied red thread to the carriage, so the road cannot make you forget.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Nell Varo"),
    STORY_LINE(
        "empty_granary.nell.helped",
        "The ovens are warm. I kept one grain, so I would remember the cold.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Nell Varo"),

    STORY_LINE(
        "character.pressing.sponsor",
        "The need is growing faster than the answer. There is still time, but less of it.",
        CC_STORY_BEAT_PRESSING, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.pressing.affected",
        "It is worse than when we last spoke. People have started making quieter plans.",
        CC_STORY_BEAT_PRESSING, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.breaking.sponsor",
        "This is the last honest chance to choose the outcome. After this, the pressure chooses.",
        CC_STORY_BEAT_BREAKING, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.breaking.affected",
        "We are past waiting calmly. Whatever you do now will be what we remember.",
        CC_STORY_BEAT_BREAKING, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL),

    STORY_LINE(
        "situation.relief.offer.sponsor",
        "The stores are short. One honest load could keep the ovens warm.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, NULL),
    STORY_LINE(
        "situation.relief.offer.affected",
        "The ovens are cold. We need the road to bring food, not another notice.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, NULL),
    STORY_LINE(
        "situation.repair.offer.sponsor",
        "The road can open. It needs tools, coin, and someone willing to be blamed.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, NULL),
    STORY_LINE(
        "situation.repair.offer.affected",
        "Nothing comes through while that road stays shut. Work has stopped with it.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_ROUTE_REPAIR, NULL),
    STORY_LINE(
        "situation.monster.offer.sponsor",
        "The lower works are lost. Bring back a road people can use.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_MONSTER_EXPEDITION, NULL),
    STORY_LINE(
        "situation.monster.offer.affected",
        "Something is moving below the old workings. The mine is not empty.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_MONSTER_EXPEDITION, NULL),
    STORY_LINE(
        "situation.night_road.offer.sponsor",
        "The public stores will not help them. I know a road that will.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_BLACK_MARKET_DELIVERY, NULL),
    STORY_LINE(
        "situation.night_road.offer.affected",
        "We cannot stand in the public line. Bring the food by the road without names.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_BLACK_MARKET_DELIVERY, NULL),
    STORY_LINE(
        "situation.courier.offer.sponsor",
        "The seal must reach the other court intact. The road may decide what follows.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_COURIER_DELIVERY, NULL),
    STORY_LINE(
        "situation.courier.offer.affected",
        "News changes on the road. That is why the road matters.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_COURIER_DELIVERY, NULL),

    STORY_LINE(
        "character.heard.sponsor",
        "You have heard the need. I have nothing honest to add to it.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.heard.affected",
        "Thank you for hearing me. Most people only ask what the work pays.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.promised.sponsor",
        "Then the promise is yours. I will remember who carried it.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.promised.affected",
        "I told them help was coming. Please do not make me a liar.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.helped.sponsor",
        "You kept the promise. The account will carry your name.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.helped.affected",
        "You kept your word. I will not forget it.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.withdrew.sponsor",
        "You gave your word, then took it back. The need did not leave with you.",
        CC_STORY_BEAT_WITHDREW, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.withdrew.affected",
        "You said you would help. We waited for you.",
        CC_STORY_BEAT_WITHDREW, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.resolved.sponsor",
        "The matter is closed. The road will remember how it was done.",
        CC_STORY_BEAT_RESOLVED, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.resolved.affected",
        "It changed before we could ask you. We are learning what that change cost.",
        CC_STORY_BEAT_RESOLVED, CC_STORY_SPEAKER_AFFECTED,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.failed.sponsor",
        "The offer is gone. Someone else chose what would happen next.",
        CC_STORY_BEAT_FAILED, CC_STORY_SPEAKER_SPONSOR,
        CC_STORY_ANY_SITUATION, NULL),
    STORY_LINE(
        "character.failed.affected",
        "The chance passed. We are living with what came after it.",
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

static CcStoryBeat CharacterBeat(const CcSim *sim,
                                 const CcSituation *situation,
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
    const CcFront *front = CcSimSituationFront(sim, situation);
    CcFrontStage stage = CcSimFrontStage(front);
    if (stage == CC_FRONT_STAGE_BREAKING) {
        return CC_STORY_BEAT_BREAKING;
    }
    if (stage == CC_FRONT_STAGE_PRESSING) {
        return CC_STORY_BEAT_PRESSING;
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
    if (situation == NULL || character == NULL) return NULL;
    CcStoryBeat beat = CharacterBeat(sim, situation, character);
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
        case CC_STORY_BEAT_PRESSING: return "pressing";
        case CC_STORY_BEAT_BREAKING: return "breaking";
        case CC_STORY_BEAT_HEARD: return "heard";
        case CC_STORY_BEAT_PROMISED: return "promised";
        case CC_STORY_BEAT_HELPED: return "helped";
        case CC_STORY_BEAT_WITHDREW: return "withdrew";
        case CC_STORY_BEAT_RESOLVED: return "resolved";
        case CC_STORY_BEAT_FAILED: return "failed";
    }
    return "unknown";
}
