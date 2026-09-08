#include "sim/cc_archive_staff.h"
#include "sim/cc_archive_internal.h"
#include "sim/cc_food_economy_internal.h"

#include <string.h>

static int32_t MaximumI32(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t MinimumI32(int32_t a, int32_t b) { return a < b ? a : b; }

int32_t CcArchiveSpareGrain(const CcSim *sim, const CcSettlement *place)
{
    if (place == NULL) return 0;
    if (sim->schema_version < 58U) return MaximumI32(
        0, place->stock[CC_GOOD_WHEAT] - CcEconomyWeeklyFoodUse(sim, place) * 2);
    int32_t rations = CcEconomyNutritionRations(place->stock, CC_NUTRITION_CIVILIAN);
    int32_t spare = MaximumI32(0, rations - CcEconomyWeeklyFoodUse(sim, place) * 2);
    /* Two wheat units provide one civilian ration. */
    return MinimumI32(place->stock[CC_GOOD_WHEAT], spare * 2);
}

const CcSettlement *CcArchiveSeat(const CcSim *sim)
{
    if (sim == NULL) return NULL;
    const CcSettlement *fallback = NULL;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (CcSettlementIsAbandoned(place)) continue;
        if (strcmp(place->name, "Gloamgate") == 0) return place;
        if (fallback == NULL &&
            (place->function == CC_SETTLEMENT_MARKET ||
             place->function == CC_SETTLEMENT_CAPITAL)) {
            fallback = place;
        }
    }
    return fallback;
}

CcMaterialChainSnapshot CcSimMaterialChainSnapshot(const CcSim *sim)
{
    CcMaterialChainSnapshot snapshot = {0};
    const CcSettlement *place = CcArchiveSeat(sim);
    if (sim == NULL || place == NULL) {
        snapshot.blocker = CC_MATERIAL_CHAIN_NO_SCRIBES;
        return snapshot;
    }
    snapshot.scriptorium_id = place->id;
    snapshot.scribes = sim->archives.scribes;
    snapshot.wheat = place->stock[CC_GOOD_WHEAT];
    snapshot.paper = place->stock[CC_GOOD_PAPER];
    snapshot.tools = place->stock[CC_GOOD_TOOLS];
    snapshot.iron = place->stock[CC_GOOD_IRON];
    snapshot.incoming_tools = CcSimIncomingGood(
        sim, place->id, CC_GOOD_TOOLS);
    snapshot.incoming_iron = CcSimIncomingGood(
        sim, place->id, CC_GOOD_IRON);
    bool binding_available = false;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *vault = &sim->settlements[i];
        if (CcSettlementIsAbandoned(vault)) continue;
        int64_t gold = (int64_t)snapshot.gold +
            vault->stock[CC_GOOD_GOLD];
        int64_t gems = (int64_t)snapshot.gems +
            vault->stock[CC_GOOD_GEMS];
        snapshot.gold = gold > CC_SIM_MAX_UNITS ?
            CC_SIM_MAX_UNITS : (int32_t)gold;
        snapshot.gems = gems > CC_SIM_MAX_UNITS ?
            CC_SIM_MAX_UNITS : (int32_t)gems;
        if (vault->stock[CC_GOOD_GOLD] > 0 &&
            vault->stock[CC_GOOD_GEMS] > 0) {
            binding_available = true;
        }
    }
    int32_t scribe_grain = CcArchiveSpareGrain(sim, place);
    snapshot.blocker = snapshot.scribes <= 0 ?
        CC_MATERIAL_CHAIN_NO_SCRIBES :
        !binding_available ? CC_MATERIAL_CHAIN_BINDING :
        snapshot.tools <= 0 ? CC_MATERIAL_CHAIN_TOOLS :
        scribe_grain < 2 ? CC_MATERIAL_CHAIN_GRAIN :
        snapshot.paper <= 0 ? CC_MATERIAL_CHAIN_PAPER :
        CC_MATERIAL_CHAIN_READY;
    return snapshot;
}

const char *CcMaterialChainBlockerName(CcMaterialChainBlocker blocker)
{
    switch (blocker) {
        case CC_MATERIAL_CHAIN_READY: return "ready";
        case CC_MATERIAL_CHAIN_NO_SCRIBES: return "scribes";
        case CC_MATERIAL_CHAIN_GRAIN: return "grain";
        case CC_MATERIAL_CHAIN_PAPER: return "paper";
        case CC_MATERIAL_CHAIN_TOOLS: return "tools";
        case CC_MATERIAL_CHAIN_BINDING: return "binding";
    }
    return "unknown";
}


CcArchiveWorkPlan CcSimArchiveWorkPlan(const CcSim *sim)
{
    CcArchiveWorkPlan plan = {0};
    if (sim == NULL) return plan;
<<<<<<< HEAD
    plan.eligible_scribes = sim->archives.scribes;
    if (sim->schema_version >= 84U && sim->archive_training_week == (sim->current_day + 6) / 7 &&
=======
    plan.eligible_scribes = CcSimArchiveStaffCount(sim);
    if (sim->schema_version >= 80U && sim->archive_training_week == (sim->current_day + 6) / 7 &&
>>>>>>> 78e1c5f (Appoint named archive staff and record their first work)
        plan.eligible_scribes > 0) plan.eligible_scribes -= 1;
    plan.recording_ready = plan.eligible_scribes > 0;
    if (sim->schema_version < 34U) return plan;
    const CcSettlement *seat = CcArchiveSeat(sim);
    plan.seat_id = seat != NULL ? seat->id : 0U;
    plan.eligible_scribes = MinimumI32(plan.eligible_scribes, CcArchiveSpareGrain(sim, seat) / 2);
    plan.wheat_required = plan.eligible_scribes * 2;
    plan.recording_ready = plan.eligible_scribes > 0 &&
        seat->stock[CC_GOOD_PAPER] > 0 && seat->stock[CC_GOOD_TOOLS] > 0 &&
        (sim->schema_version < 81U || !sim->archive_staff.active ||
         (seat->stock[CC_GOOD_GOLD] > 0 && seat->stock[CC_GOOD_GEMS] > 0));
    return plan;
}

static int32_t ArchiveSettlementSlot(const CcSim *sim, CcId id)
{
    const CcSettlement *town = CcSimSettlement(sim, id);
    return town != NULL ? (int32_t)(town - sim->settlements) : -1;
}

/* Find solvent crowns connected to the active archive by open roads. */
CcArchiveFundingPlan CcSimArchiveFundingPlan(const CcSim *sim)
{
    CcArchiveFundingPlan plan = {0};
    if (sim == NULL || sim->schema_version < 56U) {
        plan.blocker = CC_ARCHIVE_FUNDING_UNAVAILABLE;
        return plan;
    }
    const CcSettlement *archive = CcArchiveSeat(sim);
    if (archive == NULL || CcSettlementIsAbandoned(archive)) {
        plan.blocker = CC_ARCHIVE_FUNDING_NO_SEAT;
        return plan;
    }
    plan.seat_id = archive->id;
    CcMoney funding = 50 - sim->iron_ledger_reserve;
    if (funding <= 0) {
        plan.blocker = CC_ARCHIVE_FUNDING_LEDGER_FUNDED;
        return plan;
    }
    if (funding > (sim->schema_version >= 58U ? 50 : 10)) {
        plan.blocker = CC_ARCHIVE_FUNDING_TOP_UP_LIMIT;
        return plan;
    }
    if (sim->schema_version >= 58U) {
        for (int32_t k = 0; k < sim->kingdom_count; ++k) {
            const CcKingdom *host = &sim->kingdoms[k];
            if (host->id == archive->kingdom_id && host->treasury >= 800) {
                plan.donor_count = 1;
                plan.donor_ids[0] = host->id;
                plan.shares[0] = plan.total = funding;
                return plan;
            }
        }
    }
    bool reached[CC_MAX_SETTLEMENTS] = {false};
    int32_t archive_slot = ArchiveSettlementSlot(sim, archive->id);
    if (archive_slot < 0) {
        plan.blocker = CC_ARCHIVE_FUNDING_NO_SEAT;
        return plan;
    }
    reached[archive_slot] = true;
    for (int32_t pass = 0; pass < sim->settlement_count; ++pass) {
        for (int32_t r = 0; r < sim->route_count; ++r) {
            const CcRoute *road = &sim->routes[r];
            if (road->closed) continue;
            int32_t from = ArchiveSettlementSlot(sim, road->from_id);
            int32_t to = ArchiveSettlementSlot(sim, road->to_id);
            if (from < 0 || to < 0 ||
                CcSettlementIsAbandoned(&sim->settlements[from]) ||
                CcSettlementIsAbandoned(&sim->settlements[to])) continue;
            if (reached[from] || reached[to]) reached[from] = reached[to] = true;
        }
    }
    int32_t donors[2] = {-1, -1};
    int32_t count = 0;
    for (int32_t k = 0; k < sim->kingdom_count && count < 2; ++k) {
        if (sim->kingdoms[k].treasury < 800) continue;
        for (int32_t town = 0; town < sim->settlement_count; ++town) {
            if (reached[town] && sim->settlements[town].kingdom_id == sim->kingdoms[k].id) {
                donors[count++] = k;
                break;
            }
        }
    }
    if (count < 2) {
        plan.blocker = CC_ARCHIVE_FUNDING_CONNECTED_DONORS;
        return plan;
    }
    plan.donor_count = 2;
    plan.donor_ids[0] = sim->kingdoms[donors[0]].id;
    plan.donor_ids[1] = sim->kingdoms[donors[1]].id;
    plan.shares[0] = (funding + 1) / 2;
    plan.shares[1] = funding - plan.shares[0];
    plan.total = funding;
    return plan;
}

const char *CcArchiveFundingBlockerName(CcArchiveFundingBlocker blocker)
{
    switch (blocker) {
        case CC_ARCHIVE_FUNDING_READY: return "ready";
        case CC_ARCHIVE_FUNDING_UNAVAILABLE: return "unavailable";
        case CC_ARCHIVE_FUNDING_NO_SEAT: return "archive_seat";
        case CC_ARCHIVE_FUNDING_LEDGER_FUNDED: return "ledger_funded";
        case CC_ARCHIVE_FUNDING_TOP_UP_LIMIT: return "top_up_limit";
        case CC_ARCHIVE_FUNDING_CONNECTED_DONORS: return "connected_solvent_donors";
    }
    return "unknown";
}

CcArchiveRecoveryWindow CcSimArchiveRecoveryWindow(const CcSim *sim)
{
    CcArchiveRecoveryWindow window = {CC_ARCHIVE_RECOVERY_UNAVAILABLE, -1};
    if (sim == NULL || sim->schema_version < 56U) return window;
    if (sim->archives.scribes > 0) window.gate = CC_ARCHIVE_RECOVERY_STAFFED;
    else if (sim->iron_ledger_reserve >= 50) window.gate = CC_ARCHIVE_RECOVERY_LEDGER_FUNDED;
    else if (sim->archives.dead_since_day <= 0) window.gate = CC_ARCHIVE_RECOVERY_SILENCE_UNDATED;
    else {
        int64_t wait_end = (int64_t)sim->archives.dead_since_day + 1825;
        window.first_eligible_day = ((wait_end + 6) / 7) * 7;
        window.gate = sim->current_day < wait_end ? CC_ARCHIVE_RECOVERY_WAITING :
            sim->current_day % 7 != 0 ? CC_ARCHIVE_RECOVERY_CALENDAR : CC_ARCHIVE_RECOVERY_DUE;
    }
    return window;
}

const char *CcArchiveRecoveryGateName(CcArchiveRecoveryGate gate)
{
    switch (gate) {
        case CC_ARCHIVE_RECOVERY_DUE: return "due";
        case CC_ARCHIVE_RECOVERY_UNAVAILABLE: return "unavailable";
        case CC_ARCHIVE_RECOVERY_STAFFED: return "staffed";
        case CC_ARCHIVE_RECOVERY_LEDGER_FUNDED: return "ledger_funded";
        case CC_ARCHIVE_RECOVERY_SILENCE_UNDATED: return "silence_undated";
        case CC_ARCHIVE_RECOVERY_WAITING: return "waiting";
        case CC_ARCHIVE_RECOVERY_CALENDAR: return "calendar";
    }
    return "unknown";
}
