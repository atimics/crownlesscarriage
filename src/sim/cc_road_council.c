#include "sim/cc_road_council.h"
#include <inttypes.h>
#include <stdio.h>

static bool Present(const CcSim *sim, const CcCharacter *person, CcId town)
{
    return person != NULL && person->birth_day <= sim->current_day &&
        person->death_day > sim->current_day && person->current_settlement_id == town;
}

static void Person(CcRoadCouncilRow *row, const CcCharacter *person)
{
    if (person == NULL) return;
    row->actor_id = person->id;
    (void)snprintf(row->name, sizeof(row->name), "%s", person->name);
}

CcRoadCouncil CcSimRoadCouncil(const CcSim *sim, CcId settlement_id)
{
    CcRoadCouncil view = {.route_slot = -1};
    if (sim == NULL) return view;
    const CcSettlement *town = CcSimSettlement(sim, settlement_id);
    if (town == NULL || CcSettlementIsAbandoned(town)) return view;
    view.settlement_id = town->id;
    const char *labels[] = {"Grain organiser", "Road work", "Town market", "Travellers", "Road camp", "Local scout"};
    for (int i = 0; i < CC_ROAD_COUNCIL_ROWS; ++i)
        (void)snprintf(view.rows[i].name, sizeof(view.rows[i].name), "%s", labels[i]);
    const CcRoute *road = NULL;
    for (int i = 0; i < sim->route_count; ++i) {
        const CcRoute *candidate = &sim->routes[i];
        if (candidate->from_id != town->id && candidate->to_id != town->id) continue;
        int score = (candidate->closed ? 200 : 0) + 100 - candidate->condition;
        int previous = road != NULL ? (road->closed ? 200 : 0) + 100 - road->condition : -1;
        if (score > previous) { road = candidate; view.route_slot = i; view.route_id = road->id; }
    }
    const CcGrainSupply *fund = CcSimGrainSupply(sim, town->id);
    CcBakerySupportPlan bakery = CcSimBakerySupportPlan(sim, town->id);
    const CcCharacter *organiser = CcSimCharacter(sim,
        fund->organiser_id != 0U ? fund->organiser_id : bakery.contact_id);
    if (Present(sim, organiser, town->id)) Person(&view.rows[0], organiser);
    CcGrainDeliveryPlan plan = CcSimGrainDeliveryPlan(sim, town->id);
    (void)snprintf(view.rows[0].detail, sizeof(view.rows[0].detail),
        "Fund %" PRId64 "; wheat arrived %d. %.170s", fund->purse, fund->delivered, plan.reason);
    if (road != NULL) {
        const CcSettlement *other = CcSimSettlement(sim, road->from_id == town->id ? road->to_id : road->from_id);
        (void)snprintf(view.rows[1].detail, sizeof(view.rows[1].detail),
            "Road %d to %.31s: condition %d, %s. %s", view.route_slot + 1,
            other != NULL ? other->name : "another town", road->condition,
            road->closed ? "closed" : "open", road->closed ?
            "Repair: 2 Tools, 2 Wood, 2 Stone and 1 day; or 18 crowns and 3 days." : "Crews can use the road.");
    }
    int loads = 0, blocked = 0, other_cargo = 0;
    for (int i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *load = &sim->shipments[i];
        if (load->status != CC_SHIPMENT_TRAVELLING && load->status != CC_SHIPMENT_BLOCKED) continue;
        if (load->origin_id != town->id && load->final_destination_id != town->id) continue;
        ++loads;
        blocked += load->status == CC_SHIPMENT_BLOCKED;
        other_cargo += load->good != CC_GOOD_WHEAT;
    }
    (void)snprintf(view.rows[2].detail, sizeof(view.rows[2].detail),
        "Market %" PRId64 " crowns. Active loads %d; other goods %d; blocked %d. Grain shares royal carriage time.",
        town->market_coins, loads, other_cargo, blocked);
    const CcCharacter *traveller = NULL, *scout = NULL;
    for (int i = 0; i < sim->character_count; ++i) {
        const CcCharacter *person = &sim->characters[i];
        if (!Present(sim, person, town->id)) continue;
        if ((person->role == CC_CHARACTER_TRAVELLER || person->role == CC_CHARACTER_REFUGEE) &&
            person->bandit_group_id == 0U && (traveller == NULL ||
            person->hungry_days + person->unsheltered_nights > traveller->hungry_days + traveller->unsheltered_nights)) traveller = person;
        if (person->role == CC_CHARACTER_SCOUT && scout == NULL) scout = person;
    }
    Person(&view.rows[3], traveller);
    if (traveller != NULL)
        (void)snprintf(view.rows[3].detail, sizeof(view.rows[3].detail),
            "%s; purse %" PRId64 ". Hungry days %d; nights seeking shelter %d.",
            CcCharacterRoleName(traveller->role), traveller->travel_coins, traveller->hungry_days, traveller->unsheltered_nights);
    else (void)snprintf(view.rows[3].detail, sizeof(view.rows[3].detail), "Travellers may bring their needs here as the town changes.");
    const CcBanditGroup *camp = road != NULL ? CcSimBanditGroupOnRoute(sim, road->id) : NULL;
    if (camp != NULL) {
        view.rows[4].actor_id = camp->id;
        (void)snprintf(view.rows[4].name, sizeof(view.rows[4].name), "%s", camp->name);
        (void)snprintf(view.rows[4].detail, sizeof(view.rows[4].detail),
            "Members %d; supplies %d; influence %d. Hungry travellers can seek a place in a nearby camp.",
            camp->members, camp->supplies, camp->influence);
    } else (void)snprintf(view.rows[4].detail, sizeof(view.rows[4].detail), "Road danger: %d. Check conditions before departure.", road != NULL ? CcSimRouteDanger(sim, road->id) : 0);
    Person(&view.rows[5], scout);
    (void)snprintf(view.rows[5].detail, sizeof(view.rows[5].detail), "Ask people about local work and follow the accounts they share.");
    for (int i = 0; i < sim->situation_count; ++i) {
        const CcSituation *quest = &sim->situations[i];
        if (quest->status != CC_SITUATION_ACTIVE ||
            (((sim->posted_situation_mask >> (unsigned)i) & 1U) == 0U && sim->player.accepted_situation_id != quest->id)) continue;
        int row = quest->kind == CC_SITUATION_ROUTE_REPAIR && quest->target_id == view.route_id ? 1 :
            quest->kind == CC_SITUATION_RELIEF_DELIVERY && quest->target_id == town->id ? 0 : -1;
        if (row >= 0 && view.rows[row].situation_id == 0U) {
            view.rows[row].situation_id = quest->id;
            const CcCharacter *sponsor = CcSimCharacter(sim, quest->sponsor_character_id);
            if (row == 1 && Present(sim, sponsor, town->id)) Person(&view.rows[row], sponsor);
        }
        if (scout == NULL) continue;
        for (int k = 0; k < scout->knowledge_count; ++k) {
            const CcCharacterKnowledge *account = &scout->knowledge[k];
            if (account->subject_id == quest->id && !account->private_knowledge) {
                view.rows[5].situation_id = quest->id;
                (void)snprintf(view.rows[5].detail, sizeof(view.rows[5].detail),
                    "Has a shared account about %s, learned on day %d. Ask about its source.",
                    CcSituationKindName(quest->kind), account->day);
                break;
            }
        }
    }
    return view;
}
