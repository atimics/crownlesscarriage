#include "sim/cc_mine.h"
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
    int32_t jobs_accepted;
    int32_t jobs_completed;
    int32_t combats_initiated;
    int32_t combats_won;
    int32_t combats_lost;
    int32_t route_jobs_accepted;
    int32_t relief_jobs_accepted;
    int32_t jobs_resolved;
    int32_t jobs_expired;
    int32_t jobs_abandoned;
    int32_t jobs_unresolved;
    CcId tracked_job_id;
    int32_t wip_limit;
    int32_t road_site_passes;
    /* Oracle exchange probes: repair order commutativity measurement. */
    int32_t exchange_probes;
    int32_t exchange_pairs;
    int32_t exchange_order_matters;
} AgentStats;

/* Policy configuration. Greedy is the historical road steward; oracle adds
   one-step lookahead with a greedy rollout continuation. */
typedef struct AgentConfig {
    bool oracle;
    int32_t horizon;         /* rollout days per candidate */
    int32_t exchange_probes; /* max order-swap probes per seed */
} AgentConfig;

static const AgentConfig kGreedyConfig = { false, 0, 0 };
static AgentConfig g_config = { false, 60, 0 };
/* Printed in failure reproduce lines; set from the parsed command line. */
static char g_reproduce_flags[192] = "";

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

static int32_t ClosedRouteCount(const CcSim *sim)
{
    int32_t closed = 0;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        closed += sim->routes[i].closed;
    }
    return closed;
}

static void ObserveJobLifecycle(CcSim *sim, AgentStats *stats)
{
    if (stats->tracked_job_id == 0U ||
        sim->player.accepted_situation_id == stats->tracked_job_id) return;
    const CcSituation *situation = CcSimSituation(
        sim, stats->tracked_job_id);
    if (situation == NULL) {
        stats->jobs_unresolved += 1;
    } else if (situation->status == CC_SITUATION_RESOLVED) {
        stats->jobs_resolved += 1;
    } else if (situation->status == CC_SITUATION_FAILED) {
        if (situation->end_reason == CC_QUEST_END_EXPIRED) {
            stats->jobs_expired += 1;
        } else {
            stats->jobs_abandoned += 1;
        }
    } else {
        stats->jobs_unresolved += 1;
    }
    stats->tracked_job_id = 0U;
}

static bool AcceptRouteJobAtLocation(CcSim *sim, AgentStats *stats)
{
    char error[192];
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            situation->kind != CC_SITUATION_ROUTE_REPAIR ||
            CcSimSituationOfferSettlementId(sim, situation) !=
                sim->player.location_id ||
            !CcSimSituationCanAccept(sim, situation) ||
            sim->player.coins < 18) continue;
        const CcRoute *target_route = CcSimRoute(sim, situation->target_id);
        if (target_route == NULL || !target_route->closed ||
            (target_route->from_id != sim->player.location_id &&
             target_route->to_id != sim->player.location_id)) continue;
        CcCommand accept = {
            .kind = CC_COMMAND_ACCEPT_SITUATION,
            .target_id = situation->id
        };
        if (CcSimApply(sim, &accept, error, sizeof(error))) {
            stats->jobs_accepted += 1;
            stats->route_jobs_accepted += 1;
            stats->tracked_job_id = situation->id;
            return true;
        }
    }
    return false;
}

static bool AcceptReliefJobAtLocation(CcSim *sim, AgentStats *stats)
{
    if (stats->tracked_job_id != 0U || stats->wip_limit < 1) return false;
    char error[192];
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            situation->kind != CC_SITUATION_RELIEF_DELIVERY ||
            CcSimSituationOfferSettlementId(sim, situation) !=
                sim->player.location_id ||
            !CcSimSituationCanAccept(sim, situation)) continue;
        bool reachable = false;
        for (int32_t route = 0; route < sim->route_count; ++route) {
            const CcRoute *road = &sim->routes[route];
            if (!road->closed &&
                ((road->from_id == sim->player.location_id &&
                  road->to_id == situation->target_id) ||
                 (road->to_id == sim->player.location_id &&
                  road->from_id == situation->target_id)) &&
                sim->current_day + road->travel_days + 100 <
                    situation->deadline_day) {
                reachable = true;
                break;
            }
        }
        if (!reachable) continue;
        CcCommand accept = {
            .kind = CC_COMMAND_ACCEPT_SITUATION,
            .target_id = situation->id
        };
        if (CcSimApply(sim, &accept, error, sizeof(error))) {
            stats->jobs_accepted += 1;
            stats->relief_jobs_accepted += 1;
            stats->tracked_job_id = situation->id;
            return true;
        }
    }
    return false;
}

static bool DeliverAcceptedRelief(CcSim *sim, AgentStats *stats)
{
    const CcSituation *situation = CcSimAcceptedSituation(sim);
    if (situation == NULL || situation->kind != CC_SITUATION_RELIEF_DELIVERY ||
        sim->player.location_id != situation->target_id ||
        sim->resolved_journey_situation_id != situation->id) return false;
    int32_t amount = situation->quantity - situation->progress;
    if (amount <= 0) return false;
    CcCommand deliver = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = -amount
    };
    char error[192];
    if (!CcSimApply(sim, &deliver, error, sizeof(error))) return false;
    stats->jobs_completed += 1;
    return true;
}

/* ------------------------------------------------------------------ */
/* Oracle policy: one-step lookahead with greedy rollout continuation. */
/* ------------------------------------------------------------------ */

/* Ranking score over a rolled-out world state. Used only to order oracle
   candidate actions; the reported endpoint metrics stay raw. Population
   dominates; welfare means and connectivity break ties; abandonment and
   closed routes carry explicit penalties. */
static int64_t OracleWelfareScore(const CcSim *sim)
{
    CcWelfareSnapshot welfare = CcSimWelfareSnapshot(sim);
    int32_t closed = ClosedRouteCount(sim);
    double score = (double)welfare.population
        + 10.0 * (welfare.prosperity + welfare.security - welfare.hunger)
        - 25.0 * (double)closed
        - 100.0 * (double)welfare.abandoned_settlements;
    return (int64_t)score;
}

static void AdvanceAgent(CcSim *sim, AgentStats *stats, const AgentConfig *cfg,
                         CcSim *roll);

/* Run the greedy steward on a forked world for `horizon` days. */
static void ContinueGreedy(CcSim *sim, int32_t horizon, int32_t wip_limit)
{
    AgentStats stats = { .wip_limit = wip_limit };
    int32_t start_day = sim->current_day;
    int64_t steps = 0;
    const int64_t limit = (int64_t)horizon * 200 + 4000;
    while (sim->current_day < start_day + horizon) {
        int32_t day_before = sim->current_day;
        bool journey_before = sim->journey.active;
        AdvanceAgent(sim, &stats, &kGreedyConfig, NULL);
        if (!journey_before && !sim->journey.active &&
            sim->current_day == day_before) {
            CcSimAdvanceDays(sim, 1);
        }
        if (++steps > limit) break;
    }
}

/* Fork `base` into `roll`, apply `first` (when non-NULL), roll the greedy
   steward forward for the configured horizon, and score the endpoint.
   Returns INT64_MIN when the first command is rejected. */
static int64_t RolloutScore(const CcSim *base, CcSim *roll,
                            const CcCommand *first, const AgentConfig *cfg,
                            int32_t wip_limit)
{
    memcpy(roll, base, sizeof(*roll));
    char error[192];
    if (first != NULL && !CcSimApply(roll, first, error, sizeof(error))) {
        return INT64_MIN;
    }
    ContinueGreedy(roll, cfg->horizon, wip_limit);
    return OracleWelfareScore(roll);
}

static bool RepairRouteInSim(CcSim *sim, CcId route_id)
{
    CcCommand repair = {
        .kind = CC_COMMAND_REPAIR_ROUTE,
        .target_id = route_id,
        .amount = 2
    };
    char error[192];
    return CcSimApply(sim, &repair, error, sizeof(error));
}

/* Exchange probe: does repairing A then B score the same as B then A?
   A score difference means repair order matters at this decision point —
   the practical failure mode of exchange-convex (M-convex-like) value. */
static void ExchangeProbe(const CcSim *sim, CcSim *roll, AgentStats *stats,
                          const AgentConfig *cfg, CcId route_a, CcId route_b)
{
    stats->exchange_probes += 1;
    CcCommand first = {
        .kind = CC_COMMAND_REPAIR_ROUTE,
        .target_id = route_a,
        .amount = 2
    };
    int64_t score_ab = RolloutScore(sim, roll, &first, cfg, stats->wip_limit);
    if (score_ab == INT64_MIN) return;
    if (!RepairRouteInSim(roll, route_b)) return;
    ContinueGreedy(roll, cfg->horizon, stats->wip_limit);
    score_ab = OracleWelfareScore(roll);

    CcCommand reversed = {
        .kind = CC_COMMAND_REPAIR_ROUTE,
        .target_id = route_b,
        .amount = 2
    };
    int64_t score_ba = RolloutScore(sim, roll, &reversed, cfg,
                                    stats->wip_limit);
    if (score_ba == INT64_MIN) return;
    if (!RepairRouteInSim(roll, route_a)) return;
    ContinueGreedy(roll, cfg->horizon, stats->wip_limit);
    score_ba = OracleWelfareScore(roll);

    stats->exchange_pairs += 1;
    if (score_ab != score_ba) stats->exchange_order_matters += 1;
}

static bool GreedyRepairAtLocation(CcSim *sim, AgentStats *stats)
{
    if (sim->player.coins < 18 || stats->tracked_job_id != 0U ||
        stats->wip_limit < 1) return false;
    char error[192];
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    CcId required_route = accepted != NULL &&
        accepted->kind == CC_SITUATION_ROUTE_REPAIR ? accepted->target_id : 0U;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        CcRoute *route = &sim->routes[i];
        if (required_route != 0U && route->id != required_route) continue;
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
            if (sim->resolved_journey_situation_id == 0U &&
                sim->player.accepted_situation_id == 0U) {
                stats->jobs_completed += 1;
            }
            return true;
        }
        stats->repair_failures += 1;
    }
    return false;
}

/* Oracle repair: evaluate every adjacent closed route (plus doing nothing)
   by rolling a greedy continuation forward and scoring the endpoint. */
static bool OracleRepairAtLocation(CcSim *sim, AgentStats *stats,
                                   const AgentConfig *cfg, CcSim *roll)
{
    if (sim->player.coins < 18 || stats->tracked_job_id != 0U ||
        stats->wip_limit < 1) return false;
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    CcId required_route = accepted != NULL &&
        accepted->kind == CC_SITUATION_ROUTE_REPAIR ? accepted->target_id : 0U;

    CcId candidates[CC_MAX_ROUTES];
    int32_t count = 0;
    for (int32_t i = 0; i < sim->route_count && count < CC_MAX_ROUTES; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (required_route != 0U && route->id != required_route) continue;
        if (!route->closed ||
            (route->from_id != sim->player.location_id &&
             route->to_id != sim->player.location_id)) continue;
        candidates[count++] = route->id;
    }
    if (count == 0) return false;

    if (count >= 2 && stats->exchange_probes < cfg->exchange_probes) {
        ExchangeProbe(sim, roll, stats, cfg, candidates[0], candidates[1]);
    }

    int32_t best_index = -1;
    int64_t best_score = INT64_MIN;
    for (int32_t i = 0; i < count; ++i) {
        CcCommand repair = {
            .kind = CC_COMMAND_REPAIR_ROUTE,
            .target_id = candidates[i],
            .amount = 2
        };
        int64_t score = RolloutScore(sim, roll, &repair, cfg,
                                     stats->wip_limit);
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }
    if (best_index < 0) {
        stats->repair_failures += 1;
        return false;
    }
    /* Doing nothing only wins when it is strictly better than repairing. */
    int64_t idle_score = RolloutScore(sim, roll, NULL, cfg, stats->wip_limit);
    if (idle_score > best_score) return false;

    if (!RepairRouteInSim(sim, candidates[best_index])) {
        stats->repair_failures += 1;
        return false;
    }
    stats->repairs += 1;
    if (sim->resolved_journey_situation_id == 0U &&
        sim->player.accepted_situation_id == 0U) {
        stats->jobs_completed += 1;
    }
    return true;
}

static bool GreedyTravelTowardWork(CcSim *sim, AgentStats *stats)
{
    CcId destination = 0U;
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    if (accepted != NULL &&
        (accepted->kind == CC_SITUATION_RELIEF_DELIVERY ||
         accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY)) {
        destination = accepted->target_id;
    }
    if (destination == 0U) destination = BestDestination(sim);
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

/* Oracle travel: with an accepted delivery the destination is fixed and the
   greedy walk applies. Otherwise, score every reachable neighbour (plus
   staying put) by rolling a greedy continuation forward. */
static bool OracleTravelTowardWork(CcSim *sim, AgentStats *stats,
                                   const AgentConfig *cfg, CcSim *roll)
{
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    if (accepted != NULL &&
        (accepted->kind == CC_SITUATION_RELIEF_DELIVERY ||
         accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY)) {
        return GreedyTravelTowardWork(sim, stats);
    }
    if (ClosedRouteCount(sim) == 0) return false;

    CcId candidates[CC_MAX_ROUTES];
    int32_t count = 0;
    for (int32_t i = 0; i < sim->route_count && count < CC_MAX_ROUTES; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (route->closed) continue;
        CcId next = route->from_id == sim->player.location_id ? route->to_id :
                     route->to_id == sim->player.location_id ? route->from_id : 0U;
        if (next == 0U) continue;
        bool duplicate = false;
        for (int32_t j = 0; j < count; ++j) {
            if (candidates[j] == next) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) candidates[count++] = next;
    }
    if (count == 0) return false;

    int32_t best_index = -1;
    int64_t best_score = INT64_MIN;
    for (int32_t i = 0; i < count; ++i) {
        CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = candidates[i]};
        int64_t score = RolloutScore(sim, roll, &travel, cfg, stats->wip_limit);
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }
    /* Staying only wins when strictly better than every move. */
    int64_t idle_score = RolloutScore(sim, roll, NULL, cfg, stats->wip_limit);
    if (best_index < 0 || idle_score > best_score) return false;

    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = candidates[best_index]
    };
    char error[192];
    stats->travel_attempts += 1;
    if (!CcSimApply(sim, &travel, error, sizeof(error))) return false;
    stats->travel_successes += 1;
    return true;
}

static void AdvanceAgent(CcSim *sim, AgentStats *stats, const AgentConfig *cfg,
                         CcSim *roll)
{
    ObserveJobLifecycle(sim, stats);
    char error[192];
    if (sim->journey.active) {
        if (sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
            /* Journeys pause permanently at an unvisited mine branch until
               the company passes it; without this command the sweep
               stalls. Ordinary roadside sites do not block travel, so the
               pass is only issued at the mine branch itself. */
            if (sim->mine.phase == CC_MINE_NONE &&
                CcMineBranchSubtick(sim) >= 0 &&
                CcMineBranchSubtick(sim) == sim->journey.elapsed_subticks) {
                const CcRoadSite *site = CcSimJourneyRoadSiteStop(sim);
                if (site != NULL) {
                    CcCommand pass = {
                        .kind = CC_COMMAND_PASS_ROAD_SITE,
                        .target_id = site->id
                    };
                    if (CcSimApply(sim, &pass, error, sizeof(error))) {
                        stats->road_site_passes += 1;
                        return;
                    }
                }
            }
        }
        if (sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
            CcCommand command = {0};
            if (sim->journey.danger >= 30 &&
                sim->journey.danger <= 70 &&
                sim->journey.bargain_cost > 0) {
                command.kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT;
                stats->combats_initiated += 1;
                if (CcSimApply(sim, &command, error, sizeof(error)) &&
                    sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_COMBAT) {
                    stats->combats_won += 1;
                } else {
                    stats->combats_lost += 1;
                }
            } else if (sim->journey.danger > 70 ||
                       sim->journey.bargain_cost <= 0) {
                command.kind = CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE;
                if (!CcSimApply(sim, &command, error, sizeof(error))) {
                    command.kind = CC_COMMAND_WITHDRAW_ENCOUNTER;
                    (void)CcSimApply(sim, &command, error, sizeof(error));
                }
            } else {
                command.kind = CC_COMMAND_WITHDRAW_ENCOUNTER;
                (void)CcSimApply(sim, &command, error, sizeof(error));
            }
        } else if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
            CcCommand rest = {
                .kind = CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
                    CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP
            };
            if (!CcSimApply(sim, &rest, error, sizeof(error))) {
                CcSimAdvanceDays(sim, 1);
            }
        } else {
            CcSimAdvanceRuntimeTicks(
                sim, CC_WORLD_TICKS_PER_SECOND * 60);
        }
        return;
    }
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    if (accepted != NULL && accepted->deadline_day <= sim->current_day + 7) {
        CcCommand abandon = {
            .kind = CC_COMMAND_ABANDON_SITUATION,
            .target_id = accepted->id
        };
        char abandon_error[192];
        (void)CcSimApply(sim, &abandon, abandon_error, sizeof(abandon_error));
        return;
    }
    if (DeliverAcceptedRelief(sim, stats)) return;
    if (AcceptRouteJobAtLocation(sim, stats)) return;
    if (AcceptReliefJobAtLocation(sim, stats)) return;
    if (cfg->oracle && roll != NULL) {
        if (OracleRepairAtLocation(sim, stats, cfg, roll)) return;
        if (OracleTravelTowardWork(sim, stats, cfg, roll)) return;
    } else {
        if (GreedyRepairAtLocation(sim, stats)) return;
        if (GreedyTravelTowardWork(sim, stats)) return;
    }
    CcSimAdvanceDays(sim, 1);
}

static bool ParsePositive(const char *text, int32_t *value)
{
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 1 || parsed > INT32_MAX) return false;
    *value = (int32_t)parsed;
    return true;
}

static bool ParseNonNegative(const char *text, int32_t *value)
{
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 0 || parsed > INT32_MAX) return false;
    *value = (int32_t)parsed;
    return true;
}

/* Validate the first completed agent step at or beyond each annual boundary. */
static bool ValidateCheckpoint(const CcSim *sim, const char *role, int32_t seed,
    int32_t years, int32_t wip_limit, int64_t checkpoint_day)
{
    char error[192];
    if (CcSimValidate(sim, error, sizeof(error))) return true;
    (void)fprintf(stderr,
        "%s seed=%d world_seed=%" PRIu32 " checkpoint_day=%" PRId64
        " actual_day=%d schema=%" PRIu32 " generator=%" PRIu32
        " hash=%016" PRIx64 ": %s\n"
        "reproduce: crownless_agent_sweep --seed %d --years %d --wip-limit %d%s\n",
        role, seed, sim->world_seed, checkpoint_day, sim->current_day,
        sim->schema_version, sim->generator_version, CcSimHash(sim), error,
        seed, years, wip_limit, g_reproduce_flags);
    (void)fprintf(stderr,
        "player location=%" PRIu64 " accepted=%" PRIu64 " situations=%d"
        " cargo=%d/%d coins=%" PRId64 " reputation=%d treasure_slots=%d maps=%d/%d\n",
        sim->player.location_id, sim->player.accepted_situation_id,
        sim->situation_count, CcPlayerCargoUsed(&sim->player),
        sim->player.cargo_capacity, sim->player.coins, sim->player.reputation,
        sim->player.treasure_cargo_slots, CcPlayerMapCount(sim), sim->player.map_capacity);
    for (int32_t i = 0; i < sim->treasure_count && i < CC_MAX_TREASURES; ++i) {
        const CcTreasure *treasure = &sim->treasures[i];
        (void)fprintf(stderr,
            "treasure[%d] name=%.40s id=%" PRIu64 " owner=%" PRIu64
            " location=%" PRIu64 " maker=%" PRIu64
            " gold=%d gems=%d work=%d value=%d created=%d destroyed=%d\n",
            i, treasure->name, treasure->id, treasure->owner_id,
            treasure->location_id, treasure->maker_settlement_id,
            treasure->gold_content, treasure->gem_content, treasure->craft_work,
            treasure->appraised_value, treasure->created_day, treasure->destroyed ? 1 : 0);
    }
    return false;
}

static bool ValidateAnnualCheckpoint(const CcSim *sim, const char *role,
    int32_t seed, int32_t years, int32_t wip_limit, int64_t *next_day)
{
    if (sim->current_day < *next_day) return true;
    if (!ValidateCheckpoint(sim, role, seed, years, wip_limit, *next_day)) return false;
    /* One completed command may cross several boundaries; identify its real day. */
    *next_day += ((sim->current_day - *next_day) / 365 + 1) * 365;
    return true;
}

static void WriteReportHeader(FILE *output, bool oracle)
{
    (void)fputs("seed,control_population,agent_population,control_prosperity,agent_prosperity,"
               "control_hunger,agent_hunger,control_active_settlements,agent_active_settlements,"
               "control_closed_routes,agent_closed_routes,repairs,repair_failures,"
               "travel_attempts,travel_successes,jobs_accepted,jobs_completed,"
               "combats_initiated,combats_won,combats_lost,"
               "route_jobs_accepted,relief_jobs_accepted,jobs_resolved,"
               "jobs_expired,jobs_abandoned,jobs_unresolved,"
               "objective_loss,objective_pass,world_seed,target_day,control_day,agent_day,"
               "control_maximum_hunger,agent_maximum_hunger,"
               "control_population_weighted_hunger,agent_population_weighted_hunger,"
               "control_abandoned_settlements,agent_abandoned_settlements,"
               "schema_version,generator_version,control_hash,agent_hash", output);
    if (oracle) {
        /* Gap columns are oriented so positive means the oracle ended better:
           population/prosperity are oracle minus agent; hunger/closed routes
           are agent minus oracle. */
        (void)fputs(",oracle_population,oracle_prosperity,oracle_hunger,"
               "oracle_active_settlements,oracle_closed_routes,"
               "oracle_abandoned_settlements,oracle_repairs,oracle_jobs_completed,"
               "oracle_travel_successes,oracle_exchange_probes,oracle_exchange_pairs,"
               "oracle_exchange_order_matters,"
               "gap_population,gap_prosperity,gap_hunger,gap_closed_routes\n", output);
    } else {
        (void)fputc('\n', output);
    }
}

static int32_t AverageProsperity(const CcSim *sim)
{
    int64_t prosperity = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        prosperity += sim->settlements[i].prosperity;
    }
    return sim->settlement_count > 0 ?
        (int32_t)(prosperity / sim->settlement_count) : -1;
}

static void WriteReportRow(FILE *output, int32_t seed, int32_t target_day,
    const CcSim *control, const CcSim *agent, const AgentStats *stats,
    const CcSim *oracle, const AgentStats *oracle_stats, bool oracle_on)
{
    CcHungerSnapshot control_hunger = CcSimHungerSnapshot(control);
    CcHungerSnapshot agent_hunger = CcSimHungerSnapshot(agent);
    int32_t control_closed = ClosedRouteCount(control);
    int32_t agent_closed = ClosedRouteCount(agent);
    int32_t objective_loss = stats->jobs_expired * 10 +
        stats->jobs_abandoned * 10 + stats->jobs_unresolved * 20 +
        stats->combats_lost * 5;
    (void)fprintf(output, "%d,%" PRId64 ",%" PRId64 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                 seed, control_hunger.population, agent_hunger.population,
                 AverageProsperity(control), AverageProsperity(agent),
                 control_hunger.average,
                 agent_hunger.average,
                 control_hunger.inhabited_settlements, agent_hunger.inhabited_settlements, control_closed, agent_closed,
                 stats->repairs, stats->repair_failures,
                 stats->travel_attempts, stats->travel_successes,
                 stats->jobs_accepted, stats->jobs_completed,
                 stats->combats_initiated, stats->combats_won,
                 stats->combats_lost,
                 stats->route_jobs_accepted, stats->relief_jobs_accepted,
                 stats->jobs_resolved, stats->jobs_expired,
                 stats->jobs_abandoned, stats->jobs_unresolved,
                 objective_loss, objective_loss == 0 ? 1 : 0);
    (void)fprintf(output, ",%" PRIu32 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%" PRIu32 ",%" PRIu32 ",%016" PRIx64 ",%016" PRIx64,
        control->world_seed, target_day, control->current_day, agent->current_day,
        control_hunger.maximum, agent_hunger.maximum,
        control_hunger.population_weighted, agent_hunger.population_weighted,
        control_hunger.abandoned_settlements, agent_hunger.abandoned_settlements,
        control->schema_version, control->generator_version,
        CcSimHash(control), CcSimHash(agent));
    if (!oracle_on) {
        (void)fputc('\n', output);
        return;
    }
    CcHungerSnapshot oracle_hunger = CcSimHungerSnapshot(oracle);
    int32_t oracle_closed = ClosedRouteCount(oracle);
    int32_t oracle_prosperity = AverageProsperity(oracle);
    int32_t agent_prosperity = AverageProsperity(agent);
    (void)fprintf(output, ",%" PRId64 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%" PRId64 ",%d,%d,%d\n",
        oracle_hunger.population, oracle_prosperity, oracle_hunger.average,
        oracle_hunger.inhabited_settlements, oracle_closed,
        oracle_hunger.abandoned_settlements,
        oracle_stats->repairs, oracle_stats->jobs_completed,
        oracle_stats->travel_successes,
        oracle_stats->exchange_probes, oracle_stats->exchange_pairs,
        oracle_stats->exchange_order_matters,
        oracle_hunger.population - agent_hunger.population,
        oracle_prosperity - agent_prosperity,
        agent_hunger.average - oracle_hunger.average,
        agent_closed - oracle_closed);
}

/* Run one agent policy world to the target day with annual validation.
   Mirrors the historical agent loop, including the stall guard. */
static bool RunPolicyWorld(CcSim *world, AgentStats *stats,
    const AgentConfig *cfg, CcSim *roll, int32_t seed, int32_t years,
    int32_t wip_limit, int32_t target_day, const char *role)
{
    int64_t checkpoint = (int64_t)world->current_day + 365;
    if (!ValidateCheckpoint(world, role, seed, years, wip_limit,
                             world->current_day)) return false;
    int64_t steps = 0;
    while (world->current_day < target_day || world->journey.active) {
        int32_t day_before = world->current_day;
        bool journey_before = world->journey.active;
        AdvanceAgent(world, stats, cfg, roll);
        if (!journey_before && !world->journey.active &&
            world->current_day == day_before) {
            CcSimAdvanceDays(world, 1);
        }
        if (!ValidateAnnualCheckpoint(world, role, seed, years,
                wip_limit, &checkpoint)) return false;
        steps += 1;
        if (steps > (int64_t)years * 365 * 100) {
            (void)fprintf(stderr,
                          "%s stalled at seed %d day %d phase=%d active=%d encounter=%d location=%" PRIu64 "\n"
                          "  journey: origin=%" PRIu64 " dest=%" PRIu64 " route=%" PRIu64
                          " elapsed=%d total=%d pace=%d departure_day=%d situation=%" PRIu64 "\n",
                          role, seed, world->current_day,
                          (int32_t)world->journey.phase,
                          world->journey.active ? 1 : 0,
                          world->journey.encounter_triggered ? 1 : 0,
                          world->player.location_id,
                          world->journey.origin_id, world->journey.destination_id,
                          world->journey.route_id, world->journey.elapsed_subticks,
                          world->journey.total_subticks, (int32_t)world->journey.pace,
                          world->journey.departure_day, world->journey.situation_id);
            return false;
        }
        if (world->current_day > target_day + 365) break;
    }
    ObserveJobLifecycle(world, stats);
    return ValidateCheckpoint(world, role, seed, years, wip_limit, target_day);
}

int main(int argc, char **argv)
{
    int32_t seeds = 8;
    int32_t first_seed = 1;
    int32_t years = 10;
    int32_t wip_limit = 1;
    for (int32_t i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--wip-limit") == 0 && i + 1 < argc) {
            if (!ParsePositive(argv[++i], &wip_limit)) return EXIT_FAILURE;
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            if (!ParsePositive(argv[++i], &first_seed)) return EXIT_FAILURE;
            seeds = 1;
        } else if (strcmp(argv[i], "--seeds") == 0 && i + 1 < argc) {
            if (!ParsePositive(argv[++i], &seeds)) return EXIT_FAILURE;
        } else if (strcmp(argv[i], "--years") == 0 && i + 1 < argc) {
            if (!ParsePositive(argv[++i], &years)) return EXIT_FAILURE;
        } else if (strcmp(argv[i], "--oracle") == 0) {
            g_config.oracle = true;
        } else if (strcmp(argv[i], "--oracle-horizon") == 0 && i + 1 < argc) {
            if (!ParsePositive(argv[++i], &g_config.horizon)) return EXIT_FAILURE;
        } else if (strcmp(argv[i], "--exchange-probes") == 0 && i + 1 < argc) {
            if (!ParseNonNegative(argv[++i], &g_config.exchange_probes)) {
                return EXIT_FAILURE;
            }
        } else {
            (void)fprintf(stderr, "Usage: %s [--seed NUMBER | --seeds COUNT] [--years COUNT] [--wip-limit COUNT]\n"
                          "       %*s [--oracle] [--oracle-horizon DAYS] [--exchange-probes COUNT]\n",
                          argv[0], (int)strlen(argv[0]), "");
            return EXIT_FAILURE;
        }
    }
    if ((int64_t)first_seed + seeds - 1 > INT32_MAX ||
        years > (INT32_MAX - 366) / 365) {
        (void)fprintf(stderr, "Seed range or duration exceeds the supported day/index range.\n");
        return EXIT_FAILURE;
    }
    if (g_config.oracle) {
        (void)snprintf(g_reproduce_flags, sizeof(g_reproduce_flags),
                       " --oracle --oracle-horizon %d --exchange-probes %d",
                       g_config.horizon, g_config.exchange_probes);
    }
    CcSim *roll = NULL;
    if (g_config.oracle) {
        roll = malloc(sizeof(*roll));
        if (roll == NULL) {
            (void)fprintf(stderr, "Out of memory for oracle rollouts.\n");
            return EXIT_FAILURE;
        }
    }
    WriteReportHeader(stdout, g_config.oracle);
    for (int64_t index = first_seed; index < (int64_t)first_seed + seeds; ++index) {
        int32_t seed = (int32_t)index;
        uint32_t world_seed = (uint32_t)seed * UINT32_C(0x9e3779b9);
        CcSim control;
        CcSimInit(&control, world_seed);
        CcSim agent;
        CcSimInit(&agent, world_seed);
        AgentStats stats = {.wip_limit = wip_limit};
        int32_t target_day = control.current_day + years * 365;
        if (!ValidateCheckpoint(&control, "control", seed, years, wip_limit, control.current_day))
            return EXIT_FAILURE;
        int64_t control_checkpoint = (int64_t)control.current_day + 365;
        while (control.current_day < target_day) {
            CcSimAdvanceDays(&control, 1);
            if (!ValidateAnnualCheckpoint(&control, "control", seed, years,
                    wip_limit, &control_checkpoint)) return EXIT_FAILURE;
        }
        if (!RunPolicyWorld(&agent, &stats, &kGreedyConfig, NULL, seed, years,
                wip_limit, target_day, "agent")) return EXIT_FAILURE;
        if (!ValidateCheckpoint(&control, "control", seed, years, wip_limit, target_day))
            return EXIT_FAILURE;
        CcSim oracle;
        AgentStats oracle_stats = {.wip_limit = wip_limit};
        if (g_config.oracle) {
            CcSimInit(&oracle, world_seed);
            if (!RunPolicyWorld(&oracle, &oracle_stats, &g_config, roll, seed,
                    years, wip_limit, target_day, "oracle")) return EXIT_FAILURE;
        }
        WriteReportRow(stdout, seed, target_day, &control, &agent, &stats,
                       &oracle, &oracle_stats, g_config.oracle);
    }
    free(roll);
    return EXIT_SUCCESS;
}
