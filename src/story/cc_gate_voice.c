#include "story/cc_gate_voice.h"

#include "story/cc_core_participant.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool SameKey(const CcGateVoiceKey *key, const CcReturnChange *change)
{
    return key->kind == change->kind && key->detail == change->detail &&
        key->subject_id == change->subject_id;
}

static bool AlreadySpoken(const CcGateVoice *voice, const CcReturnChange *change)
{
    for (int32_t i = 0; i < voice->spoken_count; ++i)
        if (SameKey(&voice->spoken[i], change)) return true;
    return false;
}

/* The first change, in digest order, the company does not know and has not
   been told during this visit. */
static const CcReturnChange *NextUnknown(const CcReturnDigest *digest,
                                         const CcGateVoice *voice)
{
    for (int32_t i = 0; i < digest->change_count; ++i) {
        const CcReturnChange *change = &digest->changes[i];
        if (change->knowledge != CC_RETURN_UNKNOWN) continue;
        if (voice != NULL && AlreadySpoken(voice, change)) continue;
        return change;
    }
    return NULL;
}

static int32_t StorySlot(const CcSim *sim, CcId event_id)
{
    if (event_id == 0U) return -1;
    for (int32_t slot = 0; slot < CC_MAX_GOSSIP; ++slot)
        if (sim->gossip[slot].event_id == event_id) return slot;
    return -1;
}

static bool Holds(const CcSim *sim, CcId person_id, int32_t slot)
{
    if (slot < 0) return false;
    const CcGossipCarrier *carrier = CcSimGossipCarrier(sim, person_id);
    return carrier != NULL &&
        (carrier->stories & (UINT32_C(1) << (uint32_t)slot)) != 0U;
}

/* Alive, in town, grown, and not on the road. The dead leave the character
   table, so a missing record is a dead or departed face. */
static const CcCharacter *AtGate(const CcSim *sim, CcId id, CcId town)
{
    const CcCharacter *person = CcSimCharacter(sim, id);
    if (person == NULL || person->current_settlement_id != town ||
        person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING ||
        CcCharacterAgeYears(sim, person) < 16) return NULL;
    return person;
}

/* Gatekeepers and traders are the first people a carriage meets. */
static int32_t GateRank(const CcCharacter *person)
{
    if (person->role == CC_CHARACTER_OFFICIAL) return 3;
    if (person->occupation == CC_OCCUPATION_INNKEEPER ||
        person->role == CC_CHARACTER_COURIER) return 2;
    if (person->occupation != CC_OCCUPATION_NONE) return 1;
    return 0;
}

CcId CcGateVoicePickSpeaker(const CcSim *sim, CcId settlement_id,
                            CcId evidence_event_id)
{
    if (sim == NULL || settlement_id == 0U) return 0U;
    int32_t slot = StorySlot(sim, evidence_event_id);
    const CcTownSeen *seen = CcReturnLastSeen(sim, settlement_id);
    if (seen != NULL) {
        /* A face the company remembers, in face order; one who holds the
           story is preferred over one who does not. */
        CcId first = 0U;
        for (int32_t i = 0; i < CC_RETURN_FACES; ++i) {
            CcId id = seen->face_ids[i];
            if (id == 0U || AtGate(sim, id, settlement_id) == NULL) continue;
            if (Holds(sim, id, slot)) return id;
            if (first == 0U) first = id;
        }
        if (first != 0U) return first;
    }
    const CcCharacter *best = NULL;
    int32_t best_score = -1;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *person = &sim->characters[i];
        if (AtGate(sim, person->id, settlement_id) == NULL) continue;
        int32_t score = (Holds(sim, person->id, slot) ? 8 : 0) +
            (person->home_settlement_id == settlement_id ? 4 : 0) + GateRank(person);
        if (score > best_score || (score == best_score && person->id < best->id)) {
            best = person;
            best_score = score;
        }
    }
    return best != NULL ? best->id : 0U;
}

/* The role a change is about: who did it, or where it happened. */
static CcCoreRole AskedRole(CcReturnChangeKind kind)
{
    switch (kind) {
    case CC_RETURN_CHANGE_HUNGER:
    case CC_RETURN_CHANGE_FED:
    case CC_RETURN_CHANGE_REBUILT:
    case CC_RETURN_CHANGE_ABANDONED:
        return CC_CORE_PLACE;
    default:
        return CC_CORE_ACTOR;
    }
}

static void Copy(char *out, size_t capacity, const char *text)
{
    (void)snprintf(out, capacity, "%s", text != NULL ? text : "");
}

static bool AddClause(CcGateVoice *voice, CcGateVoicePart part,
                      CcGateVoiceEvidence evidence, CcId evidence_id,
                      bool town_state, const char *text)
{
    if (voice->clause_count >= CC_GATE_VOICE_CLAUSES || text[0] == '\0') return false;
    CcGateVoiceClause *clause = &voice->clauses[voice->clause_count++];
    Copy(clause->text, sizeof(clause->text), text);
    clause->part = part;
    clause->evidence = evidence;
    clause->evidence_id = evidence_id;
    clause->town_state = town_state;
    return true;
}

/* The typed facts of the held account and the teacher's choice: the fact in
   the asked role, most certain first, or a defer. */
static void SelectFact(CcGateVoice *voice, const CcCoreAccount *account,
                       CcGateVoiceCertainty certainty)
{
    voice->fact_count = 0;
    voice->chosen_fact = -1;
    for (size_t i = 0U; i < account->field_count &&
         voice->fact_count < CC_GATE_VOICE_FACTS; ++i) {
        const CcCoreField *field = &account->fields[i];
        if (!field->spoken || field->role == CC_CORE_QUANTITY) continue;
        CcGateVoiceFact *fact = &voice->facts[voice->fact_count];
        fact->role = field->role;
        fact->certainty = certainty;
        fact->field = (int32_t)i;
        size_t length = field->length < sizeof(fact->value) - 1U ?
            field->length : sizeof(fact->value) - 1U;
        memcpy(fact->value, account->text + field->start, length);
        fact->value[length] = '\0';
        if (fact->role == voice->asked_role &&
            (voice->chosen_fact < 0 ||
             fact->certainty < voice->facts[voice->chosen_fact].certainty))
            voice->chosen_fact = voice->fact_count;
        ++voice->fact_count;
    }
}

static const CcGateVoiceFact *FactInRole(const CcGateVoice *voice, CcCoreRole role)
{
    for (int32_t i = 0; i < voice->fact_count; ++i)
        if (voice->facts[i].role == role) return &voice->facts[i];
    return NULL;
}

static void FirstName(const char *name, char *out, size_t capacity)
{
    size_t length = strcspn(name, " ");
    if (length >= capacity) length = capacity - 1U;
    memcpy(out, name, length);
    out[length] = '\0';
}

/* "Mara at the inn", "Oren the smith", or "Tamsin Reed of Thornford". */
static void SourcePhrase(const CcSim *sim, CcId source_id, CcId town,
                         char *out, size_t capacity, char *name, size_t name_capacity)
{
    out[0] = '\0';
    name[0] = '\0';
    const CcCharacter *source = CcSimCharacter(sim, source_id);
    if (source == NULL) {
        const CcHistoricCharacter *past = CcSimHistoricCharacter(sim, source_id);
        if (past != NULL && past->name[0] != '\0') {
            Copy(name, name_capacity, past->name);
            Copy(out, capacity, past->name);
        }
        return;
    }
    Copy(name, name_capacity, source->name);
    char first[CC_NAME_CAPACITY];
    FirstName(source->name, first, sizeof(first));
    switch (source->occupation) {
    case CC_OCCUPATION_INNKEEPER: (void)snprintf(out, capacity, "%s at the inn", first); return;
    case CC_OCCUPATION_BAKER: (void)snprintf(out, capacity, "%s at the bakery", first); return;
    case CC_OCCUPATION_MILLER: (void)snprintf(out, capacity, "%s at the mill", first); return;
    case CC_OCCUPATION_SMITH: (void)snprintf(out, capacity, "%s at the forge", first); return;
    case CC_OCCUPATION_NONE: break;
    default:
        (void)snprintf(out, capacity, "%s the %s", first,
                       CcCoreOccupationName(source->occupation));
        return;
    }
    const CcSettlement *home = CcSimSettlement(sim, source->home_settlement_id);
    if (home != NULL && home->id != town)
        (void)snprintf(out, capacity, "%s of %s", source->name, home->name);
    else
        Copy(out, capacity, source->name);
}

static void TrimPeriod(char *text)
{
    size_t length = strlen(text);
    while (length > 0U && (text[length - 1U] == '.' || text[length - 1U] == ' '))
        text[--length] = '\0';
}

static bool WordEdge(char c)
{
    return !isalnum((unsigned char)c) && c != '\'';
}

/* A resident does not name their own town: "in Gloamgate" becomes "here" and
   "Gloamgate" becomes "the town". Only whole names are replaced. */
static void Localize(char *text, size_t capacity, const char *town)
{
    size_t town_length = town != NULL ? strlen(town) : 0U;
    if (town_length == 0U) return;
    char out[CC_SPEECH_TEXT_CAPACITY];
    size_t used = 0U;
    for (const char *at = text; *at != '\0' && used + 16U < sizeof(out);) {
        bool start = at == text || WordEdge(at[-1]);
        const char *swap = NULL;
        size_t skip = 0U;
        if (strncmp(at, " in ", 4U) == 0 && strncmp(at + 4, town, town_length) == 0 &&
            WordEdge(at[4 + town_length]) && at[4 + town_length] != '\'') {
            swap = " here";
            skip = 4U + town_length;
        } else if (start && strncmp(at, town, town_length) == 0 &&
                   (WordEdge(at[town_length]) || at[town_length] == '\'')) {
            swap = at == text ? "The town" : "the town";
            skip = town_length;
        }
        if (swap != NULL) {
            size_t length = strlen(swap);
            if (used + length >= sizeof(out)) break;
            memcpy(out + used, swap, length);
            used += length;
            at += skip;
        } else {
            out[used++] = *at++;
        }
    }
    out[used] = '\0';
    (void)snprintf(text, capacity, "%s", out);
}

static const char *FireExtent(int32_t damage)
{
    return damage >= 60 ? "most of the town" : damage >= 40 ? "half the town" :
        damage >= 20 ? "part of the town" : "a corner of the town";
}

/* On the road the town is named: "most of Gloamgate". */
static void FireExtentOf(int32_t damage, const char *town, char *out, size_t capacity)
{
    (void)snprintf(out, capacity, "%s %s",
                   damage >= 60 ? "most of" : damage >= 40 ? "half of" :
                   damage >= 20 ? "part of" : "a corner of", town);
}

static const char *GoodWord(int32_t good)
{
    return good >= 0 && good < CC_GOOD_COUNT ? CcGoodName((CcGood)good) : "goods";
}

static void Lower(char *text)
{
    for (; *text != '\0'; ++text) *text = (char)tolower((unsigned char)*text);
}

/* What anyone at the gate can see for themselves. Every value here comes
   from the change, which compares the town now with the company's record,
   or from the town's stalls now. */
static bool SaySeen(const CcSim *sim, CcGateVoice *voice)
{
    const CcReturnChange *change = &voice->change;
    const CcSettlement *place = CcSimSettlement(sim, voice->settlement_id);
    char text[CC_GATE_VOICE_CLAUSE_CAPACITY];
    char good[CC_NAME_CAPACITY];
    Copy(good, sizeof(good), GoodWord(change->detail));
    Lower(good);
    const char *subject = change->subject_name[0] != '\0' ? change->subject_name : NULL;
    const char *previous = change->previous_name[0] != '\0' ? change->previous_name : NULL;
    int32_t a = change->after;
    switch (change->kind) {
    case CC_RETURN_CHANGE_ABANDONED:
        (void)snprintf(text, sizeof(text), "Hardly anyone lives here now.");
        break;
    case CC_RETURN_CHANGE_FIRE:
        (void)snprintf(text, sizeof(text), "Fire took %s.", FireExtent(a));
        break;
    case CC_RETURN_CHANGE_REBUILT:
        (void)snprintf(text, sizeof(text), "The builders have mended %s of the fire damage.",
                       a == 0 ? "all" : "much");
        break;
    case CC_RETURN_CHANGE_NEW_RULER:
        if (subject == NULL) return false;
        if (previous != NULL)
            (void)snprintf(text, sizeof(text), "%s rules here now, not %s.", subject, previous);
        else
            (void)snprintf(text, sizeof(text), "%s rules here now.", subject);
        break;
    case CC_RETURN_CHANGE_NEW_KINGDOM:
        if (subject == NULL) return false;
        (void)snprintf(text, sizeof(text), "We answer to %s now.", subject);
        break;
    case CC_RETURN_CHANGE_FACE_DIED:
        if (subject == NULL) return false;
        (void)snprintf(text, sizeof(text), "%s is dead.", subject);
        break;
    case CC_RETURN_CHANGE_FACE_GONE:
        if (subject == NULL) return false;
        (void)snprintf(text, sizeof(text), "%s does not live here any more.", subject);
        break;
    case CC_RETURN_CHANGE_HUNGER: {
        /* Concrete first: the empty bread stall, then the hunger. */
        bool no_bread = place != NULL && place->stock[CC_GOOD_BREAD] == 0;
        (void)snprintf(text, sizeof(text), "%s%s", no_bread ? "Bread's gone. " : "",
                       a >= 50 ? "People are going hungry here." : "Food is short here.");
        break;
    }
    case CC_RETURN_CHANGE_FED:
        (void)snprintf(text, sizeof(text), "There is more to eat here now.");
        break;
    case CC_RETURN_CHANGE_LAWLESS:
        (void)snprintf(text, sizeof(text), "The streets are not safe now.");
        break;
    case CC_RETURN_CHANGE_SAFER:
        (void)snprintf(text, sizeof(text), "The streets are safer now.");
        break;
    case CC_RETURN_CHANGE_THRIVING:
        (void)snprintf(text, sizeof(text), "Trade is good here now.");
        break;
    case CC_RETURN_CHANGE_POORER:
        (void)snprintf(text, sizeof(text), "Times are harder here now.");
        break;
    case CC_RETURN_CHANGE_POPULATION:
        (void)snprintf(text, sizeof(text), "%s",
                       change->after < change->before ?
                           "Fewer people live here than before." :
                           "More people live here than before.");
        break;
    case CC_RETURN_CHANGE_BANDIT_CAMP:
        (void)snprintf(text, sizeof(text), "%s",
                       change->detail == 1 ? "Bandits mean to raid us." :
                                             "Bandits have made camp near the town.");
        break;
    case CC_RETURN_CHANGE_BANDITS_GONE:
        (void)snprintf(text, sizeof(text), "The bandits have left their camp.");
        break;
    case CC_RETURN_CHANGE_DRAGON_OMEN:
        (void)snprintf(text, sizeof(text), "%s is angry with us.",
                       sim->dragon.name[0] != '\0' ? sim->dragon.name : "The dragon");
        break;
    case CC_RETURN_CHANGE_SERVICE_LOST: {
        char service[CC_NAME_CAPACITY];
        Copy(service, sizeof(service), CcServiceName((CcServiceKind)change->detail));
        Lower(service);
        (void)snprintf(text, sizeof(text), "The %s is closed.", service);
        break;
    }
    case CC_RETURN_CHANGE_SERVICE_OPENED: {
        char service[CC_NAME_CAPACITY];
        Copy(service, sizeof(service), CcServiceName((CcServiceKind)change->detail));
        Lower(service);
        (void)snprintf(text, sizeof(text), "A %s has opened.", service);
        break;
    }
    case CC_RETURN_CHANGE_STALL_EMPTY: {
        /* The lead good, and one more from the emptied stalls if any. */
        char also[CC_NAME_CAPACITY] = "";
        for (int32_t g = 0; g < CC_GOOD_COUNT && also[0] == '\0'; ++g) {
            if (g == change->detail ||
                (change->goods_mask & (UINT32_C(1) << (uint32_t)g)) == 0U) continue;
            Copy(also, sizeof(also), GoodWord(g));
            Lower(also);
        }
        (void)snprintf(text, sizeof(text), "There's no %s%s%s in the market.",
                       good, also[0] != '\0' ? " or " : "", also);
        break;
    }
    case CC_RETURN_CHANGE_STALL_RESTOCKED:
        good[0] = (char)toupper((unsigned char)good[0]);
        (void)snprintf(text, sizeof(text), "%s is for sale again.", good);
        break;
    case CC_RETURN_CHANGE_PRICE:
        good[0] = (char)toupper((unsigned char)good[0]);
        (void)snprintf(text, sizeof(text), "%s costs %s than it did.", good,
                       change->after > change->before ? "more" : "less");
        break;
    default:
        return false;
    }
    if (voice->telling == CC_GATE_VOICE_SEEN) voice->confidence = 100;
    return AddClause(voice, CC_GATE_PART_EVENT, CC_GATE_EVIDENCE_TOWN,
                     voice->settlement_id, true, text);
}

/* How the speaker came by the story. */
typedef struct HeardFrom {
    CcId story_id;
    CcId source_id;
    int32_t confidence;
    bool witnessed;
    bool named;
    /* "Tamsin Reed of Thornford" at most: two names and a joining word. */
    char who[2 * CC_NAME_CAPACITY + 8];
} HeardFrom;

/* Add the last story clause with its hedge and, when there is one, who said
   so: "..., I hear.", "... — I heard it from Thora at the inn.", or
   "... I saw it myself." A clause that already hedges ("Folk say ...") does
   not hedge twice. */
static void AddAttributed(CcGateVoice *voice, const HeardFrom *from,
                          CcGateVoicePart part, bool town_state,
                          const char *text, bool hedged)
{
    char clause[CC_GATE_VOICE_CLAUSE_CAPACITY];
    int32_t c = from->confidence;
    if (from->witnessed) {
        (void)snprintf(clause, sizeof(clause), "%s.", text);
        (void)AddClause(voice, part, CC_GATE_EVIDENCE_STORY, from->story_id, town_state, clause);
        (void)AddClause(voice, CC_GATE_PART_SOURCE, CC_GATE_EVIDENCE_SOURCE,
                        from->source_id, false, "I saw it myself.");
        return;
    }
    if (from->named) {
        (void)AddClause(voice, part, CC_GATE_EVIDENCE_STORY, from->story_id, town_state, text);
        if (c >= 70)
            (void)snprintf(clause, sizeof(clause), "— %s told me.", from->who);
        else if (c >= 40)
            (void)snprintf(clause, sizeof(clause), "— I heard it from %s.", from->who);
        else
            (void)snprintf(clause, sizeof(clause), "— %s said so, but I'm not sure of it.",
                           from->who);
        (void)AddClause(voice, CC_GATE_PART_SOURCE, CC_GATE_EVIDENCE_SOURCE,
                        from->source_id, false, clause);
        return;
    }
    (void)snprintf(clause, sizeof(clause), "%s%s", text,
                   hedged ? (c >= 40 ? "." : ", if the story's true.") :
                   c >= 70 ? ", I hear." : c >= 40 ? ", so people say." :
                   ", if the story's true.");
    (void)AddClause(voice, part, CC_GATE_EVIDENCE_STORY, from->story_id, town_state, clause);
}

/* The why of a held account, when its grammar rule carries one. The account
   matched the rule in full, so the rule's fixed words are part of what the
   speaker holds. */
static const char *CauseOf(const char *rule)
{
    if (strcmp(rule, "dragon_retaliation_0") == 0) return "over missing hoard money";
    return NULL;
}

/* Whether a held story is the change itself, told as an event; otherwise it
   is the reason behind a change anyone can see. */
static bool StoryIsEvent(CcReturnChangeKind change, CcEventKind story)
{
    switch (change) {
    case CC_RETURN_CHANGE_FIRE: return story == CC_EVENT_DRAGON_RETALIATION;
    case CC_RETURN_CHANGE_NEW_RULER: return story == CC_EVENT_ROYAL_SUCCESSION;
    case CC_RETURN_CHANGE_FACE_DIED: return story == CC_EVENT_CHARACTER_DIED;
    case CC_RETURN_CHANGE_DRAGON_OMEN: return story == CC_EVENT_DRAGON_OMEN;
    case CC_RETURN_CHANGE_REBUILT: return story == CC_EVENT_MASONRY_REPAIR;
    case CC_RETURN_CHANGE_SERVICE_OPENED: return story == CC_EVENT_SERVICE_OPENED;
    default: return false;
    }
}

/* Render the held account with the actor first when a wording allows it:
   say what happened before why. */
static bool RenderEventFirst(const CcCoreAccount *account, const CcGateVoiceFact *actor,
                             bool actor_named, char *text, size_t capacity)
{
    char variants[2][CC_SPEECH_TEXT_CAPACITY];
    bool ok[2];
    for (uint32_t v = 0U; v < 2U; ++v)
        ok[v] = CcCoreAccountRender(account, v, variants[v], sizeof(variants[v])) &&
            variants[v][0] != '\0';
    int32_t pick = ok[0] ? 0 : ok[1] ? 1 : -1;
    if (pick < 0) return false;
    if (ok[0] && ok[1]) {
        const char *lead = actor != NULL && actor_named ? actor->value : "Someone";
        size_t length = strlen(lead);
        bool first0 = strncmp(variants[0], lead, length) == 0;
        bool first1 = strncmp(variants[1], lead, length) == 0;
        if (!first0 && first1) pick = 1;
    }
    size_t length = strlen(variants[pick]);
    if (length >= capacity) length = capacity - 1U;
    memcpy(text, variants[pick], length);
    text[length] = '\0';
    return true;
}

/* The resident's own telling, spoken in gate order: what happened, why, and
   who said so. */
static bool SayHeard(const CcSim *sim, const CcCharacter *speaker,
                     CcGateVoice *voice)
{
    const CcGossip *story = CcSimGossipStory(sim, voice->story_slot);
    const CcGossipCarrier *carrier = CcSimGossipCarrier(sim, speaker->id);
    if (story == NULL || carrier == NULL) return false;
    const CcGossipVersion *version = &carrier->versions[voice->story_slot];
    voice->version = *version;
    voice->confidence = version->confidence;

    /* The telling without court stance or alarm: those are opinion. */
    CcGossipVersion plain = *version;
    plain.court_bias = 0;
    plain.alarm = 0;
    char telling[CC_EVENT_TEXT_CAPACITY];
    CcGossipText(sim, story, &plain, telling, sizeof(telling));
    CcCoreAccount account;
    if (!CcCoreAccountPrepare(story->kind, telling, version->confidence,
                              version->retellings, &account)) return false;

    HeardFrom from = {.story_id = story->event_id,
                      .source_id = version->source_character_id,
                      .confidence = version->confidence};
    from.witnessed = version->source_character_id == speaker->id;
    CcGateVoiceCertainty certainty = from.witnessed ? CC_GATE_CERTAIN_WITNESSED :
        version->confidence >= 40 ? CC_GATE_CERTAIN_TOLD : CC_GATE_CERTAIN_DOUBTFUL;
    SelectFact(voice, &account, certainty);
    /* A doubtful answer to the asked role is withheld: the renderer says
       "someone" or "somewhere" rather than repeat a shaky name. */
    bool withheld = voice->chosen_fact >= 0 &&
        voice->facts[voice->chosen_fact].certainty == CC_GATE_CERTAIN_DOUBTFUL;
    if (withheld)
        account.fields[voice->facts[voice->chosen_fact].field].knowledge = CC_CORE_COARSE;
    from.named = !from.witnessed && from.source_id != 0U && from.source_id != sim->player.id;
    if (from.named) SourcePhrase(sim, from.source_id, voice->settlement_id, from.who,
                                 sizeof(from.who), voice->source_name,
                                 sizeof(voice->source_name));
    if (from.who[0] == '\0') from.named = false;

    const CcSettlement *place = CcSimSettlement(sim, voice->settlement_id);
    const char *town = place != NULL ? place->name : "";
    const CcGateVoiceFact *actor = FactInRole(voice, CC_CORE_ACTOR);
    const CcGateVoiceFact *where = FactInRole(voice, CC_CORE_PLACE);
    bool here = where == NULL || strcmp(where->value, town) == 0;
    bool actor_named = actor != NULL && !withheld;
    const char *cause = CauseOf(CcCoreAccountRule(&account));
    char text[CC_GATE_VOICE_CLAUSE_CAPACITY];

    /* A story that only names this town adds nothing to what the speaker
       can see: say that, plainly. The story still counts as told. */
    bool only_here = voice->fact_count > 0;
    for (int32_t i = 0; i < voice->fact_count; ++i)
        if (voice->facts[i].role != CC_CORE_PLACE || strcmp(voice->facts[i].value, town) != 0)
            only_here = false;
    /* On the road the story naming the town is the news itself. */
    if (only_here && !voice->on_road) return SaySeen(sim, voice);

    char claim[CC_SPEECH_TEXT_CAPACITY];
    if (!RenderEventFirst(&account, actor, actor_named, claim, sizeof(claim))) return false;
    TrimPeriod(claim);
    if (!voice->on_road) Localize(claim, sizeof(claim), town);

    if (voice->change.kind == CC_RETURN_CHANGE_FIRE && here && cause != NULL &&
        StoryIsEvent(voice->change.kind, story->kind)) {
        /* The gate render mode for a burning: who burned how much of the
           town, then why, then who said so. */
        bool from_story = actor_named;
        char extent[2 * CC_NAME_CAPACITY];
        if (voice->on_road)
            FireExtentOf(voice->change.after, town, extent, sizeof(extent));
        else
            Copy(extent, sizeof(extent), FireExtent(voice->change.after));
        if (actor_named)
            (void)snprintf(text, sizeof(text), "%s burned %s.", actor->value, extent);
        else
            (void)snprintf(text, sizeof(text), "Fire took %s.", extent);
        (void)AddClause(voice, CC_GATE_PART_EVENT,
                        from_story ? CC_GATE_EVIDENCE_STORY : CC_GATE_EVIDENCE_TOWN,
                        from_story ? story->event_id : voice->settlement_id, true, text);
        bool hedged = !from.witnessed && version->confidence < 70;
        (void)snprintf(text, sizeof(text), "%s was %s",
                       !hedged ? "It" : version->confidence >= 40 ? "Folk say it" :
                       "Some say it", cause);
        AddAttributed(voice, &from, CC_GATE_PART_CAUSE, false, text, hedged);
        return true;
    }
    if (StoryIsEvent(voice->change.kind, story->kind) || voice->on_road) {
        /* A traveller has not seen the town now: they pass on the story. */
        AddAttributed(voice, &from, CC_GATE_PART_EVENT, false, claim, false);
        return true;
    }
    /* The story is the reason: the visible change first, then the story. */
    if (!SaySeen(sim, voice)) return false;
    AddAttributed(voice, &from, CC_GATE_PART_CAUSE, false, claim, false);
    return true;
}

static void JoinLine(CcGateVoice *voice)
{
    size_t used = 0U;
    voice->line[0] = '\0';
    for (int32_t i = 0; i < voice->clause_count; ++i) {
        const char *text = voice->clauses[i].text;
        /* Clauses that continue a sentence start with punctuation. */
        bool joined = text[0] == ',' || text[0] == '.';
        int written = snprintf(voice->line + used, sizeof(voice->line) - used,
                               "%s%s", used > 0U && !joined ? " " : "", text);
        if (written < 0 || (size_t)written >= sizeof(voice->line) - used) break;
        used += (size_t)written;
    }
}

/* A traveller's telling as the sim words it, hedged and attributed the same
   way, when the grammar has no rule for the story. */
static bool SayTelling(const CcSim *sim, const CcCharacter *speaker,
                       CcGateVoice *voice)
{
    const CcGossip *story = CcSimGossipStory(sim, voice->story_slot);
    const CcGossipCarrier *carrier = CcSimGossipCarrier(sim, speaker->id);
    if (story == NULL || carrier == NULL) return false;
    const CcGossipVersion *version = &carrier->versions[voice->story_slot];
    voice->version = *version;
    voice->confidence = version->confidence;
    CcGossipVersion plain = *version;
    plain.court_bias = 0;
    plain.alarm = 0;
    char telling[CC_EVENT_TEXT_CAPACITY];
    CcGossipText(sim, story, &plain, telling, sizeof(telling));
    /* The event, not the ledger: "raids Gloamgate: 16 Bread" says the raid. */
    char *tally = strchr(telling, ':');
    if (tally != NULL) *tally = '\0';
    TrimPeriod(telling);
    if (telling[0] == '\0') return false;
    HeardFrom from = {.story_id = story->event_id,
                      .source_id = version->source_character_id,
                      .confidence = version->confidence};
    from.witnessed = version->source_character_id == speaker->id;
    from.named = !from.witnessed && from.source_id != 0U && from.source_id != sim->player.id;
    if (from.named) SourcePhrase(sim, from.source_id, voice->settlement_id, from.who,
                                 sizeof(from.who), voice->source_name,
                                 sizeof(voice->source_name));
    if (from.who[0] == '\0') from.named = false;
    AddAttributed(voice, &from, CC_GATE_PART_EVENT, false, telling, false);
    return voice->clause_count > 0;
}

/* A traveller who has just left the town says so, in a few words about their
   own road. Traced to the traveller's journey: where they set out from. */
static void AddRoadClause(const CcSim *sim, const CcCharacter *speaker,
                          CcGateVoice *voice)
{
    (void)sim;
    if (speaker->current_settlement_id != voice->settlement_id ||
        speaker->activity != CC_CHARACTER_ACTIVITY_TRAVELLING) return;
    const char *text =
        voice->change.kind == CC_RETURN_CHANGE_FIRE ? "I left before the smoke cleared." :
        voice->change.kind == CC_RETURN_CHANGE_HUNGER ||
        voice->change.kind == CC_RETURN_CHANGE_STALL_EMPTY ?
            "I left while there was still bread for the road." :
        voice->change.kind == CC_RETURN_CHANGE_BANDIT_CAMP ||
        voice->change.kind == CC_RETURN_CHANGE_LAWLESS ?
            "I didn't stay to see more." :
            "I've just come from there.";
    (void)AddClause(voice, CC_GATE_PART_SOURCE, CC_GATE_EVIDENCE_ROAD,
                    speaker->current_settlement_id, false, text);
}

bool CcGateVoiceSayOnRoad(const CcSim *sim, CcId traveller_id, CcId town,
                          const CcReturnChange *change, CcGateVoice *voice)
{
    if (voice == NULL) return false;
    *voice = (CcGateVoice){0};
    voice->story_slot = -1;
    voice->chosen_fact = -1;
    voice->settlement_id = town;
    voice->on_road = true;
    if (sim == NULL || town == 0U) return false;
    return CcGateVoiceSay(sim, traveller_id, change, voice);
}

bool CcGateVoiceSay(const CcSim *sim, CcId speaker_id,
                    const CcReturnChange *change, CcGateVoice *voice)
{
    if (sim == NULL || change == NULL || voice == NULL) return false;
    const CcCharacter *speaker = CcSimCharacter(sim, speaker_id);
    if (speaker == NULL) return false;
    /* The town is the voice's own; a fresh voice takes the speaker's. */
    if (voice->settlement_id == 0U) voice->settlement_id = speaker->current_settlement_id;
    voice->speaker_id = speaker->id;
    Copy(voice->speaker, sizeof(voice->speaker), speaker->name);
    Copy(voice->speaker_label, sizeof(voice->speaker_label),
         speaker->role == CC_CHARACTER_OFFICIAL && speaker->occupation == CC_OCCUPATION_NONE ?
             "official" : CcCoreOccupationName(speaker->occupation));
    voice->remembered_face = false;
    const CcTownSeen *seen = CcReturnLastSeen(sim, voice->settlement_id);
    for (int32_t i = 0; seen != NULL && !voice->on_road && i < CC_RETURN_FACES; ++i)
        if (seen->face_ids[i] == speaker->id) voice->remembered_face = true;

    voice->change = *change;
    voice->clause_count = 0;
    voice->fact_count = 0;
    voice->chosen_fact = -1;
    voice->source_name[0] = '\0';
    voice->version = (CcGossipVersion){0};
    voice->asked_role = AskedRole(change->kind);
    voice->story_slot = StorySlot(sim, change->evidence_event_id);
    if (!Holds(sim, speaker->id, voice->story_slot)) voice->story_slot = -1;

    if (voice->remembered_face)
        (void)AddClause(voice, CC_GATE_PART_GREETING, CC_GATE_EVIDENCE_FACE,
                        speaker->id, false, "You're back.");
    int32_t opening = voice->clause_count;
    voice->telling = CC_GATE_VOICE_HEARD;
    if (voice->on_road) {
        /* A traveller only passes on a story they hold: in the grammar's
           words when it can read the telling, else in the telling's own. */
        if (voice->story_slot < 0) return false;
        if (!SayHeard(sim, speaker, voice)) {
            voice->clause_count = opening;
            voice->fact_count = 0;
            voice->chosen_fact = -1;
            if (!SayTelling(sim, speaker, voice)) return false;
        }
        AddRoadClause(sim, speaker, voice);
        JoinLine(voice);
        return voice->line[0] != '\0';
    }
    if (voice->story_slot < 0 || !SayHeard(sim, speaker, voice)) {
        /* Not their story, or not one the grammar can say: fall back to
           what is plain to see. */
        voice->clause_count = opening;
        voice->fact_count = 0;
        voice->chosen_fact = -1;
        voice->source_name[0] = '\0';
        voice->story_slot = -1;
        voice->version = (CcGossipVersion){0};
        voice->telling = CC_GATE_VOICE_SEEN;
        if (!SaySeen(sim, voice)) return false;
    }
    JoinLine(voice);
    return voice->line[0] != '\0';
}

static bool Speak(const CcSim *sim, CcGateVoice *voice)
{
    CcReturnDigest digest;
    if (!CcReturnDigestBuild(sim, voice->settlement_id, &digest) || digest.first_visit)
        return false;
    const CcReturnChange *change = NextUnknown(&digest, voice);
    if (change == NULL) return false;
    if (voice->speaker_id == 0U ||
        AtGate(sim, voice->speaker_id, voice->settlement_id) == NULL)
        voice->speaker_id = CcGateVoicePickSpeaker(sim, voice->settlement_id,
                                                   change->evidence_event_id);
    if (voice->speaker_id == 0U ||
        !CcGateVoiceSay(sim, voice->speaker_id, change, voice)) return false;
    if (voice->spoken_count < CC_RETURN_MAX_CHANGES) {
        CcGateVoiceKey *key = &voice->spoken[voice->spoken_count++];
        key->kind = change->kind;
        key->detail = change->detail;
        key->subject_id = change->subject_id;
    }
    voice->more = NextUnknown(&digest, voice) != NULL;
    return true;
}

bool CcGateVoiceBegin(const CcSim *sim, CcId settlement_id, CcGateVoice *voice)
{
    if (voice == NULL) return false;
    *voice = (CcGateVoice){0};
    voice->story_slot = -1;
    voice->chosen_fact = -1;
    voice->settlement_id = settlement_id;
    if (sim == NULL || settlement_id == 0U) return false;
    return Speak(sim, voice);
}

bool CcGateVoiceNext(const CcSim *sim, CcGateVoice *voice)
{
    if (sim == NULL || voice == NULL || voice->settlement_id == 0U) return false;
    return Speak(sim, voice);
}

bool CcGateVoiceHasMore(const CcSim *sim, const CcGateVoice *voice)
{
    if (sim == NULL || voice == NULL || voice->settlement_id == 0U) return false;
    CcReturnDigest digest;
    return CcReturnDigestBuild(sim, voice->settlement_id, &digest) &&
        !digest.first_visit && NextUnknown(&digest, voice) != NULL;
}

bool CcGateVoiceToldCommand(const CcGateVoice *voice, CcCommand *command)
{
    if (voice == NULL || command == NULL || voice->telling != CC_GATE_VOICE_HEARD ||
        voice->story_slot < 0 || voice->speaker_id == 0U) return false;
    *command = (CcCommand){.kind = CC_COMMAND_HEARD_STORY,
                           .target_id = voice->speaker_id,
                           .amount = voice->story_slot};
    return true;
}

bool CcGateVoiceSpeech(const CcSim *sim, const CcGateVoice *voice, CcSpeech *speech)
{
    if (sim == NULL || voice == NULL || speech == NULL || voice->line[0] == '\0')
        return false;
    const CcCharacter *speaker = CcSimCharacter(sim, voice->speaker_id);
    uint32_t voice_index = speaker != NULL ? CcSpeechCharacterVoice(sim, speaker) : 0U;
    CcSpeechDelivery delivery =
        voice->change.kind == CC_RETURN_CHANGE_FIRE ||
        voice->change.kind == CC_RETURN_CHANGE_HUNGER ||
        voice->change.kind == CC_RETURN_CHANGE_FACE_DIED ||
        voice->change.kind == CC_RETURN_CHANGE_ABANDONED ? CC_SPEECH_WORRIED :
        voice->confidence < 40 ? CC_SPEECH_QUIET : CC_SPEECH_PLAIN;
    CcId source = voice->telling == CC_GATE_VOICE_HEARD ?
        voice->change.evidence_event_id : 0U;
    return CcSpeechCompose(speech, voice->on_road ? "return.road" : "return.gate",
                           voice->speaker_id, voice->speaker,
                           voice_index, voice->line, delivery,
                           CC_SPEECH_CONVERSATION, source);
}

const char *CcGateVoiceEvidenceName(CcGateVoiceEvidence evidence)
{
    switch (evidence) {
    case CC_GATE_EVIDENCE_STORY: return "story";
    case CC_GATE_EVIDENCE_SOURCE: return "source";
    case CC_GATE_EVIDENCE_TOWN: return "town";
    case CC_GATE_EVIDENCE_FACE: return "face";
    case CC_GATE_EVIDENCE_ROAD: return "road";
    default: return "none";
    }
}

const char *CcGateVoicePartName(CcGateVoicePart part)
{
    switch (part) {
    case CC_GATE_PART_GREETING: return "greeting";
    case CC_GATE_PART_EVENT: return "event";
    case CC_GATE_PART_CAUSE: return "cause";
    case CC_GATE_PART_SOURCE: return "source";
    default: return "none";
    }
}
