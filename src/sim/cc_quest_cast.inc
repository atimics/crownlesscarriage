/* Quest responsibilities refer to people; occupations and allegiance belong
 * to their continuing lives. Schema 74 keeps those records separate. */
static CcCharacter *PresentQuestActor(CcSim *sim, CcId town,
    CcCharacterRole preferred, CcId faction, CcId first, CcId second)
{
    CcCharacter *best = NULL;
    int best_score = INT32_MIN;
    for (int i = 0; i < sim->character_count; ++i) {
        CcCharacter *person = &sim->characters[i];
        if (person->id == first || person->id == second ||
            person->current_settlement_id != town ||
            person->death_day <= sim->current_day || CcCharacterAgeYears(sim, person) < 16 ||
            person->bandit_group_id != 0U || person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING ||
            (faction != 0U && person->faction_id != 0U && person->faction_id != faction)) continue;
        int score = (person->role == preferred ? 100 : 0) +
            (person->faction_id == faction && faction != 0U ? 20 : 0) +
            CcCharacterAgeYears(sim, person) / 5;
        if (best == NULL || score > best_score || (score == best_score && person->id < best->id)) {
            best = person;
            best_score = score;
        }
    }
    return best;
}

static CcCharacter *RetainedQuestActor(CcSim *sim, CcId id)
{
    CcCharacter *person = CharacterMutable(sim, id);
    return person != NULL && person->death_day > sim->current_day ? person : NULL;
}

/* This changes only the proposed cast, and commits it only when complete. */
static bool SelectPresentQuestCast(CcSim *sim, CcSituation *situation)
{
    int slot = SituationSettlementSlot(sim, situation->kind, situation->target_id);
    if (slot < 0 || slot >= sim->settlement_count) return false;
    CcId home = sim->settlements[slot].id;
    CcId sponsor_home = situation->kind == CC_SITUATION_MONSTER_EXPEDITION ?
        sim->settlements[0].id : CcSimSituationOfferSettlementId(sim, situation);
    if (sponsor_home == 0U) sponsor_home = home;
    CcCharacter *sponsor = RetainedQuestActor(sim, situation->sponsor_character_id);
    CcCharacter *affected = RetainedQuestActor(sim, situation->affected_character_id);
    CcCharacter *witness = RetainedQuestActor(sim, situation->witness_character_id);
    if (sponsor == NULL) sponsor = PresentQuestActor(sim, sponsor_home, CC_CHARACTER_OFFICIAL,
        situation->issuer_faction_id, affected != NULL ? affected->id : 0U, witness != NULL ? witness->id : 0U);
    CcCharacterRole role = situation->kind == CC_SITUATION_MONSTER_EXPEDITION ? CC_CHARACTER_SCOUT :
        situation->kind == CC_SITUATION_RELIEF_DELIVERY ? CC_CHARACTER_REFUGEE :
        situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY ? CC_CHARACTER_TRAVELLER :
        situation->kind == CC_SITUATION_COURIER_DELIVERY ? CC_CHARACTER_COURIER : CC_CHARACTER_LABORER;
    if (affected == NULL) affected = PresentQuestActor(sim, home, role, 0U,
        sponsor != NULL ? sponsor->id : 0U, witness != NULL ? witness->id : 0U);
    if (sponsor == NULL || affected == NULL) return false;
    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION && witness == NULL)
        witness = PresentQuestActor(sim, home, CC_CHARACTER_LABORER, 0U, sponsor->id, affected->id);
    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION && witness == NULL) return false;
    situation->sponsor_character_id = sponsor->id;
    situation->affected_character_id = affected->id;
    situation->witness_character_id = witness != NULL ? witness->id : 0U;
    CopyName(situation->sponsor_name, sponsor->name);
    CopyName(situation->affected_name, affected->name);
    return true;
}

static bool AssignSituationCast(CcSim *sim, CcSituation *situation, bool establish_knowledge)
{
    if (sim->schema_version < 74U || sim->current_day == 1) {
        AssignSituationCastLegacy(sim, situation, establish_knowledge);
        return true;
    }
    if (!SelectPresentQuestCast(sim, situation)) return false;
    CcCharacter *sponsor = CharacterMutable(sim, situation->sponsor_character_id);
    CcCharacter *affected = CharacterMutable(sim, situation->affected_character_id);
    if (establish_knowledge && situation->kind == CC_SITUATION_RELIEF_DELIVERY && situation->cause_event_id != 0U) {
        RememberKnowledge(sim, sponsor, CC_KNOWLEDGE_OFFER, situation->id, sponsor->id,
            situation->cause_event_id, CC_KNOWLEDGE_WITNESSED, false, sim->current_day);
        RememberKnowledge(sim, affected, CC_KNOWLEDGE_IMMEDIATE_STAKE, situation->id, affected->id,
            situation->cause_event_id, CC_KNOWLEDGE_WITNESSED, false, sim->current_day);
    }
    if (establish_knowledge && situation->kind == CC_SITUATION_MONSTER_EXPEDITION)
        InitializeMineSocialThread(sim, situation, affected, sponsor, CharacterMutable(sim, situation->witness_character_id));
    RefreshSituationCharacterActivities(sim, situation);
    return true;
}
