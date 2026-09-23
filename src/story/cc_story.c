#include "story/cc_story.h"

#include <stdio.h>
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
        "Another town is running out of food.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.heard",
        "They need food. I have the boxes ready.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.promised",
        "The food boxes are loaded. Take my carriage.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, "Mara Venn"),
    STORY_LINE(
        "empty_granary.mara.helped",
        "The food arrived. You kept your word.",
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
        "Take eight food boxes to the miners by the old road. No soldiers. No inspections. Better pay.",
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
        "Alderwatch is hungry. If I open the gate, I answer for every food box that crosses.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.promised",
        "Bring two crates each of tools, wood, and stone. My crew will call it bridge repair.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_ROUTE_REPAIR, "Ilyra Senn"),
    STORY_LINE(
        "treaty_bridge.ilyra.helped",
        "I cut the gate chain myself. My report says the bridge machinery failed.",
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
        "We are running out of food.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.heard",
        "We need the whole delivery.",
        CC_STORY_BEAT_HEARD, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.promised",
        "Bring the food here. People are waiting.",
        CC_STORY_BEAT_PROMISED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),
    STORY_LINE(
        "empty_granary.jory.helped",
        "The food arrived. Thank you.",
        CC_STORY_BEAT_HELPED, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, "Jory Fen"),

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
        "Another town needs food. I can pay for the delivery.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_SPONSOR,
        CC_SITUATION_RELIEF_DELIVERY, NULL),
    STORY_LINE(
        "situation.relief.offer.affected",
        "We are running out of food.",
        CC_STORY_BEAT_OFFER, CC_STORY_SPEAKER_AFFECTED,
        CC_SITUATION_RELIEF_DELIVERY, NULL),
    STORY_LINE(
        "situation.repair.offer.sponsor",
        "The road crew can open the gate. They need tools, wood, and stone, or coin for the guards.",
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

typedef struct CcUnderroadFragment {
    int32_t room_index;
    const char *room_name;
    CcDungeonRoomKind room_kind;
    char lines[CC_STORY_UNDERROAD_MAX_LINES][CC_STORY_UNDERROAD_LINE_CAPACITY];
} CcUnderroadFragment;

static const CcUnderroadFragment UNDERROAD_FRAGMENTS[] = {
    {8, "Bell Shaft", CC_DUNGEON_ROOM_SHAFT, {
        "Old journal, The Bell That Was Eaten: 'Eira went first, her arm barring the shaft.'",
        "'Arin unfolded his map. Where was the bell? I wrote down the silence instead.'",
        "'Eira: Stay behind me. Arin: A shaft for a bell, but no room for its swing?'",
        "Under the journal's folded leaf: 'The bell was swallowed, a porter told us. Not stolen.'",
        "'Arin: Then where did its sound go? Eira: Do not lean over to ask.'",
        "'I kept writing. Below my question, someone had drawn a second mouth.'"
    }},
    {10, "Hall of Ash Clerks", CC_DUNGEON_ROOM_ARCHIVE, {
        "An ash clerk's deposition: 'Red, Purple, Blue: rival goblin colors, not one congregation.'",
        "'Human and goblin cultists knelt together. The porters waited outside, still owed their bread.'",
        "'A Red collector said: The dragon favors us. A Blue porter asked: Does favor carry its own sacks?'",
        "On the deposition's reverse: 'A Purple clerk refused to count the kneeling as all goblins.'",
        "'We carry tribute, a porter said. That does not mean we worship what takes it.'",
        "The clerk records the cult's belief: 'The swallowed bell still calls. Silence is its congregation.'"
    }},
    {17, "Furnace Shrine", CC_DUNGEON_ROOM_SHRINE, {
        "A scorched ritual copy calls the dragon an unending invocation; this is the cult's belief.",
        "Its speaker's part reads: 'The cult does not summon me. I am the summoning that never ended'.",
        "Beside it, a witness wrote: 'Two violet lights opened where I had expected an altar.'",
        "Beneath the ritual copy, an untranslated liturgical refrain:",
        "'Zrrathek voruul. Kna'zoth deri'shka.'",
        "A witness's marginal note: 'The second mouth moved. The kneeling mouths tried to keep up.'"
    }},
    {18, "Porters' Grave", CC_DUNGEON_ROOM_VAULT, {
        "A porter's memorial: 'They entered our names as offerings. We had signed for wages.'",
        "'Red sacks, Blue straps, Purple tallies. Under the loads our shoulders were the same.'",
        "'Tell the clerks: a dead carrier is not proof that the road wanted carrying.'",
        "Behind the memorial, a copied exchange: 'Priest: The silence knows you. Porter: Then it knows my price.'",
        "'Eira: We can turn back. Arin: But the map ends here. Eira: We do not have to.'",
        "The journal's writer adds: 'I copied their refusal before I copied another prayer.'"
    }},
    {19, "Hoard Threshold", CC_DUNGEON_ROOM_THRESHOLD, {
        "A torn witness account: 'Two violet lights found us. Beneath them, the second mouth opened.'",
        "'Arin said, You are-- and stopped. Eira put herself between him and what he could not name.'",
        "'It felt like recognition, but hungry. I wrote that it knew us. I cannot prove that it did.'",
        "Inside the torn fold: 'A cultist called it welcome. The porter beside me called it appetite.'",
        "'Dragon favor is not friendship, she said. Do not mistake being spared for being loved.'",
        "'Eira took Arin's map hand. I shut the book before I could finish his sentence for him.'"
    }},
    {23, "King's Survey Room", CC_DUNGEON_ROOM_ARCHIVE, {
        "An old survey copy bears a note signed Arin: 'The Bell That Was Eaten is testimony, not a scale map.'",
        "'I drew a circle for the missing bell. Eira asked whether it marked a place or a fear.'",
        "The accompanying journal says: 'I am the one who wrote our descent. Do not mistake ink for witness.'",
        "Under the survey backing: 'Arin: Should I close the circle? Eira: Leave us a way out.'",
        "The writer's last note: 'The cult called silence an answer. The porters kept the right to disagree.'",
        "'I left You are-- unfinished. Let no later hand turn our uncertainty into a command to kneel.'"
    }}
};

CcStoryUnderroadExcerpt CcStoryUnderroadCurrentExcerpt(const CcSim *sim)
{
    CcStoryUnderroadExcerpt excerpt = {0};
    if (sim == NULL || !sim->dungeon_expedition.active ||
        sim->dungeon_expedition.encounter_kind != CC_DUNGEON_ENCOUNTER_NONE ||
        sim->dungeon_count <= 0 || sim->dungeon_count > CC_MAX_DUNGEONS) return excerpt;
    const CcDungeon *dungeon = CcSimDungeon(sim, sim->dungeon_expedition.dungeon_id);
    int32_t index = sim->dungeon_expedition.current_room;
    if (dungeon == NULL || dungeon->room_count <= 0 ||
        dungeon->room_count > CC_MAX_DUNGEON_ROOMS || index < 0 ||
        index >= dungeon->room_count) return excerpt;
    const CcDungeonRoom *room = &dungeon->rooms[index];
    if ((room->state_flags & CC_DUNGEON_ROOM_DISCOVERED) == 0U) return excerpt;
    for (size_t i = 0U; i < sizeof(UNDERROAD_FRAGMENTS) / sizeof(UNDERROAD_FRAGMENTS[0]); ++i) {
        const CcUnderroadFragment *fragment = &UNDERROAD_FRAGMENTS[i];
        if (index != fragment->room_index || room->kind != fragment->room_kind ||
            strncmp(room->name, fragment->room_name, sizeof(room->name)) != 0) continue;
        excerpt.title = "The Bell That Was Eaten";
        excerpt.searched = (room->state_flags & CC_DUNGEON_ROOM_SEARCHED) != 0U;
        excerpt.line_count = excerpt.searched ? CC_STORY_UNDERROAD_MAX_LINES :
                                               CC_STORY_UNDERROAD_STANZA_LINES;
        for (size_t line = 0U; line < excerpt.line_count; ++line) {
            excerpt.lines[line] = fragment->lines[line];
        }
        break;
    }
    return excerpt;
}

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
    /* A recipient can see the accepted delivery when the company arrives.
       This observation supplies the handoff reply before a second pledge. */
    if (sim != NULL && character != NULL &&
        situation->kind == CC_SITUATION_RELIEF_DELIVERY &&
        situation->affected_character_id == character->id &&
        sim->player.accepted_situation_id == situation->id &&
        character->current_settlement_id == situation->target_id &&
        character->activity != CC_CHARACTER_ACTIVITY_TRAVELLING &&
        sim->player.location_id == situation->target_id &&
        sim->carriage.location_id == situation->target_id &&
        situation->quantity > 0 &&
        sim->player.cargo[CC_GOOD_FOOD] >= situation->quantity &&
        CcCharacterKnows(character, CC_KNOWLEDGE_IMMEDIATE_STAKE, situation->id)) {
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
    CcStoryBeat beat = CharacterBeat(sim, situation, character);
    CcStorySpeakerRole role = CharacterRole(situation, character);
    size_t count = sizeof(STORY_LINES) / sizeof(STORY_LINES[0]);
    for (size_t i = 0U; i < count; ++i) {
        if (TemplateMatches(sim, &STORY_LINES[i], situation, character,
                            beat, role)) return &STORY_LINES[i].line;
    }
    return NULL;
}

bool CcStoryCharacterText(
    const CcSim *sim, const CcSituation *situation,
    const CcCharacter *character, char *text, size_t text_capacity)
{
    if (sim == NULL || situation == NULL || character == NULL ||
        text == NULL || text_capacity == 0U) return false;
    text[0] = '\0';
    CcStoryBeat beat = CharacterBeat(sim, situation, character);
    CcStorySpeakerRole role = CharacterRole(situation, character);
    if (situation->kind == CC_SITUATION_RELIEF_DELIVERY &&
        (role == CC_STORY_SPEAKER_SPONSOR ||
         role == CC_STORY_SPEAKER_AFFECTED)) {
        CcKnowledgeKind needed = role == CC_STORY_SPEAKER_SPONSOR ?
            CC_KNOWLEDGE_OFFER : CC_KNOWLEDGE_IMMEDIATE_STAKE;
        if (!CcCharacterKnows(character, needed, situation->id)) return false;
        const CcSettlement *target = CcSimSettlement(
            sim, situation->target_id);
        const CcCharacter *affected = CcSimSituationAffectedCharacter(
            sim, situation);
        const char *target_name = target != NULL ? target->name : "the town";
        const char *affected_name = affected != NULL ?
            affected->name : "someone there";
        if (role == CC_STORY_SPEAKER_SPONSOR) {
            switch (beat) {
                case CC_STORY_BEAT_OFFER:
                    (void)snprintf(text, text_capacity,
                                   "%s is running out of food.", target_name);
                    return true;
                case CC_STORY_BEAT_HEARD:
                    (void)snprintf(text, text_capacity,
                                   "%d boxes of food. I will load them.",
                                   situation->quantity);
                    return true;
                case CC_STORY_BEAT_PROMISED:
                    if (CcSimReliefLoadingComplete(situation)) {
                        (void)snprintf(
                            text, text_capacity,
                            "All %d food boxes are aboard. %s will meet you in %s.",
                            situation->quantity, affected_name, target_name);
                    } else if (situation->loading_crate_carried) {
                        (void)snprintf(text,text_capacity,
                            "Bring that crate to the carriage. I will steady it while you load.");
                    } else {
                        (void)snprintf(text,text_capacity,
                            "Carry %d food boxes from the granary stack. %s will meet you in %s.",
                            CcSimReliefCratesToLoad(situation),affected_name,
                            target_name);
                    }
                    return true;
                case CC_STORY_BEAT_HELPED:
                case CC_STORY_BEAT_RESOLVED:
                    (void)snprintf(text, text_capacity,
                                   "%s received all %d food boxes.", target_name,
                                   situation->quantity);
                    return true;
                case CC_STORY_BEAT_WITHDREW:
                case CC_STORY_BEAT_FAILED:
                    (void)snprintf(text, text_capacity,
                                   "The food did not reach %s.", target_name);
                    return true;
                default:
                    break;
            }
        } else {
            switch (beat) {
                case CC_STORY_BEAT_OFFER:
                    (void)snprintf(text, text_capacity,
                                   "We are running out of food.");
                    return true;
                case CC_STORY_BEAT_HEARD:
                    (void)snprintf(text, text_capacity,
                                   "We need all %d food boxes.",
                                   situation->quantity);
                    return true;
                case CC_STORY_BEAT_PROMISED:
                    if (sim->player.location_id == situation->target_id) {
                        (void)snprintf(
                            text, text_capacity,
                            "Take the food to the market hall. We can unload it there.");
                    } else {
                        (void)snprintf(text, text_capacity,
                                       "Bring the food to %s.", target_name);
                    }
                    return true;
                case CC_STORY_BEAT_HELPED:
                case CC_STORY_BEAT_RESOLVED:
                    (void)snprintf(text, text_capacity,
                                   "The food arrived. Thank you.");
                    return true;
                case CC_STORY_BEAT_WITHDREW:
                case CC_STORY_BEAT_FAILED:
                    (void)snprintf(text, text_capacity,
                                   "The food never came.");
                    return true;
                default:
                    break;
            }
        }
    }
    const CcStoryLine *line = CcStoryCharacterLine(
        sim, situation, character);
    if (line == NULL && situation->kind ==
        CC_SITUATION_MONSTER_EXPEDITION) {
        const CcCharacter *witness = CcSimCharacter(
            sim, situation->witness_character_id);
        const CcCharacter *participant = CcSimSituationAffectedCharacter(
            sim, situation);
        const char *witness_name = witness != NULL ?
            witness->name : "a mine witness";
        const char *participant_name = participant != NULL ?
            participant->name : "a miner";
        if (role == CC_STORY_SPEAKER_AFFECTED &&
            beat == CC_STORY_BEAT_LEAD) {
            (void)snprintf(text, text_capacity,
                           "%s fled the west gallery. Help me learn why.",
                           witness_name);
            return true;
        }
        if (role == CC_STORY_SPEAKER_WITNESS) {
            (void)snprintf(text, text_capacity,
                           "I heard stone moving behind the old wall. Tell %s.",
                           participant_name);
            return true;
        }
        if (role == CC_STORY_SPEAKER_AFFECTED) {
            (void)snprintf(text, text_capacity,
                           "The mine official must hear this. I trust your choice.");
            return true;
        }
        if (role == CC_STORY_SPEAKER_SPONSOR) {
            (void)snprintf(text, text_capacity,
                           "Take tools into the west gallery and bring our people home.");
            return true;
        }
    }
    if (line == NULL) return false;
    (void)snprintf(text, text_capacity, "%s", line->text);
    return true;
}

size_t CcStoryAuthoredLineCount(void)
{
    return sizeof(STORY_LINES) / sizeof(STORY_LINES[0]);
}

const CcStoryLine *CcStoryAuthoredLineAt(size_t index)
{
    return index < CcStoryAuthoredLineCount() ? &STORY_LINES[index].line : NULL;
}

const char *CcStoryAuthoredSpeakerAt(size_t index)
{
    return index < CcStoryAuthoredLineCount() ? STORY_LINES[index].character_name : NULL;
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
        case CC_STORY_BEAT_PRESSING: return "pressing";
        case CC_STORY_BEAT_BREAKING: return "breaking";
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
                return "1  What do they need?";
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
                return "2  I'll take the job.";
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
