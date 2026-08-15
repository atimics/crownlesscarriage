#ifndef CROWNLESS_SIM_H
#define CROWNLESS_SIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CC_MAX_KINGDOMS 3
#define CC_MAX_SETTLEMENTS 6
#define CC_MAX_ROUTES 8
#define CC_MAX_FACTIONS 9
#define CC_MAX_SHIPMENTS 24
#define CC_MAX_BANDITS 3
#define CC_MAX_MONSTERS 3
#define CC_MAX_DUNGEONS 3
#define CC_MAX_MAPS CC_MAX_ROUTES
#define CC_MAX_SITUATIONS 12
#define CC_MAX_EVENTS 256
#define CC_NAME_CAPACITY 32
#define CC_MAP_NAME_CAPACITY 48
#define CC_EVENT_TEXT_CAPACITY 144
#define CC_CARGO_CAPACITY 12
#define CC_MAP_CAPACITY 3

#define CC_SIM_SCHEMA_VERSION 4
#define CC_GENERATOR_VERSION 3
#define CC_WORLD_TICKS_PER_SECOND 60
#define CC_WORLD_MINUTE_SUBTICKS 60
#define CC_WORLD_DAY_SUBTICKS (24 * 60 * CC_WORLD_MINUTE_SUBTICKS)
#define CC_IDLE_GAME_MINUTES_PER_SECOND 0
#define CC_TRAVEL_GAME_MINUTES_PER_SECOND 30

typedef uint64_t CcId;
typedef int64_t CcMoney;

typedef enum CcEntityKind {
    CC_ENTITY_NONE = 0,
    CC_ENTITY_KINGDOM = 1,
    CC_ENTITY_SETTLEMENT = 2,
    CC_ENTITY_ROUTE = 3,
    CC_ENTITY_FACTION = 4,
    CC_ENTITY_SHIPMENT = 5,
    CC_ENTITY_BANDIT_GROUP = 6,
    CC_ENTITY_MONSTER_POPULATION = 7,
    CC_ENTITY_DUNGEON = 8,
    CC_ENTITY_EVENT = 9,
    CC_ENTITY_PLAYER_COMPANY = 10,
    CC_ENTITY_SITUATION = 11,
    CC_ENTITY_MAP = 12
} CcEntityKind;

typedef enum CcGood {
    CC_GOOD_FOOD,
    CC_GOOD_MATERIAL,
    CC_GOOD_TOOLS,
    CC_GOOD_COUNT
} CcGood;

typedef enum CcSettlementFunction {
    CC_SETTLEMENT_FARMING,
    CC_SETTLEMENT_MINING,
    CC_SETTLEMENT_MARKET,
    CC_SETTLEMENT_FORTRESS,
    CC_SETTLEMENT_CAPITAL,
    CC_SETTLEMENT_DUNGEON_TOWN
} CcSettlementFunction;

typedef enum CcFactionKind {
    CC_FACTION_CROWN,
    CC_FACTION_GUILD,
    CC_FACTION_COMMONS
} CcFactionKind;

typedef enum CcDungeonState {
    CC_DUNGEON_SEALED,
    CC_DUNGEON_DISTURBED,
    CC_DUNGEON_EXPLORED,
    CC_DUNGEON_PUBLIC_ROUTE,
    CC_DUNGEON_SMUGGLER_ROUTE,
    CC_DUNGEON_RESEALED
} CcDungeonState;

typedef enum CcEventKind {
    CC_EVENT_HARVEST_FAILED,
    CC_EVENT_ROUTE_CLOSED,
    CC_EVENT_SHORTAGE,
    CC_EVENT_SHIPMENT_DEPARTED,
    CC_EVENT_SHIPMENT_ARRIVED,
    CC_EVENT_BANDIT_PRESSURE,
    CC_EVENT_MONSTER_PRESSURE,
    CC_EVENT_PLAYER_TRADE,
    CC_EVENT_PLAYER_TRAVEL,
    CC_EVENT_ROUTE_REPAIRED,
    CC_EVENT_DUNGEON_CHANGED,
    CC_EVENT_RELIEF,
    CC_EVENT_SHIPMENT_LOST,
    CC_EVENT_KINGDOM_ACTION,
    CC_EVENT_ROUTE_DECAY,
    CC_EVENT_FACTION_SHIFT,
    CC_EVENT_SITUATION_CREATED,
    CC_EVENT_SITUATION_RESOLVED,
    CC_EVENT_SITUATION_FAILED,
    CC_EVENT_PLAYER_AMBUSH,
    CC_EVENT_MAP_BOUGHT,
    CC_EVENT_MAP_SOLD,
    CC_EVENT_CHARTER_ACCEPTED,
    CC_EVENT_CHARTER_ABANDONED,
    CC_EVENT_JOURNEY_ENCOUNTER,
    CC_EVENT_ENCOUNTER_COMBAT,
    CC_EVENT_ENCOUNTER_NEGOTIATED,
    CC_EVENT_DELAYED_ECHO,
    CC_EVENT_JOURNEY_DEPARTED
} CcEventKind;

typedef enum CcCommandKind {
    CC_COMMAND_NONE,
    CC_COMMAND_TRADE,
    CC_COMMAND_TRAVEL,
    CC_COMMAND_REPAIR_ROUTE,
    CC_COMMAND_CHANGE_DUNGEON,
    CC_COMMAND_BUY_MAP,
    CC_COMMAND_SELL_MAP,
    CC_COMMAND_ACCEPT_SITUATION,
    CC_COMMAND_ABANDON_SITUATION,
    CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT,
    CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE
} CcCommandKind;

typedef struct CcKingdom {
    CcId id;
    char name[CC_NAME_CAPACITY];
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    CcMoney treasury;
    int32_t legitimacy;
} CcKingdom;

typedef struct CcSettlement {
    CcId id;
    CcId kingdom_id;
    char name[CC_NAME_CAPACITY];
    CcSettlementFunction function;
    int32_t map_x;
    int32_t map_y;
    int32_t population;
    int32_t security;
    int32_t prosperity;
    int32_t hunger;
    int32_t stock[CC_GOOD_COUNT];
    int32_t reserve_target[CC_GOOD_COUNT];
    int32_t production[CC_GOOD_COUNT];
    int32_t consumption[CC_GOOD_COUNT];
    int32_t price[CC_GOOD_COUNT];
} CcSettlement;

typedef struct CcRoute {
    CcId id;
    CcId from_id;
    CcId to_id;
    int32_t travel_days;
    int32_t capacity;
    int32_t security;
    int32_t condition;
    bool closed;
    bool smuggler_route;
} CcRoute;

typedef struct CcMap {
    CcId id;
    CcId route_id;
    CcId maker_settlement_id;
    CcId owner_id;
    char name[CC_MAP_NAME_CAPACITY];
    int32_t surveyed_day;
    int32_t accuracy;
    int32_t recorded_condition;
    int32_t recorded_danger;
    int32_t ask_price;
    bool contraband;
} CcMap;

typedef struct CcFaction {
    CcId id;
    CcId kingdom_id;
    char name[CC_NAME_CAPACITY];
    CcFactionKind kind;
    int32_t power;
    int32_t support;
} CcFaction;

typedef enum CcShipmentStatus {
    CC_SHIPMENT_UNUSED,
    CC_SHIPMENT_TRAVELLING,
    CC_SHIPMENT_ARRIVED,
    CC_SHIPMENT_LOST
} CcShipmentStatus;

typedef struct CcShipment {
    CcId id;
    CcId origin_id;
    CcId destination_id;
    CcId final_destination_id;
    CcId route_id;
    CcGood good;
    int32_t quantity;
    int32_t departure_day;
    int32_t arrival_day;
    CcShipmentStatus status;
} CcShipment;

typedef struct CcBanditGroup {
    CcId id;
    CcId route_id;
    char name[CC_NAME_CAPACITY];
    int32_t members;
    int32_t supplies;
    int32_t influence;
} CcBanditGroup;

typedef struct CcMonsterPopulation {
    CcId id;
    CcId dungeon_id;
    char name[CC_NAME_CAPACITY];
    int32_t population;
    int32_t pressure;
    int32_t hunting_pressure;
} CcMonsterPopulation;

typedef struct CcDungeon {
    CcId id;
    CcId settlement_id;
    char name[CC_NAME_CAPACITY];
    CcDungeonState state;
    int32_t depth;
    int32_t regional_pressure;
} CcDungeon;

typedef enum CcSituationKind {
    CC_SITUATION_RELIEF_DELIVERY,
    CC_SITUATION_ROUTE_REPAIR,
    CC_SITUATION_MONSTER_EXPEDITION,
    CC_SITUATION_BLACK_MARKET_DELIVERY
} CcSituationKind;

typedef enum CcSituationStatus {
    CC_SITUATION_ACTIVE,
    CC_SITUATION_RESOLVED,
    CC_SITUATION_FAILED
} CcSituationStatus;

typedef struct CcSituation {
    CcId id;
    CcSituationKind kind;
    CcSituationStatus status;
    CcId issuer_faction_id;
    CcId target_id;
    CcId cause_event_id;
    CcGood good;
    int32_t quantity;
    int32_t progress;
    CcMoney reward;
    int32_t created_day;
    int32_t deadline_day;
    char sponsor_name[CC_NAME_CAPACITY];
    char affected_name[CC_NAME_CAPACITY];
} CcSituation;

typedef enum CcJourneyOutcome {
    CC_JOURNEY_OUTCOME_NONE,
    CC_JOURNEY_OUTCOME_COMBAT,
    CC_JOURNEY_OUTCOME_NEGOTIATED
} CcJourneyOutcome;

typedef enum CcJourneyPhase {
    CC_JOURNEY_PHASE_NONE,
    CC_JOURNEY_PHASE_TRAVELLING,
    CC_JOURNEY_PHASE_BLOCKED
} CcJourneyPhase;

typedef enum CcCarriageMode {
    CC_CARRIAGE_PARKED,
    CC_CARRIAGE_MOVING,
    CC_CARRIAGE_STOPPED
} CcCarriageMode;

typedef struct CcWorldClock {
    uint64_t tick;
    int32_t minute_subticks;
    int32_t game_minutes_per_second;
} CcWorldClock;

typedef struct CcJourneyEncounter {
    bool active;
    CcJourneyPhase phase;
    CcId situation_id;
    CcId origin_id;
    CcId destination_id;
    CcId route_id;
    int32_t danger;
    int32_t bargain_cost;
    int32_t departure_day;
    int32_t elapsed_subticks;
    int32_t total_subticks;
    int32_t encounter_subticks;
    int32_t fare_reserved;
    bool encounter_triggered;
    bool ambush_pending;
    bool ambush_resolved;
    CcId parent_event_id;
} CcJourneyEncounter;

typedef struct CcCarriageState {
    CcCarriageMode mode;
    CcId location_id;
    CcId route_id;
    CcId origin_id;
    CcId destination_id;
    int32_t progress_milli;
    int32_t speed_milli_per_second;
    int32_t condition;
} CcCarriageState;

typedef struct CcDelayedEcho {
    bool active;
    CcId situation_id;
    CcId settlement_id;
    CcId parent_event_id;
    CcJourneyOutcome outcome;
    int32_t due_day;
    char character_name[CC_NAME_CAPACITY];
} CcDelayedEcho;

typedef struct CcEvent {
    CcId id;
    int32_t day;
    CcEventKind kind;
    CcId subject_id;
    CcId location_id;
    CcId parent_id;
    int32_t magnitude;
    char text[CC_EVENT_TEXT_CAPACITY];
} CcEvent;

typedef struct CcPlayerCompany {
    CcId id;
    CcId location_id;
    CcMoney coins;
    int32_t cargo[CC_GOOD_COUNT];
    int32_t cargo_capacity;
    int32_t passenger_capacity;
    int32_t map_capacity;
    int32_t reputation;
    CcId accepted_situation_id;
} CcPlayerCompany;

typedef struct CcCommand {
    CcCommandKind kind;
    CcId target_id;
    CcGood good;
    int32_t amount;
    CcDungeonState dungeon_state;
} CcCommand;

typedef struct CcSim {
    uint32_t schema_version;
    uint32_t generator_version;
    uint32_t world_seed;
    uint32_t random_state;
    int32_t current_day;
    uint64_t next_entity_serial;
    CcKingdom kingdoms[CC_MAX_KINGDOMS];
    CcSettlement settlements[CC_MAX_SETTLEMENTS];
    CcRoute routes[CC_MAX_ROUTES];
    CcMap maps[CC_MAX_MAPS];
    CcFaction factions[CC_MAX_FACTIONS];
    CcShipment shipments[CC_MAX_SHIPMENTS];
    CcBanditGroup bandits[CC_MAX_BANDITS];
    CcMonsterPopulation monsters[CC_MAX_MONSTERS];
    CcDungeon dungeons[CC_MAX_DUNGEONS];
    CcSituation situations[CC_MAX_SITUATIONS];
    CcEvent events[CC_MAX_EVENTS];
    CcPlayerCompany player;
    CcWorldClock clock;
    CcJourneyEncounter journey;
    CcCarriageState carriage;
    CcDelayedEcho delayed_echo;
    CcId resolved_journey_situation_id;
    CcJourneyOutcome resolved_journey_outcome;
    int32_t kingdom_count;
    int32_t settlement_count;
    int32_t route_count;
    int32_t map_count;
    int32_t faction_count;
    int32_t shipment_count;
    int32_t bandit_count;
    int32_t monster_count;
    int32_t dungeon_count;
    int32_t situation_count;
    int32_t event_count;
    int32_t event_write_index;
    int32_t last_shortage_level[CC_MAX_SETTLEMENTS];
    int32_t last_bandit_level[CC_MAX_BANDITS];
    int32_t last_monster_level[CC_MAX_MONSTERS];
} CcSim;

void CcSimInit(CcSim *sim, uint32_t seed);
void CcSimAdvanceDays(CcSim *sim, int32_t days);
/* Consumes exact 60 Hz ticks only while a committed journey is travelling. */
void CcSimAdvanceRuntimeTicks(CcSim *sim, int32_t ticks);
bool CcSimApply(CcSim *sim, const CcCommand *command,
                char *error, size_t error_capacity);
bool CcSimValidate(const CcSim *sim, char *error, size_t error_capacity);
uint64_t CcSimHash(const CcSim *sim);

CcId CcMakeId(CcEntityKind kind, uint64_t serial);
CcEntityKind CcIdKind(CcId id);
const char *CcGoodName(CcGood good);
const char *CcSettlementFunctionName(CcSettlementFunction function);
const char *CcDungeonStateName(CcDungeonState state);
const char *CcEventKindName(CcEventKind kind);
const char *CcSituationKindName(CcSituationKind kind);

const CcSettlement *CcSimSettlement(const CcSim *sim, CcId id);
CcSettlement *CcSimSettlementMutable(CcSim *sim, CcId id);
const CcRoute *CcSimRoute(const CcSim *sim, CcId id);
const CcRoute *CcSimRouteBetween(const CcSim *sim, CcId a, CcId b);
const CcMap *CcSimMap(const CcSim *sim, CcId id);
const CcMap *CcSimMapForRoute(const CcSim *sim, CcId route_id, CcId owner_id);
const CcEvent *CcSimRecentEvent(const CcSim *sim, int32_t offset);
const CcEvent *CcSimEvent(const CcSim *sim, CcId id);
const CcSituation *CcSimSituation(const CcSim *sim, CcId id);
const CcSituation *CcSimAcceptedSituation(const CcSim *sim);
bool CcSimSituationTouchesSettlement(const CcSim *sim,
                                     const CcSituation *situation,
                                     CcId settlement_id);
const CcSituation *CcSimSituationForSettlement(const CcSim *sim, CcId settlement_id);
int32_t CcSimActiveSituationCount(const CcSim *sim);
int32_t CcSimIncomingGood(const CcSim *sim, CcId settlement_id, CcGood good);
int32_t CcSimRouteDanger(const CcSim *sim, CcId route_id);
int32_t CcPlayerCargoUsed(const CcPlayerCompany *player);
int32_t CcPlayerMapCount(const CcSim *sim);

#endif
