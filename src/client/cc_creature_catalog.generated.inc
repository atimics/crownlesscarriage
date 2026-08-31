
static const CcCreatureDefinition
CC_CREATURE_DEFINITIONS[CC_CREATURE_VARIANT_COUNT] = {
    [CC_CREATURE_GOBLIN_SCAVENGER] = {"goblin_scavenger", "goblin", "npc_stepped", false, false},
    [CC_CREATURE_GOBLIN_RAIDER] = {"goblin_raider", "goblin", "npc_stepped", false, false},
    [CC_CREATURE_GOBLIN_TRIBUTE_BEARER] = {"goblin_tribute_bearer", "goblin", "npc_stepped", false, false},
    [CC_CREATURE_HORSE] = {"horse", "horse", "quadruped_runtime_skin", true, true},
    [CC_CREATURE_COW] = {"cow", "cow", "quadruped_runtime_skin", true, true},
    [CC_CREATURE_DRAGON] = {"dragon", "dragon", "dragon_authored", true, false},
    [CC_CREATURE_DRAGON_WHELP] = {"dragon_whelp", "dragon", "dragon_authored", true, false},
    [CC_CREATURE_DRAGON_WANDERER] = {"dragon_wanderer", "dragon", "dragon_authored", true, false},
    [CC_CREATURE_DRAGON_DEEP_WYRM] = {"dragon_deep_wyrm", "dragon", "dragon_authored", true, false},
    [CC_CREATURE_SHEEP] = {"sheep", "sheep", "quadruped_runtime_skin", true, true},
};

static const char *const
CC_CREATURE_ASSET_PATHS[CC_CREATURE_VARIANT_COUNT]
                       [CC_CREATURE_POSE_COUNT] = {
    [CC_CREATURE_GOBLIN_SCAVENGER] = {
        [CC_CREATURE_POSE_IDLE] = "assets/exports/creatures/creature_goblin_scavenger_v01.glb",
        [CC_CREATURE_POSE_CONTACT_A] = "assets/exports/creatures/creature_goblin_scavenger_contact_a_v01.glb",
        [CC_CREATURE_POSE_DOWN_A] = "assets/exports/creatures/creature_goblin_scavenger_down_a_v01.glb",
        [CC_CREATURE_POSE_PASSING_A] = "assets/exports/creatures/creature_goblin_scavenger_passing_a_v01.glb",
        [CC_CREATURE_POSE_UP_A] = "assets/exports/creatures/creature_goblin_scavenger_up_a_v01.glb",
        [CC_CREATURE_POSE_CONTACT_B] = "assets/exports/creatures/creature_goblin_scavenger_contact_b_v01.glb",
        [CC_CREATURE_POSE_DOWN_B] = "assets/exports/creatures/creature_goblin_scavenger_down_b_v01.glb",
        [CC_CREATURE_POSE_PASSING_B] = "assets/exports/creatures/creature_goblin_scavenger_passing_b_v01.glb",
        [CC_CREATURE_POSE_UP_B] = "assets/exports/creatures/creature_goblin_scavenger_up_b_v01.glb",
    },
    [CC_CREATURE_GOBLIN_RAIDER] = {
        [CC_CREATURE_POSE_IDLE] = "assets/exports/creatures/creature_goblin_raider_v01.glb",
        [CC_CREATURE_POSE_CONTACT_A] = "assets/exports/creatures/creature_goblin_raider_contact_a_v01.glb",
        [CC_CREATURE_POSE_DOWN_A] = "assets/exports/creatures/creature_goblin_raider_down_a_v01.glb",
        [CC_CREATURE_POSE_PASSING_A] = "assets/exports/creatures/creature_goblin_raider_passing_a_v01.glb",
        [CC_CREATURE_POSE_UP_A] = "assets/exports/creatures/creature_goblin_raider_up_a_v01.glb",
        [CC_CREATURE_POSE_CONTACT_B] = "assets/exports/creatures/creature_goblin_raider_contact_b_v01.glb",
        [CC_CREATURE_POSE_DOWN_B] = "assets/exports/creatures/creature_goblin_raider_down_b_v01.glb",
        [CC_CREATURE_POSE_PASSING_B] = "assets/exports/creatures/creature_goblin_raider_passing_b_v01.glb",
        [CC_CREATURE_POSE_UP_B] = "assets/exports/creatures/creature_goblin_raider_up_b_v01.glb",
    },
    [CC_CREATURE_GOBLIN_TRIBUTE_BEARER] = {
        [CC_CREATURE_POSE_IDLE] = "assets/exports/creatures/creature_goblin_tribute_bearer_v01.glb",
        [CC_CREATURE_POSE_CONTACT_A] = "assets/exports/creatures/creature_goblin_tribute_bearer_contact_a_v01.glb",
        [CC_CREATURE_POSE_DOWN_A] = "assets/exports/creatures/creature_goblin_tribute_bearer_down_a_v01.glb",
        [CC_CREATURE_POSE_PASSING_A] = "assets/exports/creatures/creature_goblin_tribute_bearer_passing_a_v01.glb",
        [CC_CREATURE_POSE_UP_A] = "assets/exports/creatures/creature_goblin_tribute_bearer_up_a_v01.glb",
        [CC_CREATURE_POSE_CONTACT_B] = "assets/exports/creatures/creature_goblin_tribute_bearer_contact_b_v01.glb",
        [CC_CREATURE_POSE_DOWN_B] = "assets/exports/creatures/creature_goblin_tribute_bearer_down_b_v01.glb",
        [CC_CREATURE_POSE_PASSING_B] = "assets/exports/creatures/creature_goblin_tribute_bearer_passing_b_v01.glb",
        [CC_CREATURE_POSE_UP_B] = "assets/exports/creatures/creature_goblin_tribute_bearer_up_b_v01.glb",
    },
    [CC_CREATURE_HORSE] = {
        [CC_CREATURE_POSE_IDLE] = "assets/exports/creatures/creature_horse_v01.glb",
    },
    [CC_CREATURE_COW] = {
        [CC_CREATURE_POSE_IDLE] = "assets/exports/creatures/creature_cow_v01.glb",
    },
    [CC_CREATURE_DRAGON] = {
        [CC_CREATURE_POSE_IDLE] = "assets/exports/creatures/creature_dragon_v01.glb",
        [CC_CREATURE_POSE_STALK_A] = "assets/exports/creatures/creature_dragon_stalk_a_v01.glb",
        [CC_CREATURE_POSE_STALK_B] = "assets/exports/creatures/creature_dragon_stalk_b_v01.glb",
        [CC_CREATURE_POSE_THREAT] = "assets/exports/creatures/creature_dragon_threat_v01.glb",
        [CC_CREATURE_POSE_REST] = "assets/exports/creatures/creature_dragon_rest_v01.glb",
    },
    [CC_CREATURE_DRAGON_WHELP] = {
        [CC_CREATURE_POSE_IDLE] = "assets/exports/creatures/creature_dragon_whelp_v01.glb",
        [CC_CREATURE_POSE_STALK_A] = "assets/exports/creatures/creature_dragon_whelp_stalk_a_v01.glb",
        [CC_CREATURE_POSE_STALK_B] = "assets/exports/creatures/creature_dragon_whelp_stalk_b_v01.glb",
        [CC_CREATURE_POSE_THREAT] = "assets/exports/creatures/creature_dragon_whelp_threat_v01.glb",
        [CC_CREATURE_POSE_REST] = "assets/exports/creatures/creature_dragon_whelp_rest_v01.glb",
    },
    [CC_CREATURE_DRAGON_WANDERER] = {
        [CC_CREATURE_POSE_IDLE] = "assets/exports/creatures/creature_dragon_wanderer_v01.glb",
        [CC_CREATURE_POSE_STALK_A] = "assets/exports/creatures/creature_dragon_wanderer_stalk_a_v01.glb",
        [CC_CREATURE_POSE_STALK_B] = "assets/exports/creatures/creature_dragon_wanderer_stalk_b_v01.glb",
        [CC_CREATURE_POSE_THREAT] = "assets/exports/creatures/creature_dragon_wanderer_threat_v01.glb",
        [CC_CREATURE_POSE_REST] = "assets/exports/creatures/creature_dragon_wanderer_rest_v01.glb",
    },
    [CC_CREATURE_DRAGON_DEEP_WYRM] = {
        [CC_CREATURE_POSE_IDLE] = "assets/exports/creatures/creature_dragon_deep_wyrm_v01.glb",
        [CC_CREATURE_POSE_STALK_A] = "assets/exports/creatures/creature_dragon_deep_wyrm_stalk_a_v01.glb",
        [CC_CREATURE_POSE_STALK_B] = "assets/exports/creatures/creature_dragon_deep_wyrm_stalk_b_v01.glb",
        [CC_CREATURE_POSE_THREAT] = "assets/exports/creatures/creature_dragon_deep_wyrm_threat_v01.glb",
        [CC_CREATURE_POSE_REST] = "assets/exports/creatures/creature_dragon_deep_wyrm_rest_v01.glb",
    },
    [CC_CREATURE_SHEEP] = {
        [CC_CREATURE_POSE_IDLE] = "assets/exports/creatures/creature_sheep_v01.glb",
    },
};
