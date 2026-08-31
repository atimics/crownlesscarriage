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
#define CC_MAX_COURIERS 12
#define CC_MAX_BANDITS 3
#define CC_MAX_MONSTERS 3
#define CC_MAX_DUNGEONS 3
#define CC_MAX_DUNGEON_ROOMS 24
#define CC_MAX_DUNGEON_LINKS 36
#define CC_MAX_MAPS 13
#define CC_MAX_TREASURES 24
#define CC_MAX_SITUATIONS 12
#define CC_MAX_CHARACTERS 24
#define CC_CHARACTER_MEMORY_CAPACITY 4
#define CC_CHARACTER_KNOWLEDGE_CAPACITY 8
#define CC_MAX_RELATIONSHIPS 48
#define CC_MAX_EVENTS 256
#define CC_CARRIAGE_HORSE_COUNT 2
#define CC_MAX_STABLE_HORSES 6
#define CC_NAME_CAPACITY 32
#define CC_MAP_NAME_CAPACITY 48
#define CC_EVENT_TEXT_CAPACITY 144
#define CC_CARGO_CAPACITY 12
#define CC_MAP_CAPACITY 3
#define CC_MAP_COLLECTION_COUNT 13
#define CC_SIM_MAX_DAY INT32_C(2147000000)
#define CC_SIM_MAX_UNITS INT32_C(1000000)
#define CC_SIM_MAX_MONEY INT64_C(1000000000000)
#define CC_SIM_MAX_ROUTE_DAYS INT32_C(365)
#define CC_GLOAMGATE_ALDERWATCH_MAP_NAME "Gloamgate to Alderwatch"
#define CC_CROWNLESS_ATLAS_MAP_NAME "The Crownless Atlas"
#define CC_DRAGON_HOARD_MAP_NAME "The Hoard Vault of Varkesh"

#define CC_SIM_SCHEMA_VERSION 20
#define CC_GENERATOR_VERSION 19
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
    CC_ENTITY_MAP = 12,
    CC_ENTITY_GOBLIN_CULT = 13,
    CC_ENTITY_DRAGON = 14,
    CC_ENTITY_HOARD_RAIDERS = 15,
    CC_ENTITY_TREASURE = 16,
    CC_ENTITY_COURIER = 17,
    CC_ENTITY_HORSE = 18,
    CC_ENTITY_CHARACTER = 19
} CcEntityKind;

typedef enum CcGood {
    CC_GOOD_FOOD = 0,
    CC_GOOD_IRON = 1,
    /* Source compatibility for older callers and save migrations. */
    CC_GOOD_MATERIAL = CC_GOOD_IRON,
    CC_GOOD_TOOLS = 2,
    CC_GOOD_WEAPONS = 3,
    CC_GOOD_GOLD = 4,
    CC_GOOD_GEMS = 5,
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

typedef enum CcSettlementSize {
    CC_SETTLEMENT_HAMLET,
    CC_SETTLEMENT_VILLAGE,
    CC_SETTLEMENT_TOWN,
    CC_SETTLEMENT_CITY,
    CC_SETTLEMENT_CAPITAL_SIZE
} CcSettlementSize;

typedef enum CcServiceKind {
    CC_SERVICE_NONE = -1,
    CC_SERVICE_MARKET,
    CC_SERVICE_INN,
    CC_SERVICE_GRANARY,
    CC_SERVICE_SMITHY,
    CC_SERVICE_HEALER,
    CC_SERVICE_STABLE,
    CC_SERVICE_SHRINE,
    CC_SERVICE_BARRACKS,
    CC_SERVICE_CARTOGRAPHER,
    CC_SERVICE_GUILDHALL,
    CC_SERVICE_MINE,
    CC_SERVICE_FARM,
    CC_SERVICE_BLACK_MARKET,
    CC_SERVICE_DUNGEON_WARD,
    CC_SERVICE_COUNT
} CcServiceKind;

typedef enum CcFactionKind {
    CC_FACTION_CROWN,
    CC_FACTION_GUILD,
    CC_FACTION_COMMONS
} CcFactionKind;

typedef enum CcKingdomCalling {
    CC_KINGDOM_CALLING_ROAD,
    CC_KINGDOM_CALLING_IRON,
    CC_KINGDOM_CALLING_DEEP,
    CC_KINGDOM_CALLING_COUNT
} CcKingdomCalling;

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
    CC_EVENT_JOURNEY_DEPARTED,
    CC_EVENT_SERVICE_OPENED,
    CC_EVENT_BANDIT_RAID_DEPARTED,
    CC_EVENT_SETTLEMENT_RAIDED,
    CC_EVENT_BANDIT_RAID_RETURNED,
    CC_EVENT_GOBLIN_TRIBUTE_DEPARTED,
    CC_EVENT_GOBLIN_TRIBUTE_TAKEN,
    CC_EVENT_GOBLIN_TRIBUTE_DELIVERED,
    CC_EVENT_DRAGON_HOARD_STOLEN,
    CC_EVENT_DRAGON_OMEN,
    CC_EVENT_DRAGON_TREASURE_RETURNED,
    CC_EVENT_DRAGON_RETALIATION,
    CC_EVENT_INEQUALITY_PRESSURE,
    CC_EVENT_HOARD_HEIST_DEPARTED,
    CC_EVENT_HOARD_HEIST_RETURNED,
    CC_EVENT_WAR_PRESSURE,
    CC_EVENT_WAR_CHEST_FUNDED,
    CC_EVENT_WAR_SUPPLY_BOUGHT,
    CC_EVENT_WAR_SUPPLY_SHORTAGE,
    CC_EVENT_RESOURCE_EXTRACTED,
    CC_EVENT_SMITH_PRODUCTION,
    CC_EVENT_TREASURE_CRAFTED,
    CC_EVENT_GOBLIN_RAID_DEPARTED,
    CC_EVENT_GOBLIN_RAIDED,
    CC_EVENT_GOBLIN_RAID_RETURNED,
    CC_EVENT_WAR_MATERIEL_LOST,
    CC_EVENT_IRON_LEDGER_LOAN,
    CC_EVENT_IRON_LEDGER_REPAID,
    CC_EVENT_GOBLIN_HOARD_DEFENDED,
    CC_EVENT_COURIER_DEPARTED,
    CC_EVENT_COURIER_ARRIVED,
    CC_EVENT_COURIER_LOST,
    CC_EVENT_COURIER_DISTORTED,
    CC_EVENT_WAR_DECLARED,
    CC_EVENT_PEACE_DECLARED,
    CC_EVENT_ALLIANCE_DECLARED,
    CC_EVENT_DRAGON_MUSTERED,
    CC_EVENT_DRAGON_BATTLE,
    CC_EVENT_DRAGON_SLAIN,
    CC_EVENT_DRAGON_HOARD_RECOVERED,
    CC_EVENT_DRAGON_HUNT,
    CC_EVENT_DRAGON_CROWNED,
    CC_EVENT_DRAGON_UNCROWNED,
    CC_EVENT_DRAGON_BROOD,
    CC_EVENT_DRAGON_WHELP_DISPERSED,
    CC_EVENT_DRAGON_AFTERSHOCK,
    CC_EVENT_DRAGON_SUCCESSOR,
    CC_EVENT_GOBLIN_CULT_RALLIED,
    CC_EVENT_GOBLIN_DRAGON_SEED,
    CC_EVENT_ENCOUNTER_WITHDRAWN,
    CC_EVENT_COW_CALVING,
    CC_EVENT_COW_SLAUGHTERED,
    CC_EVENT_HORSE_BRED,
    CC_EVENT_FOAL_BORN,
    CC_EVENT_HORSE_TEAM_CHANGED,
    CC_EVENT_GOBLIN_RAID_PREPARED,
    CC_EVENT_GOBLIN_TARGET_WARNED,
    CC_EVENT_GOBLIN_EXPEDITION_INTERCEPTED,
    CC_EVENT_GOBLIN_TRADE,
    CC_EVENT_GOBLIN_DRAGON_SEED_RUMORED,
    CC_EVENT_GOBLIN_DRAGON_SEED_PREPARED,
    CC_EVENT_CHARACTER_INTERACTION,
    CC_EVENT_JOURNEY_WARNING,
    CC_EVENT_AMBUSH_EVADED,
    CC_EVENT_ENCOUNTER_LOOT,
    CC_EVENT_GOBLIN_TUNNEL_TRAVERSED,
    CC_EVENT_DUNGEON_EXPEDITION_BEGAN,
    CC_EVENT_DUNGEON_ROOM_ENTERED,
    CC_EVENT_DUNGEON_ENCOUNTER,
    CC_EVENT_DUNGEON_SHORTCUT_OPENED,
    CC_EVENT_DUNGEON_LOOT,
    CC_EVENT_DUNGEON_THRESHOLD_REACHED,
    CC_EVENT_DUNGEON_EXPEDITION_ENDED,
    CC_EVENT_RELATIONSHIP_HISTORY,
    CC_EVENT_RUMOR_SHARED,
    CC_EVENT_FACT_REVEALED,
    CC_EVENT_RELATIONSHIP_CHANGED
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
    CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE,
    CC_COMMAND_REFUSE_SITUATION,
    CC_COMMAND_STEAL_DRAGON_HOARD,
    CC_COMMAND_RETURN_DRAGON_TREASURE,
    CC_COMMAND_BUY_TREASURE,
    CC_COMMAND_SELL_TREASURE,
    CC_COMMAND_ARCHIVE_MAP,
    CC_COMMAND_RETRIEVE_MAP,
    CC_COMMAND_STEAL_DRAGON_NAMED_TREASURE,
    CC_COMMAND_RETURN_DRAGON_NAMED_TREASURE,
    /* Appended so command journals written by earlier schema versions keep
       their stable numeric meanings. */
    CC_COMMAND_RESOLVE_ENCOUNTER_PROVISIONS,
    CC_COMMAND_WITHDRAW_ENCOUNTER,
    CC_COMMAND_BREED_HORSES,
    CC_COMMAND_ASSIGN_HORSE,
    CC_COMMAND_INTERCEPT_DRAGON_TRIBUTE,
    CC_COMMAND_GOBLIN_TRADE,
    CC_COMMAND_GOBLIN_WARN,
    CC_COMMAND_GOBLIN_INTERCEPT,
    CC_COMMAND_CHARACTER_RESPONSE,
    CC_COMMAND_SET_JOURNEY_PACE,
    CC_COMMAND_TRAVERSE_GOBLIN_TUNNEL,
    CC_COMMAND_BEGIN_DUNGEON_EXPEDITION,
    CC_COMMAND_MOVE_DUNGEON,
    CC_COMMAND_SEARCH_DUNGEON,
    CC_COMMAND_OPEN_DUNGEON_SHORTCUT,
    CC_COMMAND_RESOLVE_DUNGEON_ENCOUNTER,
    CC_COMMAND_RETREAT_DUNGEON
} CcCommandKind;

typedef enum CcHorseSex {
    CC_HORSE_MARE,
    CC_HORSE_STALLION
} CcHorseSex;

typedef enum CcCollectibleMapSlot {
    CC_MAP_THORNFORD_FORDINGS,
    CC_MAP_GLOAMGATE_ALDERWATCH,
    CC_MAP_SILVERWICK_MINE_ROADS,
    CC_MAP_ROSESPIRE_PILGRIM_WAY,
    CC_MAP_TRIBUTE_ROADS,
    CC_MAP_BROKEN_MARCH,
    CC_MAP_GLOAMGATE_NIGHT_ROAD,
    CC_MAP_ALDERWATCH_MUSTER,
    CC_MAP_TREATY_BRIDGE_SURVEY,
    CC_MAP_LOWER_SILVERWORKS,
    CC_MAP_ASH_POOR_SKIN,
    CC_MAP_CROWNLESS_ATLAS,
    /* Appended so older campaign catalogue bits keep their meaning. */
    CC_MAP_DRAGON_HOARD
} CcCollectibleMapSlot;

typedef struct CcKingdom {
    CcId id;
    char name[CC_NAME_CAPACITY];
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    CcMoney treasury;
    CcMoney iron_ledger_debt;
    int32_t legitimacy;
} CcKingdom;

typedef enum CcDiplomaticState {
    CC_DIPLOMACY_PEACE,
    CC_DIPLOMACY_WAR,
    CC_DIPLOMACY_ALLIANCE
} CcDiplomaticState;

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
    CcSettlementSize size;
    uint32_t service_mask;
    CcServiceKind service_project;
    int32_t service_project_days;
    int32_t stock[CC_GOOD_COUNT];
    int32_t reserve_target[CC_GOOD_COUNT];
    int32_t production[CC_GOOD_COUNT];
    int32_t consumption[CC_GOOD_COUNT];
    int32_t price[CC_GOOD_COUNT];
    CcMoney market_coins;
    CcMoney war_chest;
    int32_t field_yield;
    int32_t iron_deposit;
    bool gold_seam;
    bool gem_seam;
    int32_t gold_progress;
    int32_t gem_progress;
    int32_t farm_tool_wear;
    int32_t mine_tool_wear;
    int32_t smith_tool_wear;
    int32_t treasure_gold_committed;
    int32_t treasure_gems_committed;
    int32_t treasure_work;
    int32_t cow_adults;
    int32_t cow_calves;
    int32_t cow_condition;
    int32_t cow_hunger;
} CcSettlement;

typedef struct CcHorse {
    CcId id;
    char name[CC_NAME_CAPACITY];
    int32_t age_days;
    int32_t health;
    int32_t fatigue;
    int32_t hunger;
    CcHorseSex sex;
    CcId sire_id;
    CcId dam_id;
    CcId stable_settlement_id;
    CcId pregnant_by_id;
    int32_t pregnancy_days_remaining;
    int32_t breeding_cooldown_days;
    int32_t training;
    int32_t strength;
    int32_t temperament;
    int32_t hardiness;
} CcHorse;

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

typedef enum CcCourierKind {
    CC_COURIER_WAR_DECLARATION,
    CC_COURIER_PEACE_OFFER,
    CC_COURIER_DRAGON_ALLIANCE,
    CC_COURIER_DRAGON_MUSTER
} CcCourierKind;

typedef enum CcCourierStatus {
    CC_COURIER_WAITING,
    CC_COURIER_TRAVELLING,
    CC_COURIER_WITH_PLAYER,
    CC_COURIER_DELIVERED,
    CC_COURIER_LOST,
    CC_COURIER_DISTORTED
} CcCourierStatus;

typedef struct CcCourier {
    CcId id;
    CcCourierKind kind;
    CcCourierStatus status;
    CcId issuer_kingdom_id;
    CcId recipient_kingdom_id;
    CcId origin_settlement_id;
    CcId destination_settlement_id;
    CcId current_settlement_id;
    CcId route_id;
    CcId cause_event_id;
    CcId situation_id;
    int32_t departure_day;
    int32_t arrival_day;
    int32_t reliability;
} CcCourier;

typedef enum CcBanditCampSize {
    CC_BANDIT_HIDEOUT,
    CC_BANDIT_CAMP,
    CC_BANDIT_WAR_CAMP,
    CC_BANDIT_OUTLAW_TOWN
} CcBanditCampSize;

typedef enum CcBanditRaidPhase {
    CC_BANDIT_RAID_IDLE,
    CC_BANDIT_RAID_SCOUTING,
    CC_BANDIT_RAID_MUSTERING,
    CC_BANDIT_RAID_OUTBOUND,
    CC_BANDIT_RAID_RETURNING
} CcBanditRaidPhase;

typedef struct CcBanditGroup {
    CcId id;
    CcId route_id;
    char name[CC_NAME_CAPACITY];
    int32_t members;
    int32_t supplies;
    int32_t influence;
    CcBanditCampSize camp_size;
    uint32_t service_mask;
    CcBanditRaidPhase raid_phase;
    CcId raid_target_id;
    CcGood raid_good;
    int32_t raid_quantity;
    int32_t raid_days_remaining;
    int32_t raids_completed;
} CcBanditGroup;

typedef enum CcGoblinTributePhase {
    CC_GOBLIN_TRIBUTE_IDLE,
    /* The old names remain stable in saved games. They now describe a raid. */
    CC_GOBLIN_TRIBUTE_OUTBOUND,
    CC_GOBLIN_TRIBUTE_RETURNING,
    CC_GOBLIN_TRIBUTE_TO_DRAGON,
    /* Added last so older save values remain stable. */
    CC_GOBLIN_TRIBUTE_PREPARING
} CcGoblinTributePhase;

typedef enum CcGoblinRaidMotive {
    CC_GOBLIN_RAID_NONE,
    CC_GOBLIN_RAID_HUNGER,
    CC_GOBLIN_RAID_EQUIPMENT,
    CC_GOBLIN_RAID_DRAGON_TRIBUTE
} CcGoblinRaidMotive;

typedef enum CcGoblinDragonSeedPhase {
    CC_GOBLIN_DRAGON_SEED_NONE,
    CC_GOBLIN_DRAGON_SEED_RUMORED,
    CC_GOBLIN_DRAGON_SEED_PREPARING
} CcGoblinDragonSeedPhase;

typedef struct CcGoblinCult {
    CcId id;
    char name[CC_NAME_CAPACITY];
    int32_t members;
    /* Devotion is the saved legacy name for commitment to the covenant. */
    int32_t devotion;
    int32_t cohesion;
    CcId lair_settlement_id;
    CcGoblinTributePhase tribute_phase;
    CcGoblinRaidMotive raid_motive;
    CcId tribute_target_id;
    CcId last_tribute_origin_id;
    CcId tribute_event_id;
    CcMoney carried_tribute;
    CcMoney lair_coins;
    int32_t carried_goods[CC_GOOD_COUNT];
    int32_t lair_stock[CC_GOOD_COUNT];
    CcId carried_treasure_id;
    int32_t tribute_days_remaining;
    int32_t tribute_cooldown_days;
    int32_t tributes_delivered;
    int32_t hoard_defenses;
    bool target_warned;
    int32_t expeditions_intercepted;
    CcGoblinDragonSeedPhase dragon_seed_phase;
    int32_t dragon_seed_days_remaining;
} CcGoblinCult;

typedef enum CcDragonLifeStage {
    CC_DRAGON_STAGE_EGG,
    CC_DRAGON_STAGE_WHELP,
    CC_DRAGON_STAGE_WANDERER,
    CC_DRAGON_STAGE_CROWNED,
    CC_DRAGON_STAGE_DEEP_WYRM,
    CC_DRAGON_STAGE_UNCROWNED,
    CC_DRAGON_STAGE_AFTERDRAGON
} CcDragonLifeStage;

typedef enum CcDragonActivity {
    CC_DRAGON_ACTIVITY_DORMANT,
    CC_DRAGON_ACTIVITY_HUNTING,
    CC_DRAGON_ACTIVITY_RETALIATING,
    CC_DRAGON_ACTIVITY_BROODING,
    CC_DRAGON_ACTIVITY_AFTERMATH
} CcDragonActivity;

typedef struct CcDragon {
    CcId id;
    char name[CC_NAME_CAPACITY];
    CcId lair_settlement_id;
    CcMoney hoard;
    int32_t hoard_goods[CC_GOOD_COUNT];
    CcId stolen_treasure_id;
    CcMoney stolen_outstanding;
    CcId theft_actor_id;
    CcId retaliation_target_id;
    CcId hoard_event_id;
    CcId omen_event_id;
    int32_t omen_days_remaining;
    int32_t retaliations;
    bool slain;
    int32_t slain_day;
    CcDragonLifeStage life_stage;
    CcDragonActivity activity;
    int32_t age_days;
    int32_t body_condition;
    int32_t crown_strength;
    int32_t memory_integrity;
    int32_t territory_stability;
    int32_t regional_influence;
    int32_t crown_continuity_days;
    int32_t hunt_cooldown_days;
    int32_t hunts;
    int32_t egg_count;
    int32_t brood_days_remaining;
    int32_t brood_cooldown_days;
    int32_t broods_laid;
    int32_t whelps_dispersed;
    int32_t afterdeath_days;
    CcId lifecycle_event_id;
} CcDragon;

typedef enum CcDragonCampaignPhase {
    CC_DRAGON_CAMPAIGN_IDLE,
    CC_DRAGON_CAMPAIGN_OUTBOUND,
    CC_DRAGON_CAMPAIGN_RETURNING
} CcDragonCampaignPhase;

typedef struct CcDragonCampaign {
    CcDragonCampaignPhase phase;
    uint32_t pledged_kingdom_mask;
    uint32_t alliance_kingdom_mask;
    CcId origin_settlement_id;
    CcId cause_event_id;
    int32_t days_remaining;
    int32_t cooldown_days;
    int32_t supplies[CC_GOOD_COUNT];
    CcMoney recovered_coins;
    int32_t attempts;
    int32_t victories;
    int32_t defeats;
} CcDragonCampaign;

typedef struct CcTreasure {
    CcId id;
    char name[CC_MAP_NAME_CAPACITY];
    CcId maker_settlement_id;
    CcId owner_id;
    CcId location_id;
    int32_t gold_content;
    int32_t gem_content;
    int32_t craft_work;
    int32_t appraised_value;
    int32_t created_day;
    bool destroyed;
} CcTreasure;

typedef enum CcHoardRaiderPhase {
    CC_HOARD_RAIDERS_IDLE,
    CC_HOARD_RAIDERS_OUTBOUND,
    CC_HOARD_RAIDERS_RETURNING
} CcHoardRaiderPhase;

typedef enum CcHoardRaidMotive {
    CC_HOARD_RAID_NO_MOTIVE,
    CC_HOARD_RAID_SOCIAL_RELIEF,
    CC_HOARD_RAID_WAR_FINANCE
} CcHoardRaidMotive;

typedef struct CcHoardRaiders {
    CcId id;
    char name[CC_NAME_CAPACITY];
    CcHoardRaiderPhase phase;
    CcHoardRaidMotive motive;
    CcId origin_settlement_id;
    CcId cause_event_id;
    CcMoney carried_treasure;
    int32_t days_remaining;
    int32_t cooldown_days;
    int32_t raids_completed;
    int32_t war_raids_completed;
    bool social_raid_latched;
    bool war_raid_latched;
} CcHoardRaiders;

typedef struct CcMonsterPopulation {
    CcId id;
    CcId dungeon_id;
    char name[CC_NAME_CAPACITY];
    int32_t population;
    int32_t pressure;
    int32_t hunting_pressure;
} CcMonsterPopulation;

typedef enum CcDungeonRoomKind {
    CC_DUNGEON_ROOM_MINE_MOUTH,
    CC_DUNGEON_ROOM_RAIL,
    CC_DUNGEON_ROOM_FLOODWAY,
    CC_DUNGEON_ROOM_WORKSHOP,
    CC_DUNGEON_ROOM_BRIDGE,
    CC_DUNGEON_ROOM_SHAFT,
    CC_DUNGEON_ROOM_ARCHIVE,
    CC_DUNGEON_ROOM_MARKET,
    CC_DUNGEON_ROOM_BARRACKS,
    CC_DUNGEON_ROOM_SHRINE,
    CC_DUNGEON_ROOM_VAULT,
    CC_DUNGEON_ROOM_THRESHOLD
} CcDungeonRoomKind;

typedef enum CcDungeonRoomFlag {
    CC_DUNGEON_ROOM_SAFE = 1U << 0U,
    CC_DUNGEON_ROOM_HAZARD = 1U << 1U,
    CC_DUNGEON_ROOM_STONEBACK = 1U << 2U,
    CC_DUNGEON_ROOM_GOBLIN = 1U << 3U,
    CC_DUNGEON_ROOM_DRAGON_SIGN = 1U << 4U,
    CC_DUNGEON_ROOM_OBJECTIVE = 1U << 5U,
    CC_DUNGEON_ROOM_SMUGGLER = 1U << 6U
} CcDungeonRoomFlag;

typedef enum CcDungeonRoomStateFlag {
    CC_DUNGEON_ROOM_DISCOVERED = 1U << 0U,
    CC_DUNGEON_ROOM_SEARCHED = 1U << 1U,
    CC_DUNGEON_ROOM_CLEARED = 1U << 2U,
    CC_DUNGEON_ROOM_OBJECTIVE_REACHED = 1U << 3U
} CcDungeonRoomStateFlag;

typedef enum CcDungeonLinkKind {
    CC_DUNGEON_LINK_PASSAGE,
    CC_DUNGEON_LINK_SECRET,
    CC_DUNGEON_LINK_SHORTCUT,
    CC_DUNGEON_LINK_DROP
} CcDungeonLinkKind;

typedef enum CcDungeonLinkFlag {
    CC_DUNGEON_LINK_DISCOVERED = 1U << 0U,
    CC_DUNGEON_LINK_OPEN = 1U << 1U
} CcDungeonLinkFlag;

typedef struct CcDungeonRoom {
    char name[CC_MAP_NAME_CAPACITY];
    CcDungeonRoomKind kind;
    int32_t depth;
    int32_t map_x;
    int32_t map_y;
    uint32_t flags;
    uint32_t state_flags;
    CcGood loot_good;
    int32_t loot_quantity;
} CcDungeonRoom;

typedef struct CcDungeonLink {
    int32_t from_room;
    int32_t to_room;
    CcDungeonLinkKind kind;
    uint32_t flags;
} CcDungeonLink;

typedef struct CcDungeon {
    CcId id;
    CcId settlement_id;
    char name[CC_NAME_CAPACITY];
    CcDungeonState state;
    int32_t depth;
    int32_t regional_pressure;
    uint32_t layout_seed;
    uint32_t encounter_random_state;
    int32_t room_count;
    int32_t link_count;
    CcDungeonRoom rooms[CC_MAX_DUNGEON_ROOMS];
    CcDungeonLink links[CC_MAX_DUNGEON_LINKS];
} CcDungeon;

typedef enum CcDungeonEncounterKind {
    CC_DUNGEON_ENCOUNTER_NONE,
    CC_DUNGEON_ENCOUNTER_STONEBACKS,
    CC_DUNGEON_ENCOUNTER_TITHE_KEEPERS,
    CC_DUNGEON_ENCOUNTER_GOBLIN_DESERTERS,
    CC_DUNGEON_ENCOUNTER_MONSTERS,
    CC_DUNGEON_ENCOUNTER_SMUGGLERS
} CcDungeonEncounterKind;

typedef enum CcDungeonEncounterApproach {
    CC_DUNGEON_APPROACH_PARLEY = 1,
    CC_DUNGEON_APPROACH_EVADE = 2,
    CC_DUNGEON_APPROACH_FORCE = 3
} CcDungeonEncounterApproach;

typedef struct CcDungeonExpedition {
    bool active;
    CcId dungeon_id;
    int32_t current_room;
    int32_t turns_elapsed;
    int32_t days_elapsed;
    int32_t light_remaining;
    int32_t noise;
    int32_t strain;
    int32_t maximum_depth;
    CcDungeonEncounterKind encounter_kind;
    int32_t encounter_reaction;
    int32_t encounter_room;
} CcDungeonExpedition;

typedef enum CcSituationKind {
    CC_SITUATION_RELIEF_DELIVERY,
    CC_SITUATION_ROUTE_REPAIR,
    CC_SITUATION_MONSTER_EXPEDITION,
    CC_SITUATION_BLACK_MARKET_DELIVERY,
    CC_SITUATION_COURIER_DELIVERY
} CcSituationKind;

typedef enum CcSituationStatus {
    CC_SITUATION_ACTIVE,
    CC_SITUATION_RESOLVED,
    CC_SITUATION_FAILED
} CcSituationStatus;

typedef enum CcCharacterRole {
    CC_CHARACTER_OFFICIAL,
    CC_CHARACTER_LABORER,
    CC_CHARACTER_SCOUT,
    CC_CHARACTER_TRAVELLER,
    CC_CHARACTER_REFUGEE,
    CC_CHARACTER_COURIER
} CcCharacterRole;

typedef enum CcCharacterGoal {
    CC_CHARACTER_GOAL_KEEP_ORDER,
    CC_CHARACTER_GOAL_SECURE_LIVELIHOOD,
    CC_CHARACTER_GOAL_SURVIVE_CRISIS,
    CC_CHARACTER_GOAL_CARRY_NEWS
} CcCharacterGoal;

typedef enum CcCharacterActivity {
    CC_CHARACTER_ACTIVITY_WORKING,
    CC_CHARACTER_ACTIVITY_SEEKING_AID,
    CC_CHARACTER_ACTIVITY_PREPARING,
    CC_CHARACTER_ACTIVITY_RECOVERING,
    CC_CHARACTER_ACTIVITY_HIDING,
    CC_CHARACTER_ACTIVITY_TRAVELLING
} CcCharacterActivity;

typedef enum CcCharacterMemoryKind {
    CC_CHARACTER_MEMORY_NONE,
    CC_CHARACTER_MEMORY_MET_PLAYER,
    CC_CHARACTER_MEMORY_PLAYER_PROMISED,
    CC_CHARACTER_MEMORY_PLAYER_HELPED,
    CC_CHARACTER_MEMORY_PLAYER_WITHDREW
} CcCharacterMemoryKind;

typedef enum CcCharacterResponse {
    CC_CHARACTER_RESPONSE_LISTEN = 1,
    CC_CHARACTER_RESPONSE_PLEDGE_HELP = 2,
    CC_CHARACTER_RESPONSE_REPORT_EVIDENCE = 3,
    CC_CHARACTER_RESPONSE_KEEP_CONFIDENCE = 4
} CcCharacterResponse;

typedef enum CcKnowledgeKind {
    CC_KNOWLEDGE_NONE,
    CC_KNOWLEDGE_PROBLEM_RUMOR,
    CC_KNOWLEDGE_WITNESS_ACCOUNT,
    CC_KNOWLEDGE_IMMEDIATE_STAKE,
    CC_KNOWLEDGE_OFFER
} CcKnowledgeKind;

typedef enum CcKnowledgeCertainty {
    CC_KNOWLEDGE_DOUBTFUL = 1,
    CC_KNOWLEDGE_TOLD = 2,
    CC_KNOWLEDGE_WITNESSED = 3
} CcKnowledgeCertainty;

typedef enum CcRelationshipHistory {
    CC_RELATIONSHIP_HISTORY_NONE,
    CC_RELATIONSHIP_HISTORY_OLD_FRIENDS,
    CC_RELATIONSHIP_HISTORY_FORMER_PARTNERS,
    CC_RELATIONSHIP_HISTORY_PROFESSIONAL_RIVALS,
    CC_RELATIONSHIP_HISTORY_COWORKERS
} CcRelationshipHistory;

typedef enum CcSituationDiscoveryStage {
    CC_DISCOVERY_OFFER,
    CC_DISCOVERY_RUMOR,
    CC_DISCOVERY_WITNESS,
    CC_DISCOVERY_DECISION,
    CC_DISCOVERY_AUTHORITY
} CcSituationDiscoveryStage;

typedef enum CcSituationLeadPath {
    CC_LEAD_PATH_UNDECIDED,
    CC_LEAD_PATH_REPORT,
    CC_LEAD_PATH_CONFIDENCE
} CcSituationLeadPath;

typedef struct CcCharacterMemory {
    CcCharacterMemoryKind kind;
    CcId subject_id;
    CcId event_id;
    int32_t day;
} CcCharacterMemory;

typedef struct CcCharacterKnowledge {
    CcKnowledgeKind kind;
    CcId subject_id;
    CcId source_character_id;
    CcId event_id;
    CcKnowledgeCertainty certainty;
    bool private_knowledge;
    int32_t day;
} CcCharacterKnowledge;

typedef struct CcCharacter {
    CcId id;
    char name[CC_NAME_CAPACITY];
    CcId home_settlement_id;
    CcId current_settlement_id;
    CcId faction_id;
    CcCharacterRole role;
    CcCharacterGoal goal;
    CcCharacterActivity activity;
    uint32_t appearance_seed;
    int32_t player_disposition;
    int32_t stress;
    int32_t courage;
    CcCharacterMemory memories[CC_CHARACTER_MEMORY_CAPACITY];
    int32_t memory_count;
    int32_t memory_write_index;
    CcCharacterKnowledge knowledge[CC_CHARACTER_KNOWLEDGE_CAPACITY];
    int32_t knowledge_count;
    int32_t knowledge_write_index;
} CcCharacter;

typedef struct CcRelationship {
    CcId from_character_id;
    CcId to_character_id;
    int32_t affinity;
    int32_t trust;
    int32_t obligation;
    CcRelationshipHistory history;
    CcId cause_event_id;
} CcRelationship;

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
    CcId sponsor_character_id;
    CcId affected_character_id;
    CcId witness_character_id;
    CcSituationDiscoveryStage discovery_stage;
    CcSituationLeadPath lead_path;
    CcId lead_event_id;
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

typedef enum CcJourneyPace {
    CC_JOURNEY_PACE_CAREFUL = 0,
    CC_JOURNEY_PACE_STEADY,
    CC_JOURNEY_PACE_PUSH
} CcJourneyPace;

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
    CcJourneyPace pace;
    bool encounter_triggered;
    bool ambush_pending;
    bool ambush_warned;
    bool ambush_resolved;
    CcId parent_event_id;
} CcJourneyEncounter;

typedef struct CcTravelPreview {
    CcId route_id;
    CcId destination_id;
    CcMoney provision_cost;
    int32_t travel_days;
    int32_t claimed_condition;
    int32_t claimed_danger;
    int32_t chart_accuracy;
    int32_t horse_feed_required;
    int32_t horse_readiness;
    bool charted;
    bool destination_known;
    bool sponsored_guide;
} CcTravelPreview;

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
    CcId actor_id;
    CcId target_id;
    CcId beneficiary_id;
    CcId witness_id;
    int32_t magnitude;
    char text[CC_EVENT_TEXT_CAPACITY];
} CcEvent;

typedef struct CcPlayerCompany {
    CcId id;
    CcId location_id;
    CcMoney coins;
    int32_t cargo[CC_GOOD_COUNT];
    int32_t treasure_cargo_slots;
    int32_t cargo_capacity;
    int32_t passenger_capacity;
    int32_t map_capacity;
    int32_t reputation;
    uint32_t map_catalogue_mask;
    uint32_t map_archive_mask;
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
    CcMoney iron_ledger_reserve;
    CcKingdom kingdoms[CC_MAX_KINGDOMS];
    CcSettlement settlements[CC_MAX_SETTLEMENTS];
    CcRoute routes[CC_MAX_ROUTES];
    CcMap maps[CC_MAX_MAPS];
    CcTreasure treasures[CC_MAX_TREASURES];
    CcFaction factions[CC_MAX_FACTIONS];
    CcShipment shipments[CC_MAX_SHIPMENTS];
    CcCourier couriers[CC_MAX_COURIERS];
    CcBanditGroup bandits[CC_MAX_BANDITS];
    CcGoblinCult goblins;
    CcDragon dragon;
    CcDragonCampaign dragon_campaign;
    CcHoardRaiders hoard_raiders;
    CcMonsterPopulation monsters[CC_MAX_MONSTERS];
    CcDungeon dungeons[CC_MAX_DUNGEONS];
    CcDungeonExpedition dungeon_expedition;
    CcSituation situations[CC_MAX_SITUATIONS];
    CcCharacter characters[CC_MAX_CHARACTERS];
    CcRelationship relationships[CC_MAX_RELATIONSHIPS];
    int32_t relationship_count;
    CcEvent events[CC_MAX_EVENTS];
    CcPlayerCompany player;
    CcHorse horse_team[CC_CARRIAGE_HORSE_COUNT];
    CcHorse stable_horses[CC_MAX_STABLE_HORSES];
    int32_t stable_horse_count;
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
    int32_t treasure_count;
    int32_t faction_count;
    int32_t shipment_count;
    int32_t bandit_count;
    int32_t monster_count;
    int32_t dungeon_count;
    int32_t situation_count;
    int32_t character_count;
    int32_t event_count;
    int32_t event_write_index;
    int32_t courier_count;
    CcDiplomaticState diplomacy[CC_MAX_KINGDOMS][CC_MAX_KINGDOMS];
    int32_t diplomacy_changed_day[CC_MAX_KINGDOMS][CC_MAX_KINGDOMS];
    int32_t last_shortage_level[CC_MAX_SETTLEMENTS];
    int32_t last_bandit_level[CC_MAX_BANDITS];
    int32_t last_monster_level[CC_MAX_MONSTERS];
} CcSim;

void CcSimInit(CcSim *sim, uint32_t seed);
/* Used by both new worlds and save migrations. Safe to call more than once. */
void CcSimInitializeDragonCycle(CcSim *sim);
void CcSimInitializeDragonEcology(CcSim *sim);
void CcSimInitializeHoardRaiders(CcSim *sim);
void CcSimInitializeAnimalEconomy(CcSim *sim);
void CcSimInitializeHorseStableSystem(CcSim *sim);
void CcSimInitializeCharacters(CcSim *sim);
void CcSimInitializeUnderroad(CcSim *sim);
void CcSimAdvanceDays(CcSim *sim, int32_t days);
bool CcSettlementIsAbandoned(const CcSettlement *settlement);
int32_t CcSimClimateFactor(const CcSim *sim);
int32_t CcDragonCampaignExperience(const CcSim *sim);
/* Consumes exact 60 Hz ticks only while a committed journey is travelling. */
void CcSimAdvanceRuntimeTicks(CcSim *sim, int32_t ticks);
bool CcSimApply(CcSim *sim, const CcCommand *command,
                char *error, size_t error_capacity);
bool CcSimValidate(const CcSim *sim, char *error, size_t error_capacity);
uint64_t CcSimHash(const CcSim *sim);
int32_t CcSimHorseTeamReadiness(const CcSim *sim);
const char *CcJourneyPaceName(CcJourneyPace pace);
int32_t CcSimJourneyEtaMinutes(const CcSim *sim);
int32_t CcSimHorseCount(const CcSim *sim);
const CcHorse *CcSimHorseAt(const CcSim *sim, int32_t index);
const CcHorse *CcSimHorse(const CcSim *sim, CcId horse_id);
const char *CcHorseSexName(CcHorseSex sex);
const char *CcHorseLifeStageName(const CcHorse *horse);
bool CcHorseWorkingReady(const CcHorse *horse);

CcId CcMakeId(CcEntityKind kind, uint64_t serial);
CcEntityKind CcIdKind(CcId id);
const char *CcGoodName(CcGood good);
const char *CcSettlementFunctionName(CcSettlementFunction function);
const char *CcSettlementSizeName(CcSettlementSize size);
const char *CcServiceName(CcServiceKind service);
const char *CcFactionKindName(CcFactionKind kind);
const char *CcKingdomCallingName(CcKingdomCalling calling);
const char *CcBanditCampSizeName(CcBanditCampSize size);
const char *CcBanditRaidPhaseName(CcBanditRaidPhase phase);
const char *CcDungeonStateName(CcDungeonState state);
const char *CcEventKindName(CcEventKind kind);
const char *CcDragonLifeStageName(CcDragonLifeStage stage);
const char *CcDragonActivityName(CcDragonActivity activity);
const char *CcSituationKindName(CcSituationKind kind);

const CcSettlement *CcSimSettlement(const CcSim *sim, CcId id);
CcSettlement *CcSimSettlementMutable(CcSim *sim, CcId id);
const CcRoute *CcSimRoute(const CcSim *sim, CcId id);
const CcRoute *CcSimRouteBetween(const CcSim *sim, CcId a, CcId b);
const CcMap *CcSimMap(const CcSim *sim, CcId id);
const CcMap *CcSimMapForRoute(const CcSim *sim, CcId route_id, CcId owner_id);
bool CcSimTravelPreview(const CcSim *sim, CcId destination_id,
                        CcTravelPreview *preview, char *error,
                        size_t error_capacity);
const CcEvent *CcSimRecentEvent(const CcSim *sim, int32_t offset);
const CcEvent *CcSimEvent(const CcSim *sim, CcId id);
const CcSituation *CcSimSituation(const CcSim *sim, CcId id);
const CcCharacter *CcSimCharacter(const CcSim *sim, CcId id);
const CcDungeon *CcSimDungeon(const CcSim *sim, CcId id);
const CcDungeonRoom *CcSimDungeonCurrentRoom(const CcSim *sim);
int32_t CcSimDungeonVisibleExitCount(const CcSim *sim);
int32_t CcSimDungeonVisibleExitAt(const CcSim *sim, int32_t ordinal);
int32_t CcSimDungeonOpenableShortcut(const CcSim *sim);
bool CcSimDungeonOutcomeAvailable(const CcDungeon *dungeon,
                                  CcDungeonState outcome);
const char *CcDungeonRoomKindName(CcDungeonRoomKind kind);
const char *CcDungeonEncounterName(CcDungeonEncounterKind kind);
const char *CcDungeonReactionName(int32_t reaction);
const CcCharacter *CcSimSituationSponsorCharacter(
    const CcSim *sim, const CcSituation *situation);
const CcCharacter *CcSimSituationAffectedCharacter(
    const CcSim *sim, const CcSituation *situation);
const CcCharacter *CcSimSituationWitnessCharacter(
    const CcSim *sim, const CcSituation *situation);
const CcCharacter *CcSimSituationConversationCharacter(
    const CcSim *sim, const CcSituation *situation, CcId settlement_id);
bool CcCharacterRemembers(const CcCharacter *character,
                          CcCharacterMemoryKind kind, CcId subject_id);
bool CcCharacterKnows(const CcCharacter *character,
                      CcKnowledgeKind kind, CcId subject_id);
const CcRelationship *CcSimRelationship(const CcSim *sim,
                                        CcId from_character_id,
                                        CcId to_character_id);
bool CcSimSituationCanAccept(const CcSim *sim,
                             const CcSituation *situation);
const char *CcRelationshipHistoryName(CcRelationshipHistory history);
const char *CcCharacterRoleName(CcCharacterRole role);
const char *CcCharacterActivityName(CcCharacterActivity activity);
const CcSituation *CcSimAcceptedSituation(const CcSim *sim);
CcId CcSimSituationOfferSettlementId(const CcSim *sim,
                                     const CcSituation *situation);
bool CcSimSituationTouchesSettlement(const CcSim *sim,
                                     const CcSituation *situation,
                                     CcId settlement_id);
const CcSituation *CcSimSituationForSettlement(const CcSim *sim, CcId settlement_id);
const CcBanditGroup *CcSimBanditGroupOnRoute(const CcSim *sim,
                                             CcId route_id);
bool CcSimBanditProvisionDemand(const CcSim *sim, CcId route_id,
                                CcGood *good, int32_t *quantity);
int32_t CcSimBanditReactionRoll(const CcSim *sim, CcId route_id);
const char *CcBanditReactionName(int32_t roll);
int32_t CcSimActiveSituationCount(const CcSim *sim);
int32_t CcSimIncomingGood(const CcSim *sim, CcId settlement_id, CcGood good);
int32_t CcSimRouteDanger(const CcSim *sim, CcId route_id);
int32_t CcSimDragonBattleStrength(const CcSim *sim);
int32_t CcSimInequalityAtSettlement(const CcSim *sim, CcId settlement_id);
int32_t CcSimWarBurdenAtSettlement(const CcSim *sim, CcId settlement_id);
int32_t CcSimWarSupplyCrisisAtSettlement(const CcSim *sim,
                                         CcId settlement_id);
CcMoney CcSimTrackedGold(const CcSim *sim);
int32_t CcSimTrackedGood(const CcSim *sim, CcGood good);
const CcTreasure *CcSimTreasure(const CcSim *sim, CcId id);
int32_t CcSimTreasureCountForOwner(const CcSim *sim, CcId owner_id);
int32_t CcSettlementServiceCapacity(CcSettlementSize size);
int32_t CcSettlementServiceCount(const CcSettlement *settlement);
bool CcSettlementHasService(const CcSettlement *settlement,
                            CcServiceKind service);
bool CcSimStartServiceProject(CcSim *sim, CcId settlement_id,
                              CcServiceKind service,
                              char *error, size_t error_capacity);
bool CcSimKingdomsAtWar(const CcSim *sim, CcId first, CcId second);
bool CcSimKingdomsAllied(const CcSim *sim, CcId first, CcId second);
CcKingdomCalling CcSimKingdomCalling(const CcSim *sim, CcId kingdom_id);
int32_t CcSimKingdomPressure(const CcSim *sim, CcId kingdom_id);
const char *CcDiplomaticStateName(CcDiplomaticState state);
bool CcSimRouteCrossesWarBorder(const CcSim *sim, CcId route_id);
int32_t CcBanditCampServiceCapacity(CcBanditCampSize size);
bool CcSimLaunchBanditRaid(CcSim *sim, CcId bandit_id,
                           char *error, size_t error_capacity);
int32_t CcPlayerCargoUsed(const CcPlayerCompany *player);
int32_t CcPlayerMapCount(const CcSim *sim);
int32_t CcPlayerMapCollectionCount(const CcSim *sim);
bool CcSimMapIsCatalogued(const CcSim *sim, const CcMap *map);
bool CcSimMapIsArchived(const CcSim *sim, const CcMap *map);
void CcSimUpgradeMapCollection(CcSim *sim);

#endif
