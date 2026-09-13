if(NOT DEFINED CC_SOURCE_DIR OR NOT DEFINED CC_TEST_BINARY_DIR)
    message(FATAL_ERROR "Source and test build directories are required")
endif()

function(run_checked)
    execute_process(COMMAND ${ARGV}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Command failed: ${ARGV}\n${output}\n${error}")
    endif()
endfunction()

# Each run starts with its own fresh cache. The stale case then reuses that
# cache after replacing only the deployment target with an empty value.
file(REMOVE_RECURSE "${CC_TEST_BINARY_DIR}")
foreach(case fresh stale override)
    set(build "${CC_TEST_BINARY_DIR}/${case}")
    set(expected "14.0")
    set(override)
    if(case STREQUAL "override")
        set(expected "15.0")
        set(override "-DCMAKE_OSX_DEPLOYMENT_TARGET=${expected}")
    endif()
    run_checked("${CMAKE_COMMAND}" -S "${CC_SOURCE_DIR}" -B "${build}"
        -DCC_BUILD_CLIENT=OFF -DCC_BUILD_BENCHMARKS=OFF -DBUILD_TESTING=OFF
        -DCC_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Release ${override})
    if(case STREQUAL "stale")
        file(READ "${build}/CMakeCache.txt" cache)
        string(REGEX REPLACE "CMAKE_OSX_DEPLOYMENT_TARGET:STRING=[^\n]*"
            "CMAKE_OSX_DEPLOYMENT_TARGET:STRING=" cache "${cache}")
        file(WRITE "${build}/CMakeCache.txt" "${cache}")
        run_checked("${CMAKE_COMMAND}" -S "${CC_SOURCE_DIR}" -B "${build}")
    endif()
    file(STRINGS "${build}/CMakeCache.txt" target
        REGEX "^CMAKE_OSX_DEPLOYMENT_TARGET:[^=]+=")
    string(REGEX REPLACE "^[^=]+=" "" target "${target}")
    if(NOT target STREQUAL expected)
        message(FATAL_ERROR "${case}: unexpected cached target ${target}")
    endif()
    run_checked("${CMAKE_COMMAND}" --build "${build}"
        --target crownless_sim_runner --parallel 2)
    run_checked("${CMAKE_COMMAND}"
        "-DCC_BINARY=${build}/crownless_sim_runner"
        "-DCC_EXPECTED_MINOS=${expected}"
        -P "${CC_SOURCE_DIR}/tests/macos_deployment_target_tests.cmake")
    message(STATUS "${case}: cached and executable macOS target ${expected}")
endforeach()
