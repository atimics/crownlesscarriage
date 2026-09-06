#include "sim/cc_sim.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AgentStats {
    int32_t repairs;
    int32_t repair_failures;
    int32_t travel_attempts;
    int32_t travel_successes;
} AgentStats;


static CcId BestDestination(const CcSim *sim)
{
    CcId destination = 0U;
    int32_t best_closed = -1;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (place->id == sim->player.location_id ||
            CcSettlementIsAbandoned(place)) continue;
        int32_t closed = 0;
        for (int32_t route = 0; route < sim->route_count; ++route) {
            const CcRoute *road = &sim->routes[route];
            if (road->closed &&
                (road->from_id == place->id || road->to_id == place->id)) {
                closed += 1;
            }
        }
        if (closed > best_closed) {
            best_closed = closed;
            destination = place->id;
        }
    }
    return destination;
}

static bool RepairAtLocation(CcSim *sim, AgentStats *stats)
{
    if (sim->player.coins < 18) return false;
    char error[192];
    for (int32_t i = 0; i < sim->route_count; ++i) {
        CcRoute *route = &sim->routes[i];
        if (!route->closed ||
            (route->from_id != sim->player.location_id &&
             route->to_id != sim->player.location_id)) continue;
        CcCommand repair = {
            .kind = CC_COMMAND_REPAIR_ROUTE,
            .target_id = route->id,
            .amount = 2
        };
        if (CcSimApply(sim, &repair, error, sizeof(error))) {
            stats->repairs += 1;
            return true;
        }
        stats->repair_failures += 1;
    }
    return false;
}

static bool TravelTowardWork(CcSim *sim, AgentStats *stats)
{
    CcId destination = BestDestination(sim);
    if (destination == 0U) return false;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (route->closed) continue;
        CcId next = route->from_id == sim->player.location_id ? route->to_id :
                     route->to_id == sim->player.location_id ? route->from_id : 0U;
        if (next == 0U) continue;
        if (next != destination && CcSimSettlement(sim, destination) != NULL) {
            const CcSettlement *target = CcSimSettlement(sim, destination);
            bool target_touches = false;
            for (int32_t j = 0; j < sim->route_count; ++j) {
                const CcRoute *candidate = &sim->routes[j];
                if (candidate->closed &&
                    (candidate->from_id == target->id || candidate->to_id == target->id)) {
                    target_touches = true;
                    break;
                }
            }
            if (!target_touches) continue;
        }
        CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = next};
        char error[192];
        stats->travel_attempts += 1;
        if (CcSimApply(sim, &travel, error, sizeof(error))) {
            stats->travel_successes += 1;
            return true;
        }
    }
    return false;
}

static void AdvanceAgent(CcSim *sim, AgentStats *stats)
{
    if (sim->journey.active) {
        if (sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
            CcCommand withdraw = {
                .kind = CC_COMMAND_WITHDRAW_ENCOUNTER,
                .amount = 0
            };
            char error[192];
            if (!CcSimApply(sim, &withdraw, error, sizeof(error))) {
                return;
            }
        } else if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
            CcCommand rest = {
                .kind = CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
                    CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP
            };
            char error[192];
            if (!CcSimApply(sim, &rest, error, sizeof(error))) {
                CcSimAdvanceDays(sim, 1);
            }
        } else {
            CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
        }
        return;
    }
    if (RepairAtLocation(sim, stats)) return;
    if (!TravelTowardWork(sim, stats)) CcSimAdvanceDays(sim, 1);
}

static bool ParsePositive(const char *text, int32_t *value)
{
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 1 || parsed > INT32_MAX) return false;
    *value = (int32_t)parsed;
    return true;
}

int main(int argc, char **argv)
{
    int32_t seeds = 8;
    int32_t years = 10;
    for (int32_t i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--seeds") == 0 && i + 1 < argc) {
            if (!ParsePositive(argv[++i], &seeds)) return EXIT_FAILURE;
        } else if (strcmp(argv[i], "--years") == 0 && i + 1 < argc) {
            if (!ParsePositive(argv[++i], &years)) return EXIT_FAILURE;
        } else {
            (void)fprintf(stderr, "Usage: %s [--seeds COUNT] [--years COUNT]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }
    (void)puts("seed,control_population,agent_population,control_prosperity,agent_prosperity,"
               "control_hunger,agent_hunger,control_active_settlements,agent_active_settlements,"
               "control_closed_routes,agent_closed_routes,repairs,repair_failures,"
               "travel_attempts,travel_successes");
    for (int32_t seed = 1; seed <= seeds; ++seed) {
        uint32_t world_seed = (uint32_t)seed * UINT32_C(0x9e3779b9);
        CcSim control;
        CcSim agent;
        CcSimInit(&control, world_seed);
        CcSimInit(&agent, world_seed);
        AgentStats stats = {0};
        int32_t target_day = control.current_day + years * 365;
        while (control.current_day < target_day) CcSimAdvanceDays(&control, 1);
        int64_t agent_steps = 0;
        while (agent.current_day < target_day || agent.journey.active) {
            AdvanceAgent(&agent, &stats);
            agent_steps += 1;
            if (agent_steps > (int64_t)years * 365 * 10000) {
                (void)fprintf(stderr, "agent stalled at seed %d day %d\n",
                              seed, agent.current_day);
                return EXIT_FAILURE;
            }
            if (agent.current_day > target_day + 365) break;
        }
        int32_t control_active = 0;
        int32_t agent_active = 0;
        int32_t control_closed = 0;
        int32_t agent_closed = 0;
        int32_t control_hunger = 0;
        int32_t agent_hunger = 0;
        int32_t control_population = 0;
        int32_t agent_population = 0;
        int32_t control_prosperity = 0;
        int32_t agent_prosperity = 0;
        for (int32_t i = 0; i < control.settlement_count; ++i) {
            control_active += !CcSettlementIsAbandoned(&control.settlements[i]);
            agent_active += !CcSettlementIsAbandoned(&agent.settlements[i]);
            control_hunger += control.settlements[i].hunger;
            agent_hunger += agent.settlements[i].hunger;
            control_population += control.settlements[i].population;
            agent_population += agent.settlements[i].population;
            control_prosperity += control.settlements[i].prosperity;
            agent_prosperity += agent.settlements[i].prosperity;
        }
        for (int32_t i = 0; i < control.route_count; ++i) {
            control_closed += control.routes[i].closed;
            agent_closed += agent.routes[i].closed;
        }
        char error[192];
        if (!CcSimValidate(&control, error, sizeof(error)) ||
            !CcSimValidate(&agent, error, sizeof(error))) return EXIT_FAILURE;
        (void)printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                     seed, control_population, agent_population,
                     control_prosperity / control.settlement_count,
                     agent_prosperity / agent.settlement_count,
                     control_hunger / control.settlement_count,
                     agent_hunger / agent.settlement_count,
                     control_active, agent_active, control_closed, agent_closed,
                     stats.repairs, stats.repair_failures,
                     stats.travel_attempts, stats.travel_successes);
    }
    return EXIT_SUCCESS;
}
