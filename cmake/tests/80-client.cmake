if(CC_BUILD_CLIENT)
    target_compile_definitions(crownless_carriage PRIVATE
        CC_CLIENT_SELF_TESTS=1
    )
    add_test(NAME render_benchmark_contract
             COMMAND crownless_carriage --test-render-benchmark)
    add_test(NAME capture_presentation_contract
             COMMAND crownless_carriage --test-capture-presentation)
    add_test(NAME capture_scene_contract
             COMMAND crownless_carriage --test-capture-scenes)
    add_test(NAME capture_frame_contract
             COMMAND crownless_carriage --test-capture-frames)
    add_test(NAME capture_request_contract
             COMMAND crownless_carriage --test-capture-request)
    add_test(NAME travel_hold_input COMMAND crownless_carriage --test-travel-hold)
    add_test(NAME storybook_travel_boundaries
             COMMAND crownless_carriage --test-storybook-travel)
    add_test(NAME travel_audio_presentation
             COMMAND crownless_carriage --test-travel-audio)
    add_test(NAME adventure_town_routes
             COMMAND crownless_carriage --test-adventure-town-routes)
    add_test(NAME adventure_trade_terms
             COMMAND crownless_carriage --test-adventure-trade)
    add_test(NAME bridge_scene_input
             COMMAND crownless_carriage --test-bridge-scene)
    add_test(NAME world_card_input_parity
             COMMAND crownless_carriage --test-world-cards)
    add_test(NAME silverwick_mine_input
             COMMAND crownless_carriage --test-mine-input)
    add_test(NAME road_journey_save
             COMMAND crownless_carriage --test-road-journey-save)
    add_test(NAME abandoned_town_presence COMMAND crownless_carriage --test-abandoned-town)
    add_test(NAME adventure_input_flow
             COMMAND crownless_carriage --test-adventure-input)
    add_test(NAME title_and_pause_menu
             COMMAND crownless_carriage --test-frontend)
    add_test(NAME map_sale_input
             COMMAND crownless_carriage --test-map-sale-input)
    add_test(NAME town_arrival_parking
             COMMAND crownless_carriage --test-town-arrival-parking)
    add_test(NAME town_departure_to_road_book
             COMMAND crownless_carriage --test-town-departure)
    add_test(NAME roadbook_arrival_transition
             COMMAND crownless_carriage --test-roadbook-arrival)
    add_test(NAME world_session_startup_restore
             COMMAND crownless_carriage --test-world-session-startup)
    add_test(NAME town_session_startup_restore
             COMMAND crownless_carriage --test-town-session-startup)
    add_test(NAME solo_party_wipe
             COMMAND crownless_carriage --test-party-wipe)
    add_test(NAME road_encounter_session_restore
             COMMAND crownless_carriage --test-road-encounter-session)

    add_executable(renderer_regression_tests tests/renderer_regression_tests.c)
    target_link_libraries(renderer_regression_tests PRIVATE crownless_local_renderer)
    target_compile_definitions(renderer_regression_tests PRIVATE
        CC_ASSET_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    cc_strict_warnings(renderer_regression_tests)
    add_test(NAME renderer_skin_rotation COMMAND renderer_regression_tests)
    add_test(NAME physical_goods_displays COMMAND renderer_regression_tests --physical-goods)
    add_custom_target(run_renderer_graphics_tests
        COMMAND renderer_regression_tests --graphics
            "${CMAKE_CURRENT_BINARY_DIR}/renderer-viewport.png"
        COMMAND crownless_carriage --test-map-texture-lifetime
        DEPENDS renderer_regression_tests crownless_carriage
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )
    add_custom_target(run_character_material_captures
        COMMAND renderer_regression_tests --material-captures
            "${CMAKE_CURRENT_BINARY_DIR}/character-materials"
        DEPENDS renderer_regression_tests
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )
    add_custom_target(run_hero_face_captures
        COMMAND renderer_regression_tests --hero-face-captures
            "${CMAKE_CURRENT_BINARY_DIR}/hero-faces"
        DEPENDS renderer_regression_tests
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )
    add_custom_target(run_humanoid_animation_captures
        COMMAND renderer_regression_tests --animation-captures
            "${CMAKE_CURRENT_BINARY_DIR}/humanoid-animation"
        DEPENDS renderer_regression_tests
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )

    add_executable(creature_frame_tests tests/creature_frame_tests.c)
    target_link_libraries(creature_frame_tests PRIVATE crownless_local_renderer raylib)
    cc_strict_warnings(creature_frame_tests)
    add_test(NAME fixed_creature_gait_timing COMMAND creature_frame_tests)

    add_executable(creature_teleport_tests tests/creature_teleport_tests.c)
    target_link_libraries(creature_teleport_tests PRIVATE crownless_local_renderer raylib)
    cc_strict_warnings(creature_teleport_tests)
    add_test(NAME creature_pose_after_a_jump COMMAND creature_teleport_tests)

    add_executable(travel_animation_tests tests/travel_animation_tests.c)
    target_link_libraries(travel_animation_tests PRIVATE
        crownless_local_renderer crownless_client_policy raylib
    )
    cc_strict_warnings(travel_animation_tests)
    add_test(NAME travel_animation_follows_the_road
             COMMAND travel_animation_tests)

    add_executable(npc_appearance_tests
        tests/npc_appearance_tests.c
    )
    target_link_libraries(npc_appearance_tests PRIVATE
        crownless_local_renderer raylib
    )
    target_include_directories(npc_appearance_tests PRIVATE src)
    cc_strict_warnings(npc_appearance_tests)
    add_test(NAME deterministic_npc_population COMMAND npc_appearance_tests)

    add_executable(visual_palette_tests tests/visual_palette_tests.c)
    target_link_libraries(visual_palette_tests PRIVATE raylib)
    target_include_directories(visual_palette_tests PRIVATE src)
    cc_strict_warnings(visual_palette_tests)
    add_test(NAME perceptual_visual_palette COMMAND visual_palette_tests)

    add_executable(heraldry_tests tests/heraldry_tests.c)
    target_link_libraries(heraldry_tests PRIVATE
        crownless_local_renderer crownless_sim raylib
    )
    target_include_directories(heraldry_tests PRIVATE src)
    cc_strict_warnings(heraldry_tests)
    add_test(NAME dynamic_kingdom_heraldry COMMAND heraldry_tests)

    add_executable(local_movement_tests tests/local_movement_tests.c)
    target_link_libraries(local_movement_tests PRIVATE
        crownless_local_renderer
        crownless_sim crownless_locomotion
        crownless_creature_catalog crownless_local_place
        crownless_client_policy crownless_world raylib
    )
    cc_strict_warnings(local_movement_tests)
    add_test(NAME local_collision_space COMMAND local_movement_tests)

    add_executable(character_collision_course_tests tests/character_collision_course_tests.c)
    target_link_libraries(character_collision_course_tests PRIVATE
        crownless_local_renderer
        crownless_sim crownless_locomotion
        crownless_creature_catalog crownless_local_place
        crownless_client_policy crownless_world raylib
    )
    cc_strict_warnings(character_collision_course_tests)
    add_test(NAME character_collision_course COMMAND character_collision_course_tests)

    add_executable(terrain_tests tests/terrain_tests.c)
    target_link_libraries(terrain_tests PRIVATE
        crownless_local_renderer
        crownless_sim crownless_locomotion
        crownless_creature_catalog crownless_local_place
        crownless_client_policy crownless_world raylib
    )
    cc_strict_warnings(terrain_tests)
    add_test(NAME seeded_hilly_terrain COMMAND terrain_tests)

endif()
