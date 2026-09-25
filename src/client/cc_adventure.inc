#include "sim/cc_road_council.h"
#include "sim/cc_occupations.h"
/* Scene actions and modal UI share the same input boundary. */
static bool adventure_preferences_dirty;

static bool AdventureScene(const LocalState *local)
{
    return local->adventure_ui && !local->open_world &&
        local->site_kind == CC_LOCAL_SITE_NONE && !local->site_travel_active &&
        !local->journey_travel_active && !local->road_choice_active &&
        !local->journey_combat_active && !local->journey_parley_active;
}

static CcInteractionTarget *AdventureTarget(LocalState *local, const CcSim *sim,
    CcInteractionKind kind, uint64_t object, const char *name, const char *verb,
    Vector3 position, float width, float height, float depth,
    Vector2 approach, float radius)
{
    if (local->interactions.count >= CC_INTERACTION_CAPACITY) return NULL;
    CcInteractionTarget *target = &local->interactions.targets[local->interactions.count++];
    *target = (CcInteractionTarget){
        .key = {sim->player.location_id, object, kind},
        .x = position.x, .y = position.y, .z = position.z,
        .width = width, .height = height, .depth = depth,
        .approach_x = approach.x, .approach_z = approach.y, .radius = radius,
        .available = !LocalCombatActive(local)};
    (void)snprintf(target->name, sizeof(target->name), "%s", name);
    (void)snprintf(target->verb, sizeof(target->verb), "%s", verb);
    if (!target->available) (void)snprintf(target->reason, sizeof(target->reason),
        "Find safety before you %s.", verb);
    return target;
}

static const char *AdventureRoleName(CcNpcRole role)
{
    switch (role) {
        case CC_NPC_ROLE_GUARD: return "Town guard";
        case CC_NPC_ROLE_MERCHANT: return "Market trader";
        case CC_NPC_ROLE_LABORER: return "Town worker";
        case CC_NPC_ROLE_TRAVELLER: return "Traveller";
        case CC_NPC_ROLE_REFUGEE: return "Wayfarer";
        case CC_NPC_ROLE_SCOUT: return "Scout";
        case CC_NPC_ROLE_HEALER: return "Healer";
        default: return "Neighbour";
    }
}

static void AdventurePersonLabel(const CcSim *sim, const CcCharacter *person,
                                 char *label, size_t capacity)
{
    if (person == NULL || label == NULL || capacity == 0U) return;
    (void)snprintf(label, capacity, "%s, %s, %s", person->name,
        CcOccupationName(person->occupation),
        CcCharacterRoutineName(CcSimCharacterRoutine(sim, person)));
}

/* Direct introductions and visible reports belong to the saved company. */
static bool AdventureKnownPerson(const CcSim *sim, const CcCharacter *person)
{
    if (sim == NULL || person == NULL) return false;
    if (person->introduced_day > 0) return true;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (!SituationVisibleToPlayer(sim, i) &&
            situation->id != sim->player.accepted_situation_id) continue;
        if (person->id == situation->affected_character_id ||
            person->id == situation->sponsor_character_id ||
            person->id == situation->witness_character_id) return true;
    }
    return false;
}

static bool AdventurePersonEligible(const CcSim *sim,
                                    const CcCharacter *person)
{
    return sim != NULL && person != NULL &&
        person->current_settlement_id == sim->player.location_id &&
        person->activity != CC_CHARACTER_ACTIVITY_TRAVELLING &&
        CcCharacterAgeYears(sim, person) >= 16;
}

static bool AdventurePersonMatchesRole(CcNpcRole role,
                                       const CcCharacter *person)
{
    return (role == CC_NPC_ROLE_TRAVELLER &&
            person->role == CC_CHARACTER_TRAVELLER) ||
        (role == CC_NPC_ROLE_SCOUT && person->role == CC_CHARACTER_SCOUT) ||
        (role == CC_NPC_ROLE_REFUGEE && person->role == CC_CHARACTER_REFUGEE) ||
        (role == CC_NPC_ROLE_LABORER && person->role == CC_CHARACTER_LABORER) ||
        (role == CC_NPC_ROLE_GUARD && person->role == CC_CHARACTER_OFFICIAL);
}

static bool AdventureGenericPersonKey(CcInteractionKey key)
{
    return key.kind == CC_INTERACTION_PERSON &&
        key.object >= UINT64_C(0x100000000) &&
        key.object < UINT64_C(0x200000000);
}

static bool AdventureCharacterUsed(const LocalState *local, CcId id)
{
    for (int32_t i = 0; i < local->interactions.count; ++i)
        if (local->interactions.targets[i].character_id == id) return true;
    return false;
}

static bool AdventureCharacterReserved(const LocalState *local,
                                       const CcInteractionTarget *target,
                                       CcId id)
{
    for (int32_t i = 0; i < (int32_t)(sizeof(local->presented_person_binding_place) /
                                       sizeof(local->presented_person_binding_place[0]));
         ++i)
        if (local->presented_person_binding_place[i] == target->key.place &&
            local->presented_person_binding_object[i] != target->key.object &&
            local->presented_person_binding_character[i] == id) return true;
    return false;
}

static void AdventurePrunePresentedPersonBindings(const CcSim *sim,
                                                   LocalState *local)
{
    for (int32_t i = 0; i < (int32_t)(sizeof(local->presented_person_binding_place) /
                                       sizeof(local->presented_person_binding_place[0]));
         ++i) {
        if (local->presented_person_binding_character[i] == 0U) continue;
        const CcCharacter *person = CcSimCharacter(
            sim, local->presented_person_binding_character[i]);
        if (local->presented_person_binding_place[i] != sim->player.location_id ||
            !AdventurePersonEligible(sim, person)) {
            local->presented_person_binding_place[i] = 0U;
            local->presented_person_binding_object[i] = 0U;
            local->presented_person_binding_character[i] = 0U;
        }
    }
}

static const CcCharacter *AdventureBoundPresentedPerson(const CcSim *sim,
    const LocalState *local, const CcInteractionTarget *target)
{
    if (!AdventureGenericPersonKey(target->key)) return NULL;
    for (int32_t i = 0; i < (int32_t)(sizeof(local->presented_person_binding_place) /
                                       sizeof(local->presented_person_binding_place[0]));
         ++i) {
        if (local->presented_person_binding_place[i] != target->key.place ||
            local->presented_person_binding_object[i] != target->key.object) continue;
        const CcCharacter *person = CcSimCharacter(
            sim, local->presented_person_binding_character[i]);
        if (AdventurePersonEligible(sim, person) &&
            !AdventureCharacterUsed(local, person->id)) return person;
    }
    return NULL;
}

static void AdventureRememberPresentedPerson(LocalState *local,
                                              const CcInteractionTarget *target)
{
    if (!AdventureGenericPersonKey(target->key) || target->character_id == 0U) return;
    int32_t free_slot = -1;
    for (int32_t i = 0; i < (int32_t)(sizeof(local->presented_person_binding_place) /
                                       sizeof(local->presented_person_binding_place[0]));
         ++i) {
        if (local->presented_person_binding_place[i] == target->key.place &&
            local->presented_person_binding_object[i] == target->key.object) {
            local->presented_person_binding_character[i] = target->character_id;
            return;
        }
        if (free_slot < 0 && local->presented_person_binding_character[i] == 0U)
            free_slot = i;
    }
    if (free_slot < 0) free_slot = 0;
    local->presented_person_binding_place[free_slot] = target->key.place;
    local->presented_person_binding_object[free_slot] = target->key.object;
    local->presented_person_binding_character[free_slot] = target->character_id;
}

static bool AdventureBindPerson(const CcSim *sim, LocalState *local,
                                 CcInteractionTarget *target, CcNpcRole role)
{
    if (target == NULL) return false;
    const CcCharacter *bound = AdventureBoundPresentedPerson(sim, local, target);
    if (bound != NULL) {
        target->character_id = bound->id;
        (void)snprintf(target->name, sizeof(target->name), "%s", bound->name);
        return true;
    }
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *person = &sim->characters[i];
        if (!AdventurePersonEligible(sim, person) ||
            !AdventurePersonMatchesRole(role, person) ||
            AdventureCharacterUsed(local, person->id) ||
            AdventureCharacterReserved(local, target, person->id)) continue;
        target->character_id = person->id;
        (void)snprintf(target->name, sizeof(target->name), "%s", person->name);
        AdventureRememberPresentedPerson(local, target);
        return true;
    }
    return false;
}

/* Every talkable figure in town is a real resident; the scene's role is only
   a hint for which resident to prefer. Towns never offer anonymous role
   labels, so a figure with no resident to stand for gets no Talk target. */
static bool AdventureNamePresented(const CcSim *sim, LocalState *local,
    CcInteractionTarget *target, CcNpcRole role,
    const CcCharacter **residents, int32_t resident_count, int32_t slot)
{
    if (target == NULL) return false;
    if (AdventureBindPerson(sim, local, target, role)) return true;
    if (residents != NULL && resident_count > 0) {
        for (int32_t step = 0; step < resident_count; ++step) {
            const CcCharacter *person = residents[(slot + step) % resident_count];
            bool used = false;
            for (int32_t j = 0; j < local->interactions.count; ++j)
                if (local->interactions.targets[j].character_id == person->id) used = true;
            if (used || AdventureCharacterReserved(local, target, person->id)) continue;
            target->character_id = person->id;
            (void)snprintf(target->name, sizeof(target->name), "%s", person->name);
            AdventureRememberPresentedPerson(local, target);
            return true;
        }
    }
    return false;
}

static void BuildAdventureTargets(const CcSim *sim, LocalState *local,
    RenderTexture2D texture, Rectangle destination)
{
    local->interactions.count = 0;
    if (!AdventureScene(local)) return;
    AdventurePrunePresentedPersonBindings(sim, local);
    const CcLocalPlaceProfile *place = CcLocalPlaceProfileForSettlement(
        CcSimSettlement(sim, sim->player.location_id));
    const CcSettlement *town = CcSimSettlement(sim, sim->player.location_id);
    bool abandoned = town != NULL && CcSettlementIsAbandoned(town);
    const CcLocalShop *active_shop = LocalActiveShop(sim, local);
    if (local->market_interior) {
        if (!abandoned) (void)AdventureTarget(local, sim, CC_INTERACTION_COUNTER, 1U,
            active_shop != NULL ? active_shop->keeper_name : place->keeper_name,
            active_shop != NULL && active_shop->kind == CC_LOCAL_SHOP_GRAIN_MERCHANT &&
                AdventureDeliveryAtHand(sim) != NULL ? "Deliver promise" : "Trade", (Vector3){6.55f, 0, 1.9f}, 2.1f, 2.0f, 1.0f,
            (Vector2){6.55f, 3.15f}, 1.0f);
        const CcSettlement *warehouse = CcSimSettlement(sim, sim->player.location_id);
        for (int32_t good = 0; !abandoned && warehouse != NULL && good < CC_GOOD_COUNT; ++good) {
            if (!CcLocalShopSells(active_shop, (CcGood)good)) continue;
            if (warehouse->stock[good] <= 0 && sim->player.cargo[good] <= 0) continue;
            Vector3 bay = CcLocalWarehouseGoodPosition((CcGood)good);
            CcInteractionTarget *stock = AdventureTarget(local, sim, CC_INTERACTION_STOCK,
                (uint64_t)good + 1U,
                TextFormat("%s: %d here", CcGoodName((CcGood)good), warehouse->stock[good]),
                "Trade", bay, 0.52f, 0.70f, 0.64f, (Vector2){bay.x, 2.85f}, 0.80f);
            if (stock != NULL) stock->amount = good;
        }
        (void)AdventureTarget(local, sim, CC_INTERACTION_EXIT, 1U,
            "Street", "Step outside", (Vector3){1.55f, 0, 6.54f}, 1.3f, 2.5f, 0.5f,
            INTERIOR_EXIT, 0.85f);
    } else {
        for (int32_t building = 0; building < place->building_count; ++building) {
            const CcLocalShop *shop = CcLocalShopForBuilding(place, building);
            if (shop == NULL) continue;
            CcLocalLanePoint entrance = CcLocalShopDoor(place, shop);
            CcLocalLanePoint approach = CcLocalShopApproach(place, shop);
            CcInteractionTarget *door = AdventureTarget(local, sim,
                CC_INTERACTION_DOOR, (uint64_t)building + 1U,
                shop->name, shop->kind == CC_LOCAL_SHOP_GRAIN_MERCHANT &&
                    AdventureDeliveryAtHand(sim) != NULL ? "Deliver promise" : "Enter",
                (Vector3){entrance.x, 0, entrance.z}, 1.8f, 2.8f, 0.5f,
                (Vector2){approach.x, approach.z}, 1.1f);
            if (abandoned && door != NULL) {
                door->available = false;
                (void)snprintf(door->verb, sizeof(door->verb), "Closed");
                (void)snprintf(door->reason, sizeof(door->reason),
                    "The town is abandoned. Its services are closed.");
            }
        }
        const CcSettlement *oven_town = CcOvenCourtPlace(sim);
        if (!abandoned && oven_town != NULL && town == oven_town && sim->schema_version >= 107U) {
            CcLocalLanePoint oven = CcLocalOvenCourtAnchor(place);
            (void)AdventureTarget(local, sim, CC_INTERACTION_OVEN_COURT, 1U,
                "Oven Court tally", "Inspect", (Vector3){oven.x, 0, oven.z},
                2.2f, 2.4f, 0.55f, (Vector2){oven.x, oven.z + 1.1f}, 1.0f);
        }
        if (!abandoned) (void)AdventureTarget(local, sim, CC_INTERACTION_BOARD, 1U,
            place->notice_board, "Read promises", (Vector3){LOCAL_NOTICE.x, 0, LOCAL_NOTICE.y},
            1.6f, 2.5f, 0.7f, LOCAL_NOTICE, 1.05f);
        if (local->opening_step == CC_LOCAL_OPENING_COMPLETE) (void)AdventureTarget(local, sim, CC_INTERACTION_CARRIAGE, 1U,
            "Crownless carriage", "Board", (Vector3){LOCAL_CARRIAGE.x, 0, LOCAL_CARRIAGE.y},
            3.2f, 3.2f, 5.4f, LOCAL_CARRIAGE_BAY, 1.5f);
        if (!abandoned && local->course.situation_witness_active) {
            const CcCharacter *person = CcSimCharacter(sim, local->course.situation_witness_character_id);
            Vector3 position = local->course.situation_witness.position;
            CcInteractionTarget *target = AdventureTarget(local, sim, CC_INTERACTION_PERSON,
                local->course.situation_witness_character_id,
                person != NULL ? person->name : "Neighbour", "Talk",
                (Vector3){position.x, 0, position.z}, 0.9f, 2.0f, 0.7f,
                (Vector2){position.x, position.z}, 1.75f);
            if (target != NULL) {
                target->character_id = local->course.situation_witness_character_id;
                target->situation_id = local->course.situation_witness_id;
            }
        }
        CcLocalPresentedPerson people[64];
        const CcCharacter *residents[64];
        int32_t resident_count = 0;
        if (!abandoned) {
            for (int32_t i = 0; i < 64; ++i) {
                const CcCharacter *person = CcLocalTownPerson(sim, town->id, i);
                if (person == NULL) break;
                residents[resident_count++] = person;
            }
        }
        for (int32_t i = 0; !abandoned && i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            Vector3 position = local->course.runners[i].agent.position;
            CcInteractionTarget *runner = AdventureTarget(local, sim, CC_INTERACTION_PERSON,
                UINT64_C(0x200000000) + (uint64_t)i, "Town guard", "Talk",
                (Vector3){position.x, 0, position.z}, 0.9f, 2.0f, 0.7f,
                (Vector2){position.x, position.z}, 1.8f);
            if (runner != NULL &&
                AdventureNamePresented(sim, local, runner, CC_NPC_ROLE_GUARD,
                    residents, resident_count, i)) {
                if (!AdventureKnownPerson(sim, CcSimCharacter(sim, runner->character_id)))
                    (void)snprintf(runner->name, sizeof(runner->name), "%s", "Town guard");
            } else {
                --local->interactions.count;
            }
        }
        const CcDungeon *local_dungeon = DungeonAtSettlement(
            sim, sim->player.location_id);
        if (local_dungeon != NULL) {
            const CcRoadSite *mine = CcMineSite(sim);
            CcId mine_route_id = 0U;
            CcId mine_destination_id = 0U;
            bool mine_road = mine != NULL && CcRoadSiteJourneyTarget(
                sim, mine->id, &mine_route_id, &mine_destination_id);
            (void)AdventureTarget(local, sim, CC_INTERACTION_SITE, CC_LOCAL_SITE_DUNGEON,
                mine_road ? "Low Silver Pit road" : local_dungeon->name,
                mine_road ? "Drive to Low Silver Pit" : "Travel to the dungeon",
                (Vector3){LOCAL_DUNGEON.x, 0, LOCAL_DUNGEON.y},
                2.2f, 3.1f, 1.6f, LOCAL_DUNGEON, 1.3f);
        }
        if (abandoned) {
            for (int32_t i = 0; i < 8; ++i) {
                const CcCharacter *person = CcLocalTownPerson(sim, town->id, i);
                if (person == NULL) break;
                Vector2 position = {45.0f + (float)i * 1.3f, 52.0f};
                CcInteractionTarget *visitor = AdventureTarget(local, sim, CC_INTERACTION_PERSON,
                    person->id, AdventureKnownPerson(sim, person) ? person->name :
                        CcCharacterRoleName(person->role), "Talk", (Vector3){position.x, 0, position.y},
                    0.9f, 2.0f, 0.7f, position, 1.8f);
                if (visitor != NULL) visitor->character_id = person->id;
            }
        }
        int32_t count = abandoned ? 0 : CcLocalPresentedPeople(people, 64);
        for (int32_t i = 0; i < count; ++i) {
            bool duplicate = false;
            for (int32_t j = 0; j < local->interactions.count; ++j) {
                const CcInteractionTarget *other = &local->interactions.targets[j];
                if (other->key.kind == CC_INTERACTION_PERSON &&
                    (other->key.object == (UINT64_C(0x100000000) | (uint64_t)people[i].seed) ||
                    GridDistance((Vector2){other->x, other->z},
                        (Vector2){people[i].position.x, people[i].position.z}) < 0.65f)) duplicate = true;
            }
            if (duplicate || people[i].role == CC_NPC_ROLE_RAIDER) continue;
            CcNpcRole figure_role = people[i].role;
            CcInteractionTarget *resident = AdventureTarget(local, sim, CC_INTERACTION_PERSON,
                UINT64_C(0x100000000) | (uint64_t)people[i].seed,
                AdventureRoleName(figure_role), "Talk",
                (Vector3){people[i].position.x, 0, people[i].position.z},
                0.9f, 2.0f, 0.7f,
                (Vector2){people[i].position.x, people[i].position.z}, 1.8f);
            if (resident != NULL &&
                AdventureNamePresented(sim, local, resident, figure_role,
                    residents, resident_count, i)) {
                if (!AdventureKnownPerson(sim, CcSimCharacter(sim, resident->character_id)))
                    (void)snprintf(resident->name, sizeof(resident->name), "%s",
                                   AdventureRoleName(figure_role));
            } else {
                --local->interactions.count;
            }
        }
    }
    for (int32_t i = 0; i < local->interactions.count; ++i)
        CcLocalProjectInteraction(&local->agent, texture, destination, &local->interactions.targets[i]);
}

static const CcInteractionTarget *AdventureFocus(const LocalState *local)
{
    const CcInteractionTarget *target = CcInteractionFind(&local->interactions,
        local->interaction.approaching ? local->interaction.pending : local->interaction.focus);
    return target;
}

static void AdventureActivate(CcSim *sim, LocalState *local, const CcInteractionTarget *target,
    ClientView *view, ClientView *return_view, int32_t *selected_situation)
{
    CcLocalAgentStop(&local->agent);
    CcInteractionCancel(&local->interaction, "");
    local->card_page = 0;
    *return_view = VIEW_LOCAL;
    switch (target->key.kind) {
        case CC_INTERACTION_PERSON:
            local->conversation_object = target->key.object;
            local->conversation_gossip_slot = -1;
            local->conversation_gossip_source = false;
            local->conversation_report_response = false;
            local->conversation_oven_response = false;
            local->conversation_character_id = target->character_id;
            local->conversation_situation_id = target->situation_id;
            local->conversation_position = (Vector3){target->x, target->y, target->z};
            (void)snprintf(local->conversation_name, sizeof(local->conversation_name), "%s", target->name);
            (void)snprintf(local->conversation_line, sizeof(local->conversation_line),
                "Welcome. I was passing the %s. How was your journey?",
                CcLocalPlaceProfileForSettlement(CcSimSettlement(sim, sim->player.location_id))->primary_hall);
            local->agent.facing_yaw = atan2f(target->x - local->agent.position.x,
                target->z - local->agent.position.z);
            if (target->character_id != 0U) local->course.situation_witness.facing_yaw =
                local->agent.facing_yaw + PI;
            *view = VIEW_CHARACTER;
            break;
        case CC_INTERACTION_DOOR:
            local->shop_building_index = (int32_t)target->key.object - 1;
            local->market_interior = true;
            RepositionHero(local, (Vector2){2.05f, 5.35f}, true);
            local->interactions.count = 0;
            break;
        case CC_INTERACTION_EXIT:
            local->market_interior = false;
            {
                const CcLocalPlaceProfile *profile = CcLocalPlaceProfileForSettlement(
                    CcSimSettlement(sim, sim->player.location_id));
                const CcLocalShop *shop = CcLocalShopForBuilding(profile,
                    local->shop_building_index);
                CcLocalLanePoint approach = shop != NULL ?
                    CcLocalShopApproach(profile, shop) :
                    (CcLocalLanePoint){CC_LOCAL_MARKET_X, CC_LOCAL_MARKET_Z + 1.1f};
                RepositionHero(local, (Vector2){approach.x, approach.z}, false);
                local->shop_building_index = -1;
            }
            local->interactions.count = 0;
            break;
        case CC_INTERACTION_STOCK:
        case CC_INTERACTION_COUNTER:
            local->trade_quantity = 1;
            local->trade_good = target->key.kind == CC_INTERACTION_STOCK ?
                (CcGood)target->amount : LocalShopFirstGood(LocalActiveShop(sim, local));
            local->trade_mode = 0;
            const CcSituation *delivery = target->key.kind == CC_INTERACTION_COUNTER &&
                LocalActiveShop(sim, local) != NULL &&
                LocalActiveShop(sim, local)->kind == CC_LOCAL_SHOP_GRAIN_MERCHANT ?
                AdventureDeliveryAtHand(sim) : NULL;
            if (delivery != NULL) {
                local->trade_mode = 2;
                local->trade_good = delivery->good;
                local->trade_quantity = delivery->quantity - delivery->progress;
            }
            local->trade_confirmed = false;
            local->trade_quote_presented = false;
            *view = VIEW_TRADE;
            break;
        case CC_INTERACTION_BOARD:
            if (SelectedActiveSituation(sim, *selected_situation) == NULL)
                *selected_situation = FirstActiveSituationIndex(sim);
            *view = VIEW_SITUATIONS;
            break;
        case CC_INTERACTION_SITE:
            BeginSiteTravelState(local, (CcLocalSiteKind)target->key.object, false);
            break;
        case CC_INTERACTION_CARRIAGE: *view = VIEW_CARRIAGE; break;
        default: break;
    }
}

/* Called before legacy movement and command handling. A handled click is consumed. */
static bool HandleAdventureScene(CcJournal *journal, CcSim *sim,
    LocalState *local,
    ClientView *view, ClientView *return_view, int32_t *selected_situation,
    const CcInteractionTarget *card_target, float dt,
    char *message, size_t capacity)
{
    if (*view != VIEW_LOCAL || !AdventureScene(local)) return false;
    Vector2 position = LocalPosition(local);
    if (ClientKeyPressed(KEY_ESCAPE) && local->interaction.approaching) {
        CcInteractionCancel(&local->interaction, "Approach cancelled.");
        CcLocalAgentStop(&local->agent);
        return true;
    }
    if (ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_target_next : KEY_E))
        CcInteractionCycle(&local->interaction, &local->interactions,
            IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? -1 : 1);
    Vector2 mouse = ClientPointerPosition();
    const CcInteractionTarget *hover = CcInteractionPick(&local->interactions, mouse.x, mouse.y);
    bool click = ClientMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const CcInteractionTarget *focus = AdventureFocus(local);
    bool activate = ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_interact : KEY_F);
    const CcInteractionTarget *chosen = card_target != NULL ? card_target :
        click && hover != NULL ? hover : activate ? focus : NULL;
    if (chosen == NULL && activate) {
        float nearest = 3.0f;
        for (int32_t i = 0; i < local->interactions.count; ++i) {
            const CcInteractionTarget *candidate = &local->interactions.targets[i];
            float distance = GridDistance(position, (Vector2){candidate->approach_x, candidate->approach_z});
            if (candidate->visible && distance < nearest) { nearest = distance; chosen = candidate; }
        }
    }
    if (chosen != NULL) {
        (void)CcInteractionStart(&local->interaction, chosen, position.x, position.y);
        if (!local->interaction.approaching) {
            (void)snprintf(message, capacity, "%s Double-tap [N] to return to the caravan.",
                           local->interaction.feedback);
            return true;
        }
    } else if (click || IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) ||
               IsKeyDown(KEY_D) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_DOWN) ||
               IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT)) {
        if (local->interaction.approaching) CcLocalAgentStop(&local->agent);
        CcInteractionCancel(&local->interaction, "");
    }
    if (!local->interaction.approaching) return false;
    CcInteractionKey pending = local->interaction.pending;
    const CcInteractionTarget *target = CcInteractionFind(&local->interactions, pending);
    if (CcInteractionAdvance(&local->interaction, &local->interactions, position.x, position.y, dt)) {
        const CcRoadSite *mine = target != NULL &&
                target->key.kind == CC_INTERACTION_SITE &&
                target->key.object == CC_LOCAL_SITE_DUNGEON ?
            CcMineSite(sim) : NULL;
        CcId route_id = 0U;
        CcId destination_id = 0U;
        bool mine_road = journal != NULL && mine != NULL &&
            CcRoadSiteJourneyTarget(
                sim, mine->id, &route_id, &destination_id);
        if (mine_road) {
            CcCommand travel = {
                .kind = CC_COMMAND_TRAVEL,
                .target_id = destination_id
            };
            if (ApplyCommand(
                    journal, sim, travel, message, capacity)) {
                BeginRoadTravelState(sim, local);
                (void)snprintf(
                    message, capacity,
                    "The carriage takes the Alderwatch road toward the Low Silver Pit branch.");
            }
        } else if (target != NULL && target->key.kind == CC_INTERACTION_OVEN_COURT) {
            CcLocalAgentStop(&local->agent);
            if (ApplyCommand(journal, sim,
                (CcCommand){.kind = CC_COMMAND_OBSERVE_OVEN_COURT,
                    .target_id = sim->player.location_id}, message, capacity) &&
                CcOvenCourtRead(sim, &local->oven_observation)) {
                const CcEvent *note = CcOvenCourtNote(sim, 0);
                local->oven_note_event = note != NULL ? note->id : 0U;
                local->oven_page = 0;
                *return_view = VIEW_LOCAL;
                *view = VIEW_OVEN_COURT;
            }
        } else if (target != NULL) {
            AdventureActivate(
                sim, local, target, view, return_view,
                selected_situation);
        }
        return true;
    }
    if (!local->interaction.approaching) {
        CcLocalAgentStop(&local->agent);
        (void)snprintf(message, capacity, "%s Double-tap [N] to return to the caravan.",
                           local->interaction.feedback);
        return true;
    }
    if (target != NULL && local->interaction.repath_seconds <= 0.0f &&
        (target->key.kind == CC_INTERACTION_PERSON || !local->agent.target_valid)) {
        bool walking = CcLocalAgentApproachInteraction(&local->agent,
            (Vector2){target->approach_x, target->approach_z}, target->radius * 0.9f, local->market_interior);
        local->interaction.repath_seconds = target->key.kind == CC_INTERACTION_PERSON ? 0.5f : 1.5f;
        if (!walking) {
            CcInteractionCancel(&local->interaction, "Choose a clear path closer to this target.");
            (void)snprintf(message, capacity, "%s Double-tap [N] to return to the caravan.",
                           local->interaction.feedback);
        }
    }
    (void)CcLocalWorldUpdate(&local->course, &local->agent, sim, dt,
        local->market_interior, !local->market_interior);
    return true;
}

static int AdventureTextSize(int base)
{
    return base + (adventure_preferences != NULL ? adventure_preferences->text_size * 2 : 0);
}

static int AdventureText(const char *text, int x, int y, int width, int size, Color color, bool draw)
{
    char line[256] = "";
    size_t used = 0;
    int start = y;
    const char *cursor = text;
    while (cursor != NULL && *cursor != '\0') {
        const char *end = cursor;
        while (*end != '\0' && *end != ' ' && *end != '\n') ++end;
        size_t length = (size_t)(end - cursor);
        if (length > 180U) length = 180U;
        char next[256];
        (void)snprintf(next, sizeof(next), "%s%s%.*s", line, used > 0 ? " " : "", (int)length, cursor);
        if (used > 0 && CcOverlayMeasureBodyText(next, size) > width) {
            if (draw) CcOverlayDrawBodyText(line, x, y, size, color);
            y += size + 7;
            used = 0;
            line[0] = '\0';
        }
        if (used > 0 && used < sizeof(line) - 1U) line[used++] = ' ';
        size_t copy = length < sizeof(line) - used - 1U ? length : sizeof(line) - used - 1U;
        memcpy(line + used, cursor, copy); used += copy; line[used] = '\0';
        cursor = end;
        if (*cursor == '\n') {
            if (draw) CcOverlayDrawBodyText(line, x, y, size, color);
            y += size + 7;
            used = 0; line[0] = '\0';
        }
        if (*cursor != '\0') ++cursor;
    }
    if (used > 0) {
        if (draw) CcOverlayDrawBodyText(line, x, y, size, color);
        y += size + 7;
    }
    return y - start;
}

static int AdventureWrap(const char *text, int x, int y, int width, int size, Color color)
{
    return AdventureText(text, x, y, width, size, color, true);
}

static Rectangle AdventurePanel(void)
{
    float screen_width=(float)GetScreenWidth();
    float screen_height=(float)GetScreenHeight();
#if defined(CC_CLIENT_SELF_TESTS)
    if (screen_width <= 0.0f || screen_height <= 0.0f) {
        screen_width=1280.0f;
        screen_height=720.0f;
    }
#endif
    float width = fminf(980.0f, screen_width - 48.0f);
    float height = fminf(560.0f, screen_height - 100.0f);
    return (Rectangle){(screen_width - width) * 0.5f,
        (screen_height - height) * 0.5f, width, height};
}

static bool AdventureHit(Rectangle bounds)
{
    return ClientMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(ClientPointerPosition(), bounds);
}

static void AdventureButtonWithTouchLabel(Rectangle bounds, const char *label,
    const char *touch_label, bool enabled, bool active)
{
    ClientTouchAdd(bounds, touch_label, enabled, active);
    bool hover = CheckCollisionPointRec(ClientPointerPosition(), bounds);
    DrawRectangleRounded(bounds, 0.12f, 4, active || (hover && enabled) ? PANEL_HOVER : PANEL_DEEP);
    DrawRectangleRoundedLinesEx(bounds, 0.12f, 4, active ? 2.0f : 1.0f,
        enabled ? active ? CC_GOLD : TEAL : MUTED);
    int size = AdventureTextSize(14);
    while (size > 12 && CcOverlayMeasureText(label, size) > (int)bounds.width - 24) --size;
    CcOverlayDrawText(label, (int)bounds.x + 12, (int)bounds.y + 12, size, enabled ? INK : MUTED);
}

static void AdventureButton(Rectangle bounds, const char *label, bool enabled, bool active)
{
    AdventureButtonWithTouchLabel(bounds, label, label, enabled, active);
}

static Rectangle AdventureTab(Rectangle panel, int index, int count)
{
    float width = (panel.width - 48.0f) / (float)count;
    return (Rectangle){panel.x + 24.0f + (float)index * width, panel.y + 65.0f, width - 8.0f, 44.0f};
}

static Rectangle AdventureClose(Rectangle panel)
{
    return (Rectangle){panel.x + panel.width - 156.0f, panel.y + 14.0f, 132.0f, 38.0f};
}

static void AdventurePanelTitle(Rectangle panel, const char *title)
{
    ClientTouchBegin();
    ClientTouchHeading(title, "Choose an action below.");
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BACKGROUND, 0.65f));
    DrawPanel(panel, PANEL_DEEP);
    CcOverlayDrawText(title, (int)panel.x + 24, (int)panel.y + 22, AdventureTextSize(23), CC_GOLD);
    AdventureButton(AdventureClose(panel), "Back  Esc", true, false);
}

typedef struct AdventureQuote {
    CcCommand command;
    CcMoney total;
    CcMoney reward;
    CcId promise_id;
    bool available;
    char reason[160];
} AdventureQuote;

static AdventureQuote AdventureTradeQuote(const CcSim *sim, const LocalState *local)
{
    AdventureQuote quote = {.command = {.kind = CC_COMMAND_TRADE,
        .good = local->trade_good, .amount = local->trade_quantity}};
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    const CcSituation *promise = CcSimAcceptedSituation(sim);
    const CcLocalShop *shop = LocalActiveShop(sim, local);
    const char *reason = "";
    if (place == NULL || CcSettlementIsAbandoned(place) || shop == NULL) {
        reason = "This shop is closed.";
    } else if (local->trade_good < 0 || local->trade_good >= CC_GOOD_COUNT ||
        local->trade_quantity < 1 || local->trade_quantity > 999) {
        reason = "Choose a good and quantity.";
    } else if (local->trade_mode == 2 &&
        shop->kind != CC_LOCAL_SHOP_GRAIN_MERCHANT) {
        reason = "Bring the promise to the grain merchant.";
    } else if ((local->trade_mode == 0 &&
                LocalShopSupplySource(shop, local->trade_good)) ||
               (local->trade_mode == 1 &&
                LocalShopSupplyBuyer(shop, local->trade_good))) {
        CcSupplyOffer offer = CcSimSupplyOffer(sim, local->trade_good);
        int32_t amount = local->trade_quantity;
        bool source = local->trade_mode == 0;
        quote.command.kind = CC_COMMAND_TRADE_SUPPLY;
        quote.command.target_id = place->id;
        quote.command.amount = source ? amount : -amount;
        quote.total = (CcMoney)amount * (source ?
            offer.source_unit_price : offer.delivery_unit_price);
        if (source) {
            CcPlayerCompany loaded = sim->player;
            loaded.cargo[local->trade_good] += amount;
            if (offer.source_available < amount)
                reason = "Choose a smaller load from this supplier.";
            else if (CcPlayerCargoUsed(&loaded) > loaded.cargo_capacity)
                reason = "Make room in the carriage or choose a smaller load.";
            else if (sim->player.coins < quote.total)
                reason = "Choose a load your purse can cover.";
        } else {
            bool promised = promise != NULL &&
                promise->good == local->trade_good &&
                (promise->kind == CC_SITUATION_RELIEF_DELIVERY ||
                 promise->kind == CC_SITUATION_BLACK_MARKET_DELIVERY);
            if (promised)
                reason = "This load belongs to your promise. Deliver it first.";
            else if (!offer.ready)
                reason = "This workshop cannot take an order today.";
            else if (amount > offer.remaining)
                reason = "Choose a smaller load to match the current order.";
            else if (sim->player.cargo[local->trade_good] < amount)
                reason = "Bring the full load to this workshop.";
        }
    } else if (local->trade_mode != 2 &&
        !(local->trade_mode == 0 ?
          CcLocalShopSells(shop, local->trade_good) :
          CcLocalShopBuys(shop, local->trade_good)) &&
        !(local->trade_mode == 1 &&
          LocalShopHandlesSpoiledCargo(shop, local->trade_good))) {
        reason = "This shop trades different goods.";
    } else {
        bool promised_here = promise != NULL && promise->target_id == place->id &&
            (promise->kind == CC_SITUATION_RELIEF_DELIVERY || promise->kind == CC_SITUATION_BLACK_MARKET_DELIVERY);
        bool matching = promised_here && promise->good == local->trade_good;
        int32_t amount = local->trade_quantity;
        if (local->trade_mode == 2) {
            if (!promised_here) reason = "Choose Deliver at the town named in your accepted promise.";
            else {
                amount = promise->quantity - promise->progress;
                quote.command.good = promise->good;
                quote.reward = promise->reward;
                quote.promise_id = promise->id;
                if (sim->resolved_journey_situation_id != promise->id)
                    reason = "Carry the promised load here from another town first.";
            }
        } else if (local->trade_mode == 1 && matching) {
            reason = "This cargo belongs to your promise. Choose Deliver to settle it.";
        }
        CcGood good = quote.command.good;
        quote.command.amount = local->trade_mode == 0 ? amount : -amount;
        int32_t unit = local->trade_mode == 0 ? place->price[good] :
            CcSimTradeResalePrice(sim, place, good);
        if (unit < 1) unit = 1;
        quote.total = (CcMoney)amount * unit;
        if (reason[0] == '\0' && local->trade_mode == 0) {
            CcPlayerCompany loaded = sim->player;
            loaded.cargo[good] += amount;
            if (place->stock[good] < amount) reason = "Choose a smaller quantity to match the available stock.";
            else if (CcPlayerCargoUsed(&loaded) > loaded.cargo_capacity) reason = "Make room in the carriage or choose a smaller quantity.";
            else if (sim->player.coins < quote.total) reason = "Choose a quantity your purse can cover.";
        } else if (reason[0] == '\0') {
            if (amount < 1 || sim->player.cargo[good] < amount) reason = "Bring the full quantity to the counter.";
            else if (place->market_coins < quote.total) reason = "The keeper needs more crowns to buy this quantity.";
        }
    }
    quote.available = reason[0] == '\0' && !local->trade_confirmed;
    (void)snprintf(quote.reason, sizeof(quote.reason), "%s", local->trade_confirmed ?
        "Trade complete. Choose another good or quantity for your next trade." : reason);
    return quote;
}

static void AdventureTradeConfirmTouchLabel(const AdventureQuote *quote,
    int mode, char *label, size_t capacity)
{
    const char *verb = mode == 0 ? "Buy" : mode == 1 ? "Sell" : "Deliver";
    const char *amount_name = mode == 0 ? "Total cost" : "Sale value";
    int written = mode == 2 ?
        snprintf(label, capacity, "%s %d %s. Sale value: %" PRId64
            " crowns. Promise reward: %" PRId64 " crowns.",
            verb, abs(quote->command.amount),
            CcGoodName(quote->command.good), quote->total, quote->reward) :
        snprintf(label, capacity, "%s %d %s. %s: %" PRId64
            " crowns.", verb, abs(quote->command.amount),
            CcGoodName(quote->command.good), amount_name, quote->total);
    if (written < 0 || (size_t)written >= capacity) return;
    if (!quote->available && quote->reason[0] != '\0')
        (void)snprintf(label + written, capacity - (size_t)written,
            " %s", quote->reason);
}

static Rectangle AdventureGoodBounds(Rectangle panel, int32_t good)
{
    float width = (panel.width * 0.47f - 36.0f) * 0.5f;
    float pitch = fminf(44.0f, (panel.height - 240.0f) / 7.0f);
    return (Rectangle){panel.x + 24.0f + (float)(good % 2) * (width + 6.0f),
        panel.y + 140.0f + (float)(good / 2) * pitch, width, pitch - 3.0f};
}

static bool AdventureShopGoodVisible(const CcSim *sim,
    const CcLocalShop *shop, int mode, CcGood good)
{
    return (mode == 0 && CcLocalShopSells(shop, good)) ||
        (mode == 1 && CcLocalShopBuys(shop, good)) ||
        (mode == 1 && LocalShopHandlesSpoiledCargo(shop, good) &&
         sim->player.cargo[good] > 0);
}

static Rectangle AdventureShopGoodBounds(Rectangle panel, const CcSim *sim,
    const CcLocalShop *shop, int mode, CcGood good)
{
    int32_t slot = 0;
    for (int32_t current = 0; current < (int32_t)good; ++current)
        if (AdventureShopGoodVisible(sim, shop, mode, (CcGood)current)) ++slot;
    return AdventureGoodBounds(panel, slot);
}

static bool AdventureShopModeAvailable(const CcLocalShop *shop, int mode)
{
    if (shop == NULL) return false;
    if (mode == 3) return shop->kind == CC_LOCAL_SHOP_BAKERY;
    if (mode == 2 || mode == 4 || mode == 5)
        return shop->kind == CC_LOCAL_SHOP_GRAIN_MERCHANT;
    return mode == 0 || mode == 1;
}

static int AdventureShopTabIndex(const CcLocalShop *shop, int mode)
{
    int slot = 0;
    for (int i = 0; i < mode; ++i)
        if (AdventureShopModeAvailable(shop, i)) ++slot;
    return slot;
}

static Rectangle AdventureShopTab(Rectangle panel,
    const CcLocalShop *shop, int mode)
{
    int count = 0;
    for (int i = 0; i < 6; ++i)
        if (AdventureShopModeAvailable(shop, i)) ++count;
    return AdventureTab(panel, AdventureShopTabIndex(shop, mode), count);
}

static void AdventureShopTitle(Rectangle panel, const CcLocalShop *shop,
    const char *service)
{
    AdventurePanelTitle(panel, shop->name);
    CcOverlayDrawText(service, (int)panel.x + 25, (int)panel.y + 49,
        AdventureTextSize(11), MUTED);
}

static Rectangle AdventureQuantityBounds(Rectangle panel, int index)
{
    return (Rectangle){panel.x + panel.width * 0.52f + (float)index * 68.0f,
        panel.y + (panel.width >= 700.0f && panel.height >= 520.0f ? 270.0f : 230.0f),
        60.0f, 44.0f};
}

static Rectangle AdventureConfirmBounds(Rectangle panel)
{
    return (Rectangle){panel.x + panel.width * 0.52f,
        panel.y + (panel.width >= 700.0f && panel.height >= 520.0f ? 340.0f : 302.0f),
        panel.width * 0.48f - 24.0f, 46.0f};
}

static Rectangle CouncilRowButton(Rectangle panel, int row)
{
    return (Rectangle){panel.x + 28, panel.y + 140 + (float)row * 46, panel.width * .36f, 38};
}

static void DrawAdventureCouncil(const CcSim *sim, LocalState *local, const CcSettlement *town)
{
    Rectangle panel = AdventurePanel();
    CcRoadCouncil council = CcSimRoadCouncil(sim, town->id);
    int selected = (int)local->trade_good % CC_ROAD_COUNCIL_ROWS;
    if (selected < 0) selected = 0;
    const CcLocalShop *shop = LocalActiveShop(sim, local);
    AdventureShopTitle(panel, shop, "Road meeting");
    const char *tabs[] = {"Buy", "Sell", "Deliver promise", "Bakery", "Grain fund", "Road meeting"};
    for (int i = 0; i < 6; ++i)
        if (AdventureShopModeAvailable(shop, i))
            AdventureButton(AdventureShopTab(panel, shop, i), tabs[i], true, i == 5);
    const char *concerns[] = {"Grain", "Repair", "Trade", "Visitor", "Camp", "Scout"};
    for (int i = 0; i < CC_ROAD_COUNCIL_ROWS; ++i)
        AdventureButton(CouncilRowButton(panel, i), TextFormat("%s: %s", concerns[i], council.rows[i].name), true, i == selected);
    const CcRoadCouncilRow *row = &council.rows[selected];
    int x = (int)(panel.x + panel.width * .44f), y = (int)panel.y + 144;
    int width = (int)(panel.width * .56f) - 30;
    y += AdventureWrap(row->name, x, y, width, AdventureTextSize(18), INK) + 20;
    y += AdventureWrap(row->detail, x, y, width, AdventureTextSize(15), TEAL) + 20;
    const CcSituation *quest = CcSimSituation(sim, row->situation_id);
    if (quest != NULL) {
        (void)AdventureWrap(TextFormat("Public work: %s. Reward %" PRId64 " crowns. Due day %d. Open Promises to read its terms.",
            CcSituationKindName(quest->kind), quest->reward, quest->deadline_day), x, y, width, AdventureTextSize(14), INK);
    }
    (void)AdventureWrap("Choose a person or concern. Use Bakery or Grain fund to offer supplies. Road repairs and public work keep their usual terms.",
        (int)panel.x + 28, (int)(panel.y + panel.height) - 95, (int)panel.width - 56, AdventureTextSize(13), MUTED);
    ClientTouchHeading(shop->name, row->detail);
    local->trade_quote_presented = false;
}

static Rectangle GrainFundButton(Rectangle panel, bool stop)
{
    return (Rectangle){panel.x + 28 + (stop ? panel.width * .5f : 0),
        panel.y + panel.height - 100, panel.width * .5f - 40, 34};
}

static void DrawAdventureGrainSupply(const CcSim *sim, LocalState *local, const CcSettlement *town)
{
    Rectangle panel = AdventurePanel();
    const CcGrainSupply *supply = CcSimGrainSupply(sim, town->id);
    CcGrainDeliveryPlan plan = CcSimGrainDeliveryPlan(sim, town->id);
    CcBakerySupportPlan offer = CcSimBakerySupportPlan(sim, town->id);
    const CcCharacter *organiser = CcSimCharacter(sim, supply->organiser_id != 0 ? supply->organiser_id : offer.contact_id);
    const CcLocalShop *shop = LocalActiveShop(sim, local);
    AdventureShopTitle(panel, shop, "Continuing grain deliveries");
    const char *tabs[] = {"Buy", "Sell", "Deliver promise", "Bakery", "Grain fund", "Road meeting"};
    for (int i = 0; i < 6; ++i)
        if (AdventureShopModeAvailable(shop, i))
            AdventureButton(AdventureShopTab(panel, shop, i), tabs[i], true, i == 4);
    int x = (int)panel.x + 28, y = (int)panel.y + 135;
    CcOverlayDrawText(organiser != NULL ? organiser->name : "A local organiser is needed", x, y, AdventureTextSize(20), INK);
    y += 36;
    y += AdventureWrap("Give 200 crowns: eight pay the organiser, and 192 fund grain and road dues. Orders use the kingdom's carriage.",
        x, y, (int)panel.width - 56, AdventureTextSize(14), MUTED) + 14;
    CcOverlayDrawText(TextFormat("Grain fund: %" PRId64 " crowns. Spent: %" PRId64 ". Your purse: %" PRId64 ".", supply->purse, supply->spent, sim->player.coins),
        x, y, AdventureTextSize(15), INK);
    y += 30;
    CcOverlayDrawText(TextFormat("Wheat ordered: %d. Arrived: %d. Lost: %d. Unloaded elsewhere: %d.",
        supply->ordered, supply->delivered, supply->lost, supply->redirected), x, y, AdventureTextSize(13), TEAL);
    y += 30;
    const CcSettlement *supplier = CcSimSettlement(sim, plan.supplier_id != 0 ? plan.supplier_id : supply->supplier_id);
    if (supplier != NULL) {
        CcOverlayDrawText(supply->last_arrival_day > 0 ?
            TextFormat("Supplier: %s. Last delivery: %d days ago.", supplier->name, sim->current_day - supply->last_arrival_day) :
            TextFormat("Supplier: %s. First arrival pending.", supplier->name), x, y, AdventureTextSize(14), INK);
        y += 30;
    }
    const CcRoute *road = CcSimRoute(sim, plan.route_id != 0 ? plan.route_id : supply->route_id);
    if (road != NULL) {
        const CcSettlement *from = CcSimSettlement(sim, road->from_id);
        const CcSettlement *to = CcSimSettlement(sim, road->to_id);
        CcOverlayDrawText(TextFormat("Supply road: %s to %s.", from->name, to->name), x, y, AdventureTextSize(14), INK);
        y += 30;
    }
    (void)AdventureWrap(plan.reason, x, y, (int)panel.width - 56, AdventureTextSize(14), TEAL);
    bool can_fund = sim->player.coins >= 200 && offer.contact_id != 0 && CcSettlementHasService(town, CC_SERVICE_BAKERY);
    AdventureButton(GrainFundButton(panel, false), "Fund 200 crowns  Enter", can_fund, false);
    AdventureButton(GrainFundButton(panel, true), "End orders and return fund", supply->enabled || supply->purse > 0, false);
    (void)AdventureWrap(local->receipt, x, (int)(panel.y + panel.height) - 55, (int)panel.width - 56, AdventureTextSize(13), MUTED);
    ClientTouchHeading(shop->name, plan.reason);
    local->trade_quote_presented = true;
    local->trade_presented_command = (CcCommand){.kind = CC_COMMAND_FUND_GRAIN_SUPPLY, .target_id = town->id};
    local->trade_presented_total = 200;
    local->trade_presented_reward = supply->purse;
}

static void DrawAdventureTrade(const CcSim *sim, LocalState *local,
    Texture2D economic_goods)
{
    Rectangle panel = AdventurePanel();
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    const CcLocalShop *selected_shop = LocalActiveShop(sim, local);
    if (CcSettlementIsAbandoned(place) || selected_shop == NULL) {
        AdventurePanelTitle(panel, "Closed shop");
        CcOverlayDrawText("The town's services are closed.",
            (int)panel.x + 28, (int)panel.y + 140, AdventureTextSize(16), MUTED);
        return;
    }
    if (!AdventureShopModeAvailable(selected_shop, local->trade_mode))
        local->trade_mode = 0;
    if (local->trade_mode == 5) { DrawAdventureCouncil(sim, local, place); return; }
    if (local->trade_mode == 4) { DrawAdventureGrainSupply(sim, local, place); return; }
    if (local->trade_mode == 3) {
        CcBakerySupportPlan plan = CcSimBakerySupportPlan(sim, place->id);
        const CcCharacter *contact = CcSimCharacter(sim, plan.contact_id);
        AdventureShopTitle(panel, selected_shop, "Bakery support");
        const char *tabs[] = {"Buy", "Sell", "Deliver promise", "Bakery", "Grain fund", "Road meeting"};
        for (int i = 0; i < 6; ++i)
            if (AdventureShopModeAvailable(selected_shop, i))
                AdventureButton(AdventureShopTab(panel, selected_shop, i), tabs[i], true, i == 3);
        int x = (int)panel.x + 28, y = (int)panel.y + 138;
        CcOverlayDrawText(contact != NULL ? (plan.remembered ? TextFormat("%s remembers your help", contact->name) : contact->name) : "Waiting for a local contact", x, y, AdventureTextSize(20), INK);
        y += 35 + AdventureWrap(place->service_project == CC_SERVICE_BAKERY ?
            TextFormat("The bakery is being rebuilt. Work has %d days left.", place->service_project_days) : plan.building_days > 0 ?
            "Bring twelve wheat and eighty crowns. The town supplies the building materials. Building takes seven days." :
            "Bring twelve wheat and eight crowns for the workers. The bakery will use the grain in its ordinary work.",
            x, y + 35, (int)panel.width - 56, AdventureTextSize(15), MUTED);
        for (int good = 0; good < CC_GOOD_COUNT; ++good) {
            if (plan.cargo[good] > 0) {
                CcOverlayDrawText(TextFormat("%s: %d needed, %d aboard", CcGoodName((CcGood)good), plan.cargo[good], sim->player.cargo[good]),
                    x, y + 8, AdventureTextSize(14), sim->player.cargo[good] >= plan.cargo[good] ? TEAL : DANGER);
                y += 22;
            }
        }
        for (int good = 0; good < CC_GOOD_COUNT; ++good) {
            if (plan.town_materials[good] > 0) {
                CcOverlayDrawText(TextFormat("Town %s: %d needed, %d in stock", CcGoodName((CcGood)good), plan.town_materials[good], place->stock[good]),
                    x, y + 8, AdventureTextSize(14), place->stock[good] >= plan.town_materials[good] ? TEAL : DANGER);
                y += 22;
            }
        }
        CcOverlayDrawText(TextFormat("Wages: %" PRId64 " crowns. Purse: %" PRId64, plan.coins, sim->player.coins), x, y + 12, AdventureTextSize(15), INK);
        (void)AdventureWrap(plan.reason,
            x, y + 42, (int)panel.width - 56, AdventureTextSize(14), TEAL);
        Rectangle button = {(float)x, panel.y + panel.height - 100, panel.width - 56, 34};
        AdventureButton(button, plan.building_days > 0 ? "Fund rebuilding  Enter" : "Give grain and wages  Enter", plan.ready, false);
        (void)AdventureWrap(local->receipt, x, (int)(panel.y + panel.height) - 55, (int)panel.width - 56, AdventureTextSize(13), MUTED);
        ClientTouchHeading(selected_shop->name, plan.reason);
        local->trade_quote_presented = true;
        local->trade_presented_total = plan.coins;
        local->trade_presented_command = (CcCommand){.kind = CC_COMMAND_SUPPORT_BAKERY, .target_id = place->id, .amount = plan.building_days};
        return;
    }
    AdventureQuote quote = AdventureTradeQuote(sim, local);
    const CcLocalShop *shop = LocalActiveShop(sim, local);
    if (!local->trade_confirmed) {
        local->trade_quote_presented = true;
        local->trade_presented_total = quote.total;
        local->trade_presented_reward = quote.reward;
        local->trade_presented_promise_id = quote.promise_id;
        local->trade_presented_command = quote.command;
        local->trade_presented_shop_building_index = local->shop_building_index;
    }
    CcMoney displayed_total = local->trade_confirmed ? local->trade_presented_total : quote.total;
    bool supply_source = quote.command.kind == CC_COMMAND_TRADE_SUPPLY &&
        local->trade_mode == 0;
    bool supply_buyer = quote.command.kind == CC_COMMAND_TRADE_SUPPLY &&
        local->trade_mode == 1;
    CcSupplyOffer supply_offer = supply_source || supply_buyer ?
        CcSimSupplyOffer(sim, quote.command.good) : (CcSupplyOffer){0};
    char supply_detail[192] = "";
    if (supply_source) {
        (void)snprintf(supply_detail, sizeof(supply_detail),
            "%s pays %d crowns per unit. Buy here for %d; carriage margin %d per unit.",
            quote.command.good == CC_GOOD_WHEAT ? "Bakery" : "Stonecutter",
            supply_offer.delivery_unit_price, supply_offer.source_unit_price,
            supply_offer.delivery_unit_price - supply_offer.source_unit_price);
    } else if (supply_buyer) {
        (void)snprintf(supply_detail, sizeof(supply_detail),
            "%d units currently wanted. %s supplies each unit for %d crowns; this workshop pays %d.",
            supply_offer.remaining,
            quote.command.good == CC_GOOD_WHEAT ? "Grain merchant" : "Silverwick mine supplier",
            supply_offer.source_unit_price, supply_offer.delivery_unit_price);
    }
    AdventureShopTitle(panel, shop, shop->service_name);
    const char *modes[] = {"Buy", "Sell", "Deliver promise", "Bakery", "Grain fund", "Road meeting"};
    ClientTouchHeading(shop->name,
        TextFormat("%s. %s %d %s. %s: %" PRId64 " crowns. Purse: %" PRId64 ". %s %s",
            shop->service_name,
            modes[local->trade_mode], abs(quote.command.amount), CcGoodName(quote.command.good),
            local->trade_mode == 0 ? "Total cost" : "Sale value", displayed_total,
            sim->player.coins, supply_detail, quote.reason));
    for (int i = 0; i < 6; ++i)
        if (AdventureShopModeAvailable(shop, i))
            AdventureButton(AdventureShopTab(panel, shop, i), modes[i], true,
                i == local->trade_mode);
    CcOverlayDrawText(local->trade_mode == 0 ? "SHOP STOCK" : "CARRIAGE LOAD",
        (int)panel.x + 24, (int)panel.y + 119, AdventureTextSize(11), MUTED);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        if (!AdventureShopGoodVisible(sim, shop, local->trade_mode,
            (CcGood)good)) continue;
        Rectangle bounds = AdventureShopGoodBounds(panel, sim, shop,
            local->trade_mode, (CcGood)good);
        bool active = good == (int32_t)quote.command.good;
        int32_t quantity = local->trade_mode == 0 ? place->stock[good] : sim->player.cargo[good];
        ClientTouchAdd(bounds, TextFormat("%s: %d", CcGoodName((CcGood)good), quantity),
            local->trade_mode != 2, active);
        DrawRectangleRec(bounds, active ? PANEL_HOVER : PANEL);
        if (active) DrawRectangleLinesEx(bounds, 1.0f, CC_GOLD);
        (void)DrawEconomicGoodIcon(economic_goods, (CcGood)good,
            (Rectangle){bounds.x + 4, bounds.y + 4, 32, 32}, WHITE);
        int size = AdventureTextSize(12);
        const char *name = CcGoodName((CcGood)good);
        while (size > 10 && CcOverlayMeasureText(name, size) > (int)bounds.width - 47) --size;
        CcOverlayDrawText(name, (int)bounds.x + 42, (int)bounds.y + 4,
            size, active ? CC_GOLD : INK);
        CcOverlayDrawText(TextFormat("%d units", quantity),
            (int)bounds.x + 42, (int)bounds.y + 23, 12, quantity > 0 ? MUTED : DANGER);
    }
    int x = (int)(panel.x + panel.width * 0.52f), y = (int)panel.y + 134;
    (void)DrawEconomicGoodIcon(economic_goods, quote.command.good,
        (Rectangle){(float)x, (float)y - 2, 40, 40}, WHITE);
    bool readable_canvas = panel.width >= 700.0f && panel.height >= 520.0f;
    int title_size = AdventureTextSize(readable_canvas ? 32 : 18);
    const char *title = TextFormat("%s: %d %s", modes[local->trade_mode],
        abs(quote.command.amount), CcGoodName(quote.command.good));
    int summary_width = (int)(panel.width * 0.48f) - 28;
    while (title_size > 16 && CcOverlayMeasureBodyText(title, title_size) > summary_width - 48) --title_size;
    if (readable_canvas) CcOverlayDrawBodyText(title, x + 48, y + 5, title_size, INK);
    else CcOverlayDrawText(title, x + 48, y + 8, title_size, INK);
    const char *quote_line = TextFormat("%s %" PRId64 " crowns",
        local->trade_mode == 0 ? "Total cost" : "Sale value", displayed_total);
    int quote_size = AdventureTextSize(readable_canvas ? 36 : 17);
    while (quote_size > 18 && CcOverlayMeasureBodyText(quote_line, quote_size) > summary_width) --quote_size;
    if (readable_canvas) CcOverlayDrawBodyText(quote_line, x, y + 47, quote_size, CC_GOLD);
    else CcOverlayDrawText(quote_line, x, y + 34, quote_size, CC_GOLD);
    const char *purse_line = TextFormat("Purse %" PRId64 "  |  Cargo %d/%d", sim->player.coins,
        CcPlayerCargoUsed(&sim->player), sim->player.cargo_capacity);
    int purse_size = AdventureTextSize(readable_canvas ? 20 : 14);
    if (readable_canvas) CcOverlayDrawBodyText(purse_line, x, y + 84, purse_size, MUTED);
    else CcOverlayDrawText(purse_line, x, y + 66, purse_size, MUTED);
    if (local->trade_mode != 2) {
        AdventureButton(AdventureQuantityBounds(panel, 0), "-", local->trade_quantity > 1, false);
        AdventureButton(AdventureQuantityBounds(panel, 1), "+", local->trade_quantity < 999, false);
        if (readable_canvas) CcOverlayDrawBodyText("Quantity: - / + keys", x + 144,
            (int)panel.y + 294, AdventureTextSize(16), MUTED);
        else CcOverlayDrawText("Quantity: - / + keys", x + 144,
            (int)panel.y + 244, AdventureTextSize(12), MUTED);
    } else {
        CcOverlayDrawText(TextFormat("Promise reward +%" PRId64 " crowns", quote.reward), x,
            (int)panel.y + (readable_canvas ? 269 : 244), AdventureTextSize(14), TEAL);
    }
    char confirm_touch_label[384];
    AdventureTradeConfirmTouchLabel(&quote, local->trade_mode,
        confirm_touch_label, sizeof(confirm_touch_label));
    AdventureButtonWithTouchLabel(AdventureConfirmBounds(panel),
        TextFormat("%s  Enter", modes[local->trade_mode]),
        confirm_touch_label, quote.available, false);
    CcOverlayDrawText(supply_buyer ?
        TextFormat("Wanted %d  |  Carriage %d",
            supply_offer.remaining, sim->player.cargo[quote.command.good]) :
        TextFormat("Shop %d  |  Carriage %d",
            place->stock[quote.command.good], sim->player.cargo[quote.command.good]),
        x, (int)panel.y + (readable_canvas ? 390 : 278), AdventureTextSize(readable_canvas ? 15 : 12), TEAL);
    (void)AdventureWrap(quote.reason[0] != '\0' ? quote.reason : supply_detail,
        x, (int)panel.y + (readable_canvas ? 415 : 360),
        summary_width, AdventureTextSize(readable_canvas ? 15 : 13), MUTED);
    DrawLine((int)panel.x + 24, (int)(panel.y + panel.height) - (readable_canvas ? 106 : 89),
        (int)(panel.x + panel.width) - 24,
        (int)(panel.y + panel.height) - (readable_canvas ? 106 : 89), Fade(TEAL, 0.5f));
    (void)AdventureWrap(local->receipt[0] != '\0' ? local->receipt :
        "Buy and sell this shop's goods with your carriage.",
        (int)panel.x + 24, (int)(panel.y + panel.height) - (readable_canvas ? 86 : 72),
        (int)panel.width - 48, AdventureTextSize(readable_canvas ? 36 : 14), INK);
}

static bool HandleAdventureTrade(CcJournal *journal, CcSim *sim, LocalState *local,
    ClientView *view, char *message, size_t capacity)
{
    if (*view != VIEW_TRADE || ClientKeyPressed(KEY_F5)) return false;
    Rectangle panel = AdventurePanel();
    const CcLocalShop *shop = LocalActiveShop(sim, local);
    if (ClientKeyPressed(KEY_ESCAPE) || AdventureHit(AdventureClose(panel))) { *view = VIEW_LOCAL; return true; }
    const CcSettlement *town = CcSimSettlement(sim, sim->player.location_id);
    if (town == NULL || CcSettlementIsAbandoned(town) || shop == NULL) return true;
    if (!AdventureShopModeAvailable(shop, local->trade_mode)) {
        local->trade_mode = 0;
        local->trade_good = LocalShopFirstGood(shop);
        local->trade_quote_presented = false;
    }
    bool changed = false;
    for (int i = 0; i < 6; ++i) {
        if (AdventureShopModeAvailable(shop, i) &&
            (AdventureHit(AdventureShopTab(panel, shop, i)) ||
             ClientKeyPressed(KEY_ONE + AdventureShopTabIndex(shop, i)))) {
            local->trade_mode = i; changed = true;
            if (i == 0 || i == 1) {
                if (!AdventureShopGoodVisible(sim, shop, i,
                        local->trade_good))
                    local->trade_good = LocalShopFirstGood(shop);
            }
        }
    }
    if (local->trade_mode == 5) {
        for (int i = 0; i < CC_ROAD_COUNCIL_ROWS; ++i)
            if (AdventureHit(CouncilRowButton(panel, i))) local->trade_good = (CcGood)i;
        return true;
    }
    if (local->trade_mode == 4) {
        bool stop = AdventureHit(GrainFundButton(panel, true));
        if (!changed && (stop || AdventureHit(GrainFundButton(panel, false)) || ClientKeyPressed(KEY_ENTER))) {
            const CcGrainSupply *supply = CcSimGrainSupply(sim, sim->player.location_id);
            if (local->trade_quote_presented && local->trade_presented_command.kind == CC_COMMAND_FUND_GRAIN_SUPPLY &&
                local->trade_presented_command.target_id == sim->player.location_id &&
                (!stop || local->trade_presented_reward == supply->purse)) {
                CcCommand command = local->trade_presented_command;
                command.amount = stop ? -1 : 0;
                (void)ApplyCommand(journal, sim, command, message, capacity);
                (void)snprintf(local->receipt, sizeof(local->receipt), "%s", message);
            }
        }
        return true;
    }
    if (local->trade_mode == 3) {
        Rectangle button = {panel.x + 28, panel.y + panel.height - 100, panel.width - 56, 34};
        if (!changed && (AdventureHit(button) || ClientKeyPressed(KEY_ENTER))) {
            CcBakerySupportPlan plan = CcSimBakerySupportPlan(sim, sim->player.location_id);
            if (local->trade_quote_presented && local->trade_presented_command.kind == CC_COMMAND_SUPPORT_BAKERY &&
                local->trade_presented_total == plan.coins && local->trade_presented_command.amount == plan.building_days) {
                (void)ApplyCommand(journal, sim, local->trade_presented_command, message, capacity);
                (void)snprintf(local->receipt, sizeof(local->receipt), "%s", message);
            } else {
                (void)snprintf(local->receipt, sizeof(local->receipt), "Read the current bakery offer, then confirm again.");
            }
        }
        return true;
    }
    if (local->trade_mode != 2) {
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            if (AdventureShopGoodVisible(sim, shop, local->trade_mode,
                    (CcGood)good) &&
                AdventureHit(AdventureShopGoodBounds(panel, sim, shop,
                    local->trade_mode, (CcGood)good))) {
                local->trade_good = (CcGood)good; changed = true;
            }
        }
        if (ClientKeyPressed(KEY_UP) || ClientKeyPressed(KEY_DOWN)) {
            local->trade_good = LocalShopNextGood(sim, shop, local->trade_mode,
                local->trade_good,
                ClientKeyPressed(KEY_UP) ? -1 : 1); changed = true;
        }
        int step = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 10 : 1;
        if (AdventureHit(AdventureQuantityBounds(panel, 0)) || ClientKeyPressed(KEY_MINUS)) {
            local->trade_quantity -= step; changed = true;
        }
        if (AdventureHit(AdventureQuantityBounds(panel, 1)) || ClientKeyPressed(KEY_EQUAL)) {
            local->trade_quantity += step; changed = true;
        }
        if (local->trade_quantity < 1) local->trade_quantity = 1;
        if (local->trade_quantity > 999) local->trade_quantity = 999;
    }
    if (changed) local->trade_confirmed = false;
    if (!changed && (AdventureHit(AdventureConfirmBounds(panel)) || ClientKeyPressed(KEY_ENTER))) {
        AdventureQuote quote = AdventureTradeQuote(sim, local);
        bool quote_matches = local->trade_quote_presented &&
            local->trade_presented_shop_building_index == local->shop_building_index &&
            local->trade_presented_command.kind == quote.command.kind &&
            local->trade_presented_command.target_id == quote.command.target_id &&
            local->trade_presented_command.good == quote.command.good &&
            local->trade_presented_command.amount == quote.command.amount &&
            local->trade_presented_total == quote.total && local->trade_presented_reward == quote.reward &&
            local->trade_presented_promise_id == quote.promise_id;
        if (quote.available && !quote_matches) {
            (void)snprintf(local->receipt, sizeof(local->receipt), "The quote changed. Review the price and confirm again.");
        } else if (quote.available) {
            if (ApplyCommand(journal, sim, quote.command, message, capacity)) {
                local->trade_confirmed = true;
                if (local->trade_mode == 2) {
                    (void)snprintf(local->receipt, sizeof(local->receipt), "Delivery complete. %s", message);
                } else (void)snprintf(local->receipt, sizeof(local->receipt), "%s", message);
                local->receipt_serial += 1U;
                const CcLocalPlaceProfile *place = CcLocalPlaceProfileForSettlement(
                    CcSimSettlement(sim, sim->player.location_id));
                int32_t quantity = quote.command.amount < 0 ? -quote.command.amount : quote.command.amount;
                char words[CC_SPEECH_TEXT_CAPACITY];
                (void)snprintf(words, sizeof(words), local->trade_mode == 0 ?
                    "The %d %s are yours. Thank you." : "I have the %d %s. Thank you.", quantity, CcGoodName(quote.command.good));
                CcSpeech speech;
                if (CcSpeechTrade(sim, shop != NULL ? shop->keeper_name : place->keeper_name,
                    quote.command.good, quantity,
                    quote.total, local->trade_mode, words, &speech)) ClientSaySpeech(&speech);
            } else (void)snprintf(local->receipt, sizeof(local->receipt), "%s", message);
        } else (void)snprintf(message, capacity, "%s", quote.reason);
    }
    return true;
}

static Rectangle AdventureBookPageBounds(Rectangle panel, int32_t page)
{
    return AdventureTab(panel,page,6);
}

static bool AdventureBookCompact(Rectangle panel)
{
    return panel.width < 520.0f || panel.height < 480.0f;
}

static Rectangle AdventureBookMineStepBounds(Rectangle panel, int32_t step)
{
    float width=(panel.width-64.0f)/2.0f;
    return (Rectangle){panel.x+24.0f+(float)step*(width+16.0f),
        panel.y+panel.height-54.0f,width,36.0f};
}

static void DrawAdventureBookHead(Rectangle panel, const char *page_name,
                                  int32_t active_page)
{
    bool compact=AdventureBookCompact(panel);
    AdventurePanelTitle(panel,compact ? "Book" : "Company Book");
    const char *pages[]={compact ? "Ahead" : "Ahead  1",
        compact ? "People" : "People  2",
        compact ? "Mine" : "Mine notes  3",
        compact ? "Court" : "Court  4",
        compact ? "Dates" : "Calendar  5",
        compact ? "Wants" : "Requests  6"};
    for (int32_t page=0;page<6;++page)
        AdventureButton(AdventureBookPageBounds(panel,page),pages[page],true,
            page==active_page);
    ClientTouchRecordText(page_name);
}

static void DrawAdventureAhead(const CcSim *sim, const LocalState *local)
{
    Rectangle panel = AdventurePanel();
    DrawAdventureBookHead(panel, "Ahead",0);
    (void)local;
    int x = (int)panel.x + 28, y = (int)panel.y + 128;
    int width = (int)panel.width - 56;
    int size = AdventureTextSize(17);
    const CcSituation *promise = CcSimAcceptedSituation(sim);
    if (promise != NULL) {
        char next[192] = "";
        SituationNextAction(sim, promise, next, sizeof(next));
        y += AdventureWrap(TextFormat("%s. Due day %d.",
            SituationTitle(promise->kind), promise->deadline_day), x, y, width, size + 2, INK) + 16;
        y += AdventureWrap(TextFormat("Next: %s", next), x, y, width, size, TEAL) + 20;
    } else {
        y += AdventureWrap("Nothing is promised just now. Read the board or listen in the tavern.",
                           x, y, width, size, MUTED) + 18;
    }
    const CcTreasure *book = CcSimDeepWyrmProphecy(sim);
    if (book != NULL && !book->destroyed && book->owner_id == sim->player.id) {
        y += AdventureWrap("The DM points you toward Gloamgate. Carry the prophecy there.",
                           x, y, width, size, CC_GOLD) + 12;
        if (CcSimCanDeliverProphecy(sim))
            AdventureButton((Rectangle){panel.x + 28.0f, panel.y + panel.height - 112.0f, (float)width, 42.0f},
                "Deliver the DM's warning  /  E", true, false);
    }
    const CcEvent *road_receipt = NULL;
    for (int32_t offset = 0; offset < sim->event_count; ++offset) {
        const CcEvent *entry = CcSimRecentEvent(sim, offset);
        if (entry != NULL && entry->kind == CC_EVENT_ENCOUNTER_WITHDRAWN) {
            road_receipt = entry;
            break;
        }
    }
    if (road_receipt != NULL) {
        CcOverlayDrawText("ROAD RECEIPT", x, y, AdventureTextSize(12), TEAL);
        y += 25;
        y += AdventureWrap(TextFormat("Day %d: %s",
            road_receipt->day, road_receipt->text),
            x, y, width, size - 2, INK) + 16;
    }
    CcOverlayDrawText("THE BOARD", x, y, AdventureTextSize(12), CC_GOLD);
    y += 26;
    int shown = 0;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *s = &sim->situations[i];
        if (s->status != CC_SITUATION_ACTIVE ||
            s->id == sim->player.accepted_situation_id) continue;
        const CcNotice *notice = CcSimSituationNotice(sim, s->id);
        if (notice == NULL) continue;
        if (shown++ < 3 && notice->text[0] != '\0')
            y += AdventureWrap(notice->text, x, y, width, size - 2, INK) + 10;
    }
    if (shown == 0)
        (void)AdventureWrap("Nothing new has been posted.", x, y, width, size - 2, MUTED);
}

static void DrawAdventurePeople(const CcSim *sim, const LocalState *local)
{
    Rectangle panel = AdventurePanel();
    DrawAdventureBookHead(panel, "People",1);
    int x = (int)panel.x + 28, y = (int)panel.y + 128;
    (void)local;
    int width = (int)panel.width - 56;
    int size = AdventureTextSize(15);
    int32_t shown = 0;
    for (int32_t i = 0; i < sim->character_count && shown < 5; ++i) {
        const CcCharacter *person = &sim->characters[i];
        if (!AdventureKnownPerson(sim, person)) continue;
        char label[128];
        AdventurePersonLabel(sim, person, label, sizeof(label));
        CcOverlayDrawText(label, x, y, size + 2, INK);
        y += 30;
        const CcSettlement *where = CcSimSettlement(sim, person->current_settlement_id);
        (void)AdventureWrap(TextFormat("%s, %s. Born under the %s.",
            where != NULL ? where->name : "the road",
            CcCharacterActivityName(person->activity), CcZodiacName(CcCalendar(person->birth_day).sign)), x + 4, y, width - 4, size - 3, MUTED);
        y += 34;
        shown += 1;
    }
    if (shown == 0)
        (void)AdventureWrap("No one you know has been named yet. Follow the stories.", x, y, width, size, MUTED);
}

static void AdventureMineReportAccount(const CcSim *sim,char *account,
                                       size_t capacity)
{
    const CcCharacter *recipient=CcSimCharacter(sim,sim->mine.report_recipient_id);
    if (sim->mine.report_kind == CC_MINE_RETURN_HAUL)
        (void)snprintf(account,capacity,
            "Crownless Company reported %d %s from the tracked partial haul to %s",
            sim->mine.report_quantity,CcGoodName((CcGood)sim->mine.report_good),
            recipient != NULL ? recipient->name : "Jory");
    else (void)snprintf(account,capacity,
        "Crownless Company shared the attributed route account with %s",
        recipient != NULL ? recipient->name : "Jory");
}

static void DrawAdventureMineNotes(const CcSim *sim, const LocalState *local)
{
    Rectangle panel=AdventurePanel();
    DrawAdventureBookHead(panel,"Mine notes",2);
    bool compact=AdventureBookCompact(panel);
    int section=compact ? local->book_offset : -1;
    int x=(int)panel.x+28,y=(int)panel.y+128;
    int width=(int)panel.width-56,size=AdventureTextSize(15);
    CcOverlayDrawText("LOW SILVER PIT",x,y,size+2,INK); y+=32;
    if (section < 0 || section == 0) {
      if (sim->mine.lead_event_id != 0U) {
        const CcCharacter *source=CcSimCharacter(sim,sim->mine.lead_source_id);
        const CcSettlement *place=CcSimSettlement(sim,sim->mine.lead_source_id);
        y+=AdventureWrap(TextFormat("Lead, day %d. Source: %s. Low Silver Pit is a turnout on the Alderwatch-Silverwick road. Workers' records inside hold route guidance.",
            sim->mine.lead_day,
            source != NULL ? source->name : place != NULL ? place->name : "dated shift record"),
            x,y,width,size,TEAL)+16;
      } else {
        y+=AdventureWrap("Lead: ask Jory about his known concern, or read Silverwick's dated shift record.",
            x,y,width,size,MUTED)+16;
      }
    }
    if (section < 0 || section == 1) {
      if (sim->mine.survey_event_id != 0U) {
        char date[48];
        if (sim->mine.survey_observed_day > 0)
            (void)snprintf(date,sizeof(date),"observed day %d",sim->mine.survey_observed_day);
        else
            (void)snprintf(date,sizeof(date),"earlier observation date unknown");
        y+=AdventureWrap(TextFormat("Workers' records, read day %d; %s. Document claim: The western store passage goes around the barred middle passage. The stair to Lamp Hall is marked blocked.",
            sim->mine.survey_read_day,date),x,y,width,size,INK)+16;
      } else if (sim->mine.bypass_event_id != 0U) {
        y+=AdventureWrap(TextFormat("Company observation, day %d: the western store passage goes around the barred middle passage.",
            sim->mine.bypass_day),x,y,width,size,INK)+16;
      } else {
        y+=AdventureWrap("Survey: no attributed document or company route observation recorded.",
            x,y,width,size,MUTED)+16;
      }
    }
    if (section < 0 || section == 2) {
      if (sim->mine.report_event_id != 0U) {
        char account[160];
        AdventureMineReportAccount(sim,account,sizeof(account));
        const char *outcome=sim->mine.reported_encounter_outcome == CC_MINE_ENCOUNTER_BARGAINED ?
            "bargained with haulers" :
            sim->mine.reported_encounter_outcome == CC_MINE_ENCOUNTER_CONTESTED ?
            "defeated haulers" :
            sim->mine.reported_encounter_outcome == CC_MINE_ENCOUNTER_BROKEN_CONTACT ?
            "broke contact" : "hauler encounter unsettled";
        y+=AdventureWrap(TextFormat("Return, day %d: %s; perceived outcome: %s.",
            sim->mine.report_day,account,outcome),
            x,y,width,size,CC_GOLD)+12;
        (void)AdventureWrap(sim->mine.report_kind == CC_MINE_RETURN_HAUL ?
            "Jory: 'This proves the turnout can still yield.'" :
            "Jory: 'This gives the next company a fair path in.'",
            x,y,width,size,TEAL);
      } else {
        (void)AdventureWrap("Return: Jory still needs a tracked mine load or an attributed route account.",
            x,y,width,size,MUTED);
      }
    }
    if (compact) {
        AdventureButton(AdventureBookMineStepBounds(panel,0),"Earlier note",section>0,false);
        AdventureButton(AdventureBookMineStepBounds(panel,1),"Later note",section<2,false);
    }
}

#include "client/cc_scriven_book.inc"
#include "client/cc_wants_book.inc"

static void DrawAdventureBook(const CcSim *sim, const LocalState *local)
{
    if (local->book_page == 5) { DrawAdventureWants(sim, local); return; }
    if (local->book_page == 4) { DrawAdventureCalendar(sim, local); return; }
    if (local->book_page == 1) { DrawAdventurePeople(sim, local); return; }
    if (local->book_page == 2) { DrawAdventureMineNotes(sim,local); return; }
    if (local->book_page == 3) { DrawAdventureCourtNotes(sim, local); return; }
    DrawAdventureAhead(sim, local);
}

static bool HandleAdventureBook(CcJournal *journal, CcSim *sim, LocalState *local, ClientView *view, ClientView *return_view, char *message, size_t message_capacity)
{
    if (*view != VIEW_LEDGER || ClientKeyPressed(KEY_F5)) return false;
    Rectangle panel = AdventurePanel();
    if (ClientKeyPressed(KEY_ESCAPE) || ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_book : KEY_B) || ClientKeyPressed(KEY_TAB) ||
        AdventureHit(AdventureClose(panel))) { *view = SafeOverlayReturnView(*return_view); return true; }
    for (int32_t page=0;page<6;++page) {
        if (AdventureHit(AdventureBookPageBounds(panel,page))) {
            local->book_page=page;
            local->book_offset=0;
            return true;
        }
    }
    if (local->book_page == 2 && AdventureBookCompact(panel) &&
        (ClientKeyPressed(KEY_UP) || AdventureHit(AdventureBookMineStepBounds(panel,0)))) {
        if (local->book_offset > 0) local->book_offset-=1;
    } else if (local->book_page == 2 && AdventureBookCompact(panel) &&
        (ClientKeyPressed(KEY_DOWN) || AdventureHit(AdventureBookMineStepBounds(panel,1)))) {
        if (local->book_offset < 2) local->book_offset+=1;
    } else if (ClientKeyPressed(KEY_LEFT)) {
        local->book_page=(local->book_page+5)%6;
        local->book_offset=0;
    } else if (ClientKeyPressed(KEY_RIGHT)) {
        local->book_page=(local->book_page+1)%6;
        local->book_offset=0;
    } else if (ClientKeyPressed(KEY_ONE)) {
        local->book_page = 0;
        local->book_offset=0;
    } else if (ClientKeyPressed(KEY_TWO)) {
        local->book_page = 1;
        local->book_offset=0;
    } else if (ClientKeyPressed(KEY_THREE)) {
        local->book_page = 2;
        local->book_offset=0;
    }
    if (ClientKeyPressed(KEY_SIX)) { local->book_page = 5; local->book_offset = 0; }
    if (local->book_page == 5) HandleAdventureWants(journal, sim, local, message, message_capacity);
    if (ClientKeyPressed(KEY_FIVE)) { local->book_page = 4; local->book_offset = 0; }
    if (local->book_page == 4) HandleAdventureCalendar(journal, sim, local, message, message_capacity);
    if (ClientKeyPressed(KEY_FOUR)) { local->book_page = 3; local->book_offset = 0; }
    if (local->book_page == 3 &&
        (ClientKeyPressed(KEY_UP) || AdventureHit(AdventureBookMineStepBounds(panel, 0)))) local->book_offset = 1;
    if (local->book_page == 3 &&
        (ClientKeyPressed(KEY_DOWN) || AdventureHit(AdventureBookMineStepBounds(panel, 1)))) local->book_offset = 0;
    const CcTreasure *book = CcSimDeepWyrmProphecy(sim);
    if (local->book_page == 0 && book != NULL && !book->destroyed &&
        book->owner_id == sim->player.id && CcSimCanDeliverProphecy(sim) &&
        (ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_interact : KEY_E) ||
         AdventureHit((Rectangle){panel.x + 28, panel.y + panel.height - 112, panel.width - 56, 42}))) {
        CcCommand deliver = {.kind = CC_COMMAND_DELIVER_PROPHECY, .target_id = book->id};
        (void)ApplyCommand(journal, sim, deliver, message, message_capacity);
    }
    return true;
}

static Rectangle AdventureMenuItem(Rectangle panel, int index)
{
    return (Rectangle){panel.x + 24.0f, panel.y + 78.0f + (float)index * 56.0f, panel.width * 0.45f, 46.0f};
}

static void DrawAdventurePause(const LocalState *local)
{
    Rectangle panel = AdventurePanel();
    AdventurePanelTitle(panel, CcCoopClientActive() ? "Company menu" : "Paused");
    AdventureButton(AdventureMenuItem(panel, 0), "Resume  Esc", true, false);
    AdventureButton(AdventureMenuItem(panel, 1), "Save game  F5", true, false);
    AdventureButton(AdventureMenuItem(panel, 2), TextFormat("Company Book  %c", adventure_preferences != NULL ? adventure_preferences->key_book : 'B'), true, false);
    if (adventure_preferences != NULL) {
        AdventureButton(AdventureMenuItem(panel, 3), adventure_preferences->reduced_motion ?
            "Motion: gentle  F4" : "Motion: full  F4", true, false);
        AdventureButton(AdventureMenuItem(panel, 4), TextFormat("Text size: %s  T", adventure_preferences->text_size == 0 ?
            "standard" : adventure_preferences->text_size == 1 ? "large" : "largest"), true, false);
        AdventureButton(AdventureMenuItem(panel, 5), adventure_preferences->focus_hints ? "Target hints: shown  H" : "Target hints: focus only  H", true, false);
        AdventureButton(AdventureMenuItem(panel, 6), adventure_preferences->audio_mode == 0 ? "Sound: full  F6" :
            adventure_preferences->audio_mode == 1 ? "Sound: effects  F6" : "Sound: muted  F6", true, false);
        AdventureButton(AdventureMenuItem(panel, 7), "Settings & controls  S", true, false);
    }
    int x = (int)(panel.x + panel.width * 0.53f), y = (int)panel.y + 84;
    CcOverlayDrawText(CcCoopClientActive() ? "The host keeps company time" : "At your own pace", x, y, AdventureTextSize(20), CC_GOLD);
    y += 46;
    (void)AdventureWrap(TextFormat("Choose an object card or tap the object to approach and act. Browse more cards with the arrows. Click the ground to walk. Tap a road to turn, the carriage to stop or drive, and a camp to rest.\n\n%c selects the next visible target. Shift+%c goes back. %c uses the selected action.\n\n%c opens the Company Book. %c opens promises. M opens the map case.\n\nEsc cancels an approach or returns to the scene. F5 saves.",
        adventure_preferences != NULL ? adventure_preferences->key_target_next : 'E',
        adventure_preferences != NULL ? adventure_preferences->key_target_next : 'E',
        adventure_preferences != NULL ? adventure_preferences->key_interact : 'F',
        adventure_preferences != NULL ? adventure_preferences->key_book : 'B',
        adventure_preferences != NULL ? adventure_preferences->key_promises : 'Q'),
        x, y, (int)(panel.width * 0.47f - 30.0f), AdventureTextSize(14), INK);
    (void)local;
}

static bool HandleAdventurePause(LocalState *local, ClientView *view, ClientView *return_view)
{
    if (*view != VIEW_PAUSE) return false;
    Rectangle panel = AdventurePanel();
    if (ClientKeyPressed(KEY_ESCAPE) || AdventureHit(AdventureClose(panel)) || AdventureHit(AdventureMenuItem(panel, 0))) {
        *view = local->pause_return_view; return true;
    }
    if (AdventureHit(AdventureMenuItem(panel, 1))) { local->request_save = true; return false; }
    if (ClientKeyPressed(KEY_F5)) return false;
    if (ClientKeyPressed(adventure_preferences != NULL ? adventure_preferences->key_book : KEY_B) || AdventureHit(AdventureMenuItem(panel, 2))) {
        *return_view = VIEW_PAUSE; *view = VIEW_LEDGER; return true;
    }
    if (adventure_preferences != NULL) {
        if (AdventureHit(AdventureMenuItem(panel, 3))) {
            adventure_preferences->reduced_motion = !adventure_preferences->reduced_motion;
            adventure_preferences_dirty = true;
        }
        if (AdventureHit(AdventureMenuItem(panel, 4)) || ClientKeyPressed(KEY_T)) {
            adventure_preferences->text_size = (adventure_preferences->text_size + 1) % 3;
            adventure_preferences_dirty = true;
        }
        if (AdventureHit(AdventureMenuItem(panel, 5)) || ClientKeyPressed(KEY_H)) {
            adventure_preferences->focus_hints = !adventure_preferences->focus_hints;
            adventure_preferences_dirty = true;
        }
        if (AdventureHit(AdventureMenuItem(panel, 6))) {
            adventure_preferences->audio_mode = (adventure_preferences->audio_mode + 1) % 3;
            adventure_preferences_dirty = true;
        }
        if (ClientKeyPressed(KEY_S) || AdventureHit(AdventureMenuItem(panel, 7))) {
            local->settings_requested = true;
            return true;
        }
    }
    return true;
}

static Rectangle LocalViewportBounds(void);

static void WorldActionLabelLayout(const LocalState *local,
                                   const CcInteractionTarget *focus,
                                   const CcInteractionTarget *hover,
                                   Rectangle labels[CC_INTERACTION_CAPACITY])
{
    CcInteractionLabelRect placed[CC_INTERACTION_CAPACITY] = {0};
    Rectangle scene = LocalViewportBounds();
    CcInteractionLabelRect viewport = {
        scene.x + 12, scene.y + 12, scene.width - 24, scene.height - 24};
    int32_t count = 0;
    float height = (float)AdventureTextSize(14) + 14.0f;
    for (int32_t i = 0; i < CC_INTERACTION_CAPACITY; ++i) labels[i] = (Rectangle){0};
    /* Place only the labels that are currently visible. */
    for (int pass = 0; pass < 2; ++pass) {
        for (int32_t i = 0; i < local->interactions.count; ++i) {
            const CcInteractionTarget *target = &local->interactions.targets[i];
            if (!target->visible ||
                (target->key.kind == CC_INTERACTION_ACTION) != (pass == 0) ||
                (target != hover &&
                 (target->key.kind == CC_INTERACTION_ACTION || target != focus)))
                continue;
            float width = (float)CcOverlayMeasureText(target->name, AdventureTextSize(14)) + 20;
            CcInteractionLabelRect desired = {
                (target->left + target->right - width) * 0.5f,
                target->top - height - 5, width, height};
            CcInteractionLabelRect label;
            if (CcInteractionPlaceLabel(desired, viewport, placed, count, &label)) {
                placed[count++] = label;
                labels[i] = (Rectangle){label.x, label.y, label.width, label.height};
            }
        }
    }
}

static const CcInteractionTarget *WorldActionPointerTarget(const LocalState *local, Vector2 mouse)
{
    return CcInteractionPick(&local->interactions, mouse.x, mouse.y);
}

static void DrawAdventureFocus(const CcSim *sim, const LocalState *local, ClientView view,
    int32_t selected_route, int32_t selected_situation)
{
    const CcInteractionTarget *focus = AdventureFocus(local);
    Vector2 mouse = ClientPointerPosition();
    const CcInteractionTarget *hover = WorldActionPointerTarget(local, mouse);
    ContextActionSet cards = BuildContextActions(sim, local, view, selected_route, selected_situation);
    int32_t first = ContextCardFirst(local, &cards);
    int32_t shown = ContextCardCount(&cards, first);
    for (int32_t card = first; card < first + shown; ++card) {
        if (!CheckCollisionPointRec(mouse, ContextActionBounds(card - first, shown, cards.combat))) continue;
        for (int32_t i = 0; i < local->interactions.count; ++i) {
            const CcInteractionTarget *target = &local->interactions.targets[i];
            if (CcInteractionKeyEqual(target->key, cards.items[card].target) ||
                (target->key.kind == CC_INTERACTION_ACTION && target->action == (int32_t)cards.items[card].kind &&
                 target->amount == cards.items[card].amount)) hover = target;
        }
    }
    if (hover != NULL) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    else SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    bool hints = adventure_preferences == NULL || adventure_preferences->focus_hints;
    Rectangle labels[CC_INTERACTION_CAPACITY] = {0};
    WorldActionLabelLayout(local, focus, hover, labels);
    for (int32_t i = 0; i < local->interactions.count; ++i) {
        const CcInteractionTarget *target = &local->interactions.targets[i];
        if (!target->visible) continue;
        bool selected = target == hover ||
            (target->key.kind != CC_INTERACTION_ACTION && target == focus);
        float x = (target->left + target->right) * 0.5f;
        if (selected) {
            Rectangle outline = {target->left, target->top, target->right - target->left, target->bottom - target->top};
            DrawRectangleRoundedLinesEx(outline, 0.12f, 4, 2.0f, target->available ? CC_GOLD : MUTED);
        } else if (hints) DrawCircle((int)x, (int)target->top - 6, 3, Fade(CC_GOLD, 0.85f));
        if (selected) {
            Rectangle label = labels[i];
            if (label.width <= 0) continue;
            DrawRectangleRec(label, PANEL_DEEP);
            CcOverlayDrawText(target->name, (int)label.x + 10, (int)label.y + 6, AdventureTextSize(14), INK);
        }
    }

}

static void DrawAdventureConversation(const CcSim *sim, const LocalState *local)
{
    CcSpeech speech;
    bool spoken = ClientConversationSpeech(sim, local, &speech);
    const CcSpeech *playing = CcAudioCurrentSpeech();
    if (playing != NULL) { speech = *playing; spoken = true; }
    const char *name = spoken ? speech.speaker : local->conversation_name;
    const char *line = spoken ? speech.text : local->conversation_line;
    ClientTouchHeading(name, line);
    float width = fminf(720.0f, (float)GetScreenWidth() - 80.0f);
    float height = 68.0f + (float)AdventureText(line, 0, 0, (int)width - 44,
        AdventureTextSize(20), INK, false);
    Rectangle bubble = {((float)GetScreenWidth() - width) * 0.5f,
        (float)GetScreenHeight() - 108.0f - height, width, height};
    DrawPanel(bubble, Fade(PANEL_DEEP, 0.95f));
    CcOverlayDrawText(name, (int)bubble.x + 22, (int)bubble.y + 16, AdventureTextSize(20), CC_GOLD);
    (void)AdventureWrap(line, (int)bubble.x + 22, (int)bubble.y + 50,
        (int)bubble.width - 44, AdventureTextSize(20), INK);
    if (local->conversation_situation_id == 0U && core_conversation.round_shown && core_conversation.round_phase == 0U &&
        core_conversation_speaker == local->conversation_character_id &&
        speech.speaker_id != sim->player.id) {
        const CcSpeech *player = &core_conversation.player_line;
        float player_height = 54.0f + (float)AdventureText(player->text, 0, 0,
            (int)width - 44, AdventureTextSize(16), INK, false);
        Rectangle prior = {bubble.x, bubble.y - player_height - 10.0f, width, player_height};
        DrawPanel(prior, Fade(PANEL_DEEP, 0.95f));
        CcOverlayDrawText(player->speaker, (int)prior.x + 22, (int)prior.y + 12,
            AdventureTextSize(17), CC_GOLD);
        (void)AdventureWrap(player->text, (int)prior.x + 22, (int)prior.y + 38,
            (int)prior.width - 44, AdventureTextSize(16), INK);
    }
}

static Rectangle AdventureNavBounds(int index)
{
    return (Rectangle){(float)GetScreenWidth() - 420.0f + (float)index * 102.0f, 12, 96, 40};
}

static Rectangle AdventurePaceBounds(int32_t pace)
{
    return (Rectangle){(float)GetScreenWidth() - 420.0f + (float)pace * 136.0f, 58, 130, 28};
}

static Rectangle SpeechControlBounds(ClientView view, bool skip)
{
    bool modal = view != VIEW_LOCAL && view != VIEW_CHARACTER;
    if (modal) {
        return (Rectangle){(float)GetScreenWidth() - (skip ? 124.0f : 242.0f),
            12.0f, 110.0f, 32.0f};
    }
    return (Rectangle){skip ? 136.0f : 18.0f, 92.0f, 110.0f, 32.0f};
}

static void DrawAdventureHeader(const CcSim *sim, const LocalState *local,
                                ClientView view)
{
    /* Modal panels provide their own title and details. A second location HUD
       at the same top edge obscures those headings on short desktop windows. */
    if (view != VIEW_LOCAL && view != VIEW_ROADS && view != VIEW_CHARACTER) return;
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    const CcLocalPlaceProfile *profile = CcLocalPlaceProfileForSettlement(place);
    const CcSettlement *destination = CcSimSettlement(sim, sim->journey.destination_id);
    const char *title = local->site_kind != CC_LOCAL_SITE_NONE ? CcLocalSiteName(sim, local->site_kind) :
        local->journey_travel_active && destination != NULL ?
            TextFormat("%s to %s", place != NULL ? place->name : "Road", destination->name) :
        place != NULL ? place->name : "Crownless";
    char fitted_title[256];
    int header_text_width = GetScreenWidth() - 458;
    int title_size = HeaderFitText(title, header_text_width,
        AdventureTextSize(22), 12, fitted_title, sizeof(fitted_title));
    CcOverlayDrawText(fitted_title, 24, 14, title_size, INK);
    CcOverlayDrawText(TextFormat("%d %s  /  %" PRId64 " crowns  /  Cargo %d/%d", CcCalendar(sim->current_day).day_of_sign, CcZodiacName(CcCalendar(sim->current_day).sign),
        sim->player.coins, CcPlayerCargoUsed(&sim->player), sim->player.cargo_capacity),
        24, 43, AdventureTextSize(13), CC_GOLD);
    const CcRoadSite *stop = CcSimJourneyRoadSiteStop(sim);
    const char *detail = local->journey_travel_active && sim->journey.road_waiting_choice ?
        "Stopped at a junction / Choose your road below" :
        stop != NULL && CcSimJourneyRequiresRoadChoice(sim) ?
            TextFormat("Stopped at %s / Travel on or inspect", stop->name) :
        local->journey_travel_active && sim->journey.active ?
            TextFormat("%s / %d%% / ETA %dh / %s", local->carriage_stopped ? "Stopped" :
                CcJourneyPaceName(sim->journey.pace), sim->carriage.progress_milli / 10,
                (CcSimJourneyEtaMinutes(sim) + 59) / 60,
                local->carriage_stopped ? "Choose Travel to resume" :
                sim->journey.phase == CC_JOURNEY_PHASE_RESTING ? "Resting automatically" :
                CcCoopClientActive() && local->shared_travel_sample.valid &&
                    local->shared_travel_sample.since_update >= 1.5 ?
                    "Waiting for shared world" : "Travelling / Space to stop") :
        local->road_choice_active ? "Choose a road card or tap its sign" :
        local->site_travel_active ? "Tap the carriage to stop or drive" :
        local->market_interior && LocalActiveShop(sim, local) != NULL ?
            LocalActiveShop(sim, local)->service_name : profile->identity;
    if (local->journey_travel_active && sim->journey.active &&
        sim->journey.ambush_warned && sim->journey.ambush_pending) {
        const CcBanditGroup *bandits = CcSimBanditGroupOnRoute(
            sim, sim->journey.route_id);
        CcGood demanded_good = CC_GOOD_FOOD;
        int32_t demanded_quantity = 0;
        bool provisions = bandits != NULL && CcSimBanditProvisionDemand(
            sim, sim->journey.route_id,
            &demanded_good, &demanded_quantity);
        detail = provisions ? TextFormat(
            "Scouts: %.24s / likely %d %s or %d crowns / Careful pace may bypass",
            bandits->name, demanded_quantity, CcGoodName(demanded_good),
            sim->journey.bargain_cost) : bandits != NULL ? TextFormat(
            "Scouts: %.24s / likely %d crowns / Careful pace may bypass",
            bandits->name, sim->journey.bargain_cost) :
            "Scouts: armed riders / Careful pace may bypass";
    }
    char town_status[128];
    if (place != NULL &&
        view == VIEW_LOCAL && !local->market_interior &&
        !local->road_choice_active &&
        local->site_kind == CC_LOCAL_SITE_NONE && !local->journey_travel_active &&
        !local->site_travel_active) {
        CcLocalTownStatus(sim, place->id, town_status, sizeof(town_status));
        detail = town_status;
    }
    ClientTouchHeading(title, detail);
    char fitted_detail[256];
    int detail_size = HeaderFitText(detail, header_text_width,
        AdventureTextSize(12), 12, fitted_detail, sizeof(fitted_detail));
    CcOverlayDrawText(fitted_detail, 24, 67, detail_size, MUTED);
}

static void DrawAdventurePromises(const CcSim *sim, int32_t selected)
{
    Rectangle panel = AdventurePanel();
    panel.height -= 60.0f;
    panel.y -= 14.0f;
    AdventurePanelTitle(panel, "Local promises");
    const CcSituation *promise = SelectedActiveSituation(sim, selected);
    int x = (int)panel.x + 28, y = (int)panel.y + 85;
    int width = (int)panel.width - 56;
    if (promise == NULL) {
        (void)AdventureWrap("Listen to the people in town for your next lead.", x, y, width, AdventureTextSize(18), INK);
        return;
    }
    bool accepted = promise->id == sim->player.accepted_situation_id;
    bool offer = CcSimSituationCanAccept(sim, promise);
    CcOverlayDrawText(accepted ? "Accepted promise" :
        offer ? TextFormat("%s — posted day %d",
            SituationPostingKind(promise->kind), promise->created_day) : "Lead",
        x, y, AdventureTextSize(15), accepted ? TEAL : CC_GOLD);
    y += 38;
    CcOverlayDrawText(SituationTitle(promise->kind), x, y, AdventureTextSize(24), INK);
    y += 46;
    char target[96] = "";
    SituationTargetLabel(sim, promise, target, sizeof(target));
    y += AdventureWrap(TextFormat("%s  /  %s", target, promise->affected_name), x, y, width, AdventureTextSize(18), CC_GOLD) + 18;
    if (offer || accepted) {
        if (promise->kind == CC_SITUATION_RELIEF_DELIVERY || promise->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
            CcOverlayDrawText(TextFormat("%d %s  /  Progress %d of %d", promise->quantity,
                CcGoodName(promise->good), promise->progress, promise->quantity), x, y, AdventureTextSize(17), INK);
            y += 38;
        }
        CcOverlayDrawText(accepted ?
            TextFormat("Promised day %d  /  due day %d  /  reward %" PRId64 " crowns",
                promise->created_day, promise->deadline_day, promise->reward) :
            TextFormat("Due day %d  /  reward %" PRId64 " crowns",
                promise->deadline_day, promise->reward),
            x, y, AdventureTextSize(17), TEAL);
        y += 42;
    }
    char next[192] = "";
    SituationNextAction(sim, promise, next, sizeof(next));
    if (offer && !accepted) SituationFindLine(sim, promise, next, sizeof(next));
    (void)AdventureWrap(next, x, y, width, AdventureTextSize(17), MUTED);
}

static void DrawAdventureFeedback(const char *message)
{
    int size = AdventureTextSize(14);
    float width = fminf(800.0f, (float)GetScreenWidth() - 60.0f);
    float x = ((float)GetScreenWidth() - width) * 0.5f;
    float height = 26.0f + (float)AdventureText(message, 0, 0, (int)width - 32, size, INK, false);
    float y = (float)GetScreenHeight() - 104.0f - height;
    DrawPanel((Rectangle){x, y, width, height}, Fade(PANEL_DEEP, 0.96f));
    (void)AdventureWrap(message, (int)x + 16, (int)y + 13, (int)width - 32, size, INK);
}

static Rectangle AdventurePromisesPanel(void)
{
    Rectangle panel = AdventurePanel();
    panel.height -= 60.0f;
    panel.y -= 14.0f;
    return panel;
}
