/* Capture argument state is shared by the QA setup and frame hooks. */
typedef struct CcCaptureRequest {
    bool capture_ux;
    int32_t capture_ux_view;
    bool capture_board;
    bool capture_world;
    bool capture_opening;
    bool capture_interior;
    bool capture_navigation;
    bool capture_defense;
    bool capture_downclimb;
    bool capture_limbs;
    bool capture_walk_cycle;
    bool capture_road_fork;
    bool capture_road_departure;
    bool capture_storybook_arrival;
    bool capture_road_arrival;
    bool capture_road_zoom;
    float capture_road_zoom_weight;
    bool capture_map_case;
    bool capture_dragon_hoard_map;
    bool capture_carriage;
    bool capture_carriage_target;
    bool capture_dojo;
    bool capture_jump;
    bool capture_action_reel;
    bool capture_gameplay_reel;
    bool capture_encounter;
    bool capture_witness;
    bool capture_character;
    bool capture_storybook;
    float storybook_progress;
    float storybook_blend;
    bool capture_pony_book;
    bool capture_pony_encounter;
    bool capture_pony_swap;
    bool capture_travel;
    bool capture_route_sight;
    float capture_route_sight_progress;
    bool capture_road;
    bool capture_parley;
    bool capture_aftermath;
    bool capture_golden;
    bool capture_town_state;
    bool capture_town;
    bool capture_town_arrival;
    int32_t capture_town_index;
    float capture_town_x;
    float capture_town_z;
    float capture_town_arrival_progress;
    bool capture_dragon_cave;
    bool capture_mine_yard;
    bool capture_mine_menu;
    bool capture_mine_level;
    bool capture_underroad;
    bool capture_atmosphere;
    CcLocalAtmospherePreset capture_atmosphere_preset;
    bool capture_face;
    bool capture_npc_review;
    bool capture_heraldry;
    int32_t capture_npc_review_view;
    bool capture_creatures;
    bool capture_creature_reel;
    bool capture_creature_media;
    const char * capture_creature_family;
    bool capture_creature_horse;
    bool capture_creature_dragon;
    int32_t capture_face_view;
    bool capture_room;
    float capture_room_x;
    float capture_room_z;
    bool capture_title;
    bool capture_menu;
    bool capture_delete;
    bool capture;
    const char * capture_path;
} CcCaptureRequest;

static bool CcCaptureRequestParse(int argc, char **argv, CcCaptureRequest *request)
{
    *request = (CcCaptureRequest){0};
#if defined(CC_CLIENT_SELF_TESTS)
    request->capture_ux = argc >= 4 && strcmp(argv[1], "--capture-ux") == 0;
    request->capture_ux_view = request->capture_ux ? atoi(argv[2]) : 0;
    if (request->capture_ux && (request->capture_ux_view < 0 || request->capture_ux_view > 19)) return false;
    request->capture_board = argc >= 2 && strcmp(argv[1], "--capture-board") == 0;
    request->capture_world = argc >= 2 && strcmp(argv[1], "--capture-world") == 0;
    request->capture_opening = argc >= 2 &&
        strcmp(argv[1], "--capture-opening") == 0;
    request->capture_interior = argc >= 2 && strcmp(argv[1], "--capture-interior") == 0;
    request->capture_navigation = argc >= 2 && strcmp(argv[1], "--capture-nav") == 0;
    request->capture_defense = argc >= 2 &&
                           strcmp(argv[1], "--capture-defense") == 0;
    request->capture_downclimb = argc >= 2 &&
                             strcmp(argv[1], "--capture-downclimb") == 0;
    request->capture_limbs = argc >= 2 && strcmp(argv[1], "--capture-limbs") == 0;
    request->capture_walk_cycle = argc >= 2 &&
                              strcmp(argv[1], "--capture-walk-cycle") == 0;
    request->capture_road_fork = argc >= 2 &&
        strcmp(argv[1], "--capture-road-fork") == 0;
    request->capture_road_departure = argc >= 2 &&
        strcmp(argv[1], "--capture-road-departure") == 0;
    request->capture_storybook_arrival = argc >= 2 &&
        strcmp(argv[1], "--capture-storybook-arrival") == 0;
    request->capture_road_arrival = request->capture_storybook_arrival || (argc >= 2 &&
        strcmp(argv[1], "--capture-road-arrival") == 0);
    request->capture_road_zoom = request->capture_road_departure || request->capture_road_arrival;
    request->capture_road_zoom_weight = 0.5f;
    if (request->capture_road_zoom) {
        if (argc < 4) {
            (void)fprintf(
                stderr,
                "capture road departure and arrival require a weight from 0 to 1 and a frame path.\n");
            return false;
        }
        char *end = NULL;
        request->capture_road_zoom_weight = strtof(argv[2], &end);
        if (end == argv[2] || *end != '\0' ||
            request->capture_road_zoom_weight < 0.0f ||
            request->capture_road_zoom_weight > 1.0f) {
            (void)fprintf(stderr,
                          "capture road zoom weight must be from 0 to 1.\n");
            return false;
        }
    }
    request->capture_map_case = argc >= 2 &&
        strcmp(argv[1], "--capture-map-case") == 0;
    request->capture_dragon_hoard_map = argc >= 2 &&
        strcmp(argv[1], "--capture-dragon-hoard-map") == 0;
    request->capture_carriage = argc >= 2 &&
        strcmp(argv[1], "--capture-carriage") == 0;
    request->capture_carriage_target = argc >= 2 &&
        strcmp(argv[1], "--capture-carriage-target") == 0;
    request->capture_dojo = argc >= 2 && strcmp(argv[1], "--capture-dojo") == 0;
    request->capture_jump = argc >= 2 && strcmp(argv[1], "--capture-jump") == 0;
    request->capture_action_reel = argc >= 2 &&
        strcmp(argv[1], "--capture-action-reel") == 0;
    request->capture_gameplay_reel = argc >= 2 &&
        strcmp(argv[1], "--capture-gameplay-reel") == 0;
    request->capture_encounter = argc >= 2 &&
        strcmp(argv[1], "--capture-encounter") == 0;
    request->capture_witness = argc >= 2 &&
        strcmp(argv[1], "--capture-witness") == 0;
    request->capture_character = argc >= 2 &&
        strcmp(argv[1], "--capture-character") == 0;
    request->capture_storybook = argc >= 2 &&
        strcmp(argv[1], "--capture-storybook") == 0;
    request->storybook_progress = 0.25f;
    request->storybook_blend = 0.0f;
    if (request->capture_storybook) {
        char *progress_end = NULL;
        char *blend_end = NULL;
        if (argc == 5) {
            request->storybook_progress = strtof(argv[2], &progress_end);
            request->storybook_blend = strtof(argv[3], &blend_end);
        }
        if (argc != 5 || progress_end == argv[2] || *progress_end != '\0' ||
            blend_end == argv[3] || *blend_end != '\0' ||
            !isfinite(request->storybook_progress) || !isfinite(request->storybook_blend) ||
            request->storybook_progress < 0.0f || request->storybook_progress >= 1.0f ||
            request->storybook_blend < 0.0f || request->storybook_blend > 1.0f) {
            (void)fprintf(stderr, "Use --capture-storybook progress[0,1) zoom[0,1] path.\n");
            return false;
        }
    }
    request->capture_pony_book = argc >= 2 && strcmp(argv[1], "--capture-pony-book") == 0;
    request->capture_pony_encounter = argc >= 2 && strcmp(argv[1], "--capture-pony-encounter") == 0;
    request->capture_pony_swap = argc >= 2 && strcmp(argv[1], "--capture-pony-swap") == 0;
    request->capture_travel = request->capture_pony_book || request->capture_pony_encounter || request->capture_pony_swap || request->capture_storybook || (argc >= 2 &&
        strcmp(argv[1], "--capture-travel") == 0);
    request->capture_route_sight = argc >= 2 &&
        strcmp(argv[1], "--capture-route-sight") == 0;
    request->capture_route_sight_progress = 0.0f;
    if (request->capture_route_sight) {
        if (argc < 4) {
            (void)fprintf(
                stderr,
                "capture route sight requires progress from 0 up to 1 and a frame path.\n");
            return false;
        }
        char *end = NULL;
        request->capture_route_sight_progress = strtof(argv[2], &end);
        if (end == argv[2] || *end != '\0' ||
            request->capture_route_sight_progress < 0.0f ||
            request->capture_route_sight_progress >= 1.0f) {
            (void)fprintf(stderr,
                          "capture route sight progress must be from 0 up to 1.\n");
            return false;
        }
    }
    request->capture_road = argc >= 2 &&
        strcmp(argv[1], "--capture-road") == 0;
    request->capture_parley = argc >= 2 &&
        strcmp(argv[1], "--capture-parley") == 0;
    request->capture_aftermath = argc >= 2 &&
        strcmp(argv[1], "--capture-aftermath") == 0;
    request->capture_golden = argc >= 2 &&
        strcmp(argv[1], "--capture-golden") == 0;
    request->capture_town_state = argc >= 2 &&
        strcmp(argv[1], "--capture-town-state") == 0;
    request->capture_town = request->capture_town_state || (argc >= 2 &&
        strcmp(argv[1], "--capture-town") == 0);
    request->capture_town_arrival = argc >= 2 &&
        strcmp(argv[1], "--capture-town-arrival") == 0;
    request->capture_town_index = 0;
    request->capture_town_x = 44.25f;
    request->capture_town_z = 28.85f;
    if (request->capture_town_state) {
        if (argc != 7 || (strcmp(argv[6], "burnt") != 0 &&
            strcmp(argv[6], "rebuilding") != 0 && strcmp(argv[6], "lawless") != 0 &&
            strcmp(argv[6], "peaceful") != 0 && strcmp(argv[6], "thriving") != 0)) {
            (void)fprintf(stderr,
                "capture town state: INDEX X Z FRAME burnt|rebuilding|lawless|peaceful|thriving\n");
            return false;
        }
    }
    if (request->capture_town) {
        if (argc < 4) {
            (void)fprintf(stderr,
                          "capture town requires an index from 0 to 5 and a frame path.\n");
            return false;
        }
        char *end = NULL;
        long parsed = strtol(argv[2], &end, 10);
        if (end == argv[2] || *end != '\0' || parsed < 0 || parsed >= 6) {
            (void)fprintf(stderr, "capture town index must be from 0 to 5.\n");
            return false;
        }
        request->capture_town_index = (int32_t)parsed;
        if (argc >= 6) {
            char *x_end = NULL;
            char *z_end = NULL;
            request->capture_town_x = strtof(argv[3], &x_end);
            request->capture_town_z = strtof(argv[4], &z_end);
            if (x_end == argv[3] || *x_end != '\0' ||
                z_end == argv[4] || *z_end != '\0' ||
                request->capture_town_x < 0.5f ||
                request->capture_town_x > CC_LOCAL_WORLD_WIDTH - 0.5f ||
                request->capture_town_z < 0.5f ||
                request->capture_town_z > CC_LOCAL_WORLD_DEPTH - 0.5f) {
                (void)fprintf(stderr,
                              "capture town coordinates are invalid.\n");
                return false;
            }
        }
    }
    request->capture_town_arrival_progress = 0.58f;
    if (request->capture_town_arrival) {
        if (argc < 5) {
            (void)fprintf(
                stderr,
                "capture town arrival requires an index from 0 to 5, progress from 0 to 1, and a frame path.\n");
            return false;
        }
        char *index_end = NULL;
        char *progress_end = NULL;
        long parsed_index = strtol(argv[2], &index_end, 10);
        float parsed_progress = strtof(argv[3], &progress_end);
        if (index_end == argv[2] || *index_end != '\0' ||
            parsed_index < 0 || parsed_index >= 6 ||
            progress_end == argv[3] || *progress_end != '\0' ||
            parsed_progress < 0.0f || parsed_progress > 1.0f) {
            (void)fprintf(stderr,
                          "capture town arrival values are invalid.\n");
            return false;
        }
        request->capture_town_index = (int32_t)parsed_index;
        request->capture_town_arrival_progress = parsed_progress;
    }
    request->capture_dragon_cave = argc >= 2 &&
        strcmp(argv[1], "--capture-dragon-cave") == 0;
    request->capture_mine_yard = argc >= 2 && strcmp(argv[1],"--capture-mine-yard") == 0;
    request->capture_mine_menu = argc >= 2 && strcmp(argv[1],"--capture-mine-menu") == 0;
    request->capture_mine_level = request->capture_mine_menu || (argc >= 2 && strcmp(argv[1],"--capture-mine-level") == 0);
    request->capture_underroad = argc >= 2 &&
        strcmp(argv[1], "--capture-underroad") == 0;
    request->capture_atmosphere = argc >= 2 &&
        strcmp(argv[1], "--capture-atmosphere") == 0;
    request->capture_atmosphere_preset =
        CC_LOCAL_ATMOSPHERE_CLEAR_DAY;
    if (request->capture_atmosphere) {
        if (argc < 4) {
            (void)fprintf(stderr,
                          "capture atmosphere requires a mood and a frame path.\n");
            return false;
        }
        if (strcmp(argv[2], "clear") == 0) {
            request->capture_atmosphere_preset = CC_LOCAL_ATMOSPHERE_CLEAR_DAY;
        } else if (strcmp(argv[2], "rain") == 0) {
            request->capture_atmosphere_preset =
                CC_LOCAL_ATMOSPHERE_RAINY_OVERCAST;
        } else if (strcmp(argv[2], "dusk") == 0) {
            request->capture_atmosphere_preset = CC_LOCAL_ATMOSPHERE_AMBER_DUSK;
        } else if (strcmp(argv[2], "night") == 0) {
            request->capture_atmosphere_preset =
                CC_LOCAL_ATMOSPHERE_MOONLIT_NIGHT;
        } else if (strcmp(argv[2], "omen") == 0) {
            request->capture_atmosphere_preset = CC_LOCAL_ATMOSPHERE_DRAGON_OMEN;
        } else {
            (void)fprintf(stderr,
                          "atmosphere must be clear, rain, dusk, night, or omen.\n");
            return false;
        }
    }
    request->capture_face = argc >= 2 &&
        strcmp(argv[1], "--capture-face") == 0;
    request->capture_npc_review = argc >= 2 &&
        strcmp(argv[1], "--capture-npc-review") == 0;
    request->capture_heraldry = argc >= 2 &&
        strcmp(argv[1], "--capture-heraldry") == 0;
    if (request->capture_heraldry && argc < 3) {
        (void)fprintf(stderr,
                      "capture heraldry requires a frame path.\n");
        return false;
    }
    request->capture_npc_review_view = -1;
    if (request->capture_npc_review) {
        if (argc < 4) {
            (void)fprintf(stderr,
                          "NPC review requires front, side, motion, or state "
                          "and a frame path.\n");
            return false;
        }
        if (strcmp(argv[2], "front") == 0) request->capture_npc_review_view = 0;
        else if (strcmp(argv[2], "side") == 0) request->capture_npc_review_view = 1;
        else if (strcmp(argv[2], "motion") == 0) request->capture_npc_review_view = 2;
        else if (strcmp(argv[2], "state") == 0) request->capture_npc_review_view = 3;
        else {
            (void)fprintf(stderr,
                          "NPC review must be front, side, motion, or state.\n");
            return false;
        }
    }
    request->capture_creatures = argc >= 2 &&
        strcmp(argv[1], "--capture-creatures") == 0;
    request->capture_creature_reel = argc >= 2 &&
        strcmp(argv[1], "--capture-creature-reel") == 0;
    request->capture_creature_media = request->capture_creatures || request->capture_creature_reel;
    request->capture_creature_family = NULL;
    if (request->capture_creature_media) {
        if (argc < 4 ||
            (strcmp(argv[2], "goblins") != 0 &&
             strcmp(argv[2], "dragon") != 0 &&
             strcmp(argv[2], "dragon-whelp") != 0 &&
             strcmp(argv[2], "dragon-wanderer") != 0 &&
             strcmp(argv[2], "dragon-deep-wyrm") != 0 &&
             strcmp(argv[2], "animals") != 0 &&
             strcmp(argv[2], "horse") != 0 &&
             strcmp(argv[2], "cow") != 0 &&
             strcmp(argv[2], "sheep") != 0)) {
            (void)fprintf(stderr,
                          "creature capture requires goblins, a dragon stage, "
                          "horse, cow, sheep, or animals and a frame path.\n");
            return false;
        }
        request->capture_creature_family = argv[2];
    }
    request->capture_creature_horse = request->capture_creature_media &&
        strcmp(request->capture_creature_family, "horse") == 0;
    request->capture_creature_dragon = request->capture_creature_media &&
        strncmp(request->capture_creature_family, "dragon", 6) == 0;
    request->capture_face_view = -1;
    if (request->capture_face) {
        if (argc < 4) {
            (void)fprintf(stderr,
                          "capture face requires an angle and a frame path.\n");
            return false;
        }
        if (strcmp(argv[2], "front") == 0) request->capture_face_view = 0;
        else if (strcmp(argv[2], "three-quarter") == 0) {
            request->capture_face_view = 1;
        } else if (strcmp(argv[2], "profile") == 0) {
            request->capture_face_view = 2;
        } else if (strcmp(argv[2], "distant") == 0) {
            request->capture_face_view = 3;
        } else if (strcmp(argv[2], "back") == 0) {
            request->capture_face_view = 4;
        } else {
            (void)fprintf(stderr,
                          "capture face angle must be front, three-quarter, profile, back, or distant.\n");
            return false;
        }
    }
    request->capture_room = argc >= 2 &&
        strcmp(argv[1], "--capture-room") == 0;
    request->capture_room_x = 0.0f;
    request->capture_room_z = 0.0f;
    if (request->capture_room) {
        if (argc < 5) {
            (void)fprintf(stderr,
                          "capture room requires X, Z, and a frame path.\n");
            return false;
        }
        char *x_end = NULL;
        char *z_end = NULL;
        request->capture_room_x = strtof(argv[2], &x_end);
        request->capture_room_z = strtof(argv[3], &z_end);
        if (x_end == argv[2] || *x_end != '\0' ||
            z_end == argv[3] || *z_end != '\0' ||
            request->capture_room_x < 0.5f ||
            request->capture_room_x > CC_LOCAL_WORLD_WIDTH - 0.5f ||
            request->capture_room_z < 0.5f ||
            request->capture_room_z > CC_LOCAL_WORLD_DEPTH - 0.5f) {
            (void)fprintf(stderr, "capture room coordinates are invalid.\n");
            return false;
        }
    }
    request->capture_title = argc >= 2 && strcmp(argv[1], "--capture-title") == 0;
    request->capture_menu = argc >= 2 && strcmp(argv[1], "--capture-menu") == 0;
    request->capture_delete = argc >= 2 && strcmp(argv[1], "--capture-delete-menu") == 0;
    request->capture = argc >= 2 &&
                   (strcmp(argv[1], "--capture") == 0 || request->capture_mine_yard || request->capture_mine_level || request->capture_ux || request->capture_title ||
                    request->capture_menu || request->capture_delete || request->capture_world ||
                    request->capture_board ||
                    request->capture_opening ||
                    request->capture_interior || request->capture_navigation || request->capture_limbs ||
                    request->capture_walk_cycle || request->capture_defense ||
                    request->capture_downclimb || request->capture_road_fork ||
                    request->capture_road_zoom ||
                    request->capture_map_case || request->capture_dragon_hoard_map ||
                    request->capture_carriage_target || request->capture_dojo ||
                    request->capture_carriage ||
                    request->capture_jump || request->capture_action_reel ||
                    request->capture_gameplay_reel || request->capture_encounter ||
                    request->capture_witness || request->capture_character ||
                    request->capture_travel || request->capture_route_sight || request->capture_road ||
                    request->capture_parley ||
                    request->capture_aftermath || request->capture_golden || request->capture_town ||
                    request->capture_town_arrival ||
                    request->capture_dragon_cave || request->capture_underroad ||
                    request->capture_atmosphere || request->capture_face ||
                    request->capture_room || request->capture_npc_review || request->capture_heraldry ||
                    request->capture_creature_media);
    request->capture_path = request->capture_ux ? argv[3] : request->capture_storybook ? argv[4] :
                               request->capture_creature_media ? argv[3] :
                               request->capture_room ? argv[4] :
                               request->capture_face ? argv[3] :
                               request->capture_atmosphere ? argv[3] :
                               request->capture_town_arrival ? argv[4] :
                               request->capture_town ? (argc >= 6 ? argv[5] : argv[3]) :
                               request->capture_npc_review ? argv[3] :
                               request->capture_route_sight ? argv[3] :
                               request->capture_road_zoom ? argv[3] :
                               argc >= 3 ? argv[2] :
                               "architecture-proof.png";
#else
    (void)argc;
    (void)argv;
#endif
    return true;
}
