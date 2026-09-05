if(NOT DEFINED CC_SOURCE_DIR OR NOT DEFINED CC_BINARY_DIR)
    message(FATAL_ERROR "Source and web build directories are required")
endif()

set(startup_source "${CC_SOURCE_DIR}/assets/shaders/style_grade.fs")
set(lazy_source
    "${CC_SOURCE_DIR}/assets/maps/collectible_map_atlas.png")
set(startup_output "${CC_BINARY_DIR}/site/index.data")
set(loader_output "${CC_BINARY_DIR}/site/assets.js")
set(game_outputs
    "${CC_BINARY_DIR}/site/index.html"
    "${CC_BINARY_DIR}/site/index.js"
    "${CC_BINARY_DIR}/site/index.wasm")
set(lazy_output
    "${CC_BINARY_DIR}/site/assets/maps/collectible_map_atlas.png")
set(backup_dir "${CC_BINARY_DIR}/incremental-asset-test")
set(startup_backup "${backup_dir}/style_grade.fs")
set(lazy_backup "${backup_dir}/collectible_map_atlas.png")

foreach(path IN ITEMS
        "${startup_source}" "${lazy_source}"
        "${startup_output}" "${loader_output}" "${lazy_output}" ${game_outputs})
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Incremental asset test input is missing: ${path}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${backup_dir}")
file(COPY_FILE "${startup_source}" "${startup_backup}" ONLY_IF_DIFFERENT)
file(COPY_FILE "${lazy_source}" "${lazy_backup}" ONLY_IF_DIFFERENT)
file(SHA256 "${startup_output}" startup_hash_before)
file(SHA256 "${lazy_output}" lazy_hash_before)
file(SHA256 "${loader_output}" loader_hash_before)
foreach(path IN LISTS game_outputs)
    file(TIMESTAMP "${path}" timestamp_before "%s.%f")
    set("game_timestamp_${path}" "${timestamp_before}")
endforeach()

function(assert_game_outputs_unchanged)
    foreach(path IN LISTS game_outputs)
        file(TIMESTAMP "${path}" timestamp_after "%s.%f")
        if(NOT timestamp_after STREQUAL "${game_timestamp_${path}}")
            message(FATAL_ERROR "Asset-only rebuild relinked ${path}")
        endif()
    endforeach()
endfunction()

file(APPEND "${startup_source}" "\n// incremental asset refresh test\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${CC_BINARY_DIR}"
            --target crownless_carriage --parallel 2
    RESULT_VARIABLE startup_build_result
    OUTPUT_VARIABLE startup_build_output
    ERROR_VARIABLE startup_build_error
)
file(COPY_FILE "${startup_backup}" "${startup_source}" ONLY_IF_DIFFERENT)

if(NOT startup_build_result EQUAL 0)
    message(FATAL_ERROR
        "Startup asset rebuild failed:\n"
        "${startup_build_output}\n${startup_build_error}")
endif()

file(SHA256 "${startup_output}" startup_hash_after)
if(startup_hash_before STREQUAL startup_hash_after)
    message(FATAL_ERROR "The startup asset archive did not refresh")
endif()
file(SHA256 "${loader_output}" loader_hash_after)
if(loader_hash_before STREQUAL loader_hash_after)
    message(FATAL_ERROR "The startup asset loader did not refresh its metadata")
endif()
assert_game_outputs_unchanged()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${CC_BINARY_DIR}"
            --target crownless_carriage --parallel 2
    RESULT_VARIABLE startup_restore_result
    OUTPUT_VARIABLE startup_restore_output
    ERROR_VARIABLE startup_restore_error
)
if(NOT startup_restore_result EQUAL 0)
    message(FATAL_ERROR
        "Restoring the startup asset failed:\n"
        "${startup_restore_output}\n${startup_restore_error}")
endif()

file(SHA256 "${startup_output}" startup_hash_restored)
if(NOT startup_hash_before STREQUAL startup_hash_restored)
    message(FATAL_ERROR "The restored startup archive differs from the clean build")
endif()

file(APPEND "${lazy_source}" "crownless-incremental-asset-refresh-test")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${CC_BINARY_DIR}"
            --target crownless_carriage --parallel 2
    RESULT_VARIABLE lazy_build_result
    OUTPUT_VARIABLE lazy_build_output
    ERROR_VARIABLE lazy_build_error
)
file(COPY_FILE "${lazy_backup}" "${lazy_source}" ONLY_IF_DIFFERENT)

if(NOT lazy_build_result EQUAL 0)
    message(FATAL_ERROR
        "Lazy asset rebuild failed:\n${lazy_build_output}\n${lazy_build_error}")
endif()

file(SHA256 "${startup_output}" startup_hash_after_lazy_change)
file(SHA256 "${lazy_output}" lazy_hash_after)
if(NOT startup_hash_before STREQUAL startup_hash_after_lazy_change)
    message(FATAL_ERROR "A lazy asset change rebuilt the startup archive")
endif()
if(lazy_hash_before STREQUAL lazy_hash_after)
    message(FATAL_ERROR "The copied lazy asset did not refresh")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${CC_BINARY_DIR}"
            --target crownless_carriage --parallel 2
    RESULT_VARIABLE lazy_restore_result
    OUTPUT_VARIABLE lazy_restore_output
    ERROR_VARIABLE lazy_restore_error
)
if(NOT lazy_restore_result EQUAL 0)
    message(FATAL_ERROR
        "Restoring the lazy asset failed:\n"
        "${lazy_restore_output}\n${lazy_restore_error}")
endif()

file(SHA256 "${startup_output}" final_startup_hash)
file(SHA256 "${lazy_output}" final_lazy_hash)
file(SHA256 "${loader_output}" final_loader_hash)
if(NOT startup_hash_before STREQUAL final_startup_hash OR
   NOT lazy_hash_before STREQUAL final_lazy_hash OR
   NOT loader_hash_before STREQUAL final_loader_hash)
    message(FATAL_ERROR "Restored web assets differ from the clean build")
endif()
assert_game_outputs_unchanged()

file(REMOVE_RECURSE "${backup_dir}")
message(STATUS "Incremental startup and lazy asset refresh passed")
