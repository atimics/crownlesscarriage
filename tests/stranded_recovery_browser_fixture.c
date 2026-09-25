#include "persistence/cc_save.h"
#include "sim/cc_census.h"
#include "sim/cc_sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CcId OtherEnd(const CcRoute *route, CcId town)
{
    if (route->from_id == town) return route->to_id;
    if (route->to_id == town) return route->from_id;
    return 0U;
}

int main(int argc, char **argv)
{
    if (argc != 3 ||
        (strcmp(argv[1], "recover") != 0 &&
         strcmp(argv[1], "foaling") != 0)) {
        (void)fprintf(stderr,
            "Usage: stranded_recovery_browser_fixture recover|foaling output\n");
        return 2;
    }
    CcSim *sim = malloc(sizeof(*sim));
    if (sim == NULL) return 1;
    CcSimInit(sim, UINT32_C(0xc0a71a9e));
    const CcId origin = sim->player.location_id;
    CcId middle = 0U, stable = 0U;
    CcRoute *first = NULL, *second = NULL;
    for (int32_t i = 0; i < sim->route_count && first == NULL; ++i) {
        CcId next = OtherEnd(&sim->routes[i], origin);
        if (next == 0U || !CcSimPlayerKnowsSettlement(sim, next)) continue;
        for (int32_t j = 0; j < sim->route_count; ++j) {
            CcId last = OtherEnd(&sim->routes[j], next);
            if (last == 0U || last == origin) continue;
            first = &sim->routes[i];
            second = &sim->routes[j];
            middle = next;
            stable = last;
            break;
        }
    }
    if (first == NULL || second == NULL) {
        (void)fprintf(stderr, "A charted two-leg road is missing.\n");
        free(sim);
        return 1;
    }
    first->closed = false;
    second->closed = false;
    first->condition = 80;
    second->condition = 80;
    CcSettlement *lost = CcSimSettlementMutable(sim, origin);
    CcSettlement *waypoint = CcSimSettlementMutable(sim, middle);
    CcSettlement *help = CcSimSettlementMutable(sim, stable);
    if (lost == NULL || waypoint == NULL || help == NULL) {
        free(sim);
        return 1;
    }
    lost->population = 0;
    lost->hunger = 100;
    lost->service_mask = 0U;
    lost->service_project = CC_SERVICE_NONE;
    lost->service_project_days = 0;
    lost->security = 0;
    lost->prosperity = 0;
    memset(lost->stock, 0, sizeof(lost->stock));
    for (int32_t i = 0; i < sim->character_count; ++i) {
        if (sim->characters[i].current_settlement_id == origin)
            sim->characters[i].current_settlement_id = middle;
    }
    waypoint->service_mask &= ~(UINT32_C(1) << CC_SERVICE_STABLE);
    help->population = 200;
    if (!CcSettlementHasService(help, CC_SERVICE_STABLE) &&
        CcSettlementServiceCount(help) >= CcSettlementServiceCapacity(help->size)) {
        for (int32_t service = 0; service < CC_SERVICE_COUNT; ++service) {
            if ((help->service_mask & (UINT32_C(1) << (uint32_t)service)) != 0U) {
                help->service_mask &= ~(UINT32_C(1) << (uint32_t)service);
                break;
            }
        }
    }
    help->service_mask |= UINT32_C(1) << CC_SERVICE_STABLE;
    help->stock[CC_GOOD_WHEAT] = 100;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (sim->settlements[i].id != stable) continue;
        CcSettlementKnowledge *rumor = &sim->player.settlement_knowledge[i];
        rumor->kingdom_id = help->kingdom_id;
        rumor->learned_day = sim->current_day;
        rumor->source = CC_PLAYER_KNOWLEDGE_RUMOR;
        break;
    }
    sim->player.cargo[CC_GOOD_WHEAT] = 0;
    sim->player.cargo[CC_GOOD_WOOD] = 2;
    sim->player.feed_tray_wheat = 0;
    sim->horse_team[0].health = 65;
    sim->horse_team[0].hunger = 35;
    sim->horse_team[0].fatigue = 35;
    if (strcmp(argv[1], "foaling") == 0) {
        CcHorse stallion = sim->horse_team[0];
        sim->horse_team[0] = sim->horse_team[1];
        sim->horse_team[1] = stallion;
        sim->horse_team[0].pregnant_by_id = stallion.id;
        sim->horse_team[0].pregnancy_days_remaining = 20;
    }
    CcCensusReconcile(sim);
    char error[192] = "";
    CcHorseCarePreview care = {0};
    if (!CcSimHorseCarePreview(sim, &care) || care.available ||
        !CcSimValidate(sim, error, sizeof(error))) {
        (void)fprintf(stderr, "Invalid abandoned-town fixture: %s\n", error);
        free(sim);
        return 1;
    }
    unsigned char *bytes = NULL;
    size_t length = 0;
    if (!CcSaveEncode(sim, &bytes, &length, error, sizeof(error))) {
        (void)fprintf(stderr, "%s\n", error);
        free(sim);
        return 1;
    }
    FILE *file = fopen(argv[2], "wb");
    if (file == NULL || fwrite(bytes, 1, length, file) != length) {
        (void)fprintf(stderr, "The synthetic world could not be written.\n");
        if (file != NULL) fclose(file);
        CcSaveFreeBuffer(bytes);
        free(sim);
        return 1;
    }
    fclose(file);
    CcSaveFreeBuffer(bytes);
    (void)printf("%s -> %s -> %s\n", lost->name, waypoint->name, help->name);
    free(sim);
    return 0;
}
