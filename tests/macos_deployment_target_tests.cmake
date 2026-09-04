if(NOT DEFINED CC_BINARY OR NOT EXISTS "${CC_BINARY}")
    message(FATAL_ERROR "CC_BINARY must name the built macOS executable")
endif()
if(NOT DEFINED CC_EXPECTED_MINOS)
    message(FATAL_ERROR "CC_EXPECTED_MINOS is required")
endif()

execute_process(
    COMMAND xcrun vtool -show-build "${CC_BINARY}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE build_version
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not inspect macOS build version: ${error}")
endif()

string(REGEX MATCH "platform[ \t]+[Mm][Aa][Cc][Oo][Ss]" platform_match
             "${build_version}")
if(NOT platform_match)
    message(FATAL_ERROR "Executable does not contain a macOS build record")
endif()

string(REGEX MATCH "minos[ \t]+([0-9]+\\.[0-9]+)" minos_match
             "${build_version}")
if(NOT minos_match)
    message(FATAL_ERROR "Executable does not report a minimum macOS version")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL CC_EXPECTED_MINOS)
    message(FATAL_ERROR
        "Executable requires macOS ${CMAKE_MATCH_1}; expected ${CC_EXPECTED_MINOS}")
endif()

message(STATUS "macOS deployment target is ${CMAKE_MATCH_1}")
