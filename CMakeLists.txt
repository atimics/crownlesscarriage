cmake_minimum_required(VERSION 3.24)

# An empty cache entry still counts as DEFINED, and a plain set(CACHE) will not
# overwrite one, so a build directory that once configured without a target
# would keep inheriting the host version forever. Test both, and FORCE past the
# stale entry. An explicit -DCMAKE_OSX_DEPLOYMENT_TARGET short-circuits the
# guard, so it is still respected.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING
        "Oldest supported macOS version" FORCE)
endif()

project(crownless_carriage VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CC_BUILD_CLIENT "Build the raylib fixed-pixel three-quarter 3D client" ON)
option(CC_BUILD_GRAPHICS_PROBES "Build standalone GPU regression probes" OFF)
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

function(cc_large_sim_stack target)
    if(MSVC)
        target_link_options(${target} PRIVATE "/STACK:16777216")
    elseif(MINGW)
        target_link_options(${target} PRIVATE "LINKER:--stack,16777216")
    endif()
endfunction()

function(cc_manifest_export_paths output manifest section)
    file(READ "${manifest}" manifest_json)
    string(JSON export_count LENGTH "${manifest_json}" "${section}")
    set(export_paths)
    if(export_count GREATER 0)
        math(EXPR last_export "${export_count} - 1")
        foreach(export_index RANGE 0 ${last_export})
            string(JSON export_path GET "${manifest_json}"
                   "${section}" ${export_index} "export")
            set(source_path "${CMAKE_CURRENT_SOURCE_DIR}/${export_path}")
            if(NOT EXISTS "${source_path}")
                message(FATAL_ERROR
                    "Manifest ${manifest} references missing ${export_path}")
            endif()
            list(APPEND export_paths "${source_path}")
        endforeach()
    endif()
    set(${output} "${export_paths}" PARENT_SCOPE)
endfunction()

add_library(crownless_locomotion
    src/locomotion/cc_biomech.c
    src/locomotion/cc_collision.c
    src/locomotion/cc_creature.c
    src/locomotion/cc_humanoid.c
    src/locomotion/cc_humanoid_skin.c
    src/locomotion/cc_footing.c
    src/locomotion/cc_limb.c
    src/locomotion/cc_multileg.c
    src/locomotion/cc_motion.c
    src/locomotion/cc_quadruped.c
    src/locomotion/cc_robotics.c
)
target_include_directories(crownless_locomotion PUBLIC src)
if(NOT MSVC)
    target_link_libraries(crownless_locomotion PUBLIC m)
endif()
cc_strict_warnings(crownless_locomotion)

add_library(crownless_sim
    src/quest/cc_quest.c
    src/sim/cc_sim.c
    src/sim/cc_sim_hash.c
    src/sim/cc_occupations.c
    src/sim/cc_gossip_topics.c
    src/sim/cc_archive_volumes.c
    src/sim/cc_archive_staff.c
    src/sim/cc_archive_recruitment.c
    src/sim/cc_archive_queries.c
    src/sim/cc_archive_supply.c
    src/sim/cc_archive_convoy.c
    src/sim/cc_archive_relocation.c
    src/sim/cc_archive_seat_plan.c
    src/sim/cc_trade_path.c
    src/sim/cc_sim_versions.c
    src/sim/cc_war.c
    src/sim/cc_identity.c
    src/sim/cc_road_council.c
    src/sim/cc_route_rules.c
    src/sim/cc_journey_departure.c
    src/sim/cc_journey_encounter.c
    src/sim/cc_journey_runtime.c
    src/sim/cc_production.c
    src/sim/cc_food_economy.c
    src/sim/cc_goods.c
    src/sim/cc_journey.c
    src/sim/cc_journey_commands.c
    src/sim/cc_mine.c
    src/sim/cc_ponies.c
)
target_include_directories(crownless_sim PUBLIC src)
cc_strict_warnings(crownless_sim)

add_library(crownless_story
    src/story/cc_story.c
    src/story/cc_speech.c
    src/story/cc_speech_interactions.c
    src/story/cc_speech_lexicon.c
    src/story/cc_core_account.c
    src/story/cc_speech_json.c
)
target_include_directories(crownless_story PUBLIC src)
target_link_libraries(crownless_story PUBLIC crownless_sim)
cc_strict_warnings(crownless_story)

add_library(crownless_music src/client/cc_music.c src/client/cc_music_library.c)
target_include_directories(crownless_music PUBLIC src)
target_link_libraries(crownless_music PUBLIC crownless_sim)
if(NOT MSVC)
    target_link_libraries(crownless_music PUBLIC m)
endif()
cc_strict_warnings(crownless_music)

add_library(crownless_creature_catalog
    src/client/cc_creature_catalog.c
)
target_include_directories(crownless_creature_catalog PUBLIC src)
if(NOT MSVC)
    target_link_libraries(crownless_creature_catalog PUBLIC m)
endif()
cc_strict_warnings(crownless_creature_catalog)

add_library(crownless_local_place
    src/client/cc_local_place.c
)
target_include_directories(crownless_local_place PUBLIC src)
cc_strict_warnings(crownless_local_place)

add_library(crownless_world
    src/world/cc_world.c
)
target_include_directories(crownless_world PUBLIC src)
target_link_libraries(crownless_world PUBLIC crownless_sim)
if(NOT MSVC)
    target_link_libraries(crownless_world PUBLIC m)
endif()
cc_strict_warnings(crownless_world)

add_library(crownless_client_session
    src/client/cc_client_session.c
)
target_include_directories(crownless_client_session PUBLIC src)
cc_strict_warnings(crownless_client_session)

add_library(crownless_soundscape src/client/cc_soundscape.c)
target_include_directories(crownless_soundscape PUBLIC src)
if(NOT MSVC)
    target_link_libraries(crownless_soundscape PUBLIC m)
endif()
cc_strict_warnings(crownless_soundscape)

add_library(crownless_client_policy
    src/client/cc_client_policy.c
    src/client/cc_interaction.c
)
target_include_directories(crownless_client_policy PUBLIC src)
if(NOT MSVC)
    target_link_libraries(crownless_client_policy PUBLIC m)
endif()
cc_strict_warnings(crownless_client_policy)

add_library(crownless_road_book
    src/client/cc_road_book.c
)
target_include_directories(crownless_road_book PUBLIC src)
target_link_libraries(crownless_road_book PUBLIC crownless_sim)
cc_strict_warnings(crownless_road_book)

if(NOT EMSCRIPTEN)
    add_executable(crownless_audio_export tools/audio_export.c)
    target_link_libraries(crownless_audio_export PRIVATE crownless_story crownless_soundscape)
    cc_strict_warnings(crownless_audio_export)
endif()

if(EMSCRIPTEN)
    add_library(crownless_sqlite_web INTERFACE)
    target_compile_options(crownless_sqlite_web INTERFACE
        "--use-port=sqlite3"
    )
    target_link_options(crownless_sqlite_web INTERFACE
        "--use-port=sqlite3"
    )
    set(CC_SQLITE_TARGET crownless_sqlite_web)
else()
    find_package(SQLite3 REQUIRED)
    set(CC_SQLITE_TARGET SQLite::SQLite3)
endif()
add_library(crownless_persistence
    src/persistence/cc_starting_campaign.c
    src/persistence/cc_save.c
    src/persistence/cc_journal_replay.c
    src/persistence/cc_legacy_runtime.c
)
target_include_directories(crownless_persistence PUBLIC src)
target_link_libraries(crownless_persistence PUBLIC
    crownless_sim ${CC_SQLITE_TARGET}
)
cc_strict_warnings(crownless_persistence)

if(NOT EMSCRIPTEN)
    set_target_properties(crownless_sim crownless_persistence PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(crownless_coop SHARED src/multiplayer/cc_coop.c)
    target_include_directories(crownless_coop PUBLIC src)
    target_link_libraries(crownless_coop PRIVATE crownless_persistence)
    cc_strict_warnings(crownless_coop)
endif()

add_library(crownless_metagame
    src/metagame/cc_metagame.c
)
target_include_directories(crownless_metagame PUBLIC src)
target_link_libraries(crownless_metagame PUBLIC
    crownless_persistence crownless_story)
cc_strict_warnings(crownless_metagame)

add_executable(crownless_gossip_corpus tools/gossip_corpus.c)
target_link_libraries(crownless_gossip_corpus PRIVATE crownless_story)
cc_strict_warnings(crownless_gossip_corpus)
add_executable(crownless_gossip_flow tools/gossip_flow.c)
target_link_libraries(crownless_gossip_flow PRIVATE crownless_story)
cc_strict_warnings(crownless_gossip_flow)

add_executable(crownless_sim_runner tools/sim_runner.c)
target_link_libraries(crownless_sim_runner PRIVATE crownless_persistence)
cc_strict_warnings(crownless_sim_runner)

add_executable(crownless_metagame_playtest tools/metagame.c)
target_link_libraries(crownless_metagame_playtest PRIVATE crownless_metagame)
cc_strict_warnings(crownless_metagame_playtest)
cc_large_sim_stack(crownless_metagame_playtest)

add_executable(crownless_agent_courier tools/agent_courier.c)
target_link_libraries(crownless_agent_courier PRIVATE crownless_metagame)
cc_strict_warnings(crownless_agent_courier)

if(CC_BUILD_BENCHMARKS)
    add_executable(crownless_benchmark tools/benchmark.c)
    target_link_libraries(crownless_benchmark PRIVATE
        crownless_sim crownless_locomotion crownless_world
    )
    cc_strict_warnings(crownless_benchmark)
    add_executable(crownless_robotics_benchmark tools/robotics_benchmark.c)
    target_link_libraries(crownless_robotics_benchmark PRIVATE crownless_locomotion)
    cc_strict_warnings(crownless_robotics_benchmark)
    add_custom_target(run_benchmarks
        COMMAND crownless_benchmark
        COMMAND crownless_robotics_benchmark
        DEPENDS crownless_benchmark crownless_robotics_benchmark
        USES_TERMINAL
    )

    add_executable(crownless_species_metrics tools/species_metrics.c)
    target_link_libraries(crownless_species_metrics PRIVATE crownless_sim)
    cc_strict_warnings(crownless_species_metrics)

    add_executable(crownless_sim_metrics tools/sim_metrics.c)
    target_link_libraries(crownless_sim_metrics PRIVATE crownless_sim)
    cc_strict_warnings(crownless_sim_metrics)

    add_executable(crownless_quest_cast_experiment tools/quest_cast_experiment.c)
    target_link_libraries(crownless_quest_cast_experiment PRIVATE crownless_sim)
    cc_strict_warnings(crownless_quest_cast_experiment)
    add_executable(crownless_contested_road_experiment tools/contested_road_experiment.c)
    target_link_libraries(crownless_contested_road_experiment PRIVATE crownless_sim)
    cc_strict_warnings(crownless_contested_road_experiment)

    add_executable(crownless_grain_experiment tools/grain_experiment.c)
    target_link_libraries(crownless_grain_experiment PRIVATE crownless_sim)
    cc_strict_warnings(crownless_grain_experiment)

    add_executable(crownless_bakery_experiment tools/bakery_experiment.c)
    target_link_libraries(crownless_bakery_experiment PRIVATE crownless_sim)
    cc_strict_warnings(crownless_bakery_experiment)

    add_executable(crownless_letter_probe tools/letter_probe.c)
    target_link_libraries(crownless_letter_probe PRIVATE crownless_sim)
    cc_strict_warnings(crownless_letter_probe)

    add_executable(crownless_agent_sweep tools/agent_sweep.c)
    target_link_libraries(crownless_agent_sweep PRIVATE crownless_sim)
    cc_strict_warnings(crownless_agent_sweep)
endif()

if(CC_BUILD_CLIENT)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    if(EMSCRIPTEN)
        set(PLATFORM Web CACHE STRING "" FORCE)
        set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
    endif()
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG dbc56a87da87d973a9c5baa4e7438a9d20121d28
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DRAYLIB_SOURCE_DIR=${raylib_SOURCE_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PatchRaylibMatrices.cmake"
        COMMAND_ERROR_IS_FATAL ANY
    )
    if(TARGET raylib)
        get_target_property(CC_RAYLIB_IMPORTED raylib IMPORTED)
        if(NOT CC_RAYLIB_IMPORTED)
            target_compile_definitions(raylib PRIVATE SUPPORT_GPU_SKINNING=1)
            if(EMSCRIPTEN)
                set_property(TARGET raylib PROPERTY C_EXTENSIONS ON)
            endif()
        endif()
    endif()

    if(CC_BUILD_GRAPHICS_PROBES)
        add_executable(matrix_upload_tests tests/matrix_upload_tests.c)
        target_link_libraries(matrix_upload_tests PRIVATE raylib)
        cc_strict_warnings(matrix_upload_tests)
        if(EMSCRIPTEN)
            set_property(TARGET matrix_upload_tests PROPERTY C_EXTENSIONS ON)
            set_target_properties(matrix_upload_tests PROPERTIES
                OUTPUT_NAME matrix-upload SUFFIX ".html"
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/matrix-probe")
            target_link_options(matrix_upload_tests PRIVATE
                "-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2")
        else()
            add_custom_target(run_matrix_upload_tests
                COMMAND matrix_upload_tests DEPENDS matrix_upload_tests
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                USES_TERMINAL)
        endif()
    endif()

    add_library(crownless_local_renderer STATIC
        src/client/cc_heraldry.c
        src/client/cc_heraldry.h
        src/client/cc_overlay.c
        src/client/cc_npc_appearance.c
        src/client/cc_local3d.c
        src/client/cc_local_runtime.c
        src/client/cc_local_renderer_stats.c
        src/client/local3d/context_state.inc
        src/client/local3d/terrain_navigation.inc
        src/client/local3d/actor_simulation.inc
        src/client/local3d/camera_composition.inc
        src/client/local3d/asset_loading.inc
        src/client/local3d/authored_places.inc
        src/client/local3d/heraldry.inc
        src/client/local3d/actor_rendering.inc
        src/client/local3d/road_book.inc
        src/client/local3d/open_world.inc
    )
    target_include_directories(crownless_local_renderer PUBLIC src)
    target_link_libraries(crownless_local_renderer PUBLIC
        crownless_sim crownless_locomotion
        crownless_creature_catalog crownless_local_place
        crownless_client_policy crownless_road_book crownless_world raylib
    )
    target_compile_definitions(crownless_local_renderer PRIVATE
        CC_ASSET_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    cc_strict_warnings(crownless_local_renderer)

    file(GLOB CC_VOICE_ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/voice/*.wav")
    add_executable(crownless_carriage src/client/main.c src/client/cc_audio.c
                   src/client/cc_music_player.c
                   src/client/cc_music_stream.c src/client/cc_music_net.c
                   src/client/cc_voice_net.c src/client/cc_coop_client.c
                   src/client/cc_company.c)
    target_compile_definitions(crownless_carriage PRIVATE
        CC_ASSET_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    if(APPLE)
        set_target_properties(crownless_carriage PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "Crownless Carriage"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.atimics.crownless"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        )
    endif()
    if(APPLE AND NOT EMSCRIPTEN)
        add_custom_target(crownless_voice_assets ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/voice"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/audio/voice"
            DEPENDS ${CC_VOICE_ASSETS}
            VERBATIM)
        add_dependencies(crownless_voice_assets crownless_carriage)
    endif()
    if(NOT EMSCRIPTEN)
        if(APPLE)
            set(CC_CAMPAIGN_DEST "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/campaigns")
        else()
            set(CC_CAMPAIGN_DEST "$<TARGET_FILE_DIR:crownless_carriage>/assets/campaigns")
        endif()
        add_custom_command(TARGET crownless_carriage POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CC_CAMPAIGN_DEST}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/campaigns/deep-wyrm.ccsave" "${CC_CAMPAIGN_DEST}/deep-wyrm.ccsave")
    endif()
    target_link_libraries(crownless_carriage PRIVATE
        crownless_local_renderer
        crownless_persistence crownless_locomotion
        crownless_creature_catalog crownless_local_place
        crownless_client_session crownless_client_policy crownless_story crownless_soundscape
        crownless_world crownless_music
        raylib
    )
    if(NOT EMSCRIPTEN)
        find_package(CURL REQUIRED)
        find_package(Threads REQUIRED)
        target_link_libraries(crownless_carriage PRIVATE glfw CURL::libcurl Threads::Threads)
    endif()
    file(GLOB CC_SPEECH_ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/speech/*")
    if(EMSCRIPTEN)
        set(CC_SPEECH_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/site/speech")
    elseif(APPLE)
        set(CC_SPEECH_BUNDLE_DIR "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/audio/speech")
    else()
        set(CC_SPEECH_BUNDLE_DIR "$<TARGET_FILE_DIR:crownless_carriage>/assets/audio/speech")
    endif()
    add_custom_target(crownless_speech_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/speech" "${CC_SPEECH_BUNDLE_DIR}"
        DEPENDS ${CC_SPEECH_ASSETS}
        VERBATIM)
    add_dependencies(crownless_speech_assets crownless_carriage)
    file(GLOB CC_MUSIC_ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/music/*.mp3")
    if(EMSCRIPTEN)
        set(CC_MUSIC_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/site/assets/audio/music")
    elseif(APPLE)
        set(CC_MUSIC_BUNDLE_DIR "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/audio/music")
    else()
        set(CC_MUSIC_BUNDLE_DIR "$<TARGET_FILE_DIR:crownless_carriage>/assets/audio/music")
    endif()
    add_custom_target(crownless_music_assets ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CC_MUSIC_BUNDLE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CC_MUSIC_ASSETS} "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/music/offline.json"
            "${CC_MUSIC_BUNDLE_DIR}"
        DEPENDS ${CC_MUSIC_ASSETS} "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/music/offline.json"
        VERBATIM)
    if(NOT EMSCRIPTEN)
        add_dependencies(crownless_music_assets crownless_carriage)
    endif()
    if(EMSCRIPTEN)
        target_compile_definitions(crownless_local_renderer PRIVATE PLATFORM_WEB)
        set_property(TARGET crownless_local_renderer PROPERTY C_EXTENSIONS ON)
        set(CC_WEB_ASSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/web-runtime")
        file(GLOB CC_WEB_NPC_ASSET_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/npc/npc_module_*.glb"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/npc/npc_wayfarer_v01.glb"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/npc/npc_guard_v01.glb"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/npc/npc_raider_v01.glb"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/npc/npc_merchant_v01.glb"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/npc/npc_laborer_v01.glb"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/npc/npc_traveller_v01.glb"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/npc/npc_refugee_v01.glb"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/npc/npc_scout_v01.glb"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/npc/npc_healer_v01.glb"
        )
        file(GLOB CC_WEB_WORLD_KIT_ASSET_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/world_kit/wk_body_skin_standard_athletic_balanced_v01.glb"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/world_kit/wk_head_*.glb"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/world_kit/wk_hair_*.glb"
        )
        file(GLOB CC_WEB_SHADER_ASSET_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.vs"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.fs"
        )
        file(GLOB CC_WEB_ECONOMIC_CARGO_ASSET_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/glb/economy_cargo_*.glb"
        )
        set(CC_WEB_STARTUP_ASSET_SOURCES
            ${CC_VOICE_ASSETS}
            ${CC_WEB_NPC_ASSET_SOURCES}
            ${CC_WEB_WORLD_KIT_ASSET_SOURCES}
            ${CC_WEB_SHADER_ASSET_SOURCES}
            ${CC_WEB_ECONOMIC_CARGO_ASSET_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/glb/carriage_base_v01.glb"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/economic_goods_v01.png"
        )
        foreach(asset IN ITEMS
                creature_horse_v01.glb
                creature_cow_v01.glb
                creature_sheep_v01.glb
                creature_dragon_v01.glb
                creature_dragon_whelp_v01.glb
                creature_dragon_wanderer_v01.glb
                creature_dragon_deep_wyrm_v01.glb)
            list(APPEND CC_WEB_STARTUP_ASSET_SOURCES
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/creatures/${asset}")
        endforeach()
        set(CC_WEB_LAZY_ASSET_SOURCES)
        foreach(asset IN ITEMS
                environment_bridge_checkpoint_v01.glb
                environment_market_granary_v01.glb
                environment_mine_entrance_v01.glb
                state_food_shortage_v01.glb
                state_harsh_enforcement_v01.glb
                state_market_recovery_v01.glb)
            list(APPEND CC_WEB_LAZY_ASSET_SOURCES
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/glb/${asset}")
        endforeach()
        list(APPEND CC_WEB_LAZY_ASSET_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/gloamgate_to_alderwatch.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/collectible_map_atlas.png"
        )
        list(APPEND CC_WEB_STARTUP_ASSET_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/assets/campaigns/deep-wyrm.ccsave")
        set(CC_WEB_STARTUP_ASSET_OUTPUTS)
        foreach(asset IN LISTS CC_WEB_STARTUP_ASSET_SOURCES)
            file(RELATIVE_PATH relative_asset
                "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${asset}")
            list(APPEND CC_WEB_STARTUP_ASSET_OUTPUTS
                "${CC_WEB_ASSET_DIR}/assets/${relative_asset}")
        endforeach()
        set(CC_WEB_LAZY_ASSET_OUTPUTS)
        set(CC_WEB_SITE_LAZY_ASSET_OUTPUTS)
        foreach(asset IN LISTS CC_WEB_LAZY_ASSET_SOURCES)
            file(RELATIVE_PATH relative_asset
                "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${asset}")
            list(APPEND CC_WEB_LAZY_ASSET_OUTPUTS
                "${CC_WEB_ASSET_DIR}/lazy-assets/${relative_asset}")
            list(APPEND CC_WEB_SITE_LAZY_ASSET_OUTPUTS
                "${CMAKE_CURRENT_BINARY_DIR}/site/assets/${relative_asset}")
        endforeach()
        set(CC_WEB_STARTUP_ASSET_STAMP
            "${CC_WEB_ASSET_DIR}/.startup-prepared")
        set(CC_WEB_LAZY_ASSET_STAMP
            "${CC_WEB_ASSET_DIR}/.lazy-prepared")
        set(CC_WEB_LICENSE_OUTPUTS
            "${CMAKE_CURRENT_BINARY_DIR}/site/licenses.html"
            "${CMAKE_CURRENT_BINARY_DIR}/site/LICENSE.txt"
            "${CMAKE_CURRENT_BINARY_DIR}/site/THIRD_PARTY_NOTICES.txt"
        )
        add_custom_command(
            OUTPUT
                "${CC_WEB_STARTUP_ASSET_STAMP}"
                ${CC_WEB_STARTUP_ASSET_OUTPUTS}
            COMMAND ${CMAKE_COMMAND}
                -DCC_WEB_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DCC_WEB_OUTPUT_DIR=${CC_WEB_ASSET_DIR}
                -DCC_WEB_ASSET_GROUP=startup
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PrepareWebAssets.cmake
            COMMAND ${CMAKE_COMMAND} -E touch
                "${CC_WEB_STARTUP_ASSET_STAMP}"
            DEPENDS
                ${CC_WEB_STARTUP_ASSET_SOURCES}
                ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PrepareWebAssets.cmake
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/blender/batch_static_glb.py
            VERBATIM
        )
        add_custom_command(
            OUTPUT
                "${CC_WEB_LAZY_ASSET_STAMP}"
                ${CC_WEB_LAZY_ASSET_OUTPUTS}
            COMMAND ${CMAKE_COMMAND}
                -DCC_WEB_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DCC_WEB_OUTPUT_DIR=${CC_WEB_ASSET_DIR}
                -DCC_WEB_ASSET_GROUP=lazy
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PrepareWebAssets.cmake
            COMMAND ${CMAKE_COMMAND} -E touch
                "${CC_WEB_LAZY_ASSET_STAMP}"
            DEPENDS
                ${CC_WEB_LAZY_ASSET_SOURCES}
                ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PrepareWebAssets.cmake
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/blender/batch_static_glb.py
            VERBATIM
        )
        add_custom_command(
            OUTPUT ${CC_WEB_LICENSE_OUTPUTS}
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/site"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/web/licenses.html"
                "${CMAKE_CURRENT_BINARY_DIR}/site/licenses.html"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
                "${CMAKE_CURRENT_BINARY_DIR}/site/LICENSE.txt"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt"
                "${CMAKE_CURRENT_BINARY_DIR}/site/THIRD_PARTY_NOTICES.txt"
            DEPENDS
                ${CMAKE_CURRENT_SOURCE_DIR}/web/licenses.html
                ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
                ${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt
            VERBATIM
        )
        add_custom_target(crownless_web_assets ALL DEPENDS
            "${CC_WEB_STARTUP_ASSET_STAMP}"
            "${CC_WEB_LAZY_ASSET_STAMP}"
            ${CC_WEB_STARTUP_ASSET_OUTPUTS}
            ${CC_WEB_LAZY_ASSET_OUTPUTS}
            ${CC_WEB_LICENSE_OUTPUTS}
        )
        set(CC_WEB_LAZY_SITE_STAMP
            "${CMAKE_CURRENT_BINARY_DIR}/site/.lazy-assets")
        add_custom_command(
            OUTPUT
                "${CC_WEB_LAZY_SITE_STAMP}"
                "${CMAKE_CURRENT_BINARY_DIR}/site/.nojekyll"
                ${CC_WEB_SITE_LAZY_ASSET_OUTPUTS}
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/site"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CC_WEB_ASSET_DIR}/lazy-assets"
                "${CMAKE_CURRENT_BINARY_DIR}/site/assets"
            COMMAND ${CMAKE_COMMAND} -E touch
                "${CMAKE_CURRENT_BINARY_DIR}/site/.nojekyll"
            COMMAND ${CMAKE_COMMAND} -E touch
                "${CC_WEB_LAZY_SITE_STAMP}"
            DEPENDS
                "${CC_WEB_LAZY_ASSET_STAMP}"
                ${CC_WEB_LAZY_ASSET_OUTPUTS}
            VERBATIM
        )
        add_custom_target(crownless_web_lazy_assets ALL DEPENDS
            "${CC_WEB_LAZY_SITE_STAMP}"
            "${CMAKE_CURRENT_BINARY_DIR}/site/.nojekyll"
            ${CC_WEB_SITE_LAZY_ASSET_OUTPUTS}
        )
        add_dependencies(crownless_web_lazy_assets crownless_web_assets)
        add_dependencies(crownless_carriage crownless_web_lazy_assets)
        # Asset edits only run the packager; the optimized game links on code edits.
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(CC_WEB_PACKAGER "${EMSCRIPTEN_ROOT_PATH}/tools/file_packager.py")
        add_custom_command(
            OUTPUT
                "${CMAKE_CURRENT_BINARY_DIR}/site/index.data"
                "${CMAKE_CURRENT_BINARY_DIR}/site/assets.js"
            COMMAND ${Python3_EXECUTABLE} "${CC_WEB_PACKAGER}"
                "${CMAKE_CURRENT_BINARY_DIR}/site/index.data"
                --preload "${CC_WEB_ASSET_DIR}/assets@/assets"
                --js-output=${CMAKE_CURRENT_BINARY_DIR}/site/assets.js
            DEPENDS
                "${CC_WEB_PACKAGER}"
                "${CC_WEB_STARTUP_ASSET_STAMP}"
                ${CC_WEB_STARTUP_ASSET_OUTPUTS}
            VERBATIM
        )
        add_custom_target(crownless_web_startup_package ALL DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/site/index.data"
            "${CMAKE_CURRENT_BINARY_DIR}/site/assets.js"
        )
        add_dependencies(crownless_web_startup_package crownless_web_assets)
        add_dependencies(crownless_carriage crownless_web_startup_package)
        set_target_properties(crownless_carriage PROPERTIES
            OUTPUT_NAME index
            SUFFIX ".html"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/site"
            LINK_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html;${CMAKE_CURRENT_SOURCE_DIR}/web/touch.js;${CMAKE_CURRENT_SOURCE_DIR}/web/persistence.js;${CMAKE_CURRENT_SOURCE_DIR}/web/coop/avatar.js;${CMAKE_CURRENT_SOURCE_DIR}/web/coop_game.js;${CMAKE_CURRENT_SOURCE_DIR}/web/music.js;${CMAKE_CURRENT_SOURCE_DIR}/web/voice.js"
        )
        target_compile_definitions(crownless_carriage PRIVATE PLATFORM_WEB)
        set_property(TARGET crownless_carriage PROPERTY C_EXTENSIONS ON)
        target_link_options(crownless_carriage PRIVATE
            "-sASYNCIFY=1"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sINITIAL_HEAP=16777216"
            "-sMAXIMUM_MEMORY=268435456"
            "-sMEMORY_GROWTH_LINEAR_STEP=16777216"
            "-sSTACK_SIZE=4194304"
            "-sFORCE_FILESYSTEM=1"
            "-sEXPORTED_RUNTIME_METHODS=FS_createPath,FS_createDataFile,addRunDependency,removeRunDependency"
            "-sMIN_WEBGL_VERSION=2"
            "-sMAX_WEBGL_VERSION=2"
            "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
            "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/web/coop/avatar.js"
            "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/web/coop_game.js"
            "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/web/persistence.js"
            "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/web/music.js"
            "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/web/voice.js"
            "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/web/touch.js"
        )
    elseif(APPLE)
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
        set(CC_NPC_ARCHETYPE_MANIFEST
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/npc_archetype_manifest.json"
        )
        set(CC_NPC_DYNAMIC_MODULE_MANIFEST
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/npc_dynamic_module_manifest.json"
        )
        cc_manifest_export_paths(CC_NPC_ARCHETYPE_ASSETS
            "${CC_NPC_ARCHETYPE_MANIFEST}" "archetypes")
        cc_manifest_export_paths(CC_NPC_DYNAMIC_MODULE_ASSETS
            "${CC_NPC_DYNAMIC_MODULE_MANIFEST}" "modules")
        list(APPEND CC_NPC_ARCHETYPE_ASSETS
             ${CC_NPC_DYNAMIC_MODULE_ASSETS})
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
        file(GLOB CC_ECONOMIC_CARGO_ASSETS CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/glb/economy_cargo_*.glb"
        )
        set(CC_ECONOMIC_GOODS_ATLAS_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/economic_goods_v01.png"
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
        set(CC_GLOAMGATE_ALDERWATCH_MAP_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/gloamgate_to_alderwatch.png"
        )
        set(CC_COLLECTIBLE_MAP_ATLAS_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/collectible_map_atlas.png"
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
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/maps"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/ui"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/LICENSE.txt"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/THIRD_PARTY_NOTICES.txt"
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
                ${CC_ECONOMIC_CARGO_ASSETS}
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/glb"
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
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_GLOAMGATE_ALDERWATCH_MAP_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/maps/gloamgate_to_alderwatch.png"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_COLLECTIBLE_MAP_ATLAS_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/maps/collectible_map_atlas.png"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_ECONOMIC_GOODS_ATLAS_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/ui/economic_goods_v01.png"
            DEPENDS "${CC_HERO_ENGINE_ASSET}" "${CC_HERO_ENGINE_MANIFEST}"
                    ${CC_NPC_ARCHETYPE_ASSETS} "${CC_NPC_ARCHETYPE_MANIFEST}"
                    "${CC_NPC_DYNAMIC_MODULE_MANIFEST}"
                    ${CC_CREATURE_ASSETS} "${CC_CREATURE_MANIFEST}"
                    "${CC_BRIDGE_CHECKPOINT_ASSET}" "${CC_CARRIAGE_ASSET}"
                    ${CC_ECONOMIC_CARGO_ASSETS}
                    "${CC_MARKET_ASSET}"
                    "${CC_MINE_ASSET}" "${CC_SHORTAGE_ASSET}"
                    "${CC_ENFORCEMENT_ASSET}" "${CC_RECOVERY_ASSET}"
                    "${CC_STYLE_GRADE_SHADER}"
                    "${CC_WORLD_LIGHT_VERTEX_SHADER}"
                    "${CC_WORLD_LIGHT_SKINNED_VERTEX_SHADER}"
                    "${CC_WORLD_LIGHT_FRAGMENT_SHADER}"
                    "${CC_TREE_FOLIAGE_FRAGMENT_SHADER}"
                    "${CC_HERO_PIXEL_FRAGMENT_SHADER}"
                    "${CC_GLOAMGATE_ALDERWATCH_MAP_ASSET}"
                    "${CC_COLLECTIBLE_MAP_ATLAS_ASSET}"
                    "${CC_ECONOMIC_GOODS_ATLAS_ASSET}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
                    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt"
            VERBATIM
        )
        add_dependencies(crownless_carriage crownless_hero_assets)
    endif()
    cc_strict_warnings(crownless_carriage)
    set(CC_ROADBOOK_REVIEW_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/roadbook-review")
    set(CC_ROADBOOK_ROUTE_MINIMUM_FPS 25)
    set(CC_ROADBOOK_ROUTE_P95_BUDGET_MS 40)
    set(CC_ROADBOOK_NETWORK_MINIMUM_FPS 21)
    set(CC_ROADBOOK_NETWORK_P95_BUDGET_MS 45)
    add_custom_target(run_roadbook_qa
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CC_ROADBOOK_REVIEW_DIR}"
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-storybook 0.25 0.0 "roadbook-review/storybook-close.png"
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-storybook 0.25 0.5 "roadbook-review/storybook-pullback.png"
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-storybook 0.25 1.0 "roadbook-review/storybook-wide.png"
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-storybook-arrival 0.5 "roadbook-review/storybook-arrival.png"
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-travel
                "roadbook-review/travel.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/travel.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-route-sight 0.00
                "roadbook-review/route-sight-start.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/route-sight-start.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-route-sight 0.50
                "roadbook-review/route-sight-middle.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/route-sight-middle.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-road-fork
                "roadbook-review/fork.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/fork.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-road-departure 0.00
                "roadbook-review/departure-gate.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/departure-gate.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-road-departure 0.35
                "roadbook-review/departure-rise.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/departure-rise.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-road-departure 0.50
                "roadbook-review/departure.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/departure.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-road-departure 0.60
                "roadbook-review/kingdom-marker.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/kingdom-marker.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-road-departure 1.00
                "roadbook-review/departure-road-book.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/departure-road-book.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-road-arrival 1.00
                "roadbook-review/arrival-road-book.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/arrival-road-book.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-road-arrival 0.50
                "roadbook-review/arrival.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/arrival.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-road-arrival 0.00
                "roadbook-review/arrival-gate.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/arrival-gate.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-town-arrival 1 0.00
                "roadbook-review/arrival-town-gate.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/arrival-town-gate.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-town-arrival 1 0.45
                "roadbook-review/arrival-town-road.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/arrival-town-road.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --capture-town-arrival 1 0.90
                "roadbook-review/arrival-yard.png"
        COMMAND ${CMAKE_COMMAND}
                -DCC_FILE=${CC_ROADBOOK_REVIEW_DIR}/arrival-yard.png
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/require_nonempty_file.cmake
        COMMAND $<TARGET_FILE:crownless_carriage>
                --benchmark-render 360
                ${CC_ROADBOOK_ROUTE_MINIMUM_FPS}
                roadbook-route
                ${CC_ROADBOOK_ROUTE_P95_BUDGET_MS}
        COMMAND $<TARGET_FILE:crownless_carriage>
                --benchmark-render 360
                ${CC_ROADBOOK_NETWORK_MINIMUM_FPS}
                roadbook-network
                ${CC_ROADBOOK_NETWORK_P95_BUDGET_MS}
        DEPENDS crownless_carriage
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        USES_TERMINAL
    )
    add_custom_target(run_render_benchmark
        COMMAND $<TARGET_FILE:crownless_carriage>
                --benchmark-render 600 90 street
        COMMAND $<TARGET_FILE:crownless_carriage>
                --benchmark-render 600 60 market
        COMMAND $<TARGET_FILE:crownless_carriage>
                --benchmark-render 600 60 road
        COMMAND $<TARGET_FILE:crownless_carriage>
                --benchmark-render 600 60 roadbook-route
        COMMAND $<TARGET_FILE:crownless_carriage>
                --benchmark-render 600 45 roadbook-network
        COMMAND $<TARGET_FILE:crownless_carriage>
                --benchmark-render 600 60 combat
        DEPENDS crownless_carriage
        USES_TERMINAL
    )
endif()

include(CTest)
if(BUILD_TESTING)
    # Separate registration files let new test areas land independently.
    file(GLOB CC_TEST_REGISTRATIONS CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/*.cmake")
    if(NOT CC_TEST_REGISTRATIONS)
        message(FATAL_ERROR "Test registrations are missing. Include cmake/tests in the source package, or set BUILD_TESTING=OFF.")
    endif()
    list(SORT CC_TEST_REGISTRATIONS)
    foreach(CC_TEST_REGISTRATION IN LISTS CC_TEST_REGISTRATIONS)
        include("${CC_TEST_REGISTRATION}")
    endforeach()
    unset(CC_TEST_REGISTRATION)
    unset(CC_TEST_REGISTRATIONS)
endif()
