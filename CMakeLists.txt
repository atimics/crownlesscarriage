cmake_minimum_required(VERSION 3.24)

project(crownless_carriage VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CC_BUILD_CLIENT "Build the raylib orthographic 3D isometric client" ON)
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
    src/locomotion/cc_humanoid.c
    src/locomotion/cc_humanoid_skin.c
    src/locomotion/cc_limb.c
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
if(NOT MSVC)
    target_link_libraries(crownless_sim PUBLIC m)
endif()
cc_strict_warnings(crownless_sim)

find_package(SQLite3 REQUIRED)
add_library(crownless_persistence
    src/persistence/cc_save.c
)
target_include_directories(crownless_persistence PUBLIC src)
target_link_libraries(crownless_persistence PUBLIC crownless_sim SQLite::SQLite3)
cc_strict_warnings(crownless_persistence)

add_executable(crownless_sim_runner tools/sim_runner.c)
target_link_libraries(crownless_sim_runner PRIVATE crownless_persistence)
cc_strict_warnings(crownless_sim_runner)

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

    add_executable(crownless_carriage
        src/client/main.c
        src/client/cc_local3d.c
    )
    if(APPLE)
        set_target_properties(crownless_carriage PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "Crownless Carriage"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.atimics.crownlesscarriage"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        )
    endif()
    target_link_libraries(crownless_carriage PRIVATE
        crownless_persistence crownless_locomotion raylib
    )
    if(APPLE)
        set(CC_HERO_ENGINE_ASSET
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/hero/crownless_hero_engine_rig_v01.glb"
        )
        set(CC_HERO_ENGINE_MANIFEST
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/exports/hero/crownless_hero_engine_rig_v01.json"
        )
        add_custom_target(crownless_hero_assets ALL
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/hero"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_HERO_ENGINE_ASSET}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/hero/crownless_hero_engine_rig_v01.glb"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CC_HERO_ENGINE_MANIFEST}"
                "$<TARGET_BUNDLE_CONTENT_DIR:crownless_carriage>/Resources/assets/exports/hero/crownless_hero_engine_rig_v01.json"
            DEPENDS "${CC_HERO_ENGINE_ASSET}" "${CC_HERO_ENGINE_MANIFEST}"
            VERBATIM
        )
        add_dependencies(crownless_carriage crownless_hero_assets)
    endif()
    cc_strict_warnings(crownless_carriage)
endif()

include(CTest)
if(BUILD_TESTING)
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

    add_executable(limb_system_tests tests/limb_system_tests.c)
    target_link_libraries(limb_system_tests PRIVATE crownless_locomotion)
    cc_strict_warnings(limb_system_tests)
    add_test(NAME generalized_limb_system COMMAND limb_system_tests)

    add_executable(biomech_system_tests tests/biomech_system_tests.c)
    target_link_libraries(biomech_system_tests PRIVATE crownless_locomotion)
    cc_strict_warnings(biomech_system_tests)
    add_test(NAME generalized_biomechanical_system COMMAND biomech_system_tests)

    add_executable(humanoid_skin_tests tests/humanoid_skin_tests.c)
    target_link_libraries(humanoid_skin_tests PRIVATE crownless_locomotion)
    cc_strict_warnings(humanoid_skin_tests)
    add_test(NAME blender_humanoid_skin_contract COMMAND humanoid_skin_tests)

    if(CC_BUILD_CLIENT)
        add_executable(local_movement_tests
            tests/local_movement_tests.c
            src/client/cc_local3d.c
        )
        target_link_libraries(local_movement_tests PRIVATE
            crownless_sim crownless_locomotion raylib
        )
        cc_strict_warnings(local_movement_tests)
        add_test(NAME local_collision_space COMMAND local_movement_tests)
    endif()
endif()
