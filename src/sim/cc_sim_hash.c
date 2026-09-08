#include "sim/cc_sim.h"

#include <stddef.h>

static uint64_t HashU64(uint64_t hash, uint64_t value)
{
    for (int32_t byte = 0; byte < 8; ++byte) {
        hash ^= value & UINT64_C(0xff);
        hash *= UINT64_C(1099511628211);
        value >>= 8U;
    }
    return hash;
}

static uint64_t HashString(uint64_t hash, const char *text)
{
    while (*text != '\0') {
        hash ^= (uint8_t)*text;
        hash *= UINT64_C(1099511628211);
        text += 1;
    }
    return HashU64(hash, 0U);
}

static uint64_t HashGossipVersion(uint64_t hash, const CcGossipVersion *version)
{
    hash = HashU64(hash, version->source_character_id);
    hash = HashU64(hash, (uint64_t)version->retellings);
    hash = HashU64(hash, (uint64_t)version->court_bias);
    hash = HashU64(hash, (uint64_t)version->alarm);
    return HashU64(hash, (uint64_t)version->confidence);
}

uint64_t CcSimHash(const CcSim *sim)
{
    if (sim == NULL) return 0U;
    bool hash_underroad = sim->schema_version >= 20U ||
        (sim->schema_version == 19U && sim->dungeon_count > 0 &&
         sim->dungeons[0].room_count > 0);
    bool hash_social = sim->schema_version >= 20U ||
        (sim->schema_version == 19U && sim->relationship_count > 0);
    bool hash_quest = sim->schema_version >= 21U ||
        (sim->schema_version == 19U &&
         (sim->front_count > 0 || sim->quest_outcome_count > 0));
    bool hash_archives = sim->schema_version >= 22U;
    bool hash_lifecycles = sim->schema_version >= 26U;
    uint64_t hash = UINT64_C(1469598103934665603);
#define HASH_VALUE(value) hash = HashU64(hash, (uint64_t)(value))
    if (sim->schema_version >= 59U) {
        HASH_VALUE(sim->mine.phase); HASH_VALUE(sim->mine.site_id);
        HASH_VALUE(sim->mine.x); HASH_VALUE(sim->mine.y); HASH_VALUE(sim->mine.revision);
        HASH_VALUE(sim->mine.return_speed); HASH_VALUE(sim->mine.light);
        HASH_VALUE(sim->mine.steps); HASH_VALUE(sim->mine.seen);
        HASH_VALUE(sim->mine.bar_open); HASH_VALUE(sim->mine.surveyed);
        for (int32_t good=0;good<CC_GOOD_COUNT;++good) HASH_VALUE(sim->mine.pack[good]);
    }
    if (sim->schema_version >= 73U) {
        for (int i = 0; i < sim->settlement_count; ++i) {
            const CcGrainSupply *g = &sim->grain_supplies[i];
            HASH_VALUE(g->organiser_id); HASH_VALUE(g->supplier_id); HASH_VALUE(g->route_id);
            HASH_VALUE(g->shipment_id); HASH_VALUE(g->purse); HASH_VALUE(g->spent);
            HASH_VALUE(g->ordered); HASH_VALUE(g->delivered); HASH_VALUE(g->lost); HASH_VALUE(g->redirected);
            HASH_VALUE(g->last_dispatch_day); HASH_VALUE(g->last_arrival_day); HASH_VALUE(g->enabled);
        }
    }
    HASH_VALUE(sim->schema_version);
    HASH_VALUE(sim->generator_version);
    HASH_VALUE(sim->world_seed);
    HASH_VALUE(sim->random_state);
    HASH_VALUE(sim->current_day);
    HASH_VALUE(sim->next_entity_serial);
    if (sim->schema_version >= 10U) {
        HASH_VALUE(sim->iron_ledger_reserve);
    }
    HASH_VALUE(sim->kingdom_count);
    HASH_VALUE(sim->settlement_count);
    HASH_VALUE(sim->route_count);
    if (sim->schema_version >= 31U) HASH_VALUE(sim->road_site_count);
    if (sim->schema_version >= 3U) HASH_VALUE(sim->map_count);
    if (sim->schema_version >= 9U) HASH_VALUE(sim->treasure_count);
    HASH_VALUE(sim->faction_count);
    HASH_VALUE(sim->shipment_count);
    if (sim->schema_version >= 38U) {
        HASH_VALUE(sim->royal_carriage_count);
    }
    HASH_VALUE(sim->bandit_count);
    HASH_VALUE(sim->monster_count);
    HASH_VALUE(sim->dungeon_count);
    HASH_VALUE(sim->situation_count);
    HASH_VALUE(sim->event_count);
    HASH_VALUE(sim->event_write_index);
    if (sim->schema_version >= 11U) {
        HASH_VALUE(sim->courier_count);
        for (int32_t first = 0; first < sim->kingdom_count; ++first) {
            for (int32_t second = 0;
                 second < sim->kingdom_count; ++second) {
                HASH_VALUE(sim->diplomacy[first][second]);
                HASH_VALUE(sim->diplomacy_changed_day[first][second]);
            }
        }
    }
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        const CcKingdom *item = &sim->kingdoms[i];
        HASH_VALUE(item->id); hash = HashString(hash, item->name);
        HASH_VALUE(item->color_r); HASH_VALUE(item->color_g); HASH_VALUE(item->color_b);
        HASH_VALUE(item->treasury); HASH_VALUE(item->legitimacy);
        if (sim->schema_version >= 10U) {
            HASH_VALUE(item->iron_ledger_debt);
        }
        if (sim->schema_version >= 34U) {
            HASH_VALUE(item->sanction);
            HASH_VALUE(item->unsanctioned_weeks);
            HASH_VALUE(item->pretender_crises);
            HASH_VALUE(item->anointed);
        }
        if (sim->schema_version >= 35U) {
            HASH_VALUE(item->ruler_character_id);
            HASH_VALUE(item->monastery_patron_id);
            HASH_VALUE(item->anointed_by_character_id);
        }
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *item = &sim->settlements[i];
        HASH_VALUE(item->id); HASH_VALUE(item->kingdom_id);
        hash = HashString(hash, item->name); HASH_VALUE(item->function);
        HASH_VALUE(item->map_x); HASH_VALUE(item->map_y); HASH_VALUE(item->population);
        HASH_VALUE(item->security); HASH_VALUE(item->prosperity); HASH_VALUE(item->hunger);
        if (sim->schema_version >= 45U) {
            HASH_VALUE(item->fire_damage); HASH_VALUE(item->last_fire_day);
        }
        if (sim->schema_version >= 5U) {
            HASH_VALUE(item->size); HASH_VALUE(item->service_mask);
            HASH_VALUE(item->service_project);
            HASH_VALUE(item->service_project_days);
        }
        int32_t good_count = CcGoodCountForSchema(sim->schema_version);
        for (int32_t good = 0; good < good_count; ++good) {
            HASH_VALUE(item->stock[good]); HASH_VALUE(item->reserve_target[good]);
            HASH_VALUE(item->production[good]); HASH_VALUE(item->consumption[good]);
            HASH_VALUE(item->price[good]);
        }
        if (sim->schema_version >= 8U) {
            HASH_VALUE(item->market_coins);
            HASH_VALUE(item->war_chest);
        }
        if (sim->schema_version >= 9U) {
            HASH_VALUE(item->field_yield);
            HASH_VALUE(item->iron_deposit);
            HASH_VALUE(item->gold_seam);
            HASH_VALUE(item->gem_seam);
            HASH_VALUE(item->gold_progress);
            HASH_VALUE(item->gem_progress);
            HASH_VALUE(item->farm_tool_wear);
            HASH_VALUE(item->mine_tool_wear);
            HASH_VALUE(item->smith_tool_wear);
            HASH_VALUE(item->treasure_gold_committed);
            HASH_VALUE(item->treasure_gems_committed);
            HASH_VALUE(item->treasure_work);
        }
        if (sim->schema_version >= 14U) {
            HASH_VALUE(item->cow_adults);
            HASH_VALUE(item->cow_calves);
            HASH_VALUE(item->cow_condition);
            HASH_VALUE(item->cow_hunger);
        }
        if (sim->schema_version >= 32U) {
            HASH_VALUE(item->sheep_adults);
            HASH_VALUE(item->sheep_lambs);
            HASH_VALUE(item->sheep_condition);
            HASH_VALUE(item->sheep_hunger);
        }
        if (sim->schema_version >= 51U) {
            HASH_VALUE(item->pony_adults);
            HASH_VALUE(item->pony_foals);
            HASH_VALUE(item->pony_condition);
            HASH_VALUE(item->pony_hunger);
        }
        if (sim->schema_version >= 34U) {
            HASH_VALUE(item->paper_tool_wear);
        }
        HASH_VALUE(sim->last_shortage_level[i]);
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *item = &sim->routes[i];
        HASH_VALUE(item->id); HASH_VALUE(item->from_id); HASH_VALUE(item->to_id);
        HASH_VALUE(item->travel_days); HASH_VALUE(item->capacity); HASH_VALUE(item->security);
        HASH_VALUE(item->condition); HASH_VALUE(item->closed); HASH_VALUE(item->smuggler_route);
    }
    if (sim->schema_version >= 31U) {
        for (int32_t i = 0; i < sim->road_site_count; ++i) {
            const CcRoadSite *item = &sim->road_sites[i];
            HASH_VALUE(item->id); HASH_VALUE(item->route_id);
            HASH_VALUE(item->home_settlement_id);
            hash = HashString(hash, item->name);
            HASH_VALUE(item->kind); HASH_VALUE(item->input_good);
            HASH_VALUE(item->output_good); HASH_VALUE(item->progress_milli);
            HASH_VALUE(item->side); HASH_VALUE(item->spur_length);
            HASH_VALUE(item->condition); HASH_VALUE(item->blocker);
            HASH_VALUE(item->accessible);
            if (sim->schema_version >= 64U) {
                for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) HASH_VALUE(item->stock[good]);
            }
        }
    }
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *item = &sim->maps[i];
        HASH_VALUE(item->id); HASH_VALUE(item->route_id);
        HASH_VALUE(item->maker_settlement_id); HASH_VALUE(item->owner_id);
        hash = HashString(hash, item->name);
        HASH_VALUE(item->surveyed_day); HASH_VALUE(item->accuracy);
        HASH_VALUE(item->recorded_condition); HASH_VALUE(item->recorded_danger);
        if (sim->schema_version >= 34U) {
            HASH_VALUE(item->recorded_from_kingdom_id);
            HASH_VALUE(item->recorded_to_kingdom_id);
            HASH_VALUE(item->recorded_closed);
            HASH_VALUE(item->recorded_smuggler_route);
        }
        HASH_VALUE(item->ask_price); HASH_VALUE(item->contraband);
    }
    if (sim->schema_version >= 9U) {
        for (int32_t i = 0; i < sim->treasure_count; ++i) {
            const CcTreasure *item = &sim->treasures[i];
            HASH_VALUE(item->id); hash = HashString(hash, item->name);
            HASH_VALUE(item->maker_settlement_id); HASH_VALUE(item->owner_id);
            HASH_VALUE(item->location_id); HASH_VALUE(item->gold_content);
            HASH_VALUE(item->gem_content); HASH_VALUE(item->craft_work);
            HASH_VALUE(item->appraised_value); HASH_VALUE(item->created_day);
            HASH_VALUE(item->destroyed);
        }
    }
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        const CcFaction *item = &sim->factions[i];
        HASH_VALUE(item->id); HASH_VALUE(item->kingdom_id); hash = HashString(hash, item->name);
        HASH_VALUE(item->kind); HASH_VALUE(item->power); HASH_VALUE(item->support);
    }
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *item = &sim->shipments[i];
        HASH_VALUE(item->id); HASH_VALUE(item->origin_id); HASH_VALUE(item->destination_id);
        HASH_VALUE(item->final_destination_id); HASH_VALUE(item->route_id);
        HASH_VALUE(item->good); HASH_VALUE(item->quantity);
        HASH_VALUE(item->departure_day); HASH_VALUE(item->arrival_day); HASH_VALUE(item->status);
    }
    if (sim->schema_version >= 38U) {
        HASH_VALUE(sim->royal_trade_week);
        for (int32_t route = 0; route < sim->route_count; ++route) {
            HASH_VALUE(sim->royal_route_slots_used[route]);
        }
        for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
            const CcRoyalCarriage *item = &sim->royal_carriages[i];
            HASH_VALUE(item->id); HASH_VALUE(item->kingdom_id);
            HASH_VALUE(item->location_id); HASH_VALUE(item->route_id);
            HASH_VALUE(item->destination_id); HASH_VALUE(item->target_id);
            HASH_VALUE(item->active_shipment_id); HASH_VALUE(item->mode);
            HASH_VALUE(item->departure_day); HASH_VALUE(item->arrival_day);
            HASH_VALUE(item->blocked_since_day);
            HASH_VALUE(item->next_dispatch_day);
            HASH_VALUE(item->condition);
            HASH_VALUE(item->trips_completed); HASH_VALUE(item->cargo_losses);
            if (sim->schema_version >= 78U) HASH_VALUE(item->archive_contract);
        }
    }
    if (sim->schema_version >= 11U) {
        for (int32_t i = 0; i < sim->courier_count; ++i) {
            const CcCourier *item = &sim->couriers[i];
            HASH_VALUE(item->id); HASH_VALUE(item->kind);
            HASH_VALUE(item->status); HASH_VALUE(item->issuer_kingdom_id);
            HASH_VALUE(item->recipient_kingdom_id);
            HASH_VALUE(item->origin_settlement_id);
            HASH_VALUE(item->destination_settlement_id);
            HASH_VALUE(item->current_settlement_id);
            HASH_VALUE(item->route_id); HASH_VALUE(item->cause_event_id);
            HASH_VALUE(item->situation_id); HASH_VALUE(item->departure_day);
            HASH_VALUE(item->arrival_day); HASH_VALUE(item->reliability);
        }
    }
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        const CcBanditGroup *item = &sim->bandits[i];
        HASH_VALUE(item->id); HASH_VALUE(item->route_id); hash = HashString(hash, item->name);
        HASH_VALUE(item->members); HASH_VALUE(item->supplies); HASH_VALUE(item->influence);
        if (sim->schema_version >= 5U) {
            HASH_VALUE(item->camp_size); HASH_VALUE(item->service_mask);
            HASH_VALUE(item->raid_phase); HASH_VALUE(item->raid_target_id);
            HASH_VALUE(item->raid_good); HASH_VALUE(item->raid_quantity);
            HASH_VALUE(item->raid_days_remaining);
            HASH_VALUE(item->raids_completed);
            if (sim->schema_version >= 47U) {
                HASH_VALUE(item->camp_settlement_id);
            }
        }
        HASH_VALUE(sim->last_bandit_level[i]);
    }
    if (sim->schema_version >= 6U) {
        const CcGoblinSociety *goblins = &sim->goblins;
        HASH_VALUE(goblins->id); hash = HashString(hash, goblins->name);
        HASH_VALUE(goblins->members); HASH_VALUE(sim->dragon_cult.devotion);
        HASH_VALUE(goblins->tribute_phase);
        HASH_VALUE(goblins->tribute_target_id);
        HASH_VALUE(goblins->last_tribute_origin_id);
        HASH_VALUE(goblins->tribute_event_id);
        HASH_VALUE(goblins->carried_tribute);
        HASH_VALUE(goblins->tribute_days_remaining);
        HASH_VALUE(goblins->tribute_cooldown_days);
        HASH_VALUE(goblins->tributes_delivered);
        if (sim->schema_version >= 10U) {
            HASH_VALUE(goblins->hoard_defenses);
        }
        if (sim->schema_version >= 16U) {
            HASH_VALUE(goblins->cohesion);
            HASH_VALUE(goblins->target_warned);
            HASH_VALUE(goblins->expeditions_intercepted);
            HASH_VALUE(sim->dragon_cult.dragon_seed_phase);
            HASH_VALUE(sim->dragon_cult.dragon_seed_days_remaining);
        }
        if (sim->schema_version >= 9U) {
            HASH_VALUE(goblins->lair_settlement_id);
            HASH_VALUE(goblins->raid_motive);
            HASH_VALUE(goblins->lair_coins);
            HASH_VALUE(goblins->carried_treasure_id);
            int32_t good_count = CcGoodCountForSchema(sim->schema_version);
            for (int32_t good = 0; good < good_count; ++good) {
                HASH_VALUE(goblins->carried_goods[good]);
                HASH_VALUE(goblins->lair_stock[good]);
            }
        }
        const CcDragon *dragon = &sim->dragon;
        HASH_VALUE(dragon->id); hash = HashString(hash, dragon->name);
        if (sim->schema_version >= 43U) HASH_VALUE(dragon->hair_color);
        HASH_VALUE(dragon->lair_settlement_id);
        HASH_VALUE(dragon->hoard);
        HASH_VALUE(dragon->stolen_outstanding);
        if (sim->schema_version >= 7U) {
            HASH_VALUE(dragon->theft_actor_id);
        }
        HASH_VALUE(dragon->retaliation_target_id);
        HASH_VALUE(dragon->hoard_event_id);
        HASH_VALUE(dragon->omen_event_id);
        HASH_VALUE(dragon->omen_days_remaining);
        HASH_VALUE(dragon->retaliations);
        if (sim->schema_version >= 11U) {
            HASH_VALUE(dragon->slain);
            HASH_VALUE(dragon->slain_day);
        }
        if (sim->schema_version >= 12U) {
            HASH_VALUE(dragon->life_stage);
            HASH_VALUE(dragon->activity);
            HASH_VALUE(dragon->age_days);
            HASH_VALUE(dragon->body_condition);
            HASH_VALUE(dragon->crown_strength);
            HASH_VALUE(dragon->memory_integrity);
            HASH_VALUE(dragon->territory_stability);
            if (sim->schema_version >= 35U) {
                HASH_VALUE(dragon->territoryless_days);
            }
            HASH_VALUE(dragon->regional_influence);
            HASH_VALUE(dragon->crown_continuity_days);
            HASH_VALUE(dragon->hunt_cooldown_days);
            HASH_VALUE(dragon->hunts);
            HASH_VALUE(dragon->egg_count);
            HASH_VALUE(dragon->brood_days_remaining);
            HASH_VALUE(dragon->brood_cooldown_days);
            HASH_VALUE(dragon->broods_laid);
            HASH_VALUE(dragon->whelps_dispersed);
            HASH_VALUE(dragon->afterdeath_days);
            HASH_VALUE(dragon->lifecycle_event_id);
        }
        if (sim->schema_version >= 57U) {
            HASH_VALUE(dragon->wyrmheart_id);
        }
        if (sim->schema_version >= 9U) {
            HASH_VALUE(dragon->stolen_treasure_id);
            int32_t good_count = CcGoodCountForSchema(sim->schema_version);
            for (int32_t good = 0; good < good_count; ++good) {
                HASH_VALUE(dragon->hoard_goods[good]);
            }
        }
    }
    if (sim->schema_version >= 7U) {
        const CcHoardRaiders *raiders = &sim->hoard_raiders;
        HASH_VALUE(raiders->id); hash = HashString(hash, raiders->name);
        HASH_VALUE(raiders->phase);
        HASH_VALUE(raiders->motive);
        HASH_VALUE(raiders->origin_settlement_id);
        HASH_VALUE(raiders->cause_event_id);
        HASH_VALUE(raiders->carried_treasure);
        HASH_VALUE(raiders->days_remaining);
        HASH_VALUE(raiders->cooldown_days);
        HASH_VALUE(raiders->raids_completed);
        HASH_VALUE(raiders->war_raids_completed);
        if (sim->schema_version >= 10U) {
            HASH_VALUE(raiders->social_raid_latched);
            HASH_VALUE(raiders->war_raid_latched);
        }
    }
    if (sim->schema_version >= 11U) {
        const CcDragonCampaign *campaign = &sim->dragon_campaign;
        HASH_VALUE(campaign->phase);
        HASH_VALUE(campaign->pledged_kingdom_mask);
        HASH_VALUE(campaign->alliance_kingdom_mask);
        HASH_VALUE(campaign->origin_settlement_id);
        HASH_VALUE(campaign->cause_event_id);
        if (sim->schema_version >= 35U) {
            HASH_VALUE(campaign->patron_character_id);
            HASH_VALUE(campaign->hero_character_id);
        }
        HASH_VALUE(campaign->days_remaining);
        HASH_VALUE(campaign->cooldown_days);
        int32_t good_count = CcGoodCountForSchema(sim->schema_version);
        for (int32_t good = 0; good < good_count; ++good) {
            HASH_VALUE(campaign->supplies[good]);
        }
        HASH_VALUE(campaign->recovered_coins);
        HASH_VALUE(campaign->attempts);
        HASH_VALUE(campaign->victories);
        HASH_VALUE(campaign->defeats);
    }
    for (int32_t i = 0; i < sim->monster_count; ++i) {
        const CcMonsterPopulation *item = &sim->monsters[i];
        HASH_VALUE(item->id); HASH_VALUE(item->dungeon_id); hash = HashString(hash, item->name);
        HASH_VALUE(item->population); HASH_VALUE(item->pressure);
        HASH_VALUE(item->hunting_pressure); HASH_VALUE(sim->last_monster_level[i]);
    }
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        const CcDungeon *item = &sim->dungeons[i];
        HASH_VALUE(item->id); HASH_VALUE(item->settlement_id); hash = HashString(hash, item->name);
        HASH_VALUE(item->state); HASH_VALUE(item->depth); HASH_VALUE(item->regional_pressure);
        if (hash_underroad) {
            HASH_VALUE(item->layout_seed);
            HASH_VALUE(item->encounter_random_state);
            HASH_VALUE(item->room_count);
            HASH_VALUE(item->link_count);
            for (int32_t room = 0; room < item->room_count; ++room) {
                const CcDungeonRoom *room_item = &item->rooms[room];
                hash = HashString(hash, room_item->name);
                HASH_VALUE(room_item->kind);
                HASH_VALUE(room_item->depth);
                HASH_VALUE(room_item->map_x);
                HASH_VALUE(room_item->map_y);
                HASH_VALUE(room_item->flags);
                HASH_VALUE(room_item->state_flags);
                HASH_VALUE(room_item->loot_good);
                HASH_VALUE(room_item->loot_quantity);
            }
            for (int32_t link = 0; link < item->link_count; ++link) {
                const CcDungeonLink *link_item = &item->links[link];
                HASH_VALUE(link_item->from_room);
                HASH_VALUE(link_item->to_room);
                HASH_VALUE(link_item->kind);
                HASH_VALUE(link_item->flags);
            }
        }
    }
    if (hash_underroad) {
        const CcDungeonExpedition *expedition = &sim->dungeon_expedition;
        HASH_VALUE(expedition->active);
        HASH_VALUE(expedition->dungeon_id);
        HASH_VALUE(expedition->current_room);
        HASH_VALUE(expedition->turns_elapsed);
        HASH_VALUE(expedition->days_elapsed);
        HASH_VALUE(expedition->light_remaining);
        HASH_VALUE(expedition->noise);
        HASH_VALUE(expedition->strain);
        HASH_VALUE(expedition->maximum_depth);
        HASH_VALUE(expedition->encounter_kind);
        HASH_VALUE(expedition->encounter_reaction);
        HASH_VALUE(expedition->encounter_room);
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *item = &sim->situations[i];
        HASH_VALUE(item->id); HASH_VALUE(item->kind); HASH_VALUE(item->status);
        HASH_VALUE(item->issuer_faction_id); HASH_VALUE(item->target_id);
        HASH_VALUE(item->cause_event_id); HASH_VALUE(item->good);
        HASH_VALUE(item->quantity); HASH_VALUE(item->progress); HASH_VALUE(item->reward);
        HASH_VALUE(item->created_day); HASH_VALUE(item->deadline_day);
        if (sim->schema_version >= 17U) {
            HASH_VALUE(item->sponsor_character_id);
            HASH_VALUE(item->affected_character_id);
        }
        if (hash_quest) {
            HASH_VALUE(item->front_id);
            HASH_VALUE(item->end_reason);
            HASH_VALUE(item->objective.kind);
            HASH_VALUE(item->objective.target_id);
            HASH_VALUE(item->objective.good);
            HASH_VALUE(item->objective.required);
            HASH_VALUE(item->objective.progress.value);
            HASH_VALUE(item->objective.progress.limit);
            HASH_VALUE(item->objective.progress.created_by_event_id);
            HASH_VALUE(item->objective.progress.resolved_by_event_id);
            HASH_VALUE(item->objective.danger.value);
            HASH_VALUE(item->objective.danger.limit);
            HASH_VALUE(item->objective.danger.created_by_event_id);
            HASH_VALUE(item->objective.danger.resolved_by_event_id);
            HASH_VALUE(item->objective.evidence_count);
            for (int32_t evidence = 0;
                 evidence < CC_MAX_QUEST_EVIDENCE; ++evidence) {
                HASH_VALUE(item->objective.evidence_event_ids[evidence]);
            }
        }
        if (hash_social) {
            HASH_VALUE(item->witness_character_id);
            HASH_VALUE(item->discovery_stage);
            HASH_VALUE(item->lead_path);
            HASH_VALUE(item->lead_event_id);
        }
        if (item->sponsor_name[0] != '\0' || item->affected_name[0] != '\0') {
            hash = HashString(hash, item->sponsor_name);
            hash = HashString(hash, item->affected_name);
        }
    }
    if (hash_quest) {
        HASH_VALUE(sim->front_count);
        for (int32_t i = 0; i < sim->front_count; ++i) {
            const CcFront *front = &sim->fronts[i];
            HASH_VALUE(front->id);
            HASH_VALUE(front->kind);
            HASH_VALUE(front->status);
            HASH_VALUE(front->outcome);
            HASH_VALUE(front->anchor_id);
            HASH_VALUE(front->cause_event_id);
            HASH_VALUE(front->created_event_id);
            HASH_VALUE(front->resolved_event_id);
            HASH_VALUE(front->created_day);
            HASH_VALUE(front->resolved_day);
            HASH_VALUE(front->portent.value);
            HASH_VALUE(front->portent.limit);
            HASH_VALUE(front->portent.created_by_event_id);
            HASH_VALUE(front->portent.resolved_by_event_id);
            HASH_VALUE(front->situation_count);
            for (int32_t member = 0;
                 member < CC_MAX_FRONT_SITUATIONS; ++member) {
                HASH_VALUE(front->situation_ids[member]);
            }
            hash = HashString(hash, front->premise);
        }
        HASH_VALUE(sim->quest_outcome_count);
        for (int32_t i = 0; i < sim->quest_outcome_count; ++i) {
            const CcQuestOutcomeRecord *outcome = &sim->quest_outcomes[i];
            HASH_VALUE(outcome->id);
            HASH_VALUE(outcome->situation_id);
            HASH_VALUE(outcome->front_id);
            HASH_VALUE(outcome->situation_kind);
            HASH_VALUE(outcome->front_kind);
            HASH_VALUE(outcome->situation_status);
            HASH_VALUE(outcome->end_reason);
            HASH_VALUE(outcome->front_outcome);
            HASH_VALUE(outcome->target_id);
            HASH_VALUE(outcome->sponsor_character_id);
            HASH_VALUE(outcome->affected_character_id);
            HASH_VALUE(outcome->cause_event_id);
            HASH_VALUE(outcome->resolved_event_id);
            HASH_VALUE(outcome->resolved_day);
            HASH_VALUE(outcome->progress_value);
            HASH_VALUE(outcome->progress_limit);
            HASH_VALUE(outcome->danger_value);
            HASH_VALUE(outcome->danger_limit);
        }
    }
    if (sim->schema_version >= 17U) {
        HASH_VALUE(sim->character_count);
        if (hash_lifecycles) {
            HASH_VALUE(sim->character_births);
            HASH_VALUE(sim->character_deaths);
        }
        for (int32_t i = 0; i < sim->character_count; ++i) {
            const CcCharacter *character = &sim->characters[i];
            HASH_VALUE(character->id);
            hash = HashString(hash, character->name);
            if (hash_lifecycles) {
                HASH_VALUE(character->ancestor_id);
                HASH_VALUE(character->birth_day);
                HASH_VALUE(character->death_day);
                HASH_VALUE(character->generation);
            }
            HASH_VALUE(character->home_settlement_id);
            HASH_VALUE(character->current_settlement_id);
            HASH_VALUE(character->faction_id);
            HASH_VALUE(character->role);
            if (sim->schema_version >= 79U) HASH_VALUE(character->occupation);
            HASH_VALUE(character->goal);
            HASH_VALUE(character->activity);
            HASH_VALUE(character->appearance_seed);
            HASH_VALUE(character->player_disposition);
            HASH_VALUE(character->stress);
            HASH_VALUE(character->courage);
            if (sim->schema_version >= 60U) {
                HASH_VALUE(character->travel_coins);
                HASH_VALUE(character->bandit_group_id);
                HASH_VALUE(character->hungry_days);
                HASH_VALUE(character->unsheltered_nights);
            }
            if (sim->schema_version >= 82U) {
                HASH_VALUE(character->travel_destination_id);
                HASH_VALUE(character->travel_arrival_day);
            }
            HASH_VALUE(character->memory_count);
            HASH_VALUE(character->memory_write_index);
            for (int32_t memory = 0;
                 memory < CC_CHARACTER_MEMORY_CAPACITY; ++memory) {
                const CcCharacterMemory *item =
                    &character->memories[memory];
                HASH_VALUE(item->kind);
                HASH_VALUE(item->subject_id);
                HASH_VALUE(item->event_id);
                HASH_VALUE(item->day);
            }
            if (hash_social) {
                HASH_VALUE(character->knowledge_count);
                HASH_VALUE(character->knowledge_write_index);
                for (int32_t knowledge = 0;
                     knowledge < CC_CHARACTER_KNOWLEDGE_CAPACITY;
                     ++knowledge) {
                    const CcCharacterKnowledge *item =
                        &character->knowledge[knowledge];
                    HASH_VALUE(item->kind);
                    HASH_VALUE(item->subject_id);
                    HASH_VALUE(item->source_character_id);
                    HASH_VALUE(item->event_id);
                    HASH_VALUE(item->certainty);
                    HASH_VALUE(item->private_knowledge);
                    HASH_VALUE(item->day);
                    if (sim->schema_version >= 62U) hash = HashString(hash, item->source_name);
                }
            }
        }
    }
    if (sim->schema_version >= 62U) {
        HASH_VALUE(sim->historic_character_count);
        for (int32_t i = 0; i < sim->historic_character_count; ++i) {
            const CcHistoricCharacter *item = &sim->historic_characters[i];
            HASH_VALUE(item->id); HASH_VALUE(item->ancestor_id);
            HASH_VALUE(item->home_settlement_id);
            hash = HashString(hash, item->name);
            HASH_VALUE(item->birth_day); HASH_VALUE(item->death_day);
            HASH_VALUE(item->generation); HASH_VALUE(item->role); HASH_VALUE(item->importance);
        }
    }
    if (hash_social) {
        HASH_VALUE(sim->relationship_count);
        for (int32_t i = 0; i < sim->relationship_count; ++i) {
            const CcRelationship *relationship = &sim->relationships[i];
            HASH_VALUE(relationship->from_character_id);
            HASH_VALUE(relationship->to_character_id);
            HASH_VALUE(relationship->affinity);
            HASH_VALUE(relationship->trust);
            HASH_VALUE(relationship->obligation);
            HASH_VALUE(relationship->history);
            HASH_VALUE(relationship->cause_event_id);
        }
    }
    HASH_VALUE(sim->player.id); HASH_VALUE(sim->player.location_id);
    HASH_VALUE(sim->player.coins); HASH_VALUE(sim->player.cargo_capacity);
    HASH_VALUE(sim->player.passenger_capacity);
    if (sim->schema_version >= 3U) HASH_VALUE(sim->player.map_capacity);
    HASH_VALUE(sim->player.reputation);
    if (sim->schema_version >= 9U) HASH_VALUE(sim->player.treasure_cargo_slots);
    if (sim->schema_version >= 13U) {
        HASH_VALUE(sim->player.map_catalogue_mask);
        HASH_VALUE(sim->player.map_archive_mask);
    }
    if (sim->schema_version >= 23U) {
        for (int32_t i = 0; i < sim->route_count; ++i) {
            const CcRouteKnowledge *knowledge =
                &sim->player.route_knowledge[i];
            HASH_VALUE(knowledge->route_id);
            HASH_VALUE(knowledge->from_reveal_milli);
            HASH_VALUE(knowledge->to_reveal_milli);
            if (sim->schema_version >= 34U) {
                HASH_VALUE(knowledge->learned_day);
                HASH_VALUE(knowledge->recorded_condition);
                HASH_VALUE(knowledge->recorded_danger);
                HASH_VALUE(knowledge->source);
                HASH_VALUE(knowledge->recorded_closed);
                HASH_VALUE(knowledge->recorded_smuggler_route);
            }
        }
    }
    if (sim->schema_version >= 34U) {
        HASH_VALUE(sim->player.road_book_site_discovery_mask);
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            const CcSettlementKnowledge *knowledge =
                &sim->player.settlement_knowledge[i];
            HASH_VALUE(knowledge->settlement_id);
            HASH_VALUE(knowledge->kingdom_id);
            HASH_VALUE(knowledge->learned_day);
            HASH_VALUE(knowledge->source);
        }
    }
    if (sim->schema_version >= 14U) {
        for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
            const CcHorse *horse = &sim->horse_team[i];
            HASH_VALUE(horse->id);
            hash = HashString(hash, horse->name);
            HASH_VALUE(horse->age_days);
            HASH_VALUE(horse->health);
            HASH_VALUE(horse->fatigue);
            HASH_VALUE(horse->hunger);
            if (sim->schema_version >= 15U) {
                HASH_VALUE(horse->sex);
                HASH_VALUE(horse->sire_id);
                HASH_VALUE(horse->dam_id);
                HASH_VALUE(horse->stable_settlement_id);
                HASH_VALUE(horse->pregnant_by_id);
                HASH_VALUE(horse->pregnancy_days_remaining);
                HASH_VALUE(horse->breeding_cooldown_days);
                HASH_VALUE(horse->training);
                HASH_VALUE(horse->strength);
                HASH_VALUE(horse->temperament);
                HASH_VALUE(horse->hardiness);
            }
        }
    }
    if (sim->schema_version >= 15U) {
        HASH_VALUE(sim->stable_horse_count);
        for (int32_t i = 0; i < sim->stable_horse_count; ++i) {
            const CcHorse *horse = &sim->stable_horses[i];
            HASH_VALUE(horse->id);
            hash = HashString(hash, horse->name);
            HASH_VALUE(horse->age_days);
            HASH_VALUE(horse->health);
            HASH_VALUE(horse->fatigue);
            HASH_VALUE(horse->hunger);
            HASH_VALUE(horse->sex);
            HASH_VALUE(horse->sire_id);
            HASH_VALUE(horse->dam_id);
            HASH_VALUE(horse->stable_settlement_id);
            HASH_VALUE(horse->pregnant_by_id);
            HASH_VALUE(horse->pregnancy_days_remaining);
            HASH_VALUE(horse->breeding_cooldown_days);
            HASH_VALUE(horse->training);
            HASH_VALUE(horse->strength);
            HASH_VALUE(horse->temperament);
            HASH_VALUE(horse->hardiness);
        }
    }

    if (sim->player.accepted_situation_id != 0U) {
        HASH_VALUE(sim->player.accepted_situation_id);
    }
    if (sim->schema_version <= 3U) {

        if (sim->journey.active) {
            HASH_VALUE(sim->journey.active);
            HASH_VALUE(sim->journey.situation_id);
            HASH_VALUE(sim->journey.origin_id);
            HASH_VALUE(sim->journey.destination_id);
            HASH_VALUE(sim->journey.route_id);
            HASH_VALUE(sim->journey.danger);
            HASH_VALUE(sim->journey.bargain_cost);
        }
        if (sim->resolved_journey_situation_id != 0U) {
            HASH_VALUE(sim->resolved_journey_situation_id);
            HASH_VALUE(sim->resolved_journey_outcome);
            HASH_VALUE(sim->journey.destination_id);
            HASH_VALUE(sim->journey.route_id);
        }
        if (sim->delayed_echo.active) {
            HASH_VALUE(sim->delayed_echo.active);
            HASH_VALUE(sim->delayed_echo.situation_id);
            HASH_VALUE(sim->delayed_echo.settlement_id);
            HASH_VALUE(sim->delayed_echo.parent_event_id);
            HASH_VALUE(sim->delayed_echo.outcome);
            HASH_VALUE(sim->delayed_echo.due_day);
            hash = HashString(hash, sim->delayed_echo.character_name);
        }
    } else {
        HASH_VALUE(sim->clock.tick);
        HASH_VALUE(sim->clock.minute_subticks);
        HASH_VALUE(sim->clock.game_minutes_per_second);
        HASH_VALUE(sim->journey.active);
        HASH_VALUE(sim->journey.phase);
        HASH_VALUE(sim->journey.situation_id);
        HASH_VALUE(sim->journey.origin_id);
        HASH_VALUE(sim->journey.destination_id);
        HASH_VALUE(sim->journey.route_id);
        HASH_VALUE(sim->journey.danger);
        HASH_VALUE(sim->journey.bargain_cost);
        HASH_VALUE(sim->journey.departure_day);
        HASH_VALUE(sim->journey.elapsed_subticks);
        HASH_VALUE(sim->journey.total_subticks);
        HASH_VALUE(sim->journey.encounter_subticks);
        HASH_VALUE(sim->journey.fare_reserved);
        HASH_VALUE(sim->journey.encounter_triggered);
        HASH_VALUE(sim->journey.ambush_pending);
        HASH_VALUE(sim->journey.ambush_resolved);
        if (sim->schema_version >= 18U) {
            HASH_VALUE(sim->journey.pace);
            HASH_VALUE(sim->journey.ambush_warned);
        }
        if (sim->schema_version >= 39U) {
            HASH_VALUE(sim->journey.road_site_stop_mask);
        }
        HASH_VALUE(sim->journey.parent_event_id);
        HASH_VALUE(sim->carriage.mode);
        HASH_VALUE(sim->carriage.location_id);
        HASH_VALUE(sim->carriage.route_id);
        HASH_VALUE(sim->carriage.origin_id);
        HASH_VALUE(sim->carriage.destination_id);
        HASH_VALUE(sim->carriage.progress_milli);
        HASH_VALUE(sim->carriage.speed_milli_per_second);
        HASH_VALUE(sim->carriage.condition);
        HASH_VALUE(sim->resolved_journey_situation_id);
        HASH_VALUE(sim->resolved_journey_outcome);
        HASH_VALUE(sim->delayed_echo.active);
        HASH_VALUE(sim->delayed_echo.situation_id);
        HASH_VALUE(sim->delayed_echo.settlement_id);
        HASH_VALUE(sim->delayed_echo.parent_event_id);
        HASH_VALUE(sim->delayed_echo.outcome);
        HASH_VALUE(sim->delayed_echo.due_day);
        hash = HashString(hash, sim->delayed_echo.character_name);
        if (hash_quest) {
            HASH_VALUE(sim->pending_echo_count);
            for (int32_t i = 0; i < CC_MAX_PENDING_ECHOES; ++i) {
                const CcDelayedEcho *echo = &sim->pending_echoes[i];
                HASH_VALUE(echo->active);
                HASH_VALUE(echo->situation_id);
                HASH_VALUE(echo->settlement_id);
                HASH_VALUE(echo->parent_event_id);
                HASH_VALUE(echo->outcome);
                HASH_VALUE(echo->due_day);
                hash = HashString(hash, echo->character_name);
            }
        }
    }
    int32_t player_good_count = CcGoodCountForSchema(sim->schema_version);
    for (int32_t good = 0; good < player_good_count; ++good) {
        HASH_VALUE(sim->player.cargo[good]);
    }
    for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
        const CcEvent *item = &sim->events[i];
        HASH_VALUE(item->id); HASH_VALUE(item->day); HASH_VALUE(item->kind);
        HASH_VALUE(item->subject_id); HASH_VALUE(item->location_id); HASH_VALUE(item->parent_id);
        if (hash_social) {
            HASH_VALUE(item->actor_id);
            HASH_VALUE(item->target_id);
            HASH_VALUE(item->beneficiary_id);
            HASH_VALUE(item->witness_id);
        }
        HASH_VALUE(item->magnitude); hash = HashString(hash, item->text);
    }
    if (hash_archives) {
        if (sim->schema_version >= 80U) {
            HASH_VALUE(sim->archive_recruitment.status);
            HASH_VALUE(sim->archive_recruitment.person_id);
            HASH_VALUE(sim->archive_recruitment.trainer_id);
            HASH_VALUE(sim->archive_recruitment.seat_id);
            HASH_VALUE(sim->archive_recruitment.origin_id);
            HASH_VALUE(sim->archive_recruitment.first_route_id);
            HASH_VALUE(sim->archive_recruitment.first_hop_id);
            HASH_VALUE(sim->archive_recruitment.donor_ids[0]);
            HASH_VALUE(sim->archive_recruitment.donor_ids[1]);
            HASH_VALUE(sim->archive_recruitment.patron_ids[0]);
            HASH_VALUE(sim->archive_recruitment.patron_ids[1]);
            HASH_VALUE(sim->archive_recruitment.donor_shares[0]);
            HASH_VALUE(sim->archive_recruitment.donor_shares[1]);
            HASH_VALUE(sim->archive_recruitment.purse);
            HASH_VALUE(sim->archive_recruitment.wheat);
            HASH_VALUE(sim->archive_recruitment.paper);
            HASH_VALUE(sim->archive_recruitment.tools);
            HASH_VALUE(sim->archive_recruitment.travel_wheat);
            HASH_VALUE(sim->archive_recruitment.start_day);
            HASH_VALUE(sim->archive_recruitment.training_days);
            HASH_VALUE(sim->archive_recruitment.trainer_days);
            HASH_VALUE(sim->archive_recruitment.arrival_estimate);
            HASH_VALUE(sim->archive_recruitment.ready_estimate);
            if (sim->schema_version >= 79U) {
                HASH_VALUE(sim->archive_recruitment.current_id);
                HASH_VALUE(sim->archive_recruitment.leg_route_id);
                HASH_VALUE(sim->archive_recruitment.leg_hop_id);
                HASH_VALUE(sim->archive_recruitment.leg_arrival_day);
                HASH_VALUE(sim->archive_recruitment.provisioned_days);
                HASH_VALUE(sim->archive_recruitment.arrived_day);
            }
        }
        HASH_VALUE(sim->archives.scribes);
        HASH_VALUE(sim->archives.lore_stored);
        HASH_VALUE(sim->archives.lore_lost_total);
        HASH_VALUE(sim->archives.last_recorded_day);
        HASH_VALUE(sim->archives.lore_ceiling);
        if (sim->schema_version >= 34U) {
            HASH_VALUE(sim->archives.kit_tool_wear);
        }
        if (sim->schema_version >= 35U) {
            HASH_VALUE(sim->archives.abbot_character_id);
            HASH_VALUE(sim->archives.stewardship_rank);
        }
        if (sim->schema_version >= 56U) {
            HASH_VALUE(sim->archives.dead_since_day);
        }
    }
    if (sim->schema_version >= 44U) {
        HASH_VALUE(sim->gossip_last_event_id);
        /* Schema 48 fields hash only for schema 48 saves, so older
           campaigns keep the hash they were stored with. */
        if (sim->schema_version >= 48U) {
            HASH_VALUE(sim->posted_situation_mask);
        }
        for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
            const CcGossip *story = &sim->gossip[i];
            HASH_VALUE(story->event_id); HASH_VALUE(story->origin_id);
            HASH_VALUE(story->heard_event_id); HASH_VALUE(story->day);
            HASH_VALUE(story->heard_day); HASH_VALUE(story->settlement_mask);
            HASH_VALUE(story->recorded);
            HASH_VALUE(story->kind);
            hash = HashString(hash, story->text);
            hash = HashString(hash, story->heard_from);
            for (int32_t town = 0; town < CC_MAX_SETTLEMENTS; ++town) {
                if ((story->settlement_mask & (UINT32_C(1) << (uint32_t)town)) != 0U) {
                    hash = HashGossipVersion(hash, &story->local[town]);
                }
            }
            if (story->heard_day > 0) hash = HashGossipVersion(hash, &story->heard);
        }
        for (int32_t i = 0; i < CcSimGossipCarrierCapacity(sim); ++i) {
            HASH_VALUE(sim->gossip_carriers[i].id);
            HASH_VALUE(sim->gossip_carriers[i].stories);
            if (sim->schema_version >= 48U) {
                HASH_VALUE(sim->gossip_carriers[i].told_player);
            }
            for (int32_t slot = 0; slot < CC_MAX_GOSSIP; ++slot) {
                if ((sim->gossip_carriers[i].stories & (UINT32_C(1) << (uint32_t)slot)) != 0U) {
                    hash = HashGossipVersion(hash, &sim->gossip_carriers[i].versions[slot]);
                }
            }
        }
    }
    if (sim->schema_version >= 40U) {
        HASH_VALUE(sim->pony_company.team[0]);
        HASH_VALUE(sim->pony_company.team[1]);
        HASH_VALUE(sim->pony_company.encounter);
        for (int32_t i = 0; i < CC_PONY_COUNT; ++i) {
            const CcPony *pony = &sim->pony_company.ponies[i];
            HASH_VALUE(pony->route_id);
            HASH_VALUE(pony->last_seen_route);
            HASH_VALUE(pony->bond);
            HASH_VALUE(pony->quests_completed);
            HASH_VALUE(pony->releases);
            HASH_VALUE(pony->last_met_day);
            HASH_VALUE(pony->quest_kind);
            HASH_VALUE(pony->quest_amount);
            HASH_VALUE(pony->health);
            HASH_VALUE(pony->fatigue);
            HASH_VALUE(pony->hunger);
            HASH_VALUE(pony->seen);
            HASH_VALUE(pony->ready);
        }
    }
    if (sim->schema_version >= 75U) {
        HASH_VALUE(sim->dragon_cult.offering_coins);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            HASH_VALUE(sim->dragon_cult.offering_stock[good]);
        }
        const CcGoblinPolitics *p = &sim->goblin_politics;
        HASH_VALUE(p->dragon_id);
        HASH_VALUE(p->contest_started_day);
        HASH_VALUE(p->crown_faction);
        HASH_VALUE(p->raid_faction);
        HASH_VALUE(p->next_hunt_faction);
        for (int32_t i = 0; i < CC_GOBLIN_FACTION_COUNT; ++i) {
            const CcGoblinFaction *f = &p->factions[i];
            HASH_VALUE(f->members);
            HASH_VALUE(f->dungeon_id);
            HASH_VALUE(f->lair_room);
            HASH_VALUE(f->porter_room);
            HASH_VALUE(f->target_room);
            HASH_VALUE(f->coins);
            HASH_VALUE(f->gold);
            HASH_VALUE(f->gems);
            HASH_VALUE(f->carried_coins);
            HASH_VALUE(f->carried_gold);
            HASH_VALUE(f->carried_gems);
            HASH_VALUE(f->tribute);
            HASH_VALUE(f->deliveries);
            HASH_VALUE(f->hunted);
            HASH_VALUE(f->journey_event_id);
        }
        for (int32_t species = 0; species < CC_CULT_SPECIES_COUNT; ++species) {
            for (int32_t rank = 0; rank < CC_CULT_RANK_COUNT; ++rank) {
                HASH_VALUE(sim->dragon_cult.ranks[species][rank]);
            }
            HASH_VALUE(sim->dragon_cult.service[species]);
        }
    }
#undef HASH_VALUE
    return hash;
}
