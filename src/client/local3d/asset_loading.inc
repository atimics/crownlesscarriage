/* Runtime model, shader, palette, and renderer resource ownership. */
typedef struct SphereModelCache {
    Model small;
    Model character;
    Model scenery;
    bool ready;
} SphereModelCache;

static SphereModelCache sphere_models = {0};
static RenderTexture2D npc_portrait_target = {0};

typedef enum TreeCrownShape {
    TREE_CROWN_ALDER = 0,
    TREE_CROWN_OAK,
    TREE_CROWN_POLLARD
} TreeCrownShape;

typedef struct TreeCrownModelCache {
    Model alder;
    Model oak;
    Model pollard;
    bool ready;
} TreeCrownModelCache;

static TreeCrownModelCache tree_crown_models = {0};

#define CC_HERO_SKIN_MAX_BONES 64
#define CC_HERO_CAPE_BONE_COUNT 4
#define CC_HERO_HAIR_BONE_COUNT 2
#define CC_HERO_SKIN_ASSET \
    "assets/exports/hero/crownless_hero_engine_rig_v01.glb"
#define CC_SCREEN_FIRST_HERO_SKIN_ASSET \
    "assets/exports/hero/crownless_screen_first_engine_rig_v08.glb"
#define CC_BRIDGE_CHECKPOINT_ASSET \
    "assets/exports/glb/environment_bridge_checkpoint_v01.glb"
#define CC_BRIDGE_CHECKPOINT_MESH_BUDGET 96
#define CC_STYLE_GRADE_SHADER "assets/shaders/style_grade.fs"
#define CC_WORLD_LIGHT_VERTEX_SHADER "assets/shaders/world_lit.vs"
#define CC_WORLD_LIGHT_SKINNED_VERTEX_SHADER \
    "assets/shaders/world_lit_skinned.vs"
#define CC_WORLD_LIGHT_FRAGMENT_SHADER "assets/shaders/world_lit.fs"
#define CC_PAINTED_ENVIRONMENT_FRAGMENT_SHADER \
    "assets/shaders/painted_environment.fs"
#define CC_TREE_FOLIAGE_FRAGMENT_SHADER "assets/shaders/tree_foliage.fs"
#define CC_HERO_PIXEL_FRAGMENT_SHADER "assets/shaders/hero_pixel.fs"
#define CC_HERO_INK_STRENGTH 0.52f
#define CC_NPC_INDEXED_FRAGMENT_SHADER "assets/shaders/npc_indexed.fs"
#define CC_NPC_ARCHETYPE_MATERIAL_COUNT 9
#define CC_NPC_ARCHETYPE_LOCOMOTION_POSE_COUNT 8
#define CC_NPC_ARCHETYPE_WORK_POSE 9
#define CC_NPC_ARCHETYPE_REACT_POSE 10
#define CC_NPC_ARCHETYPE_POSE_COUNT 11
#define CC_NPC_DYNAMIC_HAIR_COUNT 8

typedef enum RuntimeAssetId {
    RUNTIME_ASSET_BRIDGE,
    RUNTIME_ASSET_CARRIAGE,
    RUNTIME_ASSET_MARKET,
    RUNTIME_ASSET_MINE,
    RUNTIME_ASSET_SHORTAGE,
    RUNTIME_ASSET_ENFORCEMENT,
    RUNTIME_ASSET_RECOVERY,
    RUNTIME_ASSET_COUNT
} RuntimeAssetId;

typedef struct RuntimeAsset {
    const char *path;
    const char *label;
    int32_t mesh_budget;
    Model model;
    bool ready;
    bool attempted;
} RuntimeAsset;

static RuntimeAsset runtime_assets[RUNTIME_ASSET_COUNT] = {
    [RUNTIME_ASSET_BRIDGE] = {
        .path = CC_BRIDGE_CHECKPOINT_ASSET,
        .label = "bridge checkpoint",
        .mesh_budget = 96},
    [RUNTIME_ASSET_CARRIAGE] = {
        .path = "assets/exports/glb/carriage_base_v01.glb",
        .label = "carriage",
        .mesh_budget = 144},
    [RUNTIME_ASSET_MARKET] = {
        .path = "assets/exports/glb/environment_market_granary_v01.glb",
        .label = "market and granary",
        .mesh_budget = 80},
    [RUNTIME_ASSET_MINE] = {
        .path = "assets/exports/glb/environment_mine_entrance_v01.glb",
        .label = "mine entrance",
        .mesh_budget = 64},
    [RUNTIME_ASSET_SHORTAGE] = {
        .path = "assets/exports/glb/state_food_shortage_v01.glb",
        .label = "food shortage dressing",
        .mesh_budget = 40},
    [RUNTIME_ASSET_ENFORCEMENT] = {
        .path = "assets/exports/glb/state_harsh_enforcement_v01.glb",
        .label = "enforcement dressing",
        .mesh_budget = 32},
    [RUNTIME_ASSET_RECOVERY] = {
        .path = "assets/exports/glb/state_market_recovery_v01.glb",
        .label = "market recovery dressing",
        .mesh_budget = 40},
};

typedef struct EconomicCargoAsset {
    const char *path;
    const char *label;
    Model model;
    bool ready;
} EconomicCargoAsset;

static EconomicCargoAsset economic_cargo_assets[CC_GOOD_COUNT] = {
    [CC_GOOD_FOOD] = {
        .path = "assets/exports/glb/economy_cargo_food_v01.glb",
        .label = "food cargo"},
    [CC_GOOD_IRON] = {
        .path = "assets/exports/glb/economy_cargo_iron_v01.glb",
        .label = "iron cargo"},
    [CC_GOOD_TOOLS] = {
        .path = "assets/exports/glb/economy_cargo_tools_v01.glb",
        .label = "tool cargo"},
    [CC_GOOD_WEAPONS] = {
        .path = "assets/exports/glb/economy_cargo_weapons_v01.glb",
        .label = "weapon cargo"},
    [CC_GOOD_GOLD] = {
        .path = "assets/exports/glb/economy_cargo_gold_v01.glb",
        .label = "raw gold cargo"},
    [CC_GOOD_GEMS] = {
        .path = "assets/exports/glb/economy_cargo_gems_v01.glb",
        .label = "gem cargo"},
    [CC_GOOD_WOOD] = {
        .path = "assets/exports/glb/economy_cargo_wood_v01.glb",
        .label = "wood cargo"},
    [CC_GOOD_WHEAT] = {
        .path = "assets/exports/glb/economy_cargo_wheat_v01.glb",
        .label = "wheat cargo"},
    [CC_GOOD_MEAT] = {
        .path = "assets/exports/glb/economy_cargo_meat_v01.glb",
        .label = "meat cargo"},
    [CC_GOOD_WOOL] = {
        .path = "assets/exports/glb/economy_cargo_wool_v01.glb",
        .label = "wool cargo"},
    [CC_GOOD_STONE] = {
        .path = "assets/exports/glb/economy_cargo_stone_v01.glb",
        .label = "stone cargo"},
    [CC_GOOD_PAPER] = {
        .path = "assets/exports/glb/economy_cargo_paper_v01.glb",
        .label = "paper cargo"},
    [CC_GOOD_ROTTEN_MEAT] = {
        .path = "assets/exports/glb/economy_cargo_rotten_meat_v01.glb",
        .label = "rotten meat cargo"},
    [CC_GOOD_ROTTEN_GRAIN] = {
        .path = "assets/exports/glb/economy_cargo_rotten_grain_v01.glb",
        .label = "rotten grain cargo"},
};

typedef struct HeroSkinCache {
    Model model;
    ModelAnimation animation;
    Transform pose[CC_HERO_SKIN_MAX_BONES];
    Transform *frames[1];
    int32_t skin_bone[CC_HERO_SKIN_MAX_BONES];
    int32_t cape_bone[CC_HERO_SKIN_MAX_BONES];
    int32_t hair_bone[CC_HERO_SKIN_MAX_BONES];
    bool ready;
} HeroSkinCache;

static HeroSkinCache hero_skin = {0};
static bool screen_first_hero_requested = false;
static bool screen_first_hero_active = false;

#define CC_NPC_BODY_FRAME_COUNT 3
#define CC_NPC_BODY_MUSCLE_COUNT 3
#define CC_NPC_BODY_TISSUE_COUNT 4

typedef struct NpcBodySkinCache {
    Model model;
    ModelAnimation animation;
    Transform pose[CC_HERO_SKIN_MAX_BONES];
    Transform *frames[1];
    int32_t skin_bone[CC_HERO_SKIN_MAX_BONES];
    bool ready;
} NpcBodySkinCache;

static const char *NPC_BODY_FRAME_NAMES[CC_NPC_BODY_FRAME_COUNT] = {
    "lean", "standard", "heavy",
};
static const char *NPC_BODY_MUSCLE_NAMES[CC_NPC_BODY_MUSCLE_COUNT] = {
    "slight", "athletic", "power",
};
static const char *NPC_BODY_TISSUE_NAMES[CC_NPC_BODY_TISSUE_COUNT] = {
    "low", "balanced", "central", "lower_body",
};
static NpcBodySkinCache npc_body_skins
    [CC_NPC_BODY_FRAME_COUNT]
    [CC_NPC_BODY_MUSCLE_COUNT]
    [CC_NPC_BODY_TISSUE_COUNT] = {0};

typedef struct NpcHeadFamilyCache {
    Model model;
    bool ready;
} NpcHeadFamilyCache;

static const char *NPC_HEAD_FAMILY_NAMES[CC_NPC_HEAD_FAMILY_COUNT] = {
    "square", "long", "broad", "veteran",
};
static NpcHeadFamilyCache npc_head_families[CC_NPC_HEAD_FAMILY_COUNT] = {0};

typedef struct NpcHairFamilyCache {
    Model model;
    bool ready;
} NpcHairFamilyCache;

static const char *NPC_HAIR_FAMILY_NAMES[CC_NPC_HAIR_FAMILY_COUNT] = {
    "cropped", "swept", "bob", "crest", "braided", "rear_lock",
};
static NpcHairFamilyCache npc_hair_families[CC_NPC_HAIR_FAMILY_COUNT] = {0};

typedef struct NpcArchetypeCache {
    Model model;
    bool ready;
} NpcArchetypeCache;

static const char *NPC_ARCHETYPE_ROLE_PATH_NAMES[CC_NPC_ROLE_COUNT] = {
    [CC_NPC_ROLE_WAYFARER] = "wayfarer",
    [CC_NPC_ROLE_GUARD] = "guard",
    [CC_NPC_ROLE_RAIDER] = "raider",
    [CC_NPC_ROLE_MERCHANT] = "merchant",
    [CC_NPC_ROLE_LABORER] = "laborer",
    [CC_NPC_ROLE_TRAVELLER] = "traveller",
    [CC_NPC_ROLE_REFUGEE] = "refugee",
    [CC_NPC_ROLE_SCOUT] = "scout",
    [CC_NPC_ROLE_HEALER] = "healer",
};

static const char *NPC_ARCHETYPE_POSE_PATH_SUFFIXES
    [CC_NPC_ARCHETYPE_POSE_COUNT] = {
    "", "_contact_l", "_down_l", "_passing_l", "_up_l",
    "_contact_r", "_down_r", "_passing_r", "_up_r",
    "_work", "_react",
};

static NpcArchetypeCache npc_archetypes
    [CC_NPC_ROLE_COUNT][CC_NPC_ARCHETYPE_POSE_COUNT] = {0};
static bool npc_review_exact_yaw = false;

typedef struct CreatureModelCache {
    Model model;
    ModelAnimation animation;
    Transform pose[CC_QUADRUPED_BONE_COUNT];
    Transform *frames[1];
    int32_t quadruped_bone[CC_QUADRUPED_BONE_COUNT];
    bool ready;
} CreatureModelCache;

static CreatureModelCache creature_models
    [CC_CREATURE_VARIANT_COUNT][CC_CREATURE_POSE_COUNT] = {0};

typedef enum CreatureGaitSlot {
    CREATURE_GAIT_ROAD_HORSE_LEFT,
    CREATURE_GAIT_ROAD_HORSE_RIGHT,
    CREATURE_GAIT_ROAD_COW,
    CREATURE_GAIT_STREET_COW,
    CREATURE_GAIT_SLOT_COUNT
} CreatureGaitSlot;

typedef struct CreatureGaitCache {
    CcCreatureRigController controller;
    CcCreatureRigProfile profile;
    float last_clock;
    bool ready;
} CreatureGaitCache;

static CreatureGaitCache creature_gaits[CREATURE_GAIT_SLOT_COUNT] = {0};


typedef enum NpcDynamicModuleId {
    NPC_DYNAMIC_TORSO,
    NPC_DYNAMIC_PELVIS,
    NPC_DYNAMIC_UPPER_ARM,
    NPC_DYNAMIC_FOREARM,
    NPC_DYNAMIC_THIGH,
    NPC_DYNAMIC_SHIN,
    NPC_DYNAMIC_HAND,
    NPC_DYNAMIC_FOOT,
    NPC_DYNAMIC_HEAD,
    NPC_DYNAMIC_MANTLE,
    NPC_DYNAMIC_COAT_TAIL,
    NPC_DYNAMIC_CHEST_PLATE,
    NPC_DYNAMIC_CHEST_PLATE_RAIDER,
    NPC_DYNAMIC_CHEST_PLATE_HERO,
    NPC_DYNAMIC_PAULDRON,
    NPC_DYNAMIC_PAULDRON_RAIDER,
    NPC_DYNAMIC_PAULDRON_HERO,
    NPC_DYNAMIC_APRON,
    NPC_DYNAMIC_PACK,
    NPC_DYNAMIC_SATCHEL,
    NPC_DYNAMIC_HELMET,
    NPC_DYNAMIC_HAT,
    NPC_DYNAMIC_HOOD,
    NPC_DYNAMIC_HEADWRAP,
    NPC_DYNAMIC_TOOL_SHAFT,
    NPC_DYNAMIC_TOOL_HEAD,
    NPC_DYNAMIC_HAIR_0,
    NPC_DYNAMIC_HAIR_1,
    NPC_DYNAMIC_HAIR_2,
    NPC_DYNAMIC_HAIR_3,
    NPC_DYNAMIC_HAIR_4,
    NPC_DYNAMIC_HAIR_5,
    NPC_DYNAMIC_HAIR_6,
    NPC_DYNAMIC_HAIR_7,
    NPC_DYNAMIC_MODULE_COUNT
} NpcDynamicModuleId;

typedef struct NpcDynamicModuleCache {
    const char *path;
    const char *label;
    Model model;
    bool ready;
} NpcDynamicModuleCache;

static NpcDynamicModuleCache npc_dynamic_modules[NPC_DYNAMIC_MODULE_COUNT] = {
    [NPC_DYNAMIC_TORSO] = {
        .path = "assets/exports/npc/npc_module_torso_v01.glb",
        .label = "torso"},
    [NPC_DYNAMIC_PELVIS] = {
        .path = "assets/exports/npc/npc_module_pelvis_v01.glb",
        .label = "pelvis"},
    [NPC_DYNAMIC_UPPER_ARM] = {
        .path = "assets/exports/npc/npc_module_upper_arm_v01.glb",
        .label = "upper arm"},
    [NPC_DYNAMIC_FOREARM] = {
        .path = "assets/exports/npc/npc_module_forearm_v01.glb",
        .label = "forearm"},
    [NPC_DYNAMIC_THIGH] = {
        .path = "assets/exports/npc/npc_module_thigh_v01.glb",
        .label = "thigh"},
    [NPC_DYNAMIC_SHIN] = {
        .path = "assets/exports/npc/npc_module_shin_v01.glb",
        .label = "shin"},
    [NPC_DYNAMIC_HAND] = {
        .path = "assets/exports/npc/npc_module_hand_v01.glb",
        .label = "hand"},
    [NPC_DYNAMIC_FOOT] = {
        .path = "assets/exports/npc/npc_module_foot_v01.glb",
        .label = "foot"},
    [NPC_DYNAMIC_HEAD] = {
        .path = "assets/exports/npc/npc_module_head_v01.glb",
        .label = "head"},
    [NPC_DYNAMIC_MANTLE] = {
        .path = "assets/exports/npc/npc_module_mantle_v01.glb",
        .label = "mantle"},
    [NPC_DYNAMIC_COAT_TAIL] = {
        .path = "assets/exports/npc/npc_module_coat_tail_v01.glb",
        .label = "coat tail"},
    [NPC_DYNAMIC_CHEST_PLATE] = {
        .path = "assets/exports/npc/npc_module_chest_plate_v01.glb",
        .label = "guard chest plate"},
    [NPC_DYNAMIC_CHEST_PLATE_RAIDER] = {
        .path = "assets/exports/npc/npc_module_chest_plate_raider_v01.glb",
        .label = "raider chest plate"},
    [NPC_DYNAMIC_CHEST_PLATE_HERO] = {
        .path = "assets/exports/npc/npc_module_chest_plate_hero_v01.glb",
        .label = "hero chest plate"},
    [NPC_DYNAMIC_PAULDRON] = {
        .path = "assets/exports/npc/npc_module_pauldron_v01.glb",
        .label = "guard pauldron"},
    [NPC_DYNAMIC_PAULDRON_RAIDER] = {
        .path = "assets/exports/npc/npc_module_pauldron_raider_v01.glb",
        .label = "raider pauldron"},
    [NPC_DYNAMIC_PAULDRON_HERO] = {
        .path = "assets/exports/npc/npc_module_pauldron_hero_v01.glb",
        .label = "hero pauldron"},
    [NPC_DYNAMIC_APRON] = {
        .path = "assets/exports/npc/npc_module_apron_v01.glb",
        .label = "apron"},
    [NPC_DYNAMIC_PACK] = {
        .path = "assets/exports/npc/npc_module_pack_v01.glb",
        .label = "pack"},
    [NPC_DYNAMIC_SATCHEL] = {
        .path = "assets/exports/npc/npc_module_satchel_v01.glb",
        .label = "satchel"},
    [NPC_DYNAMIC_HELMET] = {
        .path = "assets/exports/npc/npc_module_helmet_v01.glb",
        .label = "helmet"},
    [NPC_DYNAMIC_HAT] = {
        .path = "assets/exports/npc/npc_module_hat_v01.glb",
        .label = "hat"},
    [NPC_DYNAMIC_HOOD] = {
        .path = "assets/exports/npc/npc_module_hood_v01.glb",
        .label = "hood"},
    [NPC_DYNAMIC_HEADWRAP] = {
        .path = "assets/exports/npc/npc_module_headwrap_v01.glb",
        .label = "headwrap"},
    [NPC_DYNAMIC_TOOL_SHAFT] = {
        .path = "assets/exports/npc/npc_module_tool_shaft_v01.glb",
        .label = "tool shaft"},
    [NPC_DYNAMIC_TOOL_HEAD] = {
        .path = "assets/exports/npc/npc_module_tool_head_v01.glb",
        .label = "tool head"},
    [NPC_DYNAMIC_HAIR_0] = {
        .path = "assets/exports/npc/npc_module_hair_0_v01.glb",
        .label = "hair 0"},
    [NPC_DYNAMIC_HAIR_1] = {
        .path = "assets/exports/npc/npc_module_hair_1_v01.glb",
        .label = "hair 1"},
    [NPC_DYNAMIC_HAIR_2] = {
        .path = "assets/exports/npc/npc_module_hair_2_v01.glb",
        .label = "hair 2"},
    [NPC_DYNAMIC_HAIR_3] = {
        .path = "assets/exports/npc/npc_module_hair_3_v01.glb",
        .label = "hair 3"},
    [NPC_DYNAMIC_HAIR_4] = {
        .path = "assets/exports/npc/npc_module_hair_4_v01.glb",
        .label = "hair 4"},
    [NPC_DYNAMIC_HAIR_5] = {
        .path = "assets/exports/npc/npc_module_hair_5_v01.glb",
        .label = "hair 5"},
    [NPC_DYNAMIC_HAIR_6] = {
        .path = "assets/exports/npc/npc_module_hair_6_v01.glb",
        .label = "hair 6"},
    [NPC_DYNAMIC_HAIR_7] = {
        .path = "assets/exports/npc/npc_module_hair_7_v01.glb",
        .label = "hair 7"},
};

static Matrix NpcModuleTransform(Vector3 origin, Vector3 right, Vector3 up,
                                 Vector3 forward, Vector3 scale);
static bool DrawNpcDynamicModule(NpcDynamicModuleId id, Matrix transform,
                                 Color color);
static bool DrawNpcHairFamily(const CcNpcAppearance *appearance,
                              Matrix transform, Matrix fallback_transform);

static NpcDynamicModuleId NpcHeadwearModule(uint8_t style)
{
    static const NpcDynamicModuleId modules[4] = {
        NPC_DYNAMIC_HELMET, NPC_DYNAMIC_HAT,
        NPC_DYNAMIC_HOOD, NPC_DYNAMIC_HEADWRAP,
    };
    return modules[style % 4U];
}

typedef struct PaintedEnvironmentStyle {
    Shader shader;
    int32_t light_direction_location;
    int32_t light_color_location;
    int32_t ambient_color_location;
    int32_t camera_position_location;
    int32_t camera_forward_location;
    int32_t shadow_color_location;
    int32_t fog_color_location;
    int32_t fog_near_location;
    int32_t fog_far_location;
    int32_t focal_point_location;
    int32_t story_axis_location;
    int32_t foreground_anchor_location;
    int32_t depth_splits_location;
    int32_t depth_strength_location;
    int32_t focal_contrast_location;
    int32_t reveal_cut_height_location;
    int32_t foreground_reveal_location;
    bool ready;
} PaintedEnvironmentStyle;

typedef struct VisualStyleCache {
    Shader grade;
    Texture2D palette_lut;
    int32_t palette_lut_location;
    int32_t grade_exposure_location;
    int32_t grade_shadow_tone_location;
    int32_t grade_highlight_tone_location;
    int32_t grade_chroma_location;
    int32_t grade_practical_glow_location;
    bool grade_ready;
    Shader world;
    int32_t light_direction_location;
    int32_t light_color_location;
    int32_t ambient_color_location;
    int32_t camera_position_location;
    int32_t camera_forward_location;
    int32_t shadow_color_location;
    int32_t fog_color_location;
    int32_t fog_near_location;
    int32_t fog_far_location;
    int32_t focal_point_location;
    int32_t story_axis_location;
    int32_t foreground_anchor_location;
    int32_t depth_splits_location;
    int32_t depth_strength_location;
    int32_t focal_contrast_location;
    int32_t reveal_cut_height_location;
    int32_t foreground_reveal_location;
    int32_t terrain_surface_location;
    int32_t weather_wetness_location;
    bool world_ready;
    PaintedEnvironmentStyle painted_environment;
    Shader foliage;
    int32_t foliage_light_direction_location;
    int32_t foliage_light_color_location;
    int32_t foliage_ambient_color_location;
    int32_t foliage_camera_position_location;
    int32_t foliage_camera_forward_location;
    int32_t foliage_shadow_color_location;
    int32_t foliage_fog_color_location;
    int32_t foliage_fog_near_location;
    int32_t foliage_fog_far_location;
    int32_t foliage_depth_splits_location;
    int32_t foliage_depth_strength_location;
    bool foliage_ready;
    Shader hero;
    int32_t hero_light_direction_location;
    int32_t hero_camera_position_location;
    int32_t hero_shadow_color_location;
    int32_t hero_fog_color_location;
    int32_t hero_fog_near_location;
    int32_t hero_fog_far_location;
    int32_t hero_ink_strength_location;
    int32_t hero_rim_tint_location;
    int32_t hero_rim_strength_location;
    bool hero_ready;
    Shader npc;
    int32_t npc_light_direction_location;
    int32_t npc_camera_position_location;
    int32_t npc_shadow_color_location;
    int32_t npc_fog_color_location;
    int32_t npc_fog_near_location;
    int32_t npc_fog_far_location;
    int32_t npc_ink_strength_location;
    int32_t npc_palette_location;
    int32_t npc_palette_ink_location;
    int32_t npc_hero_emphasis_location;
    int32_t npc_hero_head_position_location;
    int32_t npc_rim_tint_location;
    int32_t npc_rim_strength_location;
    bool npc_ready;
    Shader npc_skinned;
    int32_t npc_skinned_light_direction_location;
    int32_t npc_skinned_camera_position_location;
    int32_t npc_skinned_shadow_color_location;
    int32_t npc_skinned_fog_color_location;
    int32_t npc_skinned_fog_near_location;
    int32_t npc_skinned_fog_far_location;
    int32_t npc_skinned_ink_strength_location;
    int32_t npc_skinned_palette_location;
    int32_t npc_skinned_palette_ink_location;
    int32_t npc_skinned_hero_emphasis_location;
    int32_t npc_skinned_hero_head_position_location;
    int32_t npc_skinned_body_skin_remap_location;
    int32_t npc_skinned_rim_tint_location;
    int32_t npc_skinned_rim_strength_location;
    bool npc_skinned_ready;
    ArtAtmosphereDefinition presentation_atmosphere;
    float presentation_practical_glow;
} VisualStyleCache;

static VisualStyleCache visual_style = {0};

#define CC_SHARED_PALETTE_LUT_SIZE 64
#define CC_SHARED_PALETTE_LUT_TILES 8
#define CC_FINAL_PALETTE_MAX_COLORS 64
#define CC_HERO_RETRO_MATERIAL_COUNT 19

typedef enum FinalPaletteOwnership {
    FINAL_PALETTE_ENVIRONMENT = 0,
    FINAL_PALETTE_PROTECTED
} FinalPaletteOwnership;

typedef struct FinalPaletteEntry {
    Color color;
    FinalPaletteOwnership ownership;
} FinalPaletteEntry;


static void AddFinalPaletteColor(FinalPaletteEntry *entries, int32_t capacity,
                                 int32_t *count, Color color,
                                 FinalPaletteOwnership ownership)
{
    if (entries == NULL || count == NULL) return;
    for (int32_t index = 0; index < *count; ++index) {
        Color current = entries[index].color;
        if (current.r != color.r || current.g != color.g ||
            current.b != color.b || current.a != color.a) continue;
        if (ownership == FINAL_PALETTE_PROTECTED) {
            entries[index].ownership = ownership;
        }
        return;
    }
    if (*count >= capacity) return;
    entries[*count] = (FinalPaletteEntry){color, ownership};
    *count += 1;
}

static void AddFinalPaletteRamp(FinalPaletteEntry *entries, int32_t capacity,
                                int32_t *count, CcStyleRamp ramp,
                                FinalPaletteOwnership ownership)
{
    AddFinalPaletteColor(entries, capacity, count, ramp.shadow, ownership);
    AddFinalPaletteColor(entries, capacity, count, ramp.base, ownership);
    AddFinalPaletteColor(entries, capacity, count, ramp.light, ownership);
}

static int32_t BuildFinalPalette(FinalPaletteEntry *entries, int32_t capacity)
{
    int32_t count = 0;

    AddFinalPaletteColor(entries, capacity, &count,
                         CC_VISUAL_PALETTE.cool_ink,
                         FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteColor(entries, capacity, &count,
                         CC_VISUAL_PALETTE.warm_ink,
                         FINAL_PALETTE_ENVIRONMENT);

    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.earth,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.road,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.wood,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.stone,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.grass,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.foliage,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.crop,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.metal,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.parchment,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count,
                        CC_VISUAL_PALETTE.contraband,
                        FINAL_PALETTE_ENVIRONMENT);

    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.teal,
                        FINAL_PALETTE_PROTECTED);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.gold,
                        FINAL_PALETTE_PROTECTED);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.danger,
                        FINAL_PALETTE_PROTECTED);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.violet,
                        FINAL_PALETTE_PROTECTED);
    AddFinalPaletteRamp(entries, capacity, &count,
                        CC_VISUAL_PALETTE.people_skin,
                        FINAL_PALETTE_PROTECTED);

    AddFinalPaletteColor(entries, capacity, &count,
                         CC_STYLE_HERO_SKIN_SHADOW,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_SKIN,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_SKIN_LIGHT,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_HAIR,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_UNDERLAYER,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_OUTER,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_TROUSERS,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_LEATHER,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_METAL,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_ACCENT,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_PANEL_INK,
                         FINAL_PALETTE_PROTECTED);
    return count;
}


static Color HeroRetroMaterialColor(int32_t material)
{
    switch (material) {
        case 0: return CC_STYLE_HERO_TROUSERS;
        case 1: return CC_STYLE_HERO_SKIN;
        case 2: return CC_STYLE_HERO_SKIN_LIGHT;
        case 3:
        case 4: return CC_STYLE_HERO_HAIR;
        case 5: return CC_STYLE_METAL;
        case 6:
        case 12: return CC_STYLE_METAL_SHADOW;
        case 7: return CC_STYLE_TEAL_SHADOW;
        case 8: return CC_STYLE_HERO_UNDERLAYER;
        case 9: return CC_STYLE_HERO_ACCENT;
        case 10: return CC_STYLE_HERO_OUTER;
        case 11:
        case 13: return CC_STYLE_HERO_METAL;
        case 14: return CC_STYLE_DANGER;
        case 15: return CC_STYLE_HERO_LEATHER;
        case 16: return CC_STYLE_METAL;
        case 17: return CC_STYLE_METAL_LIGHT;
        case 18: return CC_STYLE_WOOD;
        default: return CC_STYLE_HERO_TROUSERS;
    }
}

static const Vector3 HERO_REST_DIRECTIONS[CC_HUMANOID_SKIN_BONE_COUNT] = {
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {-0.469f, -0.883f, 0.0f},
    {-0.325f, -0.945f, 0.044f},
    {-0.061f, -0.979f, 0.184f},
    {0.469f, -0.883f, 0.0f},
    {0.325f, -0.945f, 0.044f},
    {0.061f, -0.979f, 0.184f},
    {-0.023f, -1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f},
    {0.0f, -0.182f, 0.983f},
    {0.023f, -1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f},
    {0.0f, -0.182f, 0.983f},
};

static const char *HERO_CAPE_BONE_NAMES[CC_HERO_CAPE_BONE_COUNT] = {
    "cape.0", "cape.1", "cape.2", "cape.3",
};

static const Vector3 HERO_CAPE_REST_DIRECTIONS[CC_HERO_CAPE_BONE_COUNT] = {
    {0.0f, -0.989f, -0.146f},
    {0.0f, -0.986f, -0.164f},
    {0.0f, -0.984f, -0.176f},
    {0.0f, -0.984f, -0.176f},
};

static const char *HERO_HAIR_BONE_NAMES[CC_HERO_HAIR_BONE_COUNT] = {
    "hair.long", "hair.rear",
};


static const Vector3 HERO_HAIR_ROOT_OFFSETS[CC_HERO_HAIR_BONE_COUNT] = {
    {-0.18f, 0.39f, 0.08f},
    {0.00f, 0.38f, -0.12f},
};

static const Vector3 HERO_HAIR_REST_DIRECTIONS[CC_HERO_HAIR_BONE_COUNT] = {
    {-0.133f, -0.986f, -0.106f},
    {0.000f, -0.979f, -0.202f},
};

static const float HERO_HAIR_TIP_DELAY[CC_HERO_HAIR_BONE_COUNT] = {
    0.12f, 0.18f,
};

static int32_t HeroCapeBoneFind(const char *name)
{
    for (int32_t bone = 0; bone < CC_HERO_CAPE_BONE_COUNT; ++bone) {
        if (strcmp(name, HERO_CAPE_BONE_NAMES[bone]) == 0) return bone;
    }
    return -1;
}

static int32_t HeroHairBoneFind(const char *name)
{
    for (int32_t bone = 0; bone < CC_HERO_HAIR_BONE_COUNT; ++bone) {
        if (strcmp(name, HERO_HAIR_BONE_NAMES[bone]) == 0) return bone;
    }
    return -1;
}

static Quaternion HeroRotationBetween(Vector3 from, Vector3 to)
{
    from = Vector3Normalize(from);
    to = Vector3Normalize(to);
    float cosine = Vector3DotProduct(from, to);
    if (cosine > -0.9995f) return QuaternionFromVector3ToVector3(from, to);
    Vector3 axis = Vector3CrossProduct(from, (Vector3){1.0f, 0.0f, 0.0f});
    if (Vector3LengthSqr(axis) < 0.0001f) {
        axis = Vector3CrossProduct(from, (Vector3){0.0f, 0.0f, 1.0f});
    }
    return QuaternionFromAxisAngle(Vector3Normalize(axis), PI);
}

static const char *HeroSkinAssetPath(void)
{
    const char *experiment = getenv("CC_SCREEN_FIRST_HERO");
    screen_first_hero_active = screen_first_hero_requested ||
        (experiment != NULL && experiment[0] != '\0' &&
         strcmp(experiment, "0") != 0);
    const char *relative_path = screen_first_hero_active ?
        CC_SCREEN_FIRST_HERO_SKIN_ASSET : CC_HERO_SKIN_ASSET;
    if (FileExists(relative_path)) return relative_path;
    static char bundled_path[1024];
    (void)snprintf(bundled_path, sizeof(bundled_path),
                   "%s/../Resources/%s", GetApplicationDirectory(),
                   relative_path);
    return FileExists(bundled_path) ? bundled_path : NULL;
}

static bool ResolveAssetPath(const char *relative_path, char *resolved,
                             size_t capacity)
{
    if (relative_path == NULL || resolved == NULL || capacity == 0U) {
        return false;
    }
    if (FileExists(relative_path)) {
        (void)snprintf(resolved, capacity, "%s", relative_path);
        return true;
    }
#if defined(CC_ASSET_SOURCE_ROOT)
    (void)snprintf(resolved, capacity, "%s/%s", CC_ASSET_SOURCE_ROOT,
                   relative_path);
    if (FileExists(resolved)) return true;
#endif
    (void)snprintf(resolved, capacity, "../%s", relative_path);
    if (FileExists(resolved)) return true;
    (void)snprintf(resolved, capacity, "%s/../Resources/%s",
                   GetApplicationDirectory(), relative_path);
    return FileExists(resolved);
}

static const char *BridgeCheckpointAssetPath(void)
{
    static char resolved[1024];
    return ResolveAssetPath(CC_BRIDGE_CHECKPOINT_ASSET, resolved,
                            sizeof(resolved)) ? resolved : NULL;
}

static bool RoadUsesAuthoredCheckpoint(void)
{
    if (bridge_checkpoint_status == BRIDGE_CHECKPOINT_UNKNOWN) {
        bridge_checkpoint_status = BridgeCheckpointAssetPath() != NULL ?
            BRIDGE_CHECKPOINT_AVAILABLE : BRIDGE_CHECKPOINT_UNAVAILABLE;
    }
    return bridge_checkpoint_status == BRIDGE_CHECKPOINT_AVAILABLE;
}

static int32_t RoadObstacleCount(void)
{
    return RoadUsesAuthoredCheckpoint() ?
        (int32_t)(sizeof(ROAD_OBSTACLES) / sizeof(ROAD_OBSTACLES[0])) :
        (int32_t)(sizeof(ROAD_FALLBACK_OBSTACLES) /
                  sizeof(ROAD_FALLBACK_OBSTACLES[0]));
}

static Rectangle RoadObstacleAt(int32_t index)
{
    return RoadUsesAuthoredCheckpoint() ? ROAD_OBSTACLES[index] :
                                          ROAD_FALLBACK_OBSTACLES[index];
}

static void ReleaseUploadedModelCpuData(Model *model)
{
#if defined(PLATFORM_WEB)
    if (model == NULL || model->meshes == NULL) return;
    for (int32_t index = 0; index < model->meshCount; ++index) {
        Mesh *mesh = &model->meshes[index];
#define CC_RELEASE_UPLOADED_BUFFER(field) \
        do { \
            if (mesh->field != NULL) { \
                MemFree(mesh->field); \
                mesh->field = NULL; \
            } \
        } while (0)
        CC_RELEASE_UPLOADED_BUFFER(vertices);
        CC_RELEASE_UPLOADED_BUFFER(texcoords);
        CC_RELEASE_UPLOADED_BUFFER(texcoords2);
        CC_RELEASE_UPLOADED_BUFFER(normals);
        CC_RELEASE_UPLOADED_BUFFER(tangents);
        CC_RELEASE_UPLOADED_BUFFER(colors);
        CC_RELEASE_UPLOADED_BUFFER(boneIndices);
        CC_RELEASE_UPLOADED_BUFFER(boneWeights);
        CC_RELEASE_UPLOADED_BUFFER(animVertices);
        CC_RELEASE_UPLOADED_BUFFER(animNormals);
#undef CC_RELEASE_UPLOADED_BUFFER
        /* DrawMesh uses this pointer as the indexed-draw flag, even after the
           index data has been uploaded, so it must remain allocated. */
    }
    /* Loading every model in one uninterrupted browser task keeps temporary
       decoder arrays and graphics-driver staging allocations alive together.
       Yield after each upload so a reload does not approach the renderer's
       page-memory limit before those temporary allocations can be reclaimed. */
    emscripten_sleep(0);
#else
    (void)model;
#endif
}

static void ApplyWorldShader(Model *model);
static void ApplyPaintedEnvironmentShader(Model *model);

static void ApplyRuntimeAssetShader(RuntimeAssetId id)
{
    if (id < 0 || id >= RUNTIME_ASSET_COUNT ||
        !runtime_assets[id].ready) return;
    if (id == RUNTIME_ASSET_MARKET) {
        ApplyPaintedEnvironmentShader(&runtime_assets[id].model);
    } else {
        ApplyWorldShader(&runtime_assets[id].model);
    }
}

static bool LoadRuntimeAsset(RuntimeAssetId id)
{
    if (id < 0 || id >= RUNTIME_ASSET_COUNT) return false;
    RuntimeAsset *asset = &runtime_assets[id];
    if (asset->ready) return true;
    if (asset->attempted) return false;
    asset->attempted = true;
    char resolved[1024];
#if defined(PLATFORM_WEB)
    bool remove_resolved = false;
    if (FileExists(asset->path)) {
        (void)snprintf(resolved, sizeof(resolved), "%s", asset->path);
    } else {
        (void)snprintf(resolved, sizeof(resolved),
                       "/tmp/crownless-runtime-%d.glb", (int32_t)id);
        if (emscripten_wget(asset->path, resolved) != 0) {
            TraceLog(LOG_WARNING,
                     "ASSET: could not download %s; using fallback",
                     asset->label);
            return false;
        }
        remove_resolved = true;
    }
#else
    if (!ResolveAssetPath(asset->path, resolved, sizeof(resolved))) {
        TraceLog(LOG_WARNING, "ASSET: %s was not found; using fallback",
                 asset->label);
        return false;
    }
#endif
    asset->model = LoadModel(resolved);
#if defined(PLATFORM_WEB)
    if (remove_resolved) (void)remove(resolved);
#endif
    int32_t mesh_count = asset->model.meshCount;
    if (mesh_count <= 0 || mesh_count > asset->mesh_budget) {
        TraceLog(LOG_WARNING,
                 "ASSET: invalid %s (%d meshes, budget %d); using fallback",
                 asset->label, mesh_count, asset->mesh_budget);
        if (mesh_count > 0) UnloadModel(asset->model);
        asset->model = (Model){0};
        return false;
    }
    ReleaseUploadedModelCpuData(&asset->model);
    asset->ready = true;
    ApplyRuntimeAssetShader(id);
    TraceLog(LOG_INFO, "ASSET: loaded %s (%d meshes)", asset->label,
             mesh_count);
    return true;
}

static void UnloadRuntimeAsset(RuntimeAssetId id)
{
    if (id < 0 || id >= RUNTIME_ASSET_COUNT) return;
    RuntimeAsset *asset = &runtime_assets[id];
    if (asset->ready) UnloadModel(asset->model);
    asset->model = (Model){0};
    asset->ready = false;
    asset->attempted = false;
    if (id == RUNTIME_ASSET_BRIDGE) {
        bridge_checkpoint_status = BRIDGE_CHECKPOINT_UNKNOWN;
    }
}

static void LoadEconomicCargoAssets(void)
{
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        EconomicCargoAsset *asset = &economic_cargo_assets[good];
        char resolved[1024];
        if (!ResolveAssetPath(asset->path, resolved, sizeof(resolved))) {
            TraceLog(LOG_WARNING, "ASSET: %s was not found", asset->label);
            continue;
        }
        asset->model = LoadModel(resolved);
        int32_t mesh_count = asset->model.meshCount;
        if (mesh_count <= 0 || mesh_count > 32 ||
            asset->model.skeleton.boneCount > 0) {
            TraceLog(LOG_WARNING,
                     "ASSET: invalid %s (%d meshes)",
                     asset->label, mesh_count);
            if (mesh_count > 0) UnloadModel(asset->model);
            asset->model = (Model){0};
            continue;
        }
        ReleaseUploadedModelCpuData(&asset->model);
        asset->ready = true;
        TraceLog(LOG_INFO, "ASSET: loaded %s (%d meshes)",
                 asset->label, mesh_count);
    }
}

static void UnloadEconomicCargoAssets(void)
{
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        EconomicCargoAsset *asset = &economic_cargo_assets[good];
        if (asset->ready) UnloadModel(asset->model);
        asset->model = (Model){0};
        asset->ready = false;
    }
}

static void PrepareRuntimeAssets(uint32_t requested)
{
#if defined(PLATFORM_WEB)
    requested |= UINT32_C(1) << RUNTIME_ASSET_CARRIAGE;
    for (int32_t id = 0; id < RUNTIME_ASSET_COUNT; ++id) {
        bool wanted = (requested & (UINT32_C(1) << id)) != 0;
        if (wanted) {
            bool loaded = LoadRuntimeAsset((RuntimeAssetId)id);
            if (id == RUNTIME_ASSET_BRIDGE) {
                bridge_checkpoint_status = loaded ?
                    BRIDGE_CHECKPOINT_AVAILABLE :
                    BRIDGE_CHECKPOINT_UNAVAILABLE;
            }
        } else if (runtime_assets[id].ready ||
                   runtime_assets[id].attempted) {
            UnloadRuntimeAsset((RuntimeAssetId)id);
        }
    }
#else
    (void)requested;
#endif
}

static uint32_t StreetRuntimeAssetMask(const CcSettlement *place)
{
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForSettlement(place);
    if (profile == NULL || profile->function != CC_SETTLEMENT_MARKET) {
        return 0U;
    }
    uint32_t requested = UINT32_C(1) << RUNTIME_ASSET_MARKET;
    if (place != NULL && place->hunger >= 30) {
        requested |= UINT32_C(1) << RUNTIME_ASSET_SHORTAGE;
    } else if (place != NULL && place->security >= 70) {
        requested |= UINT32_C(1) << RUNTIME_ASSET_ENFORCEMENT;
    } else if (place != NULL && place->prosperity >= 60) {
        requested |= UINT32_C(1) << RUNTIME_ASSET_RECOVERY;
    }
    return requested;
}

static void LoadNpcArchetypes(void)
{
    for (int32_t role = 0; role < CC_NPC_ROLE_COUNT; ++role) {
        for (int32_t pose = 0; pose < CC_NPC_ARCHETYPE_POSE_COUNT; ++pose) {
#if defined(PLATFORM_WEB)
            /* Browser actors share the idle silhouette for stepped motion.
               Keeping 90 additional uploaded poses made combat exceed the
               browser's page-memory budget on constrained devices. */
            if (pose != 0) continue;
#endif
            char path[256];
            (void)snprintf(path, sizeof(path),
                           "assets/exports/npc/npc_%s%s_v01.glb",
                           NPC_ARCHETYPE_ROLE_PATH_NAMES[role],
                           NPC_ARCHETYPE_POSE_PATH_SUFFIXES[pose]);
            char resolved[1024];
            if (!ResolveAssetPath(path, resolved, sizeof(resolved))) {
                TraceLog(LOG_WARNING, "NPC: archetype %s pose %d was not found",
                         CcNpcRoleName((CcNpcRole)role), pose);
                continue;
            }
            Model model = LoadModel(resolved);
            if (model.meshCount != 1 || model.materialCount < 1) {
                TraceLog(LOG_WARNING,
                         "NPC: invalid %s pose %d (%d meshes, %d materials)",
                         CcNpcRoleName((CcNpcRole)role), pose,
                         model.meshCount, model.materialCount);
                if (model.meshCount > 0) UnloadModel(model);
                continue;
            }
            ReleaseUploadedModelCpuData(&model);
            npc_archetypes[role][pose].model = model;
            npc_archetypes[role][pose].ready = true;
        }
        TraceLog(LOG_INFO, "NPC: loaded %s stepped archetype",
                 CcNpcRoleName((CcNpcRole)role));
    }
}

static void LoadCreatureModels(void)
{
    for (int32_t variant = 0; variant < CC_CREATURE_VARIANT_COUNT;
         ++variant) {
        const CcCreatureDefinition *definition = CcCreatureDefinitionAt(
            (CcCreatureVariant)variant);
        if (definition == NULL || !definition->skinned) {
            TraceLog(LOG_INFO, "CREATURE: %s uses procedural rendering",
                     definition != NULL ? definition->name : "unknown");
            continue;
        }
        int32_t loaded_count = 0;
        for (int32_t pose = 0; pose < CC_CREATURE_POSE_COUNT; ++pose) {
            /* Runtime-skinned creatures animate one uploaded rest mesh. Their
               authored pose files are reference outputs, not draw inputs. */
            if (pose != CC_CREATURE_POSE_IDLE) continue;
            const char *path = CcCreatureAssetPath(
                (CcCreatureVariant)variant, (CcCreaturePose)pose);
            if (path == NULL) continue;
            char resolved[1024];
            if (!ResolveAssetPath(path, resolved, sizeof(resolved))) {
                TraceLog(LOG_WARNING, "CREATURE: %s was not found", path);
                continue;
            }
            Model model = LoadModel(resolved);
            if (model.meshCount != 1 || model.materialCount < 1 ||
                model.skeleton.boneCount != CC_QUADRUPED_BONE_COUNT) {
                TraceLog(LOG_WARNING,
                         "CREATURE: invalid %s (%d meshes, %d materials, %d bones)",
                         path, model.meshCount, model.materialCount,
                         model.skeleton.boneCount);
                if (model.meshCount > 0) UnloadModel(model);
                continue;
            }
            CreatureModelCache *cached = &creature_models[variant][pose];
            cached->model = model;
            bool found[CC_QUADRUPED_BONE_COUNT] = {false};
            bool valid_skin = true;
            for (int32_t bone = 0; bone < CC_QUADRUPED_BONE_COUNT; ++bone) {
                int32_t quadruped_bone = CcQuadrupedBoneFind(
                    model.skeleton.bones[bone].name);
                cached->quadruped_bone[bone] = quadruped_bone;
                if (quadruped_bone < 0) {
                    valid_skin = false;
                } else {
                    found[quadruped_bone] = true;
                }
            }
            for (int32_t bone = 0; bone < CC_QUADRUPED_BONE_COUNT; ++bone) {
                if (!found[bone]) valid_skin = false;
            }
            if (!valid_skin) {
                TraceLog(LOG_WARNING,
                         "CREATURE: %s has the wrong quadruped bones",
                         path);
                UnloadModel(cached->model);
                *cached = (CreatureModelCache){0};
                continue;
            }
            cached->frames[0] = cached->pose;
            cached->animation.boneCount = CC_QUADRUPED_BONE_COUNT;
            cached->animation.keyframeCount = 1;
            cached->animation.keyframePoses = cached->frames;
            (void)snprintf(cached->animation.name,
                           sizeof(cached->animation.name),
                           "quadruped-runtime");
            ReleaseUploadedModelCpuData(&cached->model);
            cached->ready = true;
            loaded_count += 1;
        }
        TraceLog(LOG_INFO, "CREATURE: loaded %s (%d/%d poses)",
                 definition->name, loaded_count, 1);
    }
}

static void LoadNpcDynamicModules(void)
{
    int32_t loaded_count = 0;
    for (int32_t id = 0; id < NPC_DYNAMIC_MODULE_COUNT; ++id) {
        NpcDynamicModuleCache *module = &npc_dynamic_modules[id];
        char resolved[1024];
        if (!ResolveAssetPath(module->path, resolved, sizeof(resolved))) {
            TraceLog(LOG_WARNING, "NPC MODULE: %s was not found",
                     module->label);
            continue;
        }
        Model model = LoadModel(resolved);
        if (model.meshCount != 1 || model.materialCount < 1 ||
            model.skeleton.boneCount != 0) {
            TraceLog(LOG_WARNING,
                     "NPC MODULE: invalid %s (%d meshes, %d materials, %d bones)",
                     module->label, model.meshCount, model.materialCount,
                     model.skeleton.boneCount);
            if (model.meshCount > 0) UnloadModel(model);
            continue;
        }
        ReleaseUploadedModelCpuData(&model);
        module->model = model;
        module->ready = true;
        loaded_count += 1;
    }
    TraceLog(LOG_INFO,
             "NPC MODULE: loaded %d/%d rigid biomechanical modules",
             loaded_count, NPC_DYNAMIC_MODULE_COUNT);
}

static void LoadNpcBodySkins(void)
{
    int32_t loaded_count = 0;
    for (int32_t frame = 0; frame < CC_NPC_BODY_FRAME_COUNT; ++frame) {
        for (int32_t muscle = 0; muscle < CC_NPC_BODY_MUSCLE_COUNT; ++muscle) {
            for (int32_t tissue = 0; tissue < CC_NPC_BODY_TISSUE_COUNT;
                 ++tissue) {
#if defined(PLATFORM_WEB)
                /* A shared body retains skinned combat motion without keeping
                   all 36 body recipes resident in browser GPU memory. */
                if (frame != 1 || muscle != 1 || tissue != 1) continue;
#endif
                NpcBodySkinCache *body =
                    &npc_body_skins[frame][muscle][tissue];
                char path[256];
                (void)snprintf(
                    path, sizeof(path),
                    "assets/exports/world_kit/wk_body_skin_%s_%s_%s_v01.glb",
                    NPC_BODY_FRAME_NAMES[frame],
                    NPC_BODY_MUSCLE_NAMES[muscle],
                    NPC_BODY_TISSUE_NAMES[tissue]);
                char resolved[1024];
                if (!ResolveAssetPath(path, resolved, sizeof(resolved))) {
                    TraceLog(LOG_WARNING, "NPC BODY: %s was not found", path);
                    continue;
                }
                body->model = LoadModel(resolved);
                int32_t bone_count = body->model.skeleton.boneCount;
                if (body->model.meshCount <= 0 ||
                    body->model.meshCount > 2 ||
                    body->model.materialCount < 1 ||
                    bone_count <= 0 || bone_count > CC_HERO_SKIN_MAX_BONES) {
                    TraceLog(LOG_WARNING,
                             "NPC BODY: invalid %s (%d meshes, %d bones)",
                             path, body->model.meshCount, bone_count);
                    if (body->model.meshCount > 0) UnloadModel(body->model);
                    *body = (NpcBodySkinCache){0};
                    continue;
                }
                bool found[CC_HUMANOID_SKIN_BONE_COUNT] = {false};
                for (int32_t bone = 0; bone < bone_count; ++bone) {
                    int32_t skin_bone = CcHumanoidSkinBoneFind(
                        body->model.skeleton.bones[bone].name);
                    body->skin_bone[bone] = skin_bone;
                    if (skin_bone >= 0) found[skin_bone] = true;
                }
                bool complete = true;
                for (int32_t bone = 0;
                     bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
                    if (found[bone]) continue;
                    TraceLog(LOG_WARNING,
                             "NPC BODY: %s is missing bone %s", path,
                             CcHumanoidSkinBoneName(
                                 (CcHumanoidSkinBone)bone));
                    complete = false;
                }
                if (!complete) {
                    UnloadModel(body->model);
                    *body = (NpcBodySkinCache){0};
                    continue;
                }
                body->frames[0] = body->pose;
                body->animation.boneCount = bone_count;
                body->animation.keyframeCount = 1;
                body->animation.keyframePoses = body->frames;
                (void)snprintf(body->animation.name,
                               sizeof(body->animation.name),
                               "physics-body");
                ReleaseUploadedModelCpuData(&body->model);
                body->ready = true;
                loaded_count += 1;
            }
        }
    }
    TraceLog(LOG_INFO, "NPC BODY: loaded %d/36 continuous body recipes",
             loaded_count);
}

static void LoadNpcHeadFamilies(void)
{
    int32_t loaded_count = 0;
    for (int32_t family = 0; family < CC_NPC_HEAD_FAMILY_COUNT; ++family) {
        char path[192];
        (void)snprintf(path, sizeof(path),
                       "assets/exports/world_kit/wk_head_%s_v01.glb",
                       NPC_HEAD_FAMILY_NAMES[family]);
        char resolved[1024];
        if (!ResolveAssetPath(path, resolved, sizeof(resolved))) {
            TraceLog(LOG_WARNING, "NPC HEAD: %s was not found", path);
            continue;
        }
        Model model = LoadModel(resolved);
        if (model.meshCount < 1 || model.meshCount > 7 ||
            model.materialCount < 1 || model.skeleton.boneCount != 0) {
            TraceLog(LOG_WARNING,
                     "NPC HEAD: invalid %s (%d meshes, %d bones)",
                     path, model.meshCount, model.skeleton.boneCount);
            if (model.meshCount > 0) UnloadModel(model);
            continue;
        }
        ReleaseUploadedModelCpuData(&model);
        npc_head_families[family].model = model;
        npc_head_families[family].ready = true;
        loaded_count += 1;
    }
    TraceLog(LOG_INFO, "NPC HEAD: loaded %d/%d modular head families",
             loaded_count, CC_NPC_HEAD_FAMILY_COUNT);
}

static void LoadNpcHairFamilies(void)
{
    int32_t loaded_count = 0;
    for (int32_t family = 0; family < CC_NPC_HAIR_FAMILY_COUNT; ++family) {
        char path[192];
        (void)snprintf(path, sizeof(path),
                       "assets/exports/world_kit/wk_hair_%s_v01.glb",
                       NPC_HAIR_FAMILY_NAMES[family]);
        char resolved[1024];
        if (!ResolveAssetPath(path, resolved, sizeof(resolved))) {
            TraceLog(LOG_WARNING, "NPC HAIR: %s was not found", path);
            continue;
        }
        Model model = LoadModel(resolved);
        if (model.meshCount < 1 || model.meshCount > 8 ||
            model.materialCount < 1 || model.skeleton.boneCount != 0) {
            TraceLog(LOG_WARNING,
                     "NPC HAIR: invalid %s (%d meshes, %d bones)", path,
                     model.meshCount, model.skeleton.boneCount);
            if (model.meshCount > 0) UnloadModel(model);
            continue;
        }
        ReleaseUploadedModelCpuData(&model);
        npc_hair_families[family].model = model;
        npc_hair_families[family].ready = true;
        loaded_count += 1;
    }
    TraceLog(LOG_INFO, "NPC HAIR: loaded %d/%d molded hair families",
             loaded_count, CC_NPC_HAIR_FAMILY_COUNT);
}

static void LoadRuntimeAssets(void)
{
#if defined(PLATFORM_WEB)
    (void)LoadRuntimeAsset(RUNTIME_ASSET_CARRIAGE);
#else
    for (int32_t id = 0; id < RUNTIME_ASSET_COUNT; ++id) {
        bool loaded = LoadRuntimeAsset((RuntimeAssetId)id);
        if (id == RUNTIME_ASSET_BRIDGE) {
            bridge_checkpoint_status = loaded ? BRIDGE_CHECKPOINT_AVAILABLE :
                                                BRIDGE_CHECKPOINT_UNAVAILABLE;
        }
    }
#endif
}

static void ApplyWorldShader(Model *model)
{
    if (!visual_style.world_ready || model == NULL || model->materials == NULL) {
        return;
    }
    for (int32_t material = 0; material < model->materialCount; ++material) {
        model->materials[material].shader = visual_style.world;
    }
}

static void ApplyPaintedEnvironmentShader(Model *model)
{
    if (model == NULL || model->materials == NULL) return;
    Shader shader = visual_style.painted_environment.ready ?
        visual_style.painted_environment.shader : visual_style.world;
    for (int32_t material = 0; material < model->materialCount; ++material) {
        model->materials[material].shader = shader;
    }
}

static void ApplyTreeFoliageShader(Model *model)
{
    if (model == NULL || model->materials == NULL) return;
    Shader shader = visual_style.foliage_ready ? visual_style.foliage :
                                                visual_style.world;
    for (int32_t material = 0; material < model->materialCount; ++material) {
        model->materials[material].shader = shader;
    }
}

static void ApplyHeroStyle(Model *model)
{
    if (model == NULL || model->materials == NULL) return;
    if (screen_first_hero_active) {
        for (int32_t material = 0; material < model->materialCount;
             ++material) {
            model->materials[material].shader = visual_style.hero_ready ?
                                                 visual_style.hero :
                                                 visual_style.world;
        }
        return;
    }
    int32_t material_count = model->materialCount <
                             CC_HERO_RETRO_MATERIAL_COUNT ?
                             model->materialCount :
                             CC_HERO_RETRO_MATERIAL_COUNT;
    for (int32_t material = 0; material < material_count; ++material) {
        model->materials[material].maps[MATERIAL_MAP_DIFFUSE].color =
            HeroRetroMaterialColor(material);
        model->materials[material].shader = visual_style.hero_ready ?
                                             visual_style.hero :
                                             visual_style.world;
    }
    for (int32_t material = material_count;
         material < model->materialCount; ++material) {
        model->materials[material].shader = visual_style.hero_ready ?
                                             visual_style.hero :
                                             visual_style.world;
    }
    if (model->materialCount < CC_HERO_RETRO_MATERIAL_COUNT) {
        TraceLog(LOG_WARNING,
                 "HERO: retro palette expected %d materials, found %d",
                 CC_HERO_RETRO_MATERIAL_COUNT, model->materialCount);
    }
}

static void ApplyNpcStyle(Model *model)
{
    if (model == NULL || model->materials == NULL) return;
    for (int32_t material = 0; material < model->materialCount; ++material) {
        model->materials[material].shader = visual_style.npc_ready ?
                                             visual_style.npc :
                                             visual_style.hero_ready ?
                                                visual_style.hero :
                                                visual_style.world;
    }
}

static void ApplyNeutralNpcStyle(Model *model)
{
    ApplyNpcStyle(model);
    if (model == NULL || model->materials == NULL) return;

    for (int32_t material = 0; material < model->materialCount; ++material) {
        model->materials[material].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    }
}

static void ApplyNpcBodyStyle(Model *model)
{
    if (model == NULL || model->materials == NULL) return;
    Shader shader = visual_style.npc_skinned_ready ?
        visual_style.npc_skinned : visual_style.npc;
    for (int32_t material = 0; material < model->materialCount; ++material) {
        model->materials[material].shader = shader;
    }
}

static void SetIndexedPalette(
    const Color colors[CC_NPC_ARCHETYPE_MATERIAL_COUNT],
    float ink_strength, bool hero_emphasis, Vector3 hero_head_position)
{
    if (!visual_style.npc_ready || colors == NULL) return;
    float palette[CC_NPC_ARCHETYPE_MATERIAL_COUNT * 4];
    for (int32_t index = 0; index < CC_NPC_ARCHETYPE_MATERIAL_COUNT;
         ++index) {
        palette[index * 4 + 0] = (float)colors[index].r / 255.0f;
        palette[index * 4 + 1] = (float)colors[index].g / 255.0f;
        palette[index * 4 + 2] = (float)colors[index].b / 255.0f;
        palette[index * 4 + 3] = (float)colors[index].a / 255.0f;
    }
    static const float material_ink[CC_NPC_ARCHETYPE_MATERIAL_COUNT] = {
        0.52f, 0.88f, 0.62f, 0.58f, 0.68f,
        0.76f, 0.46f, 0.60f, 0.96f,
    };
    SetShaderValueV(visual_style.npc, visual_style.npc_palette_location,
                    palette, SHADER_UNIFORM_VEC4,
                    CC_NPC_ARCHETYPE_MATERIAL_COUNT);
    SetShaderValueV(visual_style.npc,
                    visual_style.npc_palette_ink_location,
                    material_ink, SHADER_UNIFORM_FLOAT,
                    CC_NPC_ARCHETYPE_MATERIAL_COUNT);
    SetShaderValue(visual_style.npc,
                   visual_style.npc_ink_strength_location,
                   &ink_strength, SHADER_UNIFORM_FLOAT);
    float hero = hero_emphasis ? 1.0f : 0.0f;
    SetShaderValue(visual_style.npc,
                   visual_style.npc_hero_emphasis_location,
                   &hero, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.npc,
                   visual_style.npc_hero_head_position_location,
                   &hero_head_position, SHADER_UNIFORM_VEC3);
    if (visual_style.npc_skinned_ready) {
        SetShaderValueV(visual_style.npc_skinned,
                        visual_style.npc_skinned_palette_location,
                        palette, SHADER_UNIFORM_VEC4,
                        CC_NPC_ARCHETYPE_MATERIAL_COUNT);
        SetShaderValueV(visual_style.npc_skinned,
                        visual_style.npc_skinned_palette_ink_location,
                        material_ink, SHADER_UNIFORM_FLOAT,
                        CC_NPC_ARCHETYPE_MATERIAL_COUNT);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_ink_strength_location,
                       &ink_strength, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_hero_emphasis_location,
                       &hero, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_hero_head_position_location,
                       &hero_head_position, SHADER_UNIFORM_VEC3);
    }
}

static void SetNpcPalette(const CcNpcAppearance *appearance,
                          float ink_strength, bool hero_emphasis,
                          Vector3 hero_head_position)
{
    if (appearance == NULL) return;
    const Color colors[CC_NPC_ARCHETYPE_MATERIAL_COUNT] = {
        appearance->skin,
        appearance->hair,
        appearance->underlayer,
        appearance->outer,
        appearance->trousers,
        appearance->leather,
        appearance->metal,
        appearance->accent,
        ShadeColor(appearance->hair, 0.62f),
    };
    SetIndexedPalette(colors, ink_strength, hero_emphasis,
                      hero_head_position);
}

typedef struct CreatureRenderPalette {
    Color skin;
    Color secondary;
    Color hide;
    Color cloth;
    Color leather;
    Color horn;
    Color metal;
    Color accent;
    Color eye;
} CreatureRenderPalette;

static bool IsDragonCreatureVariant(CcCreatureVariant variant)
{
    return variant == CC_CREATURE_DRAGON ||
           variant == CC_CREATURE_DRAGON_WHELP ||
           variant == CC_CREATURE_DRAGON_WANDERER ||
           variant == CC_CREATURE_DRAGON_DEEP_WYRM;
}

static float DragonCreatureGrowthScale(CcCreatureVariant variant)
{
    switch (variant) {
        case CC_CREATURE_DRAGON_WHELP: return 0.38f;
        case CC_CREATURE_DRAGON_WANDERER: return 1.50f;
        case CC_CREATURE_DRAGON_DEEP_WYRM: return 3.90f;
        case CC_CREATURE_DRAGON:
        default:
            return 2.80f;
    }
}

static CcCreatureVariant DragonCreatureForLifeStage(
    CcDragonLifeStage stage)
{
    switch (stage) {
        case CC_DRAGON_STAGE_EGG:
        case CC_DRAGON_STAGE_WHELP:
            return CC_CREATURE_DRAGON_WHELP;
        case CC_DRAGON_STAGE_WANDERER:
        case CC_DRAGON_STAGE_UNCROWNED:
            return CC_CREATURE_DRAGON_WANDERER;
        case CC_DRAGON_STAGE_DEEP_WYRM:
            return CC_CREATURE_DRAGON_DEEP_WYRM;
        case CC_DRAGON_STAGE_CROWNED:
        case CC_DRAGON_STAGE_AFTERDRAGON:
        default:
            return CC_CREATURE_DRAGON;
    }
}

static CreatureRenderPalette CreaturePalette(CcCreatureVariant variant,
                                              Color primary)
{
    Color colors[CC_NPC_ARCHETYPE_MATERIAL_COUNT] = {
        (Color){104, 130, 70, 255}, (Color){48, 58, 42, 255},
        (Color){88, 66, 48, 255}, (Color){112, 71, 54, 255},
        (Color){73, 51, 42, 255}, (Color){194, 174, 126, 255},
        (Color){126, 132, 128, 255}, (Color){183, 128, 54, 255},
        (Color){236, 196, 74, 255},
    };
    switch (variant) {
        case CC_CREATURE_GOBLIN_RAIDER:
            colors[0] = (Color){115, 135, 70, 255};
            colors[3] = (Color){113, 52, 46, 255};
            colors[6] = (Color){145, 142, 129, 255};
            colors[7] = (Color){198, 75, 48, 255};
            break;
        case CC_CREATURE_GOBLIN_TRIBUTE_BEARER:
            colors[0] = (Color){117, 141, 76, 255};
            colors[3] = (Color){78, 83, 105, 255};
            colors[7] = (Color){220, 158, 54, 255};
            break;
        case CC_CREATURE_HORSE: {
            Color coat = primary.a > 0 ? primary :
                                         (Color){104, 78, 59, 255};
            colors[0] = coat;
            colors[1] = ShadeColor(coat, 0.54f);
            colors[2] = ShadeColor(coat, 0.82f);
            colors[3] = (Color){72, 48, 38, 255};
            colors[4] = (Color){50, 37, 32, 255};
            colors[7] = (Color){128, 88, 52, 255};
            break;
        }
        case CC_CREATURE_COW: {
            Color hide = primary.a > 0 ? primary :
                                         (Color){180, 166, 137, 255};
            colors[0] = (Color){190, 154, 127, 255};
            colors[1] = (Color){61, 49, 43, 255};
            colors[2] = hide;
            colors[3] = (Color){88, 70, 59, 255};
            colors[5] = (Color){205, 186, 143, 255};
            colors[7] = (Color){137, 85, 61, 255};
            break;
        }
        case CC_CREATURE_SHEEP:
            colors[0] = (Color){58, 49, 43, 255};
            colors[1] = (Color){31, 27, 25, 255};
            colors[2] = (Color){192, 181, 153, 255};
            colors[3] = primary.a > 0 ? primary :
                                          (Color){228, 220, 185, 255};
            colors[4] = (Color){40, 33, 29, 255};
            colors[7] = (Color){142, 109, 82, 255};
            colors[8] = (Color){12, 10, 9, 255};
            break;
        case CC_CREATURE_DRAGON_WHELP:
            colors[0] = (Color){92, 132, 101, 255};
            colors[1] = (Color){42, 66, 54, 255};
            colors[2] = (Color){115, 145, 91, 255};
            colors[3] = (Color){74, 61, 49, 255};
            colors[5] = (Color){207, 177, 113, 255};
            colors[7] = (Color){181, 76, 47, 255};
            colors[8] = (Color){251, 205, 71, 255};
            break;
        case CC_CREATURE_DRAGON_WANDERER:
            colors[0] = (Color){77, 112, 82, 255};
            colors[1] = (Color){34, 55, 46, 255};
            colors[2] = (Color){99, 129, 82, 255};
            colors[3] = (Color){70, 57, 48, 255};
            colors[5] = (Color){197, 163, 101, 255};
            colors[7] = (Color){169, 65, 45, 255};
            colors[8] = (Color){248, 196, 55, 255};
            break;
        case CC_CREATURE_DRAGON_DEEP_WYRM:
            colors[0] = (Color){48, 75, 64, 255};
            colors[1] = (Color){18, 32, 30, 255};
            colors[2] = (Color){67, 89, 62, 255};
            colors[3] = (Color){53, 45, 42, 255};
            colors[5] = (Color){174, 133, 76, 255};
            colors[7] = (Color){129, 43, 39, 255};
            colors[8] = (Color){255, 174, 31, 255};
            break;
        case CC_CREATURE_DRAGON:
            colors[0] = (Color){71, 100, 73, 255};
            colors[1] = (Color){30, 48, 42, 255};
            colors[2] = (Color){91, 117, 76, 255};
            colors[3] = (Color){66, 54, 48, 255};
            colors[5] = (Color){190, 154, 94, 255};
            colors[7] = (Color){157, 57, 43, 255};
            colors[8] = (Color){245, 190, 48, 255};
            break;
        case CC_CREATURE_GOBLIN_SCAVENGER:
        default:
            break;
    }
    return (CreatureRenderPalette){
        colors[0], colors[1], colors[2], colors[3], colors[4],
        colors[5], colors[6], colors[7], colors[8]
    };
}

static CcCreatureRigProfile CreatureRigProfileForVariant(
    CcCreatureVariant variant)
{
    switch (variant) {
        case CC_CREATURE_HORSE: return CC_CREATURE_RIG_HORSE;
        case CC_CREATURE_COW: return CC_CREATURE_RIG_COW;
        case CC_CREATURE_SHEEP: return CC_CREATURE_RIG_COW;
        case CC_CREATURE_DRAGON:
        case CC_CREATURE_DRAGON_WHELP:
        case CC_CREATURE_DRAGON_WANDERER:
        case CC_CREATURE_DRAGON_DEEP_WYRM:
            return CC_CREATURE_RIG_DRAGON;
        case CC_CREATURE_GOBLIN_SCAVENGER:
        case CC_CREATURE_GOBLIN_RAIDER:
        case CC_CREATURE_GOBLIN_TRIBUTE_BEARER:
        default:
            return CC_CREATURE_RIG_GOBLIN;
    }
}

static bool ResolveControlledCreatureGait(
    CreatureGaitSlot slot, CcCreatureRigProfile profile,
    CcCreatureRigGait gait, float clock, float initial_phase,
    float forward_speed, bool moving, Vector3 ground_position,
    float yaw, CcLocalSceneKind scene,
    CcCreatureRigPose *pose)
{
    if (slot < 0 || slot >= CREATURE_GAIT_SLOT_COUNT || pose == NULL ||
        !isfinite(clock)) {
        return false;
    }
    CreatureGaitCache *cache = &creature_gaits[slot];
    CcCreatureRigWorldCommand command = {
        .ground_position = ToLimbVector(ground_position),
        .velocity = {sinf(yaw) * forward_speed, 0.0f,
                     cosf(yaw) * forward_speed},
        .yaw = yaw,
        .movement = moving ? 1.0f : 0.0f,
        .grounded = true,
    };
    LocalProbeContext context = {.scene = scene};
    float elapsed = cache->ready ? clock - cache->last_clock : 0.0f;
    bool reset = !cache->ready || cache->profile != profile ||
                 elapsed < 0.0f || elapsed > 0.35f;
    if (reset) {
        *cache = (CreatureGaitCache){0};
        if (!CcCreatureRigControllerInit(&cache->controller, profile,
                                         initial_phase, 1.0f)) {
            return false;
        }
        cache->profile = profile;
        cache->last_clock = clock;
        cache->ready = true;
        (void)CcCreatureRigControllerSetGait(&cache->controller, gait);
        if (moving) {
            CcCreatureRigPose warm_pose;
            for (int32_t frame = 0; frame < 36; ++frame) {
                if (!CcCreatureRigControllerStepWorld(
                        &cache->controller, &command, 1.0f / 60.0f,
                        ProbeLocalSurface, &context, &warm_pose)) {
                    *cache = (CreatureGaitCache){0};
                    return false;
                }
            }
        }
        elapsed = 0.0f;
    }

    (void)CcCreatureRigControllerSetGait(&cache->controller, gait);
    cache->last_clock = clock;
    return CcCreatureRigControllerStepWorld(
        &cache->controller, &command, elapsed,
        ProbeLocalSurface, &context, pose);
}

static float CreatureRigPhase(CcCreaturePose pose)
{
    switch (pose) {
        case CC_CREATURE_POSE_CONTACT_A: return 0.00f;
        case CC_CREATURE_POSE_DOWN_A: return 0.125f;
        case CC_CREATURE_POSE_PASSING_A: return 0.250f;
        case CC_CREATURE_POSE_UP_A: return 0.375f;
        case CC_CREATURE_POSE_CONTACT_B: return 0.500f;
        case CC_CREATURE_POSE_DOWN_B: return 0.625f;
        case CC_CREATURE_POSE_PASSING_B: return 0.750f;
        case CC_CREATURE_POSE_UP_B: return 0.875f;
        case CC_CREATURE_POSE_STALK_A: return 0.18f;
        case CC_CREATURE_POSE_STALK_B: return 0.68f;
        case CC_CREATURE_POSE_THREAT: return 0.34f;
        case CC_CREATURE_POSE_IDLE:
        case CC_CREATURE_POSE_REST:
        case CC_CREATURE_POSE_COUNT:
        default:
            return 0.0f;
    }
}

static float CreatureRigMovement(CcCreaturePose pose)
{
    if (pose >= CC_CREATURE_POSE_CONTACT_A &&
        pose <= CC_CREATURE_POSE_UP_B) {
        return 1.0f;
    }
    if (pose == CC_CREATURE_POSE_STALK_A ||
        pose == CC_CREATURE_POSE_STALK_B) {
        return 0.68f;
    }
    return pose == CC_CREATURE_POSE_THREAT ? 0.28f : 0.0f;
}

static void DrawCreatureMuscleLimbs(const CcCreatureRigPose *rig,
                                    const CreatureRenderPalette *palette,
                                    float yaw, float scale)
{
    bool goblin = rig->profile == CC_CREATURE_RIG_GOBLIN;
    for (int32_t limb = 0; limb < rig->limb_count; ++limb) {
        const CcCreatureRigLimbPose *leg = &rig->limbs[limb];
        float base_radius = goblin ? 0.092f :
            rig->profile == CC_CREATURE_RIG_COW ? 0.115f :
            rig->profile == CC_CREATURE_RIG_DRAGON ? 0.142f :
            rig->profile >= CC_CREATURE_RIG_HEXAPOD ? 0.062f : 0.086f;
        base_radius *= scale;
        Color upper = goblin ? palette->cloth : palette->hide;
        Color lower = goblin ? palette->skin : palette->secondary;
        if (rig->profile == CC_CREATURE_RIG_DRAGON) {
            upper = palette->skin;
            lower = palette->accent;
        }
        for (int32_t segment = 0; segment < leg->segment_count; ++segment) {
            float taper = 1.0f - (float)segment * 0.18f;
            float radius = base_radius * taper *
                (1.0f + leg->segment_activation[segment] *
                 (0.42f - (float)segment * 0.06f));
            Vector3 start = FromLimbVector(leg->joints[segment]);
            Vector3 end = FromLimbVector(leg->joints[segment + 1]);
            Color color = segment == 0 ? upper : lower;
            DrawCylinderEx(start, end, radius, radius * 0.78f, 7, color);
            if (segment + 1 < leg->segment_count) {
                DrawCharacterSphere(end, radius * 0.94f,
                                    BlendColor(upper, lower, 0.46f));
            }
        }
        Vector3 foot = FromLimbVector(leg->joints[leg->segment_count]);
        if (goblin) {
            DrawOrientedBox(foot,
                            (Vector3){0.0f, 0.025f * scale, 0.06f * scale},
                            (Vector3){0.22f * scale, 0.10f * scale,
                                      0.30f * scale},
                            yaw, palette->leather);
        } else {
            Color hoof = rig->profile == CC_CREATURE_RIG_DRAGON ?
                palette->accent : palette->leather;
            DrawOrientedBox(foot,
                            (Vector3){0.0f, 0.018f * scale, 0.03f * scale},
                            (Vector3){0.18f * scale, 0.09f * scale,
                                      0.24f * scale},
                            yaw, hoof);
        }
    }
}

static void DrawGoblinRig(CcCreatureVariant variant,
                          const CcCreatureRigPose *rig,
                          const CreatureRenderPalette *palette,
                          float yaw, float scale)
{
    Vector3 pelvis = FromLimbVector(rig->body);
    float wave = sinf(rig->phase * 2.0f * PI);
    Vector3 chest = LocalPoint(pelvis, 0.0f, 0.36f * scale,
                               0.018f * wave * scale, yaw);
    Vector3 neck = LocalPoint(pelvis, 0.0f, 0.58f * scale, 0.0f, yaw);
    Vector3 head = LocalPoint(pelvis, 0.0f, 0.76f * scale, 0.025f, yaw);
    float muscle = 1.0f + rig->mean_activation * 0.48f;

    DrawOrientedBox(pelvis, (Vector3){0.0f, 0.10f * scale, 0.0f},
                    (Vector3){0.38f * scale, 0.22f * scale,
                              0.28f * scale},
                    yaw, palette->leather);
    DrawCylinderEx(pelvis, chest, 0.20f * scale * muscle,
                   0.25f * scale * muscle, 7, palette->cloth);
    DrawCylinderEx(chest, neck, 0.20f * scale, 0.10f * scale, 7,
                   palette->cloth);
    DrawCharacterEllipsoid(head,
                           (Vector3){0.25f * scale, 0.24f * scale,
                                     0.22f * scale},
                           palette->skin);
    for (int32_t side = -1; side <= 1; side += 2) {
        Vector3 ear_root = LocalPoint(
            head, (float)side * 0.18f * scale, 0.02f * scale,
            0.0f, yaw);
        Vector3 ear_tip = LocalPoint(
            head, (float)side * 0.39f * scale, 0.04f * scale,
            -0.01f * scale, yaw);
        DrawCylinderEx(ear_root, ear_tip, 0.075f * scale, 0.012f * scale,
                       5, palette->skin);
        Vector3 eye = LocalPoint(
            head, (float)side * 0.080f * scale, 0.035f * scale,
            0.205f * scale, yaw);
        DrawCharacterSphere(eye, 0.032f * scale, palette->eye);

        float arm_wave = wave * (float)-side;
        Vector3 shoulder = LocalPoint(
            chest, (float)side * 0.26f * scale, 0.02f * scale,
            0.0f, yaw);
        Vector3 elbow = LocalPoint(
            chest, (float)side * 0.32f * scale, -0.23f * scale,
            arm_wave * 0.11f * scale, yaw);
        Vector3 hand = LocalPoint(
            chest, (float)side * 0.31f * scale, -0.48f * scale,
            arm_wave * 0.18f * scale, yaw);
        DrawCylinderEx(shoulder, elbow, 0.082f * scale * muscle,
                       0.068f * scale, 7, palette->cloth);
        DrawCylinderEx(elbow, hand, 0.068f * scale,
                       0.055f * scale, 7, palette->skin);
        DrawCharacterSphere(hand, 0.078f * scale, palette->skin);
    }

    if (variant == CC_CREATURE_GOBLIN_RAIDER) {
        Vector3 helmet_tip = LocalPoint(head, 0.0f, 0.37f * scale,
                                        0.0f, yaw);
        DrawCylinderEx(head, helmet_tip, 0.23f * scale, 0.025f * scale,
                       6, palette->metal);
        DrawOrientedBox(chest, (Vector3){0.0f, 0.0f, 0.17f * scale},
                        (Vector3){0.38f * scale, 0.30f * scale,
                                  0.07f * scale},
                        yaw, palette->metal);
        Vector3 spear_hand = LocalPoint(chest, 0.32f * scale,
                                        -0.38f * scale, 0.10f * scale, yaw);
        Vector3 spear_tip = LocalPoint(chest, 0.32f * scale,
                                       0.78f * scale, 0.08f * scale, yaw);
        DrawCylinderEx(spear_hand, spear_tip, 0.025f * scale,
                       0.018f * scale, 6, palette->leather);
        Vector3 blade_tip = LocalPoint(chest, 0.32f * scale,
                                       1.02f * scale, 0.08f * scale, yaw);
        DrawCylinderEx(spear_tip, blade_tip, 0.075f * scale, 0.0f, 5,
                       palette->metal);
    } else if (variant == CC_CREATURE_GOBLIN_TRIBUTE_BEARER) {
        DrawOrientedBox(chest,
                        (Vector3){0.0f, -0.22f * scale, 0.30f * scale},
                        (Vector3){0.48f * scale, 0.38f * scale,
                                  0.38f * scale},
                        yaw, palette->accent);
        DrawOrientedBox(chest,
                        (Vector3){0.0f, -0.22f * scale, 0.505f * scale},
                        (Vector3){0.10f * scale, 0.40f * scale,
                                  0.025f * scale},
                        yaw, palette->horn);
    } else {
        DrawOrientedBox(chest,
                        (Vector3){0.0f, -0.04f * scale, -0.20f * scale},
                        (Vector3){0.40f * scale, 0.42f * scale,
                                  0.18f * scale},
                        yaw, palette->secondary);
    }
}

static void DrawFarmAnimalRig(CcCreatureVariant variant,
                              const CcCreatureRigPose *rig,
                              const CreatureRenderPalette *palette,
                              float yaw, float scale)
{
    Vector3 body = FromLimbVector(rig->body);
    bool sheep = variant == CC_CREATURE_SHEEP;
    float breathing = 1.0f + rig->mean_activation * 0.22f;
    if (sheep) {
        DrawCharacterEllipsoid(
            body, (Vector3){0.55f * scale * breathing,
                            0.48f * scale * breathing,
                            0.72f * scale}, palette->cloth);
        const Vector3 fleece_offsets[] = {
            {-0.28f, 0.12f, -0.24f}, {0.28f, 0.12f, -0.22f},
            {-0.27f, 0.10f, 0.24f}, {0.27f, 0.10f, 0.24f},
            {0.0f, 0.31f, 0.02f},
        };
        for (int32_t index = 0; index < 5; ++index) {
            Vector3 fleece = LocalPoint(
                body, fleece_offsets[index].x * scale,
                fleece_offsets[index].y * scale,
                fleece_offsets[index].z * scale, yaw);
            DrawCharacterSphere(fleece, 0.27f * scale, palette->cloth);
        }
        Vector3 neck = LocalPoint(body, 0.0f, -0.02f * scale,
                                  rig->body_length * 0.48f, yaw);
        Vector3 head = LocalPoint(body, 0.0f, -0.04f * scale,
                                  rig->body_length * 0.70f, yaw);
        DrawCylinderEx(neck, head, 0.17f * scale, 0.14f * scale, 7,
                       palette->secondary);
        DrawCharacterEllipsoid(
            head, (Vector3){0.21f * scale, 0.24f * scale,
                            0.31f * scale}, palette->secondary);
        Vector3 cap = LocalPoint(head, 0.0f, 0.18f * scale,
                                 -0.02f * scale, yaw);
        DrawCharacterSphere(cap, 0.18f * scale, palette->cloth);
        for (int32_t side = -1; side <= 1; side += 2) {
            Vector3 eye = LocalPoint(
                head, (float)side * 0.14f * scale, 0.06f * scale,
                0.18f * scale, yaw);
            DrawCharacterSphere(eye, 0.026f * scale, palette->eye);
            Vector3 ear_root = LocalPoint(
                head, (float)side * 0.13f * scale, 0.12f * scale,
                -0.02f * scale, yaw);
            Vector3 ear_tip = LocalPoint(
                head, (float)side * 0.32f * scale, 0.10f * scale,
                -0.04f * scale, yaw);
            DrawCylinderEx(ear_root, ear_tip, 0.045f * scale,
                           0.012f * scale, 5, palette->secondary);
        }
        Vector3 tail_root = LocalPoint(body, 0.0f, 0.12f * scale,
                                       -rig->body_length * 0.47f, yaw);
        Vector3 tail_tip = LocalPoint(body, 0.0f, 0.02f * scale,
                                      -rig->body_length * 0.62f, yaw);
        DrawCylinderEx(tail_root, tail_tip, 0.10f * scale,
                       0.055f * scale, 6, palette->cloth);
        return;
    }
    if (variant == CC_CREATURE_HORSE) {
        DrawCharacterEllipsoid(
            body, (Vector3){0.61f * scale * breathing,
                            0.48f * scale * breathing,
                            0.76f * scale}, palette->skin);
        Vector3 shoulder = LocalPoint(body, 0.0f, 0.04f * scale,
                                      0.38f * scale, yaw);
        Vector3 rump = LocalPoint(body, 0.0f, 0.02f * scale,
                                  -0.38f * scale, yaw);
        DrawCharacterSphere(shoulder, 0.43f * scale, palette->skin);
        DrawCharacterSphere(rump, 0.45f * scale, palette->skin);
        Vector3 neck_base = LocalPoint(body, 0.0f, 0.14f * scale,
                                       0.48f * scale, yaw);
        Vector3 neck = LocalPoint(body, 0.0f, 0.38f * scale,
                                  0.72f * scale, yaw);
        Vector3 head = LocalPoint(body, 0.0f, 0.40f * scale,
                                  0.94f * scale, yaw);
        DrawCylinderEx(neck_base, neck, 0.25f * scale, 0.19f * scale,
                       8, palette->skin);
        DrawCharacterEllipsoid(
            head, (Vector3){0.26f * scale, 0.28f * scale,
                            0.36f * scale}, palette->skin);
        Vector3 muzzle = LocalPoint(head, 0.0f, -0.07f * scale,
                                    0.28f * scale, yaw);
        DrawCharacterEllipsoid(
            muzzle, (Vector3){0.22f * scale, 0.17f * scale,
                              0.19f * scale}, palette->secondary);
        const Vector3 mane_points[] = {neck_base, neck, head};
        const float mane_sizes[] = {0.22f, 0.24f, 0.18f};
        for (int32_t index = 0; index < 3; ++index) {
            Vector3 mane = LocalPoint(
                mane_points[index], -0.035f * scale, 0.13f * scale,
                -0.04f * scale, yaw);
            DrawCharacterSphere(mane, mane_sizes[index] * scale,
                                palette->secondary);
        }
        for (int32_t side = -1; side <= 1; side += 2) {
            Vector3 eye = LocalPoint(
                head, (float)side * 0.17f * scale, 0.06f * scale,
                0.17f * scale, yaw);
            DrawCharacterSphere(eye, 0.030f * scale, palette->eye);
            Vector3 ear_root = LocalPoint(
                head, (float)side * 0.11f * scale, 0.20f * scale,
                -0.02f * scale, yaw);
            Vector3 ear_tip = LocalPoint(
                head, (float)side * 0.13f * scale, 0.38f * scale,
                -0.02f * scale, yaw);
            DrawCylinderEx(ear_root, ear_tip, 0.050f * scale,
                           0.012f * scale, 5, palette->skin);
        }
        for (int32_t plume = 0; plume < 3; ++plume) {
            Vector3 tail = LocalPoint(
                body, 0.0f, (0.03f - (float)plume * 0.17f) * scale,
                (-0.64f - (float)plume * 0.18f) * scale, yaw);
            DrawCharacterSphere(tail, (0.18f - (float)plume * 0.01f) * scale,
                                palette->secondary);
        }
        return;
    }
    DrawOrientedBox(body, (Vector3){0.0f, 0.02f * scale, 0.0f},
                    (Vector3){rig->body_width * breathing,
                              rig->body_depth * breathing,
                              rig->body_length},
                    yaw, palette->hide);
    Vector3 neck_base = LocalPoint(
        body, 0.0f, 0.04f * scale,
        rig->body_length * 0.42f, yaw);
    Vector3 neck = LocalPoint(
        body, 0.0f, -0.06f * scale,
        rig->body_length * 0.62f, yaw);
    Vector3 head = LocalPoint(
        body, 0.0f, -0.10f * scale,
        rig->body_length * 0.82f, yaw);
    DrawCylinderEx(neck_base, neck, 0.25f * scale, 0.21f * scale,
                   7, palette->hide);
    DrawCharacterEllipsoid(
        head, (Vector3){0.31f * scale, 0.25f * scale, 0.34f * scale},
        palette->hide);
    for (int32_t side = -1; side <= 1; side += 2) {
        Vector3 eye = LocalPoint(
            head, (float)side * 0.20f * scale,
            0.045f * scale, 0.24f * scale, yaw);
        DrawCharacterSphere(eye, 0.028f * scale, palette->eye);
        Vector3 ear_root = LocalPoint(
            head, (float)side * 0.11f * scale, 0.18f * scale,
            -0.02f * scale, yaw);
        Vector3 ear_tip = LocalPoint(
            head, (float)side * 0.24f * scale,
            0.29f * scale, 0.0f, yaw);
        DrawCylinderEx(ear_root, ear_tip, 0.045f * scale,
                       0.012f * scale, 5, palette->secondary);
        Vector3 horn_tip = LocalPoint(
            head, (float)side * 0.40f * scale, 0.30f * scale,
            0.02f * scale, yaw);
        DrawCylinderEx(ear_root, horn_tip, 0.055f * scale, 0.0f, 6,
                       palette->horn);
    }
    Vector3 tail_root = LocalPoint(body, 0.0f, 0.22f * scale,
                                   -rig->body_length * 0.48f, yaw);
    Vector3 tail_tip = LocalPoint(body, 0.0f, -0.40f * scale,
                                  -rig->body_length * 0.70f, yaw);
    DrawCylinderEx(tail_root, tail_tip, 0.045f * scale,
                   0.018f * scale, 6, palette->secondary);
    Vector3 udder = LocalPoint(body, 0.0f, -0.48f * scale,
                               -0.10f * scale, yaw);
    DrawCharacterEllipsoid(udder,
                           (Vector3){0.20f * scale, 0.12f * scale,
                                     0.22f * scale},
                           palette->accent);
}

static void DrawDoubleSidedTriangle(Vector3 a, Vector3 b, Vector3 c,
                                    Color color)
{
    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(c, b, a, color);
}

static void DrawDragonRig(CcCreatureVariant variant,
                          const CcCreatureRigPose *rig,
                          const CreatureRenderPalette *palette,
                          float yaw, float scale)
{
    Vector3 body = FromLimbVector(rig->body);
    float tension = 1.0f + rig->mean_activation * 0.30f;

    if (variant == CC_CREATURE_DRAGON_WHELP) {
        DrawCharacterEllipsoid(
            body, (Vector3){0.62f * scale * tension, 0.48f * scale,
                            0.72f * scale}, palette->skin);
        Vector3 head = LocalPoint(body, 0.0f, 0.22f * scale,
                                  0.88f * scale, yaw);
        DrawCylinderEx(body, head, 0.24f * scale, 0.20f * scale, 7,
                       palette->hide);
        DrawCharacterEllipsoid(
            head, (Vector3){0.48f * scale, 0.40f * scale,
                            0.52f * scale}, palette->skin);
        Vector3 jaw = LocalPoint(head, 0.0f, -0.20f * scale,
                                 0.34f * scale, yaw);
        DrawOrientedBox(jaw, (Vector3){0},
                        (Vector3){0.54f * scale, 0.16f * scale,
                                  0.38f * scale},
                        yaw, palette->accent);
        for (int32_t side = -1; side <= 1; side += 2) {
            Vector3 eye = LocalPoint(
                head, (float)side * 0.31f * scale, 0.10f * scale,
                0.34f * scale, yaw);
            DrawCharacterSphere(eye, 0.090f * scale, palette->eye);
            Vector3 fin_root = LocalPoint(
                head, (float)side * 0.30f * scale, 0.14f * scale,
                0.02f, yaw);
            Vector3 fin_tip = LocalPoint(
                head, (float)side * 0.72f * scale, 0.34f * scale,
                -0.08f * scale, yaw);
            Vector3 fin_back = LocalPoint(
                head, (float)side * 0.34f * scale, -0.02f * scale,
                -0.18f * scale, yaw);
            DrawDoubleSidedTriangle(fin_root, fin_tip, fin_back,
                                    palette->accent);

            Vector3 wing_root = LocalPoint(
                body, (float)side * 0.36f * scale, 0.30f * scale,
                -0.04f * scale, yaw);
            Vector3 wing_tip = LocalPoint(
                body, (float)side * 0.92f * scale, 0.62f * scale,
                -0.24f * scale, yaw);
            Vector3 wing_back = LocalPoint(
                body, (float)side * 0.42f * scale, 0.20f * scale,
                -0.70f * scale, yaw);
            DrawDoubleSidedTriangle(wing_root, wing_tip, wing_back,
                                    ShadeColor(palette->accent, 0.90f));
        }
        Vector3 tail_a = LocalPoint(body, 0.0f, 0.02f * scale,
                                    -0.50f * scale, yaw);
        Vector3 tail_b = LocalPoint(body, 0.08f * scale, -0.08f * scale,
                                    -1.12f * scale, yaw);
        Vector3 tail_c = LocalPoint(body, -0.08f * scale, -0.13f * scale,
                                    -1.62f * scale, yaw);
        DrawCylinderEx(tail_a, tail_b, 0.20f * scale, 0.10f * scale, 7,
                       palette->skin);
        DrawCylinderEx(tail_b, tail_c, 0.10f * scale, 0.018f * scale, 6,
                       palette->secondary);
        for (int32_t crest = 0; crest < 3; ++crest) {
            const float crest_index = (float)crest;
            Vector3 root = LocalPoint(
                body, 0.0f, (0.52f - crest_index * 0.05f) * scale,
                (0.48f - crest_index * 0.42f) * scale, yaw);
            Vector3 tip = root;
            tip.y += (0.15f - crest_index * 0.02f) * scale;
            DrawCylinderEx(root, tip, 0.050f * scale, 0.0f, 5,
                           palette->horn);
        }
        return;
    }

    if (variant == CC_CREATURE_DRAGON_WANDERER) {
        DrawOrientedBox(body, (Vector3){0.0f, 0.04f * scale, 0.0f},
                        (Vector3){0.68f * scale * tension,
                                  0.46f * scale,
                                  3.00f * scale},
                        yaw, palette->skin);
        Vector3 neck_a = LocalPoint(body, 0.0f, 0.22f * scale,
                                    1.28f * scale, yaw);
        Vector3 neck_b = LocalPoint(body, -0.10f * scale, 0.48f * scale,
                                    2.34f * scale, yaw);
        Vector3 head = LocalPoint(body, 0.08f * scale, 0.54f * scale,
                                  3.54f * scale, yaw);
        DrawCylinderEx(neck_a, neck_b, 0.25f * scale, 0.18f * scale, 7,
                       palette->hide);
        DrawCylinderEx(neck_b, head, 0.18f * scale, 0.10f * scale, 7,
                       palette->skin);
        DrawCharacterEllipsoid(
            head, (Vector3){0.27f * scale, 0.22f * scale,
                            0.76f * scale}, palette->skin);
        Vector3 jaw = LocalPoint(head, 0.0f, -0.15f * scale,
                                 0.58f * scale, yaw);
        DrawOrientedBox(jaw, (Vector3){0},
                        (Vector3){0.40f * scale, 0.13f * scale,
                                  0.76f * scale},
                        yaw, palette->accent);
        for (int32_t side = -1; side <= 1; side += 2) {
            Vector3 eye = LocalPoint(
                head, (float)side * 0.20f * scale, 0.07f * scale,
                0.38f * scale, yaw);
            DrawCharacterSphere(eye, 0.045f * scale, palette->eye);
            Vector3 horn_root = LocalPoint(
                head, (float)side * 0.18f * scale, 0.17f * scale,
                -0.08f * scale, yaw);
            Vector3 horn_tip = LocalPoint(
                head, (float)side * 0.36f * scale, 0.50f * scale,
                -0.72f * scale, yaw);
            DrawCylinderEx(horn_root, horn_tip, 0.065f * scale, 0.0f, 6,
                           palette->horn);

            Vector3 wing_root = LocalPoint(
                body, (float)side * 0.30f * scale, 0.30f * scale,
                0.72f * scale, yaw);
            Vector3 wing_joint = LocalPoint(
                body, (float)side * 1.40f * scale, 1.36f * scale,
                -0.22f * scale, yaw);
            Vector3 wing_tip = LocalPoint(
                body, (float)side * 2.72f * scale, 1.04f * scale,
                -1.48f * scale, yaw);
            Vector3 wing_back = LocalPoint(
                body, (float)side * 1.35f * scale, 0.12f * scale,
                -2.34f * scale, yaw);
            DrawCylinderEx(wing_root, wing_joint, 0.075f * scale,
                           0.040f * scale, 6, palette->skin);
            DrawCylinderEx(wing_joint, wing_tip, 0.045f * scale,
                           0.014f * scale, 6, palette->skin);
            DrawDoubleSidedTriangle(wing_root, wing_joint, wing_back,
                                    ShadeColor(palette->accent, 0.86f));
            DrawDoubleSidedTriangle(wing_joint, wing_tip, wing_back,
                                    ShadeColor(palette->secondary, 0.92f));
        }
        Vector3 tail_a = LocalPoint(body, 0.0f, 0.04f * scale,
                                    -1.42f * scale, yaw);
        Vector3 tail_b = LocalPoint(body, 0.12f * scale, -0.10f * scale,
                                    -3.12f * scale, yaw);
        Vector3 tail_c = LocalPoint(body, -0.18f * scale, -0.22f * scale,
                                    -5.68f * scale, yaw);
        DrawCylinderEx(tail_a, tail_b, 0.24f * scale, 0.13f * scale, 7,
                       palette->skin);
        DrawCylinderEx(tail_b, tail_c, 0.13f * scale, 0.018f * scale, 7,
                       palette->secondary);
        for (int32_t blade = 0; blade < 3; ++blade) {
            const float blade_index = (float)blade;
            Vector3 root = LocalPoint(
                body, 0.0f, (0.50f - blade_index * 0.04f) * scale,
                (1.02f - blade_index * 1.00f) * scale, yaw);
            Vector3 tip = root;
            tip.y += 0.34f * scale;
            DrawCylinderEx(root, tip, 0.055f * scale, 0.0f, 5,
                           palette->horn);
        }
        return;
    }

    if (variant == CC_CREATURE_DRAGON_DEEP_WYRM) {
        Vector3 serpent[] = {
            LocalPoint(body, 0.00f * scale, 0.38f * scale,
                       3.72f * scale, yaw),
            LocalPoint(body, 0.34f * scale, 0.20f * scale,
                       2.10f * scale, yaw),
            LocalPoint(body, -0.46f * scale, 0.08f * scale,
                       0.30f * scale, yaw),
            LocalPoint(body, -0.72f * scale, 0.00f,
                       -1.72f * scale, yaw),
            LocalPoint(body, 0.16f * scale, -0.08f * scale,
                       -3.86f * scale, yaw),
            LocalPoint(body, 0.88f * scale, -0.16f * scale,
                       -6.04f * scale, yaw),
            LocalPoint(body, 0.28f * scale, -0.24f * scale,
                       -8.22f * scale, yaw),
            LocalPoint(body, -0.72f * scale, -0.30f * scale,
                       -10.34f * scale, yaw),
            LocalPoint(body, -0.46f * scale, -0.34f * scale,
                       -12.26f * scale, yaw),
            LocalPoint(body, 0.12f * scale, -0.36f * scale,
                       -13.96f * scale, yaw),
            LocalPoint(body, 0.34f * scale, -0.36f * scale,
                       -15.30f * scale, yaw),
        };
        const float serpent_radii[] = {
            0.82f, 0.88f, 0.84f, 0.76f, 0.66f, 0.54f,
            0.42f, 0.30f, 0.20f, 0.11f, 0.025f,
        };
        const int32_t serpent_count =
            (int32_t)(sizeof(serpent) / sizeof(serpent[0]));
        for (int32_t index = 0; index < serpent_count - 1; ++index) {
            DrawCylinderEx(
                serpent[index], serpent[index + 1],
                serpent_radii[index] * scale * tension,
                serpent_radii[index + 1] * scale, 10,
                index % 3 == 0 ? palette->hide : palette->skin);
        }
        Vector3 neck_a = LocalPoint(body, -0.22f * scale, 0.92f * scale,
                                    4.62f * scale, yaw);
        Vector3 neck_b = LocalPoint(body, 0.18f * scale, 1.16f * scale,
                                    5.52f * scale, yaw);
        Vector3 head = LocalPoint(body, 0.0f, 1.08f * scale,
                                  6.82f * scale, yaw);
        DrawCylinderEx(serpent[0], neck_a, 0.82f * scale,
                       0.65f * scale, 10, palette->skin);
        DrawCylinderEx(neck_a, neck_b, 0.65f * scale,
                       0.46f * scale, 10, palette->hide);
        DrawCylinderEx(neck_b, head, 0.46f * scale,
                       0.27f * scale, 10, palette->skin);
        DrawCharacterEllipsoid(
            head, (Vector3){0.62f * scale, 0.44f * scale,
                            1.05f * scale}, palette->skin);
        Vector3 jaw = LocalPoint(head, 0.0f, -0.24f * scale,
                                 0.82f * scale, yaw);
        DrawOrientedBox(jaw, (Vector3){0},
                        (Vector3){0.84f * scale, 0.24f * scale,
                                  1.18f * scale},
                        yaw, palette->accent);
        for (int32_t side = -1; side <= 1; side += 2) {
            Vector3 eye = LocalPoint(
                head, (float)side * 0.43f * scale, 0.13f * scale,
                0.68f * scale, yaw);
            DrawCharacterSphere(eye, 0.075f * scale, palette->eye);
            for (int32_t crown = 0; crown < 3; ++crown) {
                float amount = (float)crown;
                Vector3 horn_root = LocalPoint(
                    head, (float)side * (0.26f + amount * 0.10f) * scale,
                    (0.38f - amount * 0.10f) * scale,
                    (0.04f - amount * 0.22f) * scale, yaw);
                Vector3 horn_tip = LocalPoint(
                    head, (float)side * (0.74f + amount * 0.24f) * scale,
                    (1.66f - amount * 0.28f) * scale,
                    (-1.06f - amount * 0.12f) * scale, yaw);
                DrawCylinderEx(horn_root, horn_tip,
                               (0.13f - amount * 0.022f) * scale,
                               0.0f, 7, palette->horn);
            }

            Vector3 whisker_root = LocalPoint(
                head, (float)side * 0.38f * scale, -0.02f * scale,
                0.72f * scale, yaw);
            Vector3 whisker_tip = LocalPoint(
                head, (float)side * 1.48f * scale, -0.10f * scale,
                1.72f * scale, yaw);
            DrawCylinderEx(whisker_root, whisker_tip, 0.035f * scale,
                           0.008f * scale, 6, palette->horn);

            Vector3 wing_root = LocalPoint(
                body, (float)side * 0.72f * scale, 0.74f * scale,
                2.38f * scale, yaw);
            Vector3 wing_joint = LocalPoint(
                body, (float)side * 2.46f * scale, 2.50f * scale,
                1.12f * scale, yaw);
            Vector3 wing_tip = LocalPoint(
                body, (float)side * 4.25f * scale, 1.02f * scale,
                -0.42f * scale, yaw);
            Vector3 wing_notch = LocalPoint(
                body, (float)side * 1.38f * scale, 0.50f * scale,
                -0.82f * scale, yaw);
            Vector3 wing_back = LocalPoint(
                body, (float)side * 2.58f * scale, 0.12f * scale,
                -2.40f * scale, yaw);
            DrawCylinderEx(wing_root, wing_joint, 0.19f * scale,
                           0.10f * scale, 8, palette->skin);
            DrawCylinderEx(wing_joint, wing_tip, 0.11f * scale,
                           0.025f * scale, 8, palette->skin);
            DrawDoubleSidedTriangle(wing_root, wing_joint, wing_back,
                                    ShadeColor(palette->secondary, 0.82f));
            DrawDoubleSidedTriangle(wing_joint, wing_tip, wing_notch,
                                    ShadeColor(palette->accent, 0.76f));
            DrawDoubleSidedTriangle(wing_joint, wing_notch, wing_back,
                                    ShadeColor(palette->secondary, 0.90f));

            Vector3 arm_root = LocalPoint(
                body, (float)side * 0.58f * scale, 0.18f * scale,
                3.20f * scale, yaw);
            Vector3 arm_tip = LocalPoint(
                body, (float)side * 1.06f * scale, -0.32f * scale,
                3.62f * scale, yaw);
            DrawCylinderEx(arm_root, arm_tip, 0.13f * scale,
                           0.045f * scale, 7, palette->skin);
        }
        for (int32_t plate = 0; plate < serpent_count - 1; ++plate) {
            Vector3 root = serpent[plate];
            root.y += serpent_radii[plate] * 0.78f * scale;
            Vector3 tip = root;
            float plate_amount = (float)plate / (float)(serpent_count - 2);
            tip.y += (0.62f - plate_amount * 0.40f) * scale;
            DrawCylinderEx(root, tip,
                           fmaxf(0.055f, 0.16f - plate_amount * 0.10f) *
                               scale,
                           0.0f, 6,
                           palette->horn);
        }
        return;
    }

    Vector3 crowned[] = {
        LocalPoint(body, 0.0f, 0.18f * scale, 1.65f * scale, yaw),
        LocalPoint(body, 0.12f * scale, 0.08f * scale,
                   0.30f * scale, yaw),
        LocalPoint(body, -0.18f * scale, 0.0f,
                   -1.30f * scale, yaw),
        LocalPoint(body, 0.10f * scale, -0.08f * scale,
                   -2.65f * scale, yaw),
    };
    const float crowned_radii[] = {0.64f, 0.68f, 0.58f, 0.46f};
    for (int32_t index = 0; index < 3; ++index) {
        DrawCylinderEx(crowned[index], crowned[index + 1],
                       crowned_radii[index] * scale * tension,
                       crowned_radii[index + 1] * scale, 10,
                       index == 0 ? palette->hide : palette->skin);
    }
    Vector3 neck_a = LocalPoint(body, -0.12f * scale, 0.38f * scale,
                                2.38f * scale, yaw);
    Vector3 neck_b = LocalPoint(body, 0.16f * scale, 0.52f * scale,
                                3.20f * scale, yaw);
    Vector3 neck_c = LocalPoint(body, -0.12f * scale, 0.58f * scale,
                                4.08f * scale, yaw);
    Vector3 head = LocalPoint(body, 0.0f, 0.46f * scale,
                              5.28f * scale, yaw);
    DrawCylinderEx(crowned[0], neck_a, 0.64f * scale,
                   0.43f * scale, 9, palette->hide);
    DrawCylinderEx(neck_a, neck_b, 0.43f * scale,
                   0.35f * scale, 9, palette->skin);
    DrawCylinderEx(neck_b, neck_c, 0.35f * scale,
                   0.27f * scale, 9, palette->skin);
    DrawCylinderEx(neck_c, head, 0.27f * scale,
                   0.18f * scale, 9, palette->skin);
    DrawCharacterEllipsoid(
        head, (Vector3){0.40f * scale, 0.30f * scale,
                        0.72f * scale}, palette->skin);
    Vector3 jaw = LocalPoint(head, 0.0f, -0.16f * scale,
                             0.56f * scale, yaw);
    DrawOrientedBox(jaw, (Vector3){0.0f, 0.0f, 0.0f},
                    (Vector3){0.56f * scale, 0.16f * scale,
                              0.78f * scale},
                    yaw, palette->accent);
    for (int32_t side = -1; side <= 1; side += 2) {
        Vector3 eye = LocalPoint(head, (float)side * 0.27f * scale,
                                 0.09f * scale, 0.48f * scale, yaw);
        DrawCharacterSphere(eye, 0.057f * scale, palette->eye);
        Vector3 horn_root = LocalPoint(head, (float)side * 0.24f * scale,
                                       0.29f * scale, 0.05f * scale, yaw);
        Vector3 horn_tip = LocalPoint(head, (float)side * 0.72f * scale,
                                      0.92f * scale, -0.72f * scale, yaw);
        DrawCylinderEx(horn_root, horn_tip, 0.105f * scale, 0.0f, 7,
                       palette->horn);

        Vector3 wing_root = LocalPoint(body, (float)side * 0.58f * scale,
                                       0.48f * scale, 1.18f * scale, yaw);
        Vector3 wing_joint = LocalPoint(
            body, (float)side * 1.82f * scale, 1.18f * scale,
            0.36f * scale, yaw);
        Vector3 wing_tip = LocalPoint(body, (float)side * 2.45f * scale,
                                      0.34f * scale, -1.08f * scale, yaw);
        Vector3 wing_back = LocalPoint(body, (float)side * 2.05f * scale,
                                       0.18f * scale, -2.12f * scale, yaw);
        DrawCylinderEx(wing_root, wing_joint, 0.14f * scale,
                       0.078f * scale, 7, palette->skin);
        DrawDoubleSidedTriangle(wing_root, wing_joint, wing_back,
                                ShadeColor(palette->secondary, 0.82f));
        DrawDoubleSidedTriangle(wing_joint, wing_tip, wing_back,
                                ShadeColor(palette->accent, 0.86f));

        for (int32_t pair = 0; pair < 2; ++pair) {
            float longitudinal = pair == 0 ? 1.35f : -1.72f;
            Vector3 leg_root = LocalPoint(
                body, (float)side * 0.54f * scale, -0.12f * scale,
                longitudinal * scale, yaw);
            Vector3 leg_knee = LocalPoint(
                body, (float)side * 0.76f * scale, -0.52f * scale,
                (longitudinal + (pair == 0 ? 0.20f : -0.20f)) * scale,
                yaw);
            Vector3 leg_foot = LocalPoint(
                body, (float)side * 0.84f * scale, -0.92f * scale,
                (longitudinal + (pair == 0 ? 0.38f : -0.38f)) * scale,
                yaw);
            DrawCylinderEx(leg_root, leg_knee, 0.17f * scale,
                           0.11f * scale, 7, palette->skin);
            DrawCylinderEx(leg_knee, leg_foot, 0.11f * scale,
                           0.045f * scale, 7, palette->secondary);
        }
    }
    Vector3 tail[] = {
        crowned[3],
        LocalPoint(body, 0.20f * scale, -0.18f * scale,
                   -3.68f * scale, yaw),
        LocalPoint(body, -0.30f * scale, -0.28f * scale,
                   -4.90f * scale, yaw),
        LocalPoint(body, -0.56f * scale, -0.38f * scale,
                   -6.20f * scale, yaw),
        LocalPoint(body, 0.18f * scale, -0.42f * scale,
                   -7.48f * scale, yaw),
        LocalPoint(body, 0.50f * scale, -0.35f * scale,
                   -8.68f * scale, yaw),
        LocalPoint(body, 0.18f * scale, -0.22f * scale,
                   -9.76f * scale, yaw),
    };
    const float tail_radii[] = {
        0.46f, 0.42f, 0.34f, 0.25f, 0.16f, 0.085f, 0.022f,
    };
    for (int32_t index = 0; index < 6; ++index) {
        DrawCylinderEx(tail[index], tail[index + 1],
                       tail_radii[index] * scale,
                       tail_radii[index + 1] * scale, 9, palette->skin);
    }
    for (int32_t spine = 0; spine < 8; ++spine) {
        float amount = (float)spine / 7.0f;
        Vector3 root = LocalPoint(body, 0.0f,
                                  (0.78f - amount * 0.38f) * scale,
                                  (4.18f - amount * 7.40f) * scale, yaw);
        Vector3 tip = root;
        tip.y += (0.46f - amount * 0.24f) * scale;
        DrawCylinderEx(root, tip, 0.085f * scale, 0.0f, 6,
                       palette->horn);
    }
}

static CcQuadrupedMorphology QuadrupedMorphologyForCreature(
    CcCreatureVariant variant)
{
    if (variant == CC_CREATURE_HORSE) return CC_QUADRUPED_HORSE;
    if (variant == CC_CREATURE_COW) return CC_QUADRUPED_COW;
    if (variant == CC_CREATURE_SHEEP) return CC_QUADRUPED_SHEEP;
    return CC_QUADRUPED_MORPHOLOGY_COUNT;
}

static bool PoseQuadrupedCreature(CreatureModelCache *creature,
                                  CcCreatureVariant variant, float phase,
                                  bool moving,
                                  const CcCreatureRigPose *controlled_pose)
{
    if (creature == NULL || !creature->ready) return false;
    CcQuadrupedMorphology morphology = QuadrupedMorphologyForCreature(variant);
    CcQuadrupedPose rest = {0};
    CcQuadrupedPose target = {0};
    CcQuadrupedPoseResolve(morphology, 0.0f, false, &rest);
    if (controlled_pose != NULL && controlled_pose->valid) {
        CcQuadrupedPoseResolveFromRig(morphology, controlled_pose, &target);
    } else {
        CcQuadrupedPoseResolve(morphology, phase, moving, &target);
    }
    if (!rest.valid || !target.valid) return false;

    for (int32_t bone = 0; bone < creature->model.skeleton.boneCount;
         ++bone) {
        int32_t quadruped_bone = creature->quadruped_bone[bone];
        if (quadruped_bone < 0 ||
            quadruped_bone >= CC_QUADRUPED_BONE_COUNT) return false;
        const CcQuadrupedBonePose *rest_bone = &rest.bones[quadruped_bone];
        const CcQuadrupedBonePose *target_bone =
            &target.bones[quadruped_bone];
        Quaternion delta = HeroRotationBetween(
            FromLimbVector(rest_bone->up),
            FromLimbVector(target_bone->up));
        creature->pose[bone].translation = FromLimbVector(target_bone->head);
        creature->pose[bone].rotation = QuaternionMultiply(
            delta, creature->model.skeleton.bindPose[bone].rotation);
        creature->pose[bone].scale =
            creature->model.skeleton.bindPose[bone].scale;
    }
    UpdateModelAnimation(creature->model, creature->animation, 0.0f);
    CcLocalRendererRecordCreatureSkinUpdate(creature->model.meshCount);
    return true;
}

static void DrawContactShadow(Vector3 center, float width, float depth,
                              float yaw, Color color)
{
    if (width <= 0.0f || depth <= 0.0f || color.a == 0) return;


    const int32_t segment_count = 12;
    const float half_width = width * 0.5f;
    const float half_depth = depth * 0.5f;
    const float yaw_cos = cosf(yaw);
    const float yaw_sin = sinf(yaw);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(0.0f, 1.0f, 0.0f);
    for (int32_t segment = 0; segment < segment_count; ++segment) {
        float angle_a = 2.0f * PI * (float)segment /
                        (float)segment_count;
        float angle_b = 2.0f * PI * (float)(segment + 1) /
                        (float)segment_count;
        float local_ax = cosf(angle_a) * half_width;
        float local_az = sinf(angle_a) * half_depth;
        float local_bx = cosf(angle_b) * half_width;
        float local_bz = sinf(angle_b) * half_depth;
        Vector3 edge_a = {
            center.x + local_ax * yaw_cos + local_az * yaw_sin,
            center.y,
            center.z - local_ax * yaw_sin + local_az * yaw_cos,
        };
        Vector3 edge_b = {
            center.x + local_bx * yaw_cos + local_bz * yaw_sin,
            center.y,
            center.z - local_bx * yaw_sin + local_bz * yaw_cos,
        };
        rlVertex3f(center.x, center.y, center.z);
        rlVertex3f(edge_b.x, edge_b.y, edge_b.z);
        rlVertex3f(edge_a.x, edge_a.y, edge_a.z);
    }
    rlEnd();
}

static bool DrawCreatureGait3D(CcCreatureVariant variant,
                               CcCreaturePose pose, Vector3 position,
                               float yaw, float scale, Color primary,
                               float gait_phase, bool moving,
                               const CcCreatureRigPose *controlled_pose)
{
    if (variant < 0 || variant >= CC_CREATURE_VARIANT_COUNT || scale <= 0.0f) {
        return false;
    }
    if (pose < 0 || pose >= CC_CREATURE_POSE_COUNT) {
        pose = CC_CREATURE_POSE_IDLE;
    }
    if (IsDragonCreatureVariant(variant)) {
        scale *= DragonCreatureGrowthScale(variant);
    }
    const CcCreatureDefinition *definition = CcCreatureDefinitionAt(variant);
    bool skinned = definition != NULL && definition->skinned;
    float rig_phase = skinned ?
        gait_phase / (2.0f * PI) : CreatureRigPhase(pose);
    float rig_movement = skinned ? (moving ? 1.0f : 0.0f) :
                                   CreatureRigMovement(pose);
    CcCreatureRigPose generated_rig;
    const CcCreatureRigPose *rig = controlled_pose;
    if (rig == NULL || !rig->valid) {
        if (!CcCreatureRigPoseResolve(
                CreatureRigProfileForVariant(variant), rig_phase, rig_movement,
                ToLimbVector(position), yaw, scale, &generated_rig)) {
            return false;
        }
        rig = &generated_rig;
    }

    Vector2 shadow_size = {1.00f, 1.50f};
    if (variant <= CC_CREATURE_GOBLIN_TRIBUTE_BEARER) {
        shadow_size = (Vector2){0.58f, 0.46f};
    } else if (variant == CC_CREATURE_COW) {
        shadow_size = (Vector2){1.10f, 1.72f};
    } else if (variant == CC_CREATURE_SHEEP) {
        shadow_size = (Vector2){0.80f, 1.15f};
    } else if (variant == CC_CREATURE_DRAGON_WHELP) {
        shadow_size = (Vector2){2.20f, 3.60f};
    } else if (variant == CC_CREATURE_DRAGON_WANDERER) {
        shadow_size = (Vector2){4.40f, 9.60f};
    } else if (variant == CC_CREATURE_DRAGON) {
        shadow_size = (Vector2){5.20f, 15.20f};
    } else if (variant == CC_CREATURE_DRAGON_DEEP_WYRM) {
        shadow_size = (Vector2){8.60f, 22.80f};
    }
    shadow_size.x *= scale;
    shadow_size.y *= scale;
    DrawContactShadow(
        (Vector3){position.x, position.y + 0.008f, position.z},
        shadow_size.x, shadow_size.y, yaw, (Color){2, 7, 10, 104});
    CreatureRenderPalette palette = CreaturePalette(variant, primary);
    if (skinned) {
        CreatureModelCache *creature =
            &creature_models[variant][CC_CREATURE_POSE_IDLE];
        if (creature->ready && creature->model.meshCount == 1 &&
            PoseQuadrupedCreature(
                creature, variant, gait_phase, moving,
                controlled_pose != NULL && controlled_pose->valid ? rig :
                                                                    NULL)) {
            Color colors[CC_NPC_ARCHETYPE_MATERIAL_COUNT] = {
                palette.skin, palette.secondary, palette.hide,
                palette.cloth, palette.leather, palette.horn,
                palette.metal, palette.accent, palette.eye,
            };
            SetIndexedPalette(colors, 0.54f, false, (Vector3){0});
            if (visual_style.npc_skinned_ready) {
                float body_skin_remap = 0.0f;
                SetShaderValue(
                    visual_style.npc_skinned,
                    visual_style.npc_skinned_body_skin_remap_location,
                    &body_skin_remap, SHADER_UNIFORM_FLOAT);
            }
            DrawModelEx(creature->model, position,
                        (Vector3){0.0f, 1.0f, 0.0f}, yaw * RAD2DEG,
                        (Vector3){scale, scale, scale}, WHITE);
            return true;
        }
    }
    if (controlled_pose != NULL && skinned) {
        if (!CcCreatureRigPoseResolve(
                CreatureRigProfileForVariant(variant), rig_phase, rig_movement,
                ToLimbVector(position), yaw, scale, &generated_rig)) {
            return false;
        }
        rig = &generated_rig;
    }
    if (variant != CC_CREATURE_DRAGON &&
        variant != CC_CREATURE_DRAGON_DEEP_WYRM) {
        DrawCreatureMuscleLimbs(rig, &palette, yaw, scale);
    }
    if (variant <= CC_CREATURE_GOBLIN_TRIBUTE_BEARER) {
        DrawGoblinRig(variant, rig, &palette, yaw, scale);
    } else if (IsDragonCreatureVariant(variant)) {
        DrawDragonRig(variant, rig, &palette, yaw, scale);
    } else {
        DrawFarmAnimalRig(variant, rig, &palette, yaw, scale);
    }
    return true;
}

static bool DrawCreature3D(CcCreatureVariant variant, CcCreaturePose pose,
                           Vector3 position, float yaw, float scale,
                           Color primary)
{
    return DrawCreatureGait3D(variant, pose, position, yaw, scale, primary,
                              0.0f, false, NULL);
}


static void UseCharacterLighting(void)
{
    if (visual_style.hero_ready) BeginShaderMode(visual_style.hero);
}

static void RestoreWorldLighting(void)
{
    if (visual_style.hero_ready && visual_style.world_ready) {
        BeginShaderMode(visual_style.world);
    }
}

static void SetWorldForegroundReveal(float amount, float cut_height)
{
    if (!visual_style.world_ready) return;

    rlDrawRenderBatchActive();
    float active = fmaxf(0.0f, fminf(amount, 1.0f));
    SetShaderValue(visual_style.world,
                   visual_style.foreground_reveal_location,
                   &active, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world,
                   visual_style.reveal_cut_height_location,
                   &cut_height, SHADER_UNIFORM_FLOAT);
    if (visual_style.painted_environment.ready) {
        PaintedEnvironmentStyle *painted =
            &visual_style.painted_environment;
        SetShaderValue(painted->shader,
                       painted->foreground_reveal_location,
                       &active, SHADER_UNIFORM_FLOAT);
        SetShaderValue(painted->shader,
                       painted->reveal_cut_height_location,
                       &cut_height, SHADER_UNIFORM_FLOAT);
    }
}

static void SetWorldTerrainSurface(bool enabled)
{
    if (!visual_style.world_ready) return;

    rlDrawRenderBatchActive();
    float active = enabled ? 1.0f : 0.0f;
    SetShaderValue(visual_style.world,
                   visual_style.terrain_surface_location,
                   &active, SHADER_UNIFORM_FLOAT);
}

typedef struct OklabColor {
    float lightness;
    float green_red;
    float blue_yellow;
} OklabColor;

static float SrgbChannelToLinear(unsigned char channel)
{
    float value = (float)channel / 255.0f;
    return value <= 0.04045f ? value / 12.92f :
        powf((value + 0.055f) / 1.055f, 2.4f);
}

static OklabColor ColorToOklab(Color color)
{
    float red = SrgbChannelToLinear(color.r);
    float green = SrgbChannelToLinear(color.g);
    float blue = SrgbChannelToLinear(color.b);
    float long_wave = 0.4122214708f * red + 0.5363325363f * green +
                      0.0514459929f * blue;
    float medium_wave = 0.2119034982f * red + 0.6806995451f * green +
                        0.1073969566f * blue;
    float short_wave = 0.0883024619f * red + 0.2817188376f * green +
                       0.6299787005f * blue;
    long_wave = cbrtf(long_wave);
    medium_wave = cbrtf(medium_wave);
    short_wave = cbrtf(short_wave);
    return (OklabColor){
        0.2104542553f * long_wave + 0.7936177850f * medium_wave -
            0.0040720468f * short_wave,
        1.9779984951f * long_wave - 2.4285922050f * medium_wave +
            0.4505937099f * short_wave,
        0.0259040371f * long_wave + 0.7827717662f * medium_wave -
            0.8086757660f * short_wave,
    };
}

static float OklabDistanceSquared(OklabColor source, OklabColor candidate)
{

    float lightness = (source.lightness - candidate.lightness) * 1.24f;
    float green_red = source.green_red - candidate.green_red;
    float blue_yellow = source.blue_yellow - candidate.blue_yellow;
    return lightness * lightness + green_red * green_red +
           blue_yellow * blue_yellow;
}

static bool LoadSharedPaletteLookup(void)
{
    const int32_t size = CC_SHARED_PALETTE_LUT_SIZE;
    const int32_t tiles = CC_SHARED_PALETTE_LUT_TILES;
    const int32_t width = size * tiles;
    const int32_t height = size * (size / tiles);
    FinalPaletteEntry palette[CC_FINAL_PALETTE_MAX_COLORS] = {0};
    int32_t palette_count = BuildFinalPalette(
        palette, CC_FINAL_PALETTE_MAX_COLORS);
    if (palette_count <= 0) return false;
    OklabColor perceptual_palette[CC_FINAL_PALETTE_MAX_COLORS] = {0};
    int32_t protected_count = 0;
    for (int32_t index = 0; index < palette_count; ++index) {
        perceptual_palette[index] = ColorToOklab(palette[index].color);
        if (palette[index].ownership == FINAL_PALETTE_PROTECTED) {
            protected_count += 1;
        }
    }
    size_t pixel_count = (size_t)width * (size_t)height;
    Color *pixels = MemAlloc(
        (unsigned int)(pixel_count * sizeof(Color)));
    if (pixels == NULL) return false;

    for (int32_t blue = 0; blue < size; ++blue) {
        for (int32_t green = 0; green < size; ++green) {
            for (int32_t red = 0; red < size; ++red) {
                Color source = {
                    (unsigned char)((red * 255 + (size - 1) / 2) /
                                    (size - 1)),
                    (unsigned char)((green * 255 + (size - 1) / 2) /
                                    (size - 1)),
                    (unsigned char)((blue * 255 + (size - 1) / 2) /
                                    (size - 1)),
                    255,
                };
                OklabColor perceptual_source = ColorToOklab(source);
                int32_t environment_index = -1;
                int32_t protected_index = -1;
                float environment_distance = FLT_MAX;
                float protected_distance = FLT_MAX;
                for (int32_t index = 0; index < palette_count; ++index) {
                    float distance = OklabDistanceSquared(
                        perceptual_source, perceptual_palette[index]);
                    if (palette[index].ownership ==
                        FINAL_PALETTE_PROTECTED) {
                        if (distance >= protected_distance) continue;
                        protected_distance = distance;
                        protected_index = index;
                    } else {
                        if (distance >= environment_distance) continue;
                        environment_distance = distance;
                        environment_index = index;
                    }
                }

                const float protected_radius_squared = 0.003025f;
                bool choose_protected = protected_index >= 0 &&
                    protected_distance <= protected_radius_squared &&
                    protected_distance * 1.20f < environment_distance;
                int32_t best_index = choose_protected ? protected_index :
                                                       environment_index;
                if (best_index < 0) best_index = protected_index;
                int32_t pixel_x = red + (blue % tiles) * size;
                int32_t pixel_y = green + (blue / tiles) * size;
                size_t pixel = (size_t)pixel_y * (size_t)width +
                               (size_t)pixel_x;
                pixels[pixel] = palette[best_index].color;
            }
        }
    }

    Image image = {
        pixels, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    visual_style.palette_lut = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!IsTextureValid(visual_style.palette_lut)) return false;
    SetTextureFilter(visual_style.palette_lut, TEXTURE_FILTER_POINT);
    visual_style.palette_lut_location = GetShaderLocation(
        visual_style.grade, "paletteLut");
    if (visual_style.palette_lut_location < 0) {
        UnloadTexture(visual_style.palette_lut);
        visual_style.palette_lut = (Texture2D){0};
        return false;
    }
    TraceLog(LOG_INFO,
             "STYLE: loaded %d-color perceptual lookup (%d protected)",
             palette_count, protected_count);
    return true;
}

static void LoadVisualStyle(void)
{
    char grade_path[1024];
    if (!ResolveAssetPath(CC_STYLE_GRADE_SHADER, grade_path,
                          sizeof(grade_path))) {
        TraceLog(LOG_WARNING, "STYLE: grade shader was not found");
    } else {
        visual_style.grade = LoadShader(NULL, grade_path);
    }
    if (IsShaderValid(visual_style.grade) && LoadSharedPaletteLookup()) {
        visual_style.grade_exposure_location = GetShaderLocation(
            visual_style.grade, "atmosphereExposure");
        visual_style.grade_shadow_tone_location = GetShaderLocation(
            visual_style.grade, "atmosphereShadowTone");
        visual_style.grade_highlight_tone_location = GetShaderLocation(
            visual_style.grade, "atmosphereHighlightTone");
        visual_style.grade_chroma_location = GetShaderLocation(
            visual_style.grade, "atmosphereChroma");
        visual_style.grade_practical_glow_location = GetShaderLocation(
            visual_style.grade, "atmospherePracticalGlow");
        visual_style.grade_ready = true;
    } else {
        if (IsShaderValid(visual_style.grade)) {
            UnloadShader(visual_style.grade);
        }
        visual_style.grade = (Shader){0};
        TraceLog(LOG_WARNING,
                 "STYLE: grade shader or palette lookup could not be loaded");
    }

    char vertex_path[1024];
    char fragment_path[1024];
    if (!ResolveAssetPath(CC_WORLD_LIGHT_VERTEX_SHADER, vertex_path,
                          sizeof(vertex_path)) ||
        !ResolveAssetPath(CC_WORLD_LIGHT_FRAGMENT_SHADER, fragment_path,
                          sizeof(fragment_path))) {
        TraceLog(LOG_WARNING, "STYLE: world light shaders were not found");
        return;
    }
    visual_style.world = LoadShader(vertex_path, fragment_path);
    if (!IsShaderValid(visual_style.world)) {
        visual_style.world = (Shader){0};
        TraceLog(LOG_WARNING, "STYLE: world light shader could not be loaded");
        return;
    }
    visual_style.light_direction_location = GetShaderLocation(
        visual_style.world, "lightDirection");
    visual_style.light_color_location = GetShaderLocation(
        visual_style.world, "lightColor");
    visual_style.ambient_color_location = GetShaderLocation(
        visual_style.world, "ambientColor");
    visual_style.camera_position_location = GetShaderLocation(
        visual_style.world, "cameraPosition");
    visual_style.camera_forward_location = GetShaderLocation(
        visual_style.world, "cameraForward");
    visual_style.shadow_color_location = GetShaderLocation(
        visual_style.world, "shadowColor");
    visual_style.fog_color_location = GetShaderLocation(
        visual_style.world, "fogColor");
    visual_style.fog_near_location = GetShaderLocation(
        visual_style.world, "fogNear");
    visual_style.fog_far_location = GetShaderLocation(
        visual_style.world, "fogFar");
    visual_style.focal_point_location = GetShaderLocation(
        visual_style.world, "focalPoint");
    visual_style.story_axis_location = GetShaderLocation(
        visual_style.world, "storyAxis");
    visual_style.foreground_anchor_location = GetShaderLocation(
        visual_style.world, "foregroundAnchor");
    visual_style.depth_splits_location = GetShaderLocation(
        visual_style.world, "depthSplits");
    visual_style.depth_strength_location = GetShaderLocation(
        visual_style.world, "depthStrength");
    visual_style.focal_contrast_location = GetShaderLocation(
        visual_style.world, "focalContrast");
    visual_style.reveal_cut_height_location = GetShaderLocation(
        visual_style.world, "revealCutHeight");
    visual_style.foreground_reveal_location = GetShaderLocation(
        visual_style.world, "foregroundReveal");
    visual_style.terrain_surface_location = GetShaderLocation(
        visual_style.world, "terrainSurface");
    visual_style.weather_wetness_location = GetShaderLocation(
        visual_style.world, "weatherWetness");
    visual_style.world_ready = true;

    char skinned_vertex_path[1024] = {0};
    bool skinned_vertex_ready = ResolveAssetPath(
        CC_WORLD_LIGHT_SKINNED_VERTEX_SHADER, skinned_vertex_path,
        sizeof(skinned_vertex_path));
    if (!skinned_vertex_ready) {
        TraceLog(LOG_WARNING, "STYLE: skinned vertex shader was not found");
    }

    char painted_fragment_path[1024];
    if (ResolveAssetPath(CC_PAINTED_ENVIRONMENT_FRAGMENT_SHADER,
                         painted_fragment_path,
                         sizeof(painted_fragment_path))) {
        PaintedEnvironmentStyle *painted =
            &visual_style.painted_environment;
        painted->shader = LoadShader(vertex_path, painted_fragment_path);
        if (IsShaderValid(painted->shader)) {
            painted->light_direction_location = GetShaderLocation(
                painted->shader, "lightDirection");
            painted->light_color_location = GetShaderLocation(
                painted->shader, "lightColor");
            painted->ambient_color_location = GetShaderLocation(
                painted->shader, "ambientColor");
            painted->camera_position_location = GetShaderLocation(
                painted->shader, "cameraPosition");
            painted->camera_forward_location = GetShaderLocation(
                painted->shader, "cameraForward");
            painted->shadow_color_location = GetShaderLocation(
                painted->shader, "shadowColor");
            painted->fog_color_location = GetShaderLocation(
                painted->shader, "fogColor");
            painted->fog_near_location = GetShaderLocation(
                painted->shader, "fogNear");
            painted->fog_far_location = GetShaderLocation(
                painted->shader, "fogFar");
            painted->focal_point_location = GetShaderLocation(
                painted->shader, "focalPoint");
            painted->story_axis_location = GetShaderLocation(
                painted->shader, "storyAxis");
            painted->foreground_anchor_location = GetShaderLocation(
                painted->shader, "foregroundAnchor");
            painted->depth_splits_location = GetShaderLocation(
                painted->shader, "depthSplits");
            painted->depth_strength_location = GetShaderLocation(
                painted->shader, "depthStrength");
            painted->focal_contrast_location = GetShaderLocation(
                painted->shader, "focalContrast");
            painted->reveal_cut_height_location = GetShaderLocation(
                painted->shader, "revealCutHeight");
            painted->foreground_reveal_location = GetShaderLocation(
                painted->shader, "foregroundReveal");
            painted->ready = true;
        } else {
            painted->shader = (Shader){0};
            TraceLog(LOG_WARNING,
                     "STYLE: painted environment shader could not be loaded");
        }
    } else {
        TraceLog(LOG_WARNING,
                 "STYLE: painted environment shader was not found");
    }

    char foliage_fragment_path[1024];
    if (ResolveAssetPath(CC_TREE_FOLIAGE_FRAGMENT_SHADER,
                         foliage_fragment_path,
                         sizeof(foliage_fragment_path))) {
        visual_style.foliage = LoadShader(vertex_path,
                                          foliage_fragment_path);
        if (IsShaderValid(visual_style.foliage)) {
            visual_style.foliage_light_direction_location =
                GetShaderLocation(visual_style.foliage, "lightDirection");
            visual_style.foliage_light_color_location =
                GetShaderLocation(visual_style.foliage, "lightColor");
            visual_style.foliage_ambient_color_location =
                GetShaderLocation(visual_style.foliage, "ambientColor");
            visual_style.foliage_camera_position_location =
                GetShaderLocation(visual_style.foliage, "cameraPosition");
            visual_style.foliage_camera_forward_location =
                GetShaderLocation(visual_style.foliage, "cameraForward");
            visual_style.foliage_shadow_color_location =
                GetShaderLocation(visual_style.foliage, "shadowColor");
            visual_style.foliage_fog_color_location =
                GetShaderLocation(visual_style.foliage, "fogColor");
            visual_style.foliage_fog_near_location =
                GetShaderLocation(visual_style.foliage, "fogNear");
            visual_style.foliage_fog_far_location =
                GetShaderLocation(visual_style.foliage, "fogFar");
            visual_style.foliage_depth_splits_location =
                GetShaderLocation(visual_style.foliage, "depthSplits");
            visual_style.foliage_depth_strength_location =
                GetShaderLocation(visual_style.foliage, "depthStrength");
            visual_style.foliage_ready = true;
        } else {
            visual_style.foliage = (Shader){0};
            TraceLog(LOG_WARNING,
                     "STYLE: tree foliage shader could not be loaded");
        }
    } else {
        TraceLog(LOG_WARNING, "STYLE: tree foliage shader was not found");
    }

    char hero_fragment_path[1024];
    if (skinned_vertex_ready &&
        ResolveAssetPath(CC_HERO_PIXEL_FRAGMENT_SHADER, hero_fragment_path,
                         sizeof(hero_fragment_path))) {
        visual_style.hero = LoadShader(skinned_vertex_path,
                                       hero_fragment_path);
        if (IsShaderValid(visual_style.hero)) {
            visual_style.hero_light_direction_location = GetShaderLocation(
                visual_style.hero, "lightDirection");
            visual_style.hero_camera_position_location = GetShaderLocation(
                visual_style.hero, "cameraPosition");
            visual_style.hero_shadow_color_location = GetShaderLocation(
                visual_style.hero, "shadowColor");
            visual_style.hero_fog_color_location = GetShaderLocation(
                visual_style.hero, "fogColor");
            visual_style.hero_fog_near_location = GetShaderLocation(
                visual_style.hero, "fogNear");
            visual_style.hero_fog_far_location = GetShaderLocation(
                visual_style.hero, "fogFar");
            visual_style.hero_ink_strength_location = GetShaderLocation(
                visual_style.hero, "inkStrength");
            visual_style.hero_rim_tint_location = GetShaderLocation(
                visual_style.hero, "characterRimTint");
            visual_style.hero_rim_strength_location = GetShaderLocation(
                visual_style.hero, "characterRimStrength");
            float ink_strength = CC_HERO_INK_STRENGTH;
            SetShaderValue(visual_style.hero,
                           visual_style.hero_ink_strength_location,
                           &ink_strength, SHADER_UNIFORM_FLOAT);
            visual_style.hero_ready = true;
        } else {
            visual_style.hero = (Shader){0};
            TraceLog(LOG_WARNING,
                     "STYLE: retro hero shader could not be loaded");
        }
    } else {
        TraceLog(LOG_WARNING, "STYLE: retro hero shader was not found");
    }

    char npc_fragment_path[1024];
    if (ResolveAssetPath(CC_NPC_INDEXED_FRAGMENT_SHADER, npc_fragment_path,
                         sizeof(npc_fragment_path))) {
        visual_style.npc = LoadShader(vertex_path, npc_fragment_path);
        if (IsShaderValid(visual_style.npc)) {
            visual_style.npc_light_direction_location = GetShaderLocation(
                visual_style.npc, "lightDirection");
            visual_style.npc_camera_position_location = GetShaderLocation(
                visual_style.npc, "cameraPosition");
            visual_style.npc_shadow_color_location = GetShaderLocation(
                visual_style.npc, "shadowColor");
            visual_style.npc_fog_color_location = GetShaderLocation(
                visual_style.npc, "fogColor");
            visual_style.npc_fog_near_location = GetShaderLocation(
                visual_style.npc, "fogNear");
            visual_style.npc_fog_far_location = GetShaderLocation(
                visual_style.npc, "fogFar");
            visual_style.npc_ink_strength_location = GetShaderLocation(
                visual_style.npc, "inkStrength");
            visual_style.npc_palette_location = GetShaderLocation(
                visual_style.npc, "characterPalette[0]");
            visual_style.npc_palette_ink_location = GetShaderLocation(
                visual_style.npc, "paletteInk[0]");
            visual_style.npc_hero_emphasis_location = GetShaderLocation(
                visual_style.npc, "heroEmphasis");
            visual_style.npc_hero_head_position_location = GetShaderLocation(
                visual_style.npc, "heroHeadPosition");
            visual_style.npc_rim_tint_location = GetShaderLocation(
                visual_style.npc, "characterRimTint");
            visual_style.npc_rim_strength_location = GetShaderLocation(
                visual_style.npc, "characterRimStrength");
            visual_style.npc_ready = true;
        } else {
            visual_style.npc = (Shader){0};
            TraceLog(LOG_WARNING,
                     "STYLE: indexed NPC shader could not be loaded");
        }
        if (skinned_vertex_ready) {
            visual_style.npc_skinned = LoadShader(
                skinned_vertex_path, npc_fragment_path);
            if (IsShaderValid(visual_style.npc_skinned)) {
                visual_style.npc_skinned_light_direction_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "lightDirection");
                visual_style.npc_skinned_camera_position_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "cameraPosition");
                visual_style.npc_skinned_shadow_color_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "shadowColor");
                visual_style.npc_skinned_fog_color_location =
                    GetShaderLocation(visual_style.npc_skinned, "fogColor");
                visual_style.npc_skinned_fog_near_location =
                    GetShaderLocation(visual_style.npc_skinned, "fogNear");
                visual_style.npc_skinned_fog_far_location =
                    GetShaderLocation(visual_style.npc_skinned, "fogFar");
                visual_style.npc_skinned_ink_strength_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "inkStrength");
                visual_style.npc_skinned_palette_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "characterPalette[0]");
                visual_style.npc_skinned_palette_ink_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "paletteInk[0]");
                visual_style.npc_skinned_hero_emphasis_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "heroEmphasis");
                visual_style.npc_skinned_hero_head_position_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "heroHeadPosition");
                visual_style.npc_skinned_body_skin_remap_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "bodySkinRemap");
                visual_style.npc_skinned_rim_tint_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "characterRimTint");
                visual_style.npc_skinned_rim_strength_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "characterRimStrength");
                visual_style.npc_skinned_ready = true;
            } else {
                visual_style.npc_skinned = (Shader){0};
                TraceLog(LOG_WARNING,
                         "STYLE: skinned NPC shader could not be loaded");
            }
        }
    } else {
        TraceLog(LOG_WARNING, "STYLE: indexed NPC shader was not found");
    }

    ApplyWorldShader(&sphere_models.small);
    ApplyWorldShader(&sphere_models.character);
    ApplyWorldShader(&sphere_models.scenery);
    if (tree_crown_models.ready) {
        ApplyTreeFoliageShader(&tree_crown_models.alder);
        ApplyTreeFoliageShader(&tree_crown_models.oak);
        ApplyTreeFoliageShader(&tree_crown_models.pollard);
    }
    if (hero_skin.ready) ApplyHeroStyle(&hero_skin.model);
    for (int32_t role = 0; role < CC_NPC_ROLE_COUNT; ++role) {
        for (int32_t pose = 0; pose < CC_NPC_ARCHETYPE_POSE_COUNT; ++pose) {
            if (npc_archetypes[role][pose].ready) {
                ApplyNpcStyle(&npc_archetypes[role][pose].model);
            }
        }
    }
    for (int32_t variant = 0; variant < CC_CREATURE_VARIANT_COUNT;
         ++variant) {
        for (int32_t pose = 0; pose < CC_CREATURE_POSE_COUNT; ++pose) {
            if (creature_models[variant][pose].ready) {
                const CcCreatureDefinition *definition =
                    CcCreatureDefinitionAt((CcCreatureVariant)variant);
                if (definition != NULL && definition->skinned) {
                    ApplyNpcBodyStyle(&creature_models[variant][pose].model);
                } else {
                    ApplyNpcStyle(&creature_models[variant][pose].model);
                }
            }
        }
    }
    for (int32_t id = 0; id < NPC_DYNAMIC_MODULE_COUNT; ++id) {
        if (npc_dynamic_modules[id].ready) {
            ApplyNeutralNpcStyle(&npc_dynamic_modules[id].model);
        }
    }
    for (int32_t frame = 0; frame < CC_NPC_BODY_FRAME_COUNT; ++frame) {
        for (int32_t muscle = 0; muscle < CC_NPC_BODY_MUSCLE_COUNT; ++muscle) {
            for (int32_t tissue = 0; tissue < CC_NPC_BODY_TISSUE_COUNT;
                 ++tissue) {
                NpcBodySkinCache *body =
                    &npc_body_skins[frame][muscle][tissue];
                if (body->ready) ApplyNpcBodyStyle(&body->model);
            }
        }
    }
    for (int32_t family = 0; family < CC_NPC_HEAD_FAMILY_COUNT; ++family) {
        if (npc_head_families[family].ready) {
            ApplyNeutralNpcStyle(&npc_head_families[family].model);
        }
    }
    for (int32_t family = 0; family < CC_NPC_HAIR_FAMILY_COUNT; ++family) {
        if (npc_hair_families[family].ready) {
            ApplyNeutralNpcStyle(&npc_hair_families[family].model);
        }
    }
    for (int32_t id = 0; id < RUNTIME_ASSET_COUNT; ++id) {
        if (!runtime_assets[id].ready) continue;
        if (id == RUNTIME_ASSET_MARKET) {
            ApplyPaintedEnvironmentShader(&runtime_assets[id].model);
        } else {
            ApplyWorldShader(&runtime_assets[id].model);
        }
    }
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        if (economic_cargo_assets[good].ready) {
            ApplyWorldShader(&economic_cargo_assets[good].model);
        }
    }
}

static void LoadHeroSkin(void)
{
    const char *asset_path = HeroSkinAssetPath();
    if (asset_path == NULL) {
        TraceLog(LOG_WARNING, "HERO: engine skin was not found");
        return;
    }
    hero_skin.model = LoadModel(asset_path);
    int32_t bone_count = hero_skin.model.skeleton.boneCount;
    if (hero_skin.model.meshCount <= 0 ||
        hero_skin.model.meshCount > CC_LOCAL_HERO_RUNTIME_MESH_BUDGET ||
        bone_count <= 0 ||
        bone_count > CC_HERO_SKIN_MAX_BONES) {
        TraceLog(LOG_WARNING, "HERO: invalid engine skin (%d meshes, %d bones)",
                 hero_skin.model.meshCount, bone_count);
        UnloadModel(hero_skin.model);
        hero_skin = (HeroSkinCache){0};
        return;
    }
    bool found[CC_HUMANOID_SKIN_BONE_COUNT] = {false};
    bool found_cape[CC_HERO_CAPE_BONE_COUNT] = {false};
    bool found_hair[CC_HERO_HAIR_BONE_COUNT] = {false};
    for (int32_t bone = 0; bone < bone_count; ++bone) {
        int32_t skin_bone = CcHumanoidSkinBoneFind(
            hero_skin.model.skeleton.bones[bone].name);
        int32_t cape_bone = HeroCapeBoneFind(
            hero_skin.model.skeleton.bones[bone].name);
        int32_t hair_bone = HeroHairBoneFind(
            hero_skin.model.skeleton.bones[bone].name);
        hero_skin.skin_bone[bone] = skin_bone;
        hero_skin.cape_bone[bone] = cape_bone;
        hero_skin.hair_bone[bone] = hair_bone;
        if (skin_bone >= 0) found[skin_bone] = true;
        if (cape_bone >= 0) found_cape[cape_bone] = true;
        if (hair_bone >= 0) found_hair[hair_bone] = true;
    }
    for (int32_t bone = 0; bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
        if (found[bone]) continue;
        TraceLog(LOG_WARNING, "HERO: engine skin is missing bone %s",
                 CcHumanoidSkinBoneName((CcHumanoidSkinBone)bone));
        UnloadModel(hero_skin.model);
        hero_skin = (HeroSkinCache){0};
        return;
    }
    for (int32_t bone = 0; bone < CC_HERO_CAPE_BONE_COUNT; ++bone) {
        if (found_cape[bone]) continue;
        TraceLog(LOG_WARNING, "HERO: engine skin is missing cloth bone %s",
                 HERO_CAPE_BONE_NAMES[bone]);
        UnloadModel(hero_skin.model);
        hero_skin = (HeroSkinCache){0};
        return;
    }
    if (screen_first_hero_active) {
        for (int32_t bone = 0; bone < CC_HERO_HAIR_BONE_COUNT; ++bone) {
            if (found_hair[bone]) continue;
            TraceLog(LOG_WARNING, "HERO: screen-first skin is missing hair bone %s",
                     HERO_HAIR_BONE_NAMES[bone]);
            UnloadModel(hero_skin.model);
            hero_skin = (HeroSkinCache){0};
            return;
        }
    }
    hero_skin.frames[0] = hero_skin.pose;
    hero_skin.animation.boneCount = bone_count;
    hero_skin.animation.keyframeCount = 1;
    hero_skin.animation.keyframePoses = hero_skin.frames;
    (void)snprintf(hero_skin.animation.name, sizeof(hero_skin.animation.name),
                   "engine-physics");
    ReleaseUploadedModelCpuData(&hero_skin.model);
    hero_skin.ready = true;
    TraceLog(LOG_INFO,
             "HERO: loaded %s skin with %d meshes on %d physics bones",
             screen_first_hero_active ? "screen-first experiment" :
                                        "production",
             hero_skin.model.meshCount, bone_count);
}

static bool DrawHeroSkin(const CcHumanoidSkinPose *skin,
                         const CcLocalCapeState *cape, Color tint,
                         bool visible, bool record_statistics)
{
    if (!hero_skin.ready || skin == NULL || !skin->valid || cape == NULL ||
        !cape->initialized) return false;
    for (int32_t bone = 0; bone < hero_skin.model.skeleton.boneCount; ++bone) {
        int32_t skin_bone = hero_skin.skin_bone[bone];
        int32_t cape_bone = hero_skin.cape_bone[bone];
        int32_t hair_bone = hero_skin.hair_bone[bone];
        Vector3 target_head = {0};
        Vector3 target_direction = {0};
        Vector3 rest_direction = {0};
        if (skin_bone >= 0 && skin_bone < CC_HUMANOID_SKIN_BONE_COUNT) {
            const CcHumanoidSkinBonePose *target = &skin->bones[skin_bone];
            target_head = FromLimbVector(target->head);
            target_direction = FromLimbVector(target->up);
            rest_direction = HERO_REST_DIRECTIONS[skin_bone];
        } else if (cape_bone >= 0 && cape_bone < CC_HERO_CAPE_BONE_COUNT) {
            target_head = cape->point[cape_bone];
            target_direction = PhysicsNormalizeOr(
                PhysicsSubtract(cape->point[cape_bone + 1],
                                cape->point[cape_bone]),
                (Vector3){0.0f, -1.0f, 0.0f});
            rest_direction = HERO_CAPE_REST_DIRECTIONS[cape_bone];
        } else if (hair_bone >= 0 &&
                   hair_bone < CC_HERO_HAIR_BONE_COUNT) {
            const CcHumanoidSkinBonePose *head =
                &skin->bones[CC_HUMANOID_SKIN_HEAD];
            Quaternion head_rotation = {
                head->world_rotation.x,
                head->world_rotation.y,
                head->world_rotation.z,
                head->world_rotation.w,
            };
            Vector3 head_position = FromLimbVector(head->head);
            Vector3 root_offset = Vector3Scale(
                HERO_HAIR_ROOT_OFFSETS[hair_bone], 1.32f);
            target_head = Vector3Add(
                head_position,
                Vector3RotateByQuaternion(root_offset, head_rotation));
            Vector3 head_direction = Vector3RotateByQuaternion(
                HERO_HAIR_REST_DIRECTIONS[hair_bone], head_rotation);
            Vector3 cape_direction = PhysicsNormalizeOr(
                PhysicsSubtract(cape->point[1], cape->point[0]),
                head_direction);
            target_direction = PhysicsNormalizeOr(
                Vector3Lerp(head_direction, cape_direction,
                            HERO_HAIR_TIP_DELAY[hair_bone]),
                head_direction);
            rest_direction = HERO_HAIR_REST_DIRECTIONS[hair_bone];
        } else {
            hero_skin.pose[bone] = hero_skin.model.skeleton.bindPose[bone];
            continue;
        }
        Quaternion delta = HeroRotationBetween(
            rest_direction, target_direction);
        if (skin_bone == CC_HUMANOID_SKIN_HEAD) {

            const CcHumanoidSkinBonePose *head =
                &skin->bones[CC_HUMANOID_SKIN_HEAD];
            delta = (Quaternion){head->world_rotation.x,
                                 head->world_rotation.y,
                                 head->world_rotation.z,
                                 head->world_rotation.w};
        }
        hero_skin.pose[bone].translation = target_head;
        hero_skin.pose[bone].rotation = QuaternionMultiply(
            delta, hero_skin.model.skeleton.bindPose[bone].rotation);
        hero_skin.pose[bone].scale =
            hero_skin.model.skeleton.bindPose[bone].scale;
        float gameplay_scale =
            (skin_bone == CC_HUMANOID_SKIN_HEAD || hair_bone >= 0) ? 1.32f :
            (skin_bone == CC_HUMANOID_SKIN_HAND_LEFT ||
             skin_bone == CC_HUMANOID_SKIN_HAND_RIGHT) ? 0.90f : 1.0f;
        hero_skin.pose[bone].scale = Vector3Scale(
            hero_skin.pose[bone].scale, gameplay_scale);
    }
    if (record_statistics) {
        CcLocalRendererRecordHeroSkinUpdate(hero_skin.model.meshCount);
    }
    UpdateModelAnimation(hero_skin.model, hero_skin.animation, 0.0f);
    if (visible) {
        const float horizontal_scale = 0.98f;
        const float vertical_scale = 1.07f;
        Vector3 anchor = FromLimbVector(
            skin->bones[CC_HUMANOID_SKIN_ROOT].head);
        Vector3 position = {anchor.x * (1.0f - horizontal_scale),
                            anchor.y * (1.0f - vertical_scale),
                            anchor.z * (1.0f - horizontal_scale)};
        DrawModelEx(hero_skin.model, position,
                    (Vector3){0.0f, 1.0f, 0.0f}, 0.0f,
                    (Vector3){horizontal_scale, vertical_scale,
                              horizontal_scale}, tint);
    }
    return true;
}

static int32_t NpcBodyFrameForAppearance(const CcNpcAppearance *appearance)
{
    if (appearance->body_mass < 0.99f &&
        appearance->shoulder_scale < 1.04f) return 0;
    if (appearance->body_mass > 1.075f ||
        appearance->shoulder_scale > 1.085f) return 2;
    return 1;
}

static int32_t NpcBodyMuscleForAppearance(const CcNpcAppearance *appearance)
{
    if (appearance->muscularity < 0.50f) return 0;
    if (appearance->muscularity < 0.74f) return 1;
    return 2;
}

static int32_t NpcBodyTissueForAppearance(const CcNpcAppearance *appearance)
{
    if (appearance->body_mass < 0.99f) return 0;
    if (appearance->body_mass < 1.055f) return 1;
    return ((appearance->seed >> 7U) & 1U) == 0U ? 2 : 3;
}

static NpcBodySkinCache *NpcBodyForAppearance(
    const CcNpcAppearance *appearance)
{
    if (appearance == NULL) return NULL;
    int32_t frame = NpcBodyFrameForAppearance(appearance);
    int32_t muscle = NpcBodyMuscleForAppearance(appearance);
    int32_t tissue = NpcBodyTissueForAppearance(appearance);
    NpcBodySkinCache *body = &npc_body_skins[frame][muscle][tissue];
#if defined(PLATFORM_WEB)
    if (!body->ready) return &npc_body_skins[1][1][1];
#endif
    return body;
}

static bool DrawNpcBodySkin(const CcHumanoidSkinPose *skin,
                            const CcNpcAppearance *appearance,
                            bool featured_hero)
{
    NpcBodySkinCache *body = NpcBodyForAppearance(appearance);
    if (body == NULL || !body->ready || !visual_style.npc_skinned_ready ||
        skin == NULL || !skin->valid) {
        return false;
    }
    for (int32_t bone = 0; bone < body->model.skeleton.boneCount; ++bone) {
        int32_t skin_bone = body->skin_bone[bone];
        if (skin_bone < 0 || skin_bone >= CC_HUMANOID_SKIN_BONE_COUNT) {
            body->pose[bone] = body->model.skeleton.bindPose[bone];
            continue;
        }
        const CcHumanoidSkinBonePose *target = &skin->bones[skin_bone];
        Quaternion delta = HeroRotationBetween(
            HERO_REST_DIRECTIONS[skin_bone], FromLimbVector(target->up));
        if (skin_bone == CC_HUMANOID_SKIN_HEAD) {
            delta = (Quaternion){target->world_rotation.x,
                                 target->world_rotation.y,
                                 target->world_rotation.z,
                                 target->world_rotation.w};
        }
        body->pose[bone].translation = FromLimbVector(target->head);
        body->pose[bone].rotation = QuaternionMultiply(
            delta, body->model.skeleton.bindPose[bone].rotation);
        body->pose[bone].scale = body->model.skeleton.bindPose[bone].scale;
    }
    UpdateModelAnimation(body->model, body->animation, 0.0f);
    if (featured_hero) {
        CcLocalRendererRecordHeroSkinUpdate(body->model.meshCount);
    } else {
        CcLocalRendererRecordNpcSkinUpdate(body->model.meshCount);
    }
    for (int32_t material = 0; material < body->model.materialCount;
         ++material) {
        body->model.materials[material].maps[MATERIAL_MAP_DIFFUSE].color =
            WHITE;
    }

    rlDrawRenderBatchActive();
    float body_skin_remap = 1.0f;
    SetShaderValue(visual_style.npc_skinned,
                   visual_style.npc_skinned_body_skin_remap_location,
                   &body_skin_remap, SHADER_UNIFORM_FLOAT);
    DrawModelEx(body->model, (Vector3){0.0f, 0.0f, 0.0f},
                (Vector3){0.0f, 1.0f, 0.0f}, 0.0f,
                (Vector3){1.0f, 1.0f, 1.0f}, WHITE);
    rlDrawRenderBatchActive();
    body_skin_remap = 0.0f;
    SetShaderValue(visual_style.npc_skinned,
                   visual_style.npc_skinned_body_skin_remap_location,
                   &body_skin_remap, SHADER_UNIFORM_FLOAT);
    return true;
}

#define CC_TREE_CROWN_RING_VERTICES 6
#define CC_TREE_CROWN_TRIANGLES (CC_TREE_CROWN_RING_VERTICES * 4)

typedef struct TreeCrownProfile {
    float top_y;
    float upper_y;
    float lower_y;
    float bottom_y;
    Vector2 top_offset;
    Vector2 upper_offset;
    Vector2 lower_offset;
    Vector2 bottom_offset;
    float upper_angle;
    float lower_angle;
    float upper_depth;
    float lower_depth;
    float upper_radius[CC_TREE_CROWN_RING_VERTICES];
    float lower_radius[CC_TREE_CROWN_RING_VERTICES];
} TreeCrownProfile;

static const TreeCrownProfile TREE_CROWN_PROFILES[] = {

    {
        1.12f, 0.28f, -0.34f, -0.88f,
        {0.18f, -0.08f}, {-0.05f, 0.04f},
        {0.06f, -0.04f}, {-0.14f, 0.10f},
        0.08f, 0.42f, 0.76f, 0.82f,
        {0.82f, 0.98f, 0.76f, 0.92f, 0.72f, 0.88f},
        {0.92f, 0.74f, 0.96f, 0.78f, 0.88f, 0.70f}
    },

    {
        0.58f, 0.22f, -0.30f, -0.54f,
        {-0.12f, 0.08f}, {0.02f, -0.03f},
        {-0.06f, 0.05f}, {0.16f, -0.10f},
        -0.05f, 0.31f, 0.92f, 0.96f,
        {0.94f, 0.78f, 1.00f, 0.84f, 0.96f, 0.74f},
        {0.82f, 1.00f, 0.80f, 0.94f, 0.72f, 0.90f}
    },

    {
        0.72f, 0.30f, -0.20f, -1.12f,
        {0.12f, 0.02f}, {-0.06f, -0.04f},
        {0.04f, 0.06f}, {-0.10f, -0.06f},
        0.14f, 0.47f, 0.72f, 0.76f,
        {0.72f, 0.90f, 0.68f, 0.84f, 0.74f, 0.88f},
        {0.86f, 0.66f, 0.90f, 0.72f, 0.82f, 0.68f}
    }
};

static Vector3 TreeCrownRingPoint(const TreeCrownProfile *profile,
                                  bool upper, int32_t index)
{
    float angle = (upper ? profile->upper_angle : profile->lower_angle) +
                  (float)index * PI * 2.0f /
                  (float)CC_TREE_CROWN_RING_VERTICES;
    float radius = upper ? profile->upper_radius[index] :
                           profile->lower_radius[index];
    float depth = upper ? profile->upper_depth : profile->lower_depth;
    Vector2 offset = upper ? profile->upper_offset : profile->lower_offset;
    return (Vector3){offset.x + cosf(angle) * radius,
                     upper ? profile->upper_y : profile->lower_y,
                     offset.y + sinf(angle) * radius * depth};
}

static void TreeCrownMeshWriteTriangle(Mesh *mesh, int32_t *cursor,
                                      Vector3 a, Vector3 b, Vector3 c)
{
    Vector3 normal = Vector3CrossProduct(Vector3Subtract(b, a),
                                         Vector3Subtract(c, a));
    Vector3 center = Vector3Scale(Vector3Add(Vector3Add(a, b), c), 1.0f / 3.0f);
    if (Vector3DotProduct(normal, center) < 0.0f) {
        Vector3 swap = b;
        b = c;
        c = swap;
        normal = Vector3Negate(normal);
    }
    normal = Vector3Normalize(normal);
    const Vector3 points[] = {a, b, c};
    for (int32_t point = 0; point < 3; ++point) {
        int32_t vertex = *cursor;
        mesh->vertices[vertex * 3 + 0] = points[point].x;
        mesh->vertices[vertex * 3 + 1] = points[point].y;
        mesh->vertices[vertex * 3 + 2] = points[point].z;
        mesh->normals[vertex * 3 + 0] = normal.x;
        mesh->normals[vertex * 3 + 1] = normal.y;
        mesh->normals[vertex * 3 + 2] = normal.z;
        mesh->colors[vertex * 4 + 0] = 255;
        mesh->colors[vertex * 4 + 1] = 255;
        mesh->colors[vertex * 4 + 2] = 255;
        mesh->colors[vertex * 4 + 3] = 255;
        *cursor += 1;
    }
}

static Model BuildTreeCrownModel(TreeCrownShape shape)
{
    const int32_t vertex_count = CC_TREE_CROWN_TRIANGLES * 3;
    Mesh mesh = {0};
    mesh.vertexCount = vertex_count;
    mesh.triangleCount = CC_TREE_CROWN_TRIANGLES;
    mesh.vertices = MemAlloc(
        (unsigned int)((size_t)vertex_count * 3U * sizeof(float)));
    mesh.normals = MemAlloc(
        (unsigned int)((size_t)vertex_count * 3U * sizeof(float)));
    mesh.colors = MemAlloc(
        (unsigned int)((size_t)vertex_count * 4U * sizeof(unsigned char)));
    if (mesh.vertices == NULL || mesh.normals == NULL || mesh.colors == NULL) {
        if (mesh.vertices != NULL) MemFree(mesh.vertices);
        if (mesh.normals != NULL) MemFree(mesh.normals);
        if (mesh.colors != NULL) MemFree(mesh.colors);
        TraceLog(LOG_WARNING, "TREE: could not allocate crystalline crown mesh");
        return (Model){0};
    }

    const TreeCrownProfile *profile = &TREE_CROWN_PROFILES[shape];
    Vector3 top = {profile->top_offset.x, profile->top_y,
                   profile->top_offset.y};
    Vector3 bottom = {profile->bottom_offset.x, profile->bottom_y,
                      profile->bottom_offset.y};
    int32_t cursor = 0;
    for (int32_t side = 0; side < CC_TREE_CROWN_RING_VERTICES; ++side) {
        int32_t next = (side + 1) % CC_TREE_CROWN_RING_VERTICES;
        Vector3 upper = TreeCrownRingPoint(profile, true, side);
        Vector3 upper_next = TreeCrownRingPoint(profile, true, next);
        Vector3 lower = TreeCrownRingPoint(profile, false, side);
        Vector3 lower_next = TreeCrownRingPoint(profile, false, next);
        TreeCrownMeshWriteTriangle(&mesh, &cursor, top, upper, upper_next);
        TreeCrownMeshWriteTriangle(&mesh, &cursor, upper, lower, upper_next);
        TreeCrownMeshWriteTriangle(&mesh, &cursor, upper_next, lower,
                                   lower_next);
        TreeCrownMeshWriteTriangle(&mesh, &cursor, bottom, lower_next, lower);
    }
    if (cursor != vertex_count) {
        TraceLog(LOG_WARNING, "TREE: crystalline crown mesh count mismatch");
        UnloadMesh(mesh);
        return (Model){0};
    }
    UploadMesh(&mesh, false);
    Model model = LoadModelFromMesh(mesh);
    /* Tree batches rebuild from these small CPU meshes when scenery changes. */
    return model;
}

static void LoadTreeCrownModels(void)
{
    tree_crown_models.alder = BuildTreeCrownModel(TREE_CROWN_ALDER);
    tree_crown_models.oak = BuildTreeCrownModel(TREE_CROWN_OAK);
    tree_crown_models.pollard = BuildTreeCrownModel(TREE_CROWN_POLLARD);
    tree_crown_models.ready = IsModelValid(tree_crown_models.alder) &&
                              IsModelValid(tree_crown_models.oak) &&
                              IsModelValid(tree_crown_models.pollard);
    if (tree_crown_models.ready) return;
    if (IsModelValid(tree_crown_models.alder)) {
        UnloadModel(tree_crown_models.alder);
    }
    if (IsModelValid(tree_crown_models.oak)) {
        UnloadModel(tree_crown_models.oak);
    }
    if (IsModelValid(tree_crown_models.pollard)) {
        UnloadModel(tree_crown_models.pollard);
    }
    tree_crown_models = (TreeCrownModelCache){0};
    TraceLog(LOG_WARNING, "TREE: falling back to rounded foliage masses");
}

void CcLocalRendererInit(void)
{
    if (sphere_models.ready) return;
    art_atmosphere = (ArtAtmosphereState){
        .from = ART_ATMOSPHERES[CC_LOCAL_ATMOSPHERE_CLEAR_DAY],
        .target = CC_LOCAL_ATMOSPHERE_CLEAR_DAY,
        .blend = 1.0f,
        .duration = 1.0f,
    };
    street_camera_rig = (FixedCameraRig){0};
    road_camera_rig = (FixedCameraRig){0};
    combat_camera_rig = (CombatCameraRig){0};
    conversation_camera_rig = (ConversationCameraRig){0};
    face_render_context = (FaceRenderContext){0};
    (void)memset(creature_gaits, 0, sizeof(creature_gaits));
    sphere_models.small = LoadModelFromMesh(GenMeshSphere(1.0f, 6, 8));
    sphere_models.character = LoadModelFromMesh(GenMeshSphere(1.0f, 8, 8));
    sphere_models.scenery = LoadModelFromMesh(GenMeshSphere(1.0f, 10, 12));
    /* The small sphere also supplies tree shadows to the static batch. */
    ReleaseUploadedModelCpuData(&sphere_models.character);
    ReleaseUploadedModelCpuData(&sphere_models.scenery);
    sphere_models.ready = true;
    LoadTreeCrownModels();
    LoadHeroSkin();
    LoadNpcArchetypes();
    LoadCreatureModels();
    LoadNpcDynamicModules();
    LoadNpcBodySkins();
    LoadNpcHeadFamilies();
    LoadNpcHairFamilies();
    LoadRuntimeAssets();
    LoadEconomicCargoAssets();
    LoadVisualStyle();
    npc_portrait_target = LoadRenderTexture(72, 88);
    if (IsRenderTextureValid(npc_portrait_target)) {
        SetTextureFilter(npc_portrait_target.texture, TEXTURE_FILTER_POINT);
    }
}

void CcLocalRendererSetScreenFirstHero(bool enabled)
{
    if (sphere_models.ready) return;
    screen_first_hero_requested = enabled;
}

void CcLocalRendererSetReducedMotion(bool enabled)
{
    reduced_motion_enabled = enabled;
}

void CcLocalRendererSetOpeningStep(CcLocalOpeningStep step)
{
    active_opening_step = step >= CC_LOCAL_OPENING_LEGACY &&
                          step <= CC_LOCAL_OPENING_COMPLETE ?
        step : CC_LOCAL_OPENING_COMPLETE;
}

void CcLocalRendererSetDiagnosticOverlay(bool enabled)
{
    draw_hero_rig_debug = enabled;
}

void CcLocalRendererSetMovementPreview(
    const CcLocalMovementPreview *preview)
{
    renderer_movement_preview = preview != NULL ?
        *preview : (CcLocalMovementPreview){0};
}

void CcLocalRendererShutdown(void)
{
    if (!sphere_models.ready) return;
    OpenWorldRenderCacheClear();
    TerrainRenderCacheClear();
    TerrainDetailRenderCacheClear();
    TreeSceneryBatchClear();
    UnloadModel(sphere_models.small);
    UnloadModel(sphere_models.character);
    UnloadModel(sphere_models.scenery);
    if (tree_crown_models.ready) {
        UnloadModel(tree_crown_models.alder);
        UnloadModel(tree_crown_models.oak);
        UnloadModel(tree_crown_models.pollard);
    }
    if (hero_skin.ready) UnloadModel(hero_skin.model);
    for (int32_t role = 0; role < CC_NPC_ROLE_COUNT; ++role) {
        for (int32_t pose = 0; pose < CC_NPC_ARCHETYPE_POSE_COUNT; ++pose) {
            if (npc_archetypes[role][pose].ready) {
                UnloadModel(npc_archetypes[role][pose].model);
            }
            npc_archetypes[role][pose] = (NpcArchetypeCache){0};
        }
    }
    for (int32_t variant = 0; variant < CC_CREATURE_VARIANT_COUNT;
         ++variant) {
        for (int32_t pose = 0; pose < CC_CREATURE_POSE_COUNT; ++pose) {
            CreatureModelCache *creature = &creature_models[variant][pose];
            if (creature->ready) UnloadModel(creature->model);
            *creature = (CreatureModelCache){0};
        }
    }
    (void)memset(creature_gaits, 0, sizeof(creature_gaits));
    for (int32_t id = 0; id < NPC_DYNAMIC_MODULE_COUNT; ++id) {
        if (npc_dynamic_modules[id].ready) {
            UnloadModel(npc_dynamic_modules[id].model);
        }
        npc_dynamic_modules[id].model = (Model){0};
        npc_dynamic_modules[id].ready = false;
    }
    for (int32_t frame = 0; frame < CC_NPC_BODY_FRAME_COUNT; ++frame) {
        for (int32_t muscle = 0; muscle < CC_NPC_BODY_MUSCLE_COUNT; ++muscle) {
            for (int32_t tissue = 0; tissue < CC_NPC_BODY_TISSUE_COUNT;
                 ++tissue) {
                NpcBodySkinCache *body =
                    &npc_body_skins[frame][muscle][tissue];
                if (body->ready) UnloadModel(body->model);
                *body = (NpcBodySkinCache){0};
            }
        }
    }
    for (int32_t family = 0; family < CC_NPC_HEAD_FAMILY_COUNT; ++family) {
        if (npc_head_families[family].ready) {
            UnloadModel(npc_head_families[family].model);
        }
        npc_head_families[family] = (NpcHeadFamilyCache){0};
    }
    for (int32_t family = 0; family < CC_NPC_HAIR_FAMILY_COUNT; ++family) {
        if (npc_hair_families[family].ready) {
            UnloadModel(npc_hair_families[family].model);
        }
        npc_hair_families[family] = (NpcHairFamilyCache){0};
    }
    for (int32_t id = 0; id < RUNTIME_ASSET_COUNT; ++id) {
        UnloadRuntimeAsset((RuntimeAssetId)id);
    }
    UnloadEconomicCargoAssets();
    if (visual_style.npc_skinned_ready) {
        UnloadShader(visual_style.npc_skinned);
    }
    if (visual_style.npc_ready) UnloadShader(visual_style.npc);
    if (visual_style.hero_ready) UnloadShader(visual_style.hero);
    if (visual_style.foliage_ready) UnloadShader(visual_style.foliage);
    if (visual_style.painted_environment.ready) {
        UnloadShader(visual_style.painted_environment.shader);
    }
    if (visual_style.world_ready) UnloadShader(visual_style.world);
    if (IsTextureValid(visual_style.palette_lut)) {
        UnloadTexture(visual_style.palette_lut);
    }
    if (IsRenderTextureValid(npc_portrait_target)) {
        UnloadRenderTexture(npc_portrait_target);
    }
    if (visual_style.grade_ready) UnloadShader(visual_style.grade);
    sphere_models = (SphereModelCache){0};
    npc_portrait_target = (RenderTexture2D){0};
    tree_crown_models = (TreeCrownModelCache){0};
    hero_skin = (HeroSkinCache){0};
    visual_style = (VisualStyleCache){0};
    street_camera_rig = (FixedCameraRig){0};
    road_camera_rig = (FixedCameraRig){0};
    combat_camera_rig = (CombatCameraRig){0};
    conversation_camera_rig = (ConversationCameraRig){0};
    face_render_context = (FaceRenderContext){0};
    bridge_checkpoint_status = BRIDGE_CHECKPOINT_UNKNOWN;
}
