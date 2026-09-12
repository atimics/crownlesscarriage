#include "sim/cc_sim.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* Bounded armed companies (#646). A party is a named commander and a group
   count drawn from real population, not individual soldiers. Orders travel
   as physical dispatches: a checkpoint stands until a withdrawal order or a
   crossing permit is carried to it, and a report changes nobody's knowledge
   until it is delivered.

   Everything here is fenced at schema 91. Older worlds have no parties and
   no dispatches; the migration clears both arrays. */

static int32_t WarMax(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t WarClamp(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static const char *WarKingdomName(const CcSim *sim, CcId kingdom_id)
{
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if (sim->kingdoms[i].id == kingdom_id) return sim->kingdoms[i].name;
    }
    return "a crown";
}

static CcWarParty *WarPartyMutable(CcSim *sim, CcId id)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->war_party_count; ++i) {
        if (sim->war_parties[i].id == id) return &sim->war_parties[i];
    }
    return NULL;
}

const CcWarParty *CcSimWarParty(const CcSim *sim, CcId id)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->war_party_count; ++i) {
        if (sim->war_parties[i].id == id) return &sim->war_parties[i];
    }
    return NULL;
}

/* The party holding a route stands at one of its ends and holds
   CC_WAR_ORDER_CONTROL_ROUTE naming that road. A travelling party is on the
   road, not at the gate. */
const CcWarParty *CcSimRouteCheckpoint(const CcSim *sim, CcId route_id)
{
    if (sim == NULL || sim->schema_version < 94U) return NULL;
    const CcRoute *route = CcSimRoute(sim, route_id);
    if (route == NULL) return NULL;
    for (int32_t i = 0; i < sim->war_party_count; ++i) {
        const CcWarParty *party = &sim->war_parties[i];
        if (party->order != CC_WAR_ORDER_CONTROL_ROUTE ||
            party->order_route_id != route_id ||
            party->travel_route_id != 0U) continue;
        if (party->current_settlement_id == route->from_id ||
            party->current_settlement_id == route->to_id) return party;
    }
    return NULL;
}

/* The Crownless has no gate: a checkpoint never blocks the player's own
   carriage. A delivered crossing permit is recorded on the party for the
   event log; the player could pass anyway. */
bool CcSimPlayerMayCrossCheckpoint(const CcSim *sim, CcId route_id)
{
    (void)sim;
    (void)route_id;
    return true;
}

/* One route directly linking the two kingdoms. */
static CcId KingdomBorderRoute(const CcSim *sim, int32_t first, int32_t second)
{
    CcId first_id = sim->kingdoms[first].id;
    CcId second_id = sim->kingdoms[second].id;
    for (int32_t r = 0; r < sim->route_count; ++r) {
        const CcRoute *route = &sim->routes[r];
        const CcSettlement *from = CcSimSettlement(sim, route->from_id);
        const CcSettlement *to = CcSimSettlement(sim, route->to_id);
        if (from == NULL || to == NULL) continue;
        if ((from->kingdom_id == first_id && to->kingdom_id == second_id) ||
            (from->kingdom_id == second_id && to->kingdom_id == first_id)) {
            return route->id;
        }
    }
    return 0U;
}

/* The capital when there is one, otherwise the kingdom's largest living
   town. Mirrors KingdomSeat in cc_sim.c, which is private. */
static CcId KingdomSeatId(const CcSim *sim, int32_t slot)
{
    if (sim == NULL || slot < 0 || slot >= sim->kingdom_count) return 0U;
    CcId kingdom_id = sim->kingdoms[slot].id;
    const CcSettlement *best = NULL;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (place->kingdom_id != kingdom_id ||
            CcSettlementIsAbandoned(place)) continue;
        if (best == NULL || place->function == CC_SETTLEMENT_CAPITAL ||
            (best->function != CC_SETTLEMENT_CAPITAL &&
             place->population > best->population)) {
            best = place;
        }
    }
    return best != NULL ? best->id : 0U;
}

/* A named commander from the seat's own residents. The crown falls back to
   any living resident, then to the kingdom name itself. */
static CcId CommandingResident(const CcSim *sim, CcId seat_id)
{
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *person = &sim->characters[i];
        if (person->home_settlement_id != seat_id) continue;
        if (person->death_day > 0 && person->death_day <= sim->current_day) {
            continue;
        }
        if (person->role == CC_CHARACTER_OFFICIAL) return person->id;
    }
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *person = &sim->characters[i];
        if (person->home_settlement_id != seat_id) continue;
        if (person->death_day > 0 && person->death_day <= sim->current_day) {
            continue;
        }
        return person->id;
    }
    return 0U;
}

static CcWarParty *AllocateWarParty(CcSim *sim)
{
    if (sim->war_party_count >= CC_MAX_WAR_PARTIES) return NULL;
    CcWarParty *party = &sim->war_parties[sim->war_party_count];
    *party = (CcWarParty){0};
    party->id = CcMakeId(CC_ENTITY_CHARACTER, sim->next_entity_serial++);
    sim->war_party_count += 1;
    return party;
}

static void PartyTravel(CcSim *sim, CcWarParty *party, CcId destination_id,
                        CcId route_id, int32_t travel_days)
{
    party->travel_route_id = route_id;
    party->travel_destination_id = destination_id;
    party->travel_arrival_day = sim->current_day +
        (travel_days > 0 ? travel_days : 1);
}

/* Marches take the first open direct route toward the target. A closed road
   stops the party at this end and turns the order into a checkpoint on that
   road: soldiers arrive at the broken bridge and hold it. */
static void MarchToward(CcSim *sim, CcWarParty *party)
{
    const CcSettlement *here = CcSimSettlement(sim, party->current_settlement_id);
    const CcSettlement *target = CcSimSettlement(sim, party->order_target_id);
    if (here == NULL || target == NULL) {
        party->order = CC_WAR_ORDER_HOLD;
        return;
    }
    for (int32_t r = 0; r < sim->route_count; ++r) {
        const CcRoute *route = &sim->routes[r];
        bool from_here = route->from_id == here->id || route->to_id == here->id;
        if (!from_here) continue;
        CcId far = route->from_id == here->id ? route->to_id : route->from_id;
        const CcSettlement *far_place = CcSimSettlement(sim, far);
        if (far_place == NULL || CcSettlementIsAbandoned(far_place)) continue;
        if (route->closed) {
            party->order = CC_WAR_ORDER_CONTROL_ROUTE;
            party->order_route_id = route->id;
            return;
        }
        PartyTravel(sim, party, far, route->id, route->travel_days);
        return;
    }
    party->order = CC_WAR_ORDER_HOLD;
}

/* War is a summary of what the companies are doing, not a switch that makes
   them do it. This is the only hostility test: allied parties never fight,
   and co-location under anything less than an active conflict does not
   either. */
static bool PartiesHostile(const CcSim *sim, const CcWarParty *a,
                           const CcWarParty *b)
{
    if (a == NULL || b == NULL || a == b) return false;
    if (a->kingdom_id == b->kingdom_id) return false;
    if (CcSimKingdomsAllied(sim, a->kingdom_id, b->kingdom_id)) return false;
    if (CcSimKingdomsAtWar(sim, a->kingdom_id, b->kingdom_id)) return true;
    /* Entering a foreign, non-allied town under arms is itself the act of
       war: a marching party that reaches the other side's settlement makes
       the pair hostile without any declaration. */
    const CcSettlement *place = CcSimSettlement(
        sim, a->current_settlement_id);
    if (a->order == CC_WAR_ORDER_MARCH && place != NULL &&
        place->kingdom_id == b->kingdom_id) return true;
    place = CcSimSettlement(sim, b->current_settlement_id);
    if (b->order == CC_WAR_ORDER_MARCH && place != NULL &&
        place->kingdom_id == a->kingdom_id) return true;
    return false;
}

static void RemoveWarParty(CcSim *sim, int32_t slot)
{
    int32_t last = sim->war_party_count - 1;
    if (slot != last) {
        sim->war_parties[slot] = sim->war_parties[last];
    }
    sim->war_parties[last] = (CcWarParty){0};
    sim->war_party_count = last;
}

/* One battle per settlement per day, resolved after all arrivals so that
   array order cannot decide who wins. Casualties come from the parties
   present; the defeated side stands down where it is. */
static void ResolveBattles(CcSim *sim)
{
    for (int32_t place = 0; place < sim->settlement_count; ++place) {
        const CcSettlement *town = &sim->settlements[place];
        if (CcSettlementIsAbandoned(town)) continue;
        for (int32_t a = 0; a < sim->war_party_count; ++a) {
            CcWarParty *attacker = &sim->war_parties[a];
            if (attacker->travel_route_id != 0U) continue;
            if (attacker->current_settlement_id != town->id) continue;
            if (attacker->order == CC_WAR_ORDER_CEASE) continue;
            for (int32_t b = a + 1; b < sim->war_party_count; ++b) {
                CcWarParty *defender = &sim->war_parties[b];
                if (defender->travel_route_id != 0U) continue;
                if (defender->current_settlement_id != town->id) continue;
                if (defender->order == CC_WAR_ORDER_CEASE) continue;
                if (!PartiesHostile(sim, attacker, defender)) continue;

                int32_t attacker_loss = WarMax(
                    1, defender->members / 4);
                int32_t defender_loss = WarMax(
                    1, attacker->members / 4);
                attacker->members = WarMax(0, attacker->members - attacker_loss);
                defender->members = WarMax(0, defender->members - defender_loss);
                attacker->casualties += attacker_loss;
                defender->casualties += defender_loss;
                attacker->battles_fought += 1;
                defender->battles_fought += 1;
                CcWarParty *loser =
                    attacker->members <= defender->members ? attacker : defender;
                loser->order = CC_WAR_ORDER_CEASE;

                const CcCharacter *attacker_commander = CcSimCharacter(
                    sim, attacker->commander_character_id);
                const CcCharacter *defender_commander = CcSimCharacter(
                    sim, defender->commander_character_id);
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(
                    text, sizeof(text),
                    "Steel meets in %.24s: %.20s's company under %.20s and "
                    "%.20s's under %.20s clash; %d and %d fall.",
                    town->name,
                    WarKingdomName(sim, attacker->kingdom_id),
                    attacker_commander != NULL ? attacker_commander->name :
                        "an unnamed captain",
                    WarKingdomName(sim, defender->kingdom_id),
                    defender_commander != NULL ? defender_commander->name :
                        "an unnamed captain",
                    attacker_loss, defender_loss);
                CcSimPushEvent(sim, CC_EVENT_SETTLEMENT_RAIDED, town->id, town->id,
                          0U, (int32_t)town->kingdom_id, text);

                if (attacker->members <= 0) {
                    RemoveWarParty(sim, a);
                    a -= 1;
                    break;
                }
                if (defender->members <= 0) {
                    RemoveWarParty(sim, b);
                    b -= 1;
                }
                break;
            }
        }
    }
}

void CcSimAdvanceWarParties(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 94U) return;
    for (int32_t i = 0; i < sim->war_party_count; ++i) {
        CcWarParty *party = &sim->war_parties[i];
        if (party->travel_route_id != 0U) {
            if (sim->current_day < party->travel_arrival_day) continue;
            const CcSettlement *arrived = CcSimSettlement(
                sim, party->travel_destination_id);
            if (arrived != NULL && !CcSettlementIsAbandoned(arrived)) {
                party->current_settlement_id = arrived->id;
            }
            party->travel_route_id = 0U;
            party->travel_destination_id = 0U;
            party->travel_arrival_day = 0;
            if (party->order == CC_WAR_ORDER_MARCH &&
                party->order_target_id == party->current_settlement_id) {
                party->order = CC_WAR_ORDER_HOLD;
            } else if (party->order == CC_WAR_ORDER_WITHDRAW &&
                       party->current_settlement_id == party->home_settlement_id) {
                party->order = CC_WAR_ORDER_HOLD;
            }
            continue;
        }
        if (party->order == CC_WAR_ORDER_MARCH) {
            MarchToward(sim, party);
        } else if (party->order == CC_WAR_ORDER_WITHDRAW &&
                   party->current_settlement_id != party->home_settlement_id) {
            CcWarParty staged = *party;
            staged.order_target_id = staged.home_settlement_id;
            CcWarParty *slot = &sim->war_parties[i];
            *slot = staged;
            MarchToward(sim, slot);
        }
    }
    ResolveBattles(sim);
}

/* The declaration courier is the abstract label. The muster is the physical
   fact: one company from the issuer's capital marching on the recipient's
   seat, and one from the recipient holding the border road between them. */
void CcSimMusterWarPartyForDeclaration(CcSim *sim, int32_t issuer,
                                       int32_t recipient)
{
    if (sim == NULL || sim->schema_version < 94U) return;
    if (issuer < 0 || recipient < 0 || issuer == recipient ||
        issuer >= sim->kingdom_count || recipient >= sim->kingdom_count) return;
    CcId border = KingdomBorderRoute(sim, issuer, recipient);
    CcId issuer_seat = KingdomSeatId(sim, issuer);
    CcId recipient_seat = KingdomSeatId(sim, recipient);
    if (border == 0U) return;

    CcWarParty *marching = AllocateWarParty(sim);
    if (marching != NULL) {
        CcSettlement *home = CcSimSettlementMutable(sim, issuer_seat);
        int32_t levy = home != NULL ?
            WarClamp(home->population / 200, 8, 40) : 0;
        if (home != NULL && levy > home->population / 2) {
            levy = home->population / 2;
        }
        if (levy <= 0 || home == NULL) {
            sim->war_party_count -= 1;
        } else {
            home->population -= levy;
            marching->kingdom_id = sim->kingdoms[issuer].id;
            marching->commander_character_id = CommandingResident(sim, issuer_seat);
            marching->home_settlement_id = issuer_seat;
            marching->current_settlement_id = issuer_seat;
            marching->members = levy;
            marching->order = CC_WAR_ORDER_MARCH;
            marching->order_target_id = recipient_seat;
        }
    }

    CcWarParty *guarding = AllocateWarParty(sim);
    if (guarding != NULL) {
        /* Station the guard at the border road's own end inside the
           recipient's kingdom, so the checkpoint really holds that road. */
        const CcRoute *held = NULL;
        for (int32_t r = 0; r < sim->route_count && held == NULL; ++r) {
            if (sim->routes[r].id == border) held = &sim->routes[r];
        }
        CcId guard_post = 0U;
        if (held != NULL) {
            const CcSettlement *from = CcSimSettlement(sim, held->from_id);
            const CcSettlement *to = CcSimSettlement(sim, held->to_id);
            if (from != NULL && from->kingdom_id == sim->kingdoms[recipient].id) {
                guard_post = held->from_id;
            } else if (to != NULL && to->kingdom_id == sim->kingdoms[recipient].id) {
                guard_post = held->to_id;
            }
        }
        CcSettlement *home = CcSimSettlementMutable(
            sim, guard_post != 0U ? guard_post : recipient_seat);
        int32_t levy = home != NULL ?
            WarClamp(home->population / 250, 8, 40) : 0;
        if (home != NULL && levy > home->population / 2) {
            levy = home->population / 2;
        }
        if (levy <= 0 || home == NULL || guard_post == 0U) {
            sim->war_party_count -= 1;
        } else {
            home->population -= levy;
            guarding->kingdom_id = sim->kingdoms[recipient].id;
            guarding->commander_character_id =
                CommandingResident(sim, guard_post);
            if (guarding->commander_character_id == 0U) {
                guarding->commander_character_id =
                    CommandingResident(sim, recipient_seat);
            }
            guarding->home_settlement_id = recipient_seat;
            guarding->current_settlement_id = guard_post;
            guarding->members = levy;
            guarding->order = CC_WAR_ORDER_CONTROL_ROUTE;
            guarding->order_route_id = border;
        }
    }
}

/* Peace arrives by courier, which physically travelled; both companies stand
   down where they are and walk home. */
void CcSimRetireWarPartiesAtPeace(CcSim *sim, CcId first, CcId second)
{
    if (sim == NULL || sim->schema_version < 94U) return;
    for (int32_t i = 0; i < sim->war_party_count; ++i) {
        CcWarParty *party = &sim->war_parties[i];
        if (party->kingdom_id != first && party->kingdom_id != second) continue;
        party->order = CC_WAR_ORDER_WITHDRAW;
        party->order_route_id = 0U;
    }
}

/* Sealed letters. A report or an order does nothing until it reaches its
   recipient. */
CcId CcSimCreateDispatch(CcSim *sim, CcDispatchKind kind, CcId route_id,
                         CcId war_party_id, CcId origin_settlement_id,
                         CcId recipient_settlement_id)
{
    if (sim == NULL || sim->schema_version < 94U ||
        sim->dispatch_count >= CC_MAX_DISPATCHES) return 0U;
    CcDispatch *dispatch = &sim->dispatches[sim->dispatch_count];
    *dispatch = (CcDispatch){0};
    dispatch->id = CcMakeId(CC_ENTITY_CHARACTER, sim->next_entity_serial++);
    dispatch->kind = kind;
    dispatch->route_id = route_id;
    dispatch->war_party_id = war_party_id;
    dispatch->origin_settlement_id = origin_settlement_id;
    dispatch->recipient_settlement_id = recipient_settlement_id;
    dispatch->issued_day = sim->current_day;
    sim->dispatch_count += 1;
    return dispatch->id;
}

const CcDispatch *CcSimDispatch(const CcSim *sim, CcId id)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->dispatch_count; ++i) {
        if (sim->dispatches[i].id == id) return &sim->dispatches[i];
    }
    return NULL;
}

/* Delivery is the only place an effect happens. Reports become dated,
   sourced events the recipient can read; orders reach exactly one named
   party, and a withdrawal lifts the checkpoint only because the party walks
   home. */
void CcSimDeliverDispatch(CcSim *sim, CcId dispatch_id)
{
    if (sim == NULL || sim->schema_version < 94U) return;
    CcDispatch *dispatch = NULL;
    for (int32_t i = 0; i < sim->dispatch_count; ++i) {
        if (sim->dispatches[i].id == dispatch_id) {
            dispatch = &sim->dispatches[i];
            break;
        }
    }
    if (dispatch == NULL || dispatch->delivered) return;
    dispatch->delivered = true;
    dispatch->in_player_cargo = false;

    const CcSettlement *recipient = CcSimSettlement(
        sim, dispatch->recipient_settlement_id);
    const CcRoute *road = CcSimRoute(sim, dispatch->route_id);
    CcWarParty *party = WarPartyMutable(sim, dispatch->war_party_id);
    const CcSettlement *from = CcSimSettlement(
        sim, dispatch->origin_settlement_id);
    const CcSettlement *to = road != NULL ?
        CcSimSettlement(sim, road->to_id) : NULL;
    char text[CC_EVENT_TEXT_CAPACITY];
    switch (dispatch->kind) {
    case CC_DISPATCH_ROAD_REPORT:
        if (road != NULL && recipient != NULL) {
            (void)snprintf(
                text, sizeof(text),
                "A sealed report from %.20s reaches %.20s: the %.20s-%.20s road is broken.",
                from != NULL ? from->name : "the far border",
                recipient->name,
                CcSimSettlement(sim, road->from_id) != NULL ?
                    CcSimSettlement(sim, road->from_id)->name : "western",
                to != NULL ? to->name : "eastern");
            CcSimPushEvent(sim, CC_EVENT_ROUTE_CLOSED, road->id, road->id,
                      0U, (int32_t)recipient->kingdom_id, text);
        }
        break;
    case CC_DISPATCH_WITHDRAW_ORDER:
        if (party != NULL) {
            party->order = CC_WAR_ORDER_WITHDRAW;
            party->order_route_id = 0U;
            (void)snprintf(
                text, sizeof(text),
                "A sealed order reaches %.20s: the company turns for home.",
                recipient != NULL ? recipient->name : "the camp");
            CcSimPushEvent(sim, CC_EVENT_KINGDOM_ACTION,
                      party->kingdom_id, dispatch->recipient_settlement_id,
                      0U, party->members, text);
        }
        break;
    case CC_DISPATCH_CROSSING_PERMIT:
        if (party != NULL) {
            party->permits_player = true;
            (void)snprintf(
                text, sizeof(text),
                "A sealed permit reaches %.20s: the chain lifts for one carriage.",
                recipient != NULL ? recipient->name : "the gate");
            CcSimPushEvent(sim, CC_EVENT_KINGDOM_ACTION,
                      party->kingdom_id, dispatch->recipient_settlement_id,
                      0U, party->members, text);
        }
        break;
    }
}
