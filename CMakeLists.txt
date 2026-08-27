cmake_minimum_required(VERSION 3.24)

project(crownless_carriage VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CC_BUILD_CLIENT "Build the raylib fixed-pixel three-quarter 3D client" ON)
option(CC_BUILD_BENCHMARKS "Build repeatable headless benchmarks" ON)
option(CC_ENABLE_STRICT_WARNINGS "Enable strict compiler warnings" ON)
option(CC_WARNINGS_AS_ERRORS "Treat project warnings as build errors" OFF)

function(cc_strict_warnings target)
    if(NOT CC_ENABLE_STRICT_WARNINGS)
        return()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(CC_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion
        )
        if(CC_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(crownless_locomotion
    src/locomotion/cc_biomech.c
    src/locomotion/cc_creature.c
    src/locomotion/cc_humanoid.c
    src/locomotion/cc_humanoid_skin.c
    src/locomotion/cc_limb.c
    src/locomotion/cc_motion.c
)
target_include_directories(crownless_locomotion PUBLIC src)
if(NOT MSVC)
    target_link_libraries(crownless_locomotion PUBLIC m)
endif()
cc_strict_warnings(crownless_locomotion)

add_library(crownless_sim
    src/sim/cc_sim.c
)
target_include_directories(crownless_sim PUBLIC src)
cc_strict_warnings(crownless_sim)

add_library(crownless_creature_catalog
    src/client/cc_creature_catalog.c
)
target_include_directories(crownless_creature_catalog PUBLIC src)
if(NOT MSVC)
    target_link_libraries(crownless_creature_catalog PUBLIC m)
endif()
cc_strict_warnings(crownless_creature_catalog)

find_package(SQLite3 REQUIRED)
add_library(crownless_persistence
    src/persistence/cc_save.c
)
target_include_directories(crownless_persistence PUBLIC src)
target_link_libraries(crownless_persistence PUBLIC crownless_sim SQLite::SQLite3)
cc_strict_warnings(crownless_persistence)

add_library(crownless_metagame
    src/metagame/cc_metagame.c
)
target_include_directories(crownless_metagame PUBLIC src)
target_link_libraries(crownless_metagame PUBLIC crownless_persistence)
cc_strict_warnings(crownless_metagame)

add_executable(crownless_sim_runner tools/sim_runner.c)
target_link_libraries(crownless_sim_runner PRIVATE crownless_persistence)
cc_strict_warnings(crownless_sim_runner)

add_executable(crownless_metagame_playtest tools/metagame.c)
target_link_libraries(crownless_metagame_playtest PRIVATE crownless_metagame)
cc_strict_warnings(crownless_metagame_playtest)

if(CC_BUILD_BENCHMARKS)
    add_executable(crownless_benchmark tools/benchmark.c)
    target_link_libraries(crownless_benchmark PRIVATE
        crownless_sim crownless_locomotion
    )
    cc_strict_warnings(crownless_benchmark)
    add_custom_target(run_benchmarks
        COMMAND crownless_benchmark
        DEPENDS crownless_benchmark
        USES_TERMINAL
    )

    add_executable(crownless_sim_metrics tools/sim_metrics.c)
    target_link_libraries(crownless_sim_metrics PRIVATE crownless_sim)
    cc_strict_warnings(crownless_sim_metrics)
endif()

if(CC_BUILD_CLIENT)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 6.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)
    if(TARGET raylib)
        get_target_property(CC_RAYLIB_IMPORTED raylib IMPORTED)
        if(NOT CC_RAYLIB_IMPORTED)
            target_compile_definitions(raylib PRIVATE SUPPORT_GPU_SKINNING=1)
        endif()
    endif()

    add_executable(crownless_carriage
        src/client/main.c
        src/client/cc_overlay.c
        src/client/cc_npc_appearance.c
        src/client/cc_local3d.c
        src/client/cc_local_runtime.c
        src/client/cc_local_renderer_stats.c
    )
    if(APPLE)
        set_target_properties(crownless_carriage PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "Crownless Carriage"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.atimics.crownless"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        )
    endif()
    target_link_libraries(crownless_carriage PRIVATE
        crownless_persistence crownless_locomotion
        crownless_creature_catalog raylib
    )
    target_compile_definitions(crownless_carriage PRIVATE
        CC_ASSET_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    if(APPLE)
        set(CC_HERO_ENGINE_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/hero/crownless_hero_engine_rig_v01.glb"
        )
        set(CC_HERO_ENGINE_MANIFEST
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/hero/crownless_hero_engine_rig_v01.json"
        )
        set(CC_SCREEN_FIRST_HERO_ENGINE_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/hero/crownless_screen_first_engine_rig_v08.glb"
        )
        set(CC_SCREEN_FIRST_HERO_ENGINE_MANIFEST
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/hero/crownless_screen_first_engine_rig_v08.json"
        )
        file(GLOB CC_NPC_ARCHETYPE_ASSETS CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/npc/*.glb"
        )
        set(CC_NPC_ARCHETYPE_MANIFEST
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/npc_archetype_manifest.json"
        )
        set(CC_NPC_DYNAMIC_MODULE_MANIFEST
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/npc_dynamic_module_manifest.json"
        )
        file(GLOB CC_CREATURE_ASSETS CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/creatures/*.glb"
        )
        set(CC_CREATURE_MANIFEST
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/creature_manifest.json"
        )
        file(GLOB CC_WORLD_KIT_ASSETS CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/world_kit/*.glb"
        )
        set(CC_WORLD_KIT_MANIFEST
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/world_kit_manifest.json"
        )
        set(CC_WORLD_KIT_CONNECTIONS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/world_kit_connections.json"
        )
        set(CC_BRIDGE_CHECKPOINT_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/glb/environment_bridge_checkpoint_v01.glb"
        )
        set(CC_CARRIAGE_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/glb/carriage_base_v01.glb"
        )
        set(CC_CARGO_RACK_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/glb/module_cargo_rack_v01.glb"
        )
        set(CC_MARKET_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/glb/environment_market_granary_v01.glb"
        )
        set(CC_MINE_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/glb/environment_mine_entrance_v01.glb"
        )
        set(CC_SHORTAGE_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/glb/state_food_shortage_v01.glb"
        )
        set(CC_ENFORCEMENT_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/glb/state_harsh_enforcement_v01.glb"
        )
        set(CC_RECOVERY_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/glb/state_market_recovery_v01.glb"
        )
        set(CC_STYLE_GRADE_SHADER
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/style_grade.fs"
        )
        set(CC_WORLD_LIGHT_VERTEX_SHADER
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/world_lit.vs"
        )
        set(CC_WORLD_LIGHT_SKINNED_VERTEX_SHADER
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/world_lit_skinned.vs"
        )
        set(CC_WORLD_LIGHT_FRAGMENT_SHADER
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/world_lit.fs"
        )
        set(CC_PAINTED_ENVIRONMENT_FRAGMENT_SHADER
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/painted_environment.fs"
        )
        set(CC_TREE_FOLIAGE_FRAGMENT_SHADER
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/tree_foliage.fs"
        )
        set(CC_HERO_PIXEL_FRAGMENT_SHADER
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/hero_pixel.fs"
        )
        set(CC_NPC_INDEXED_FRAGMENT_SHADER
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/npc_indexed.fs"
        )
        add_custom_target(crownless_hero_assets ALL
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/hero"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/npc"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/creatures"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/glb"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/world_kit"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_HERO_ENGINE_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/hero/crownless_hero_engine_rig_v01.glb"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_HERO_ENGINE_MANIFEST}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/hero/crownless_hero_engine_rig_v01.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_SCREEN_FIRST_HERO_ENGINE_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/hero/crownless_screen_first_engine_rig_v08.glb"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_SCREEN_FIRST_HERO_ENGINE_MANIFEST}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/hero/crownless_screen_first_engine_rig_v08.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CC_NPC_ARCHETYPE_ASSETS}
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/npc"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_NPC_ARCHETYPE_MANIFEST}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/npc_archetype_manifest.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_NPC_DYNAMIC_MODULE_MANIFEST}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/npc_dynamic_module_manifest.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CC_CREATURE_ASSETS}
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/creatures"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_CREATURE_MANIFEST}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/creature_manifest.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CC_WORLD_KIT_ASSETS}
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/world_kit"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_WORLD_KIT_MANIFEST}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/world_kit_manifest.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_WORLD_KIT_CONNECTIONS}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/world_kit_connections.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_BRIDGE_CHECKPOINT_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/glb/environment_bridge_checkpoint_v01.glb"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_CARRIAGE_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/glb/carriage_base_v01.glb"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_CARGO_RACK_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/glb/module_cargo_rack_v01.glb"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_MARKET_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/glb/environment_market_granary_v01.glb"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_MINE_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/glb/environment_mine_entrance_v01.glb"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_SHORTAGE_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/glb/state_food_shortage_v01.glb"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_ENFORCEMENT_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/glb/state_harsh_enforcement_v01.glb"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_RECOVERY_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/glb/state_market_recovery_v01.glb"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_STYLE_GRADE_SHADER}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/shaders/style_grade.fs"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_WORLD_LIGHT_VERTEX_SHADER}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/shaders/world_lit.vs"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_WORLD_LIGHT_SKINNED_VERTEX_SHADER}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/shaders/world_lit_skinned.vs"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_WORLD_LIGHT_FRAGMENT_SHADER}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/shaders/world_lit.fs"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_PAINTED_ENVIRONMENT_FRAGMENT_SHADER}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/shaders/painted_environment.fs"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_TREE_FOLIAGE_FRAGMENT_SHADER}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/shaders/tree_foliage.fs"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_HERO_PIXEL_FRAGMENT_SHADER}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/shaders/hero_pixel.fs"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_NPC_INDEXED_FRAGMENT_SHADER}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/shaders/npc_indexed.fs"
            DEPENDS "${CC_HERO_ENGINE_ASSET}" "${CC_HERO_ENGINE_MANIFEST}"
                    ${CC_NPC_ARCHETYPE_ASSETS} "${CC_NPC_ARCHETYPE_MANIFEST}"
                    "${CC_NPC_DYNAMIC_MODULE_MANIFEST}"
                    ${CC_CREATURE_ASSETS} "${CC_CREATURE_MANIFEST}"
                    "${CC_BRIDGE_CHECKPOINT_ASSET}" "${CC_CARRIAGE_ASSET}"
                    "${CC_CARGO_RACK_ASSET}" "${CC_MARKET_ASSET}"
                    "${CC_MINE_ASSET}" "${CC_SHORTAGE_ASSET}"
                    "${CC_ENFORCEMENT_ASSET}" "${CC_RECOVERY_ASSET}"
                    "${CC_STYLE_GRADE_SHADER}"
                    "${CC_WORLD_LIGHT_VERTEX_SHADER}"
                    "${CC_WORLD_LIGHT_SKINNED_VERTEX_SHADER}"
                    "${CC_WORLD_LIGHT_FRAGMENT_SHADER}"
                    "${CC_TREE_FOLIAGE_FRAGMENT_SHADER}"
                    "${CC_HERO_PIXEL_FRAGMENT_SHADER}"
            VERBATIM
        )
        add_dependencies(crownless_carriage crownless_hero_assets)
    endif()
    cc_strict_warnings(crownless_carriage)
    add_custom_target(run_render_benchmark
        COMMAND $<TARGET_FILE:crownless_carriage>
                --benchmark-render 600 90
        DEPENDS crownless_carriage
        USES_TERMINAL
    )
endif()

include(CTest)
if(BUILD_TESTING)
    if(CC_BUILD_BENCHMARKS)
        add_test(NAME headless_performance_budget
                 COMMAND crownless_benchmark --quick --assert-budget)
    endif()
    add_executable(sim_tests tests/sim_tests.c)
    target_link_libraries(sim_tests PRIVATE crownless_sim)
    cc_strict_warnings(sim_tests)
    add_test(NAME deterministic_simulation COMMAND sim_tests)

    add_executable(persistence_tests tests/persistence_tests.c)
    target_link_libraries(persistence_tests PRIVATE crownless_persistence)
    cc_strict_warnings(persistence_tests)
    add_test(NAME sqlite_round_trip COMMAND persistence_tests)

    add_executable(scenario_tests tests/scenario_tests.c)
    target_link_libraries(scenario_tests PRIVATE crownless_sim)
    cc_strict_warnings(scenario_tests)
    add_test(NAME empty_granary_scenario COMMAND scenario_tests)

    add_executable(world_spine_tests tests/world_spine_tests.c)
    target_link_libraries(world_spine_tests PRIVATE crownless_sim)
    cc_strict_warnings(world_spine_tests)
    add_test(NAME living_world_feedback COMMAND world_spine_tests)

    add_executable(causal_history_tests tests/causal_history_tests.c)
    target_link_libraries(causal_history_tests PRIVATE crownless_persistence)
    cc_strict_warnings(causal_history_tests)
    add_test(NAME causal_history_retention COMMAND causal_history_tests)

    add_executable(dragon_cycle_tests tests/dragon_cycle_tests.c)
    target_link_libraries(dragon_cycle_tests PRIVATE crownless_persistence)
    cc_strict_warnings(dragon_cycle_tests)
    add_test(NAME goblin_dragon_causal_cycle COMMAND dragon_cycle_tests)

    add_executable(dragon_ecology_tests tests/dragon_ecology_tests.c)
    target_link_libraries(dragon_ecology_tests PRIVATE crownless_persistence)
    cc_strict_warnings(dragon_ecology_tests)
    add_test(NAME dragon_crown_ecology COMMAND dragon_ecology_tests)

    add_executable(material_economy_tests tests/material_economy_tests.c)
    target_link_libraries(material_economy_tests PRIVATE crownless_sim)
    cc_strict_warnings(material_economy_tests)
    add_test(NAME physical_material_economy COMMAND material_economy_tests)

    add_executable(metagame_tests tests/metagame_tests.c)
    target_link_libraries(metagame_tests PRIVATE crownless_metagame)
    cc_strict_warnings(metagame_tests)
    add_test(NAME text_first_metagame COMMAND metagame_tests)

    if(CC_BUILD_BENCHMARKS)
        add_test(NAME simulation_shape_metrics
                 COMMAND crownless_sim_metrics --seeds 8 --years 3)
    endif()

    add_executable(limb_system_tests tests/limb_system_tests.c)
    target_link_libraries(limb_system_tests PRIVATE crownless_locomotion)
    cc_strict_warnings(limb_system_tests)
    add_test(NAME generalized_limb_system COMMAND limb_system_tests)

    add_executable(biomech_system_tests tests/biomech_system_tests.c)
    target_link_libraries(biomech_system_tests PRIVATE crownless_locomotion)
    cc_strict_warnings(biomech_system_tests)
    add_test(NAME generalized_biomechanical_system COMMAND biomech_system_tests)

    add_executable(creature_rig_tests tests/creature_rig_tests.c)
    target_link_libraries(creature_rig_tests PRIVATE crownless_locomotion)
    cc_strict_warnings(creature_rig_tests)
    add_test(NAME creature_skeletal_muscular_system COMMAND creature_rig_tests)

    add_executable(humanoid_skin_tests tests/humanoid_skin_tests.c)
    target_link_libraries(humanoid_skin_tests PRIVATE crownless_locomotion)
    cc_strict_warnings(humanoid_skin_tests)
    add_test(NAME blender_humanoid_skin_contract COMMAND humanoid_skin_tests)

    add_executable(motion_system_tests tests/motion_system_tests.c)
    target_link_libraries(motion_system_tests PRIVATE crownless_locomotion)
    cc_strict_warnings(motion_system_tests)
    add_test(NAME layered_motion_contract COMMAND motion_system_tests)

    add_executable(creature_catalog_tests tests/creature_catalog_tests.c)
    target_link_libraries(creature_catalog_tests PRIVATE
        crownless_creature_catalog
    )
    cc_strict_warnings(creature_catalog_tests)
    add_test(NAME generated_creature_catalog COMMAND creature_catalog_tests)

    if(CC_BUILD_CLIENT)
        add_executable(npc_appearance_tests
            tests/npc_appearance_tests.c
            src/client/cc_npc_appearance.c
        )
        target_link_libraries(npc_appearance_tests PRIVATE raylib)
        target_include_directories(npc_appearance_tests PRIVATE src)
        cc_strict_warnings(npc_appearance_tests)
        add_test(NAME deterministic_npc_population COMMAND npc_appearance_tests)

        add_executable(local_movement_tests
            tests/local_movement_tests.c
            src/client/cc_overlay.c
            src/client/cc_npc_appearance.c
            src/client/cc_local3d.c
            src/client/cc_local_runtime.c
            src/client/cc_local_renderer_stats.c
        )
        target_link_libraries(local_movement_tests PRIVATE
            crownless_sim crownless_locomotion
            crownless_creature_catalog raylib
        )
        target_compile_definitions(local_movement_tests PRIVATE
            CC_ASSET_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
        )
        cc_strict_warnings(local_movement_tests)
        add_test(NAME local_collision_space COMMAND local_movement_tests)

        add_executable(terrain_tests
            tests/terrain_tests.c
            src/client/cc_overlay.c
            src/client/cc_npc_appearance.c
            src/client/cc_local3d.c
            src/client/cc_local_runtime.c
            src/client/cc_local_renderer_stats.c
        )
        target_link_libraries(terrain_tests PRIVATE
            crownless_sim crownless_locomotion
            crownless_creature_catalog raylib
        )
        target_compile_definitions(terrain_tests PRIVATE
            CC_ASSET_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
        )
        cc_strict_warnings(terrain_tests)
        add_test(NAME seeded_hilly_terrain COMMAND terrain_tests)

    endif()
endif()
