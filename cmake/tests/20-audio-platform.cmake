add_executable(soundscape_tests tests/soundscape_tests.c)
target_link_libraries(soundscape_tests PRIVATE crownless_soundscape crownless_locomotion)
cc_strict_warnings(soundscape_tests)
add_test(NAME soundscape_timing_and_samples COMMAND soundscape_tests)
if(CC_BUILD_CLIENT)
    add_executable(audio_playback_tests tests/audio_playback_tests.c src/client/cc_audio.c)
    target_include_directories(audio_playback_tests PRIVATE $<TARGET_PROPERTY:raylib,INTERFACE_INCLUDE_DIRECTORIES>)
    target_link_libraries(audio_playback_tests PRIVATE crownless_soundscape)
    cc_strict_warnings(audio_playback_tests)
    add_test(NAME audio_playback_lifetime COMMAND audio_playback_tests)
    if(NOT EMSCRIPTEN)
        add_executable(voice_net_tests tests/voice_net_tests.c src/client/cc_voice_net.c)
        target_link_libraries(voice_net_tests PRIVATE crownless_story CURL::libcurl Threads::Threads)
        cc_strict_warnings(voice_net_tests)
    endif()
endif()
if(NOT EMSCRIPTEN)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        if(CC_BUILD_CLIENT)
            add_test(NAME native_speech_delivery
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/voice_net_tests.py
                    $<TARGET_FILE:voice_net_tests>)
        endif()
        add_test(NAME audio_exports_and_voice_assets
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/audio_export_tests.py
                $<TARGET_FILE:crownless_audio_export>)
        add_test(NAME campaign_speech_pack
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/speech_pack_tests.py
                $<TARGET_FILE:crownless_audio_export>)
        add_test(NAME speech_worker_and_cache
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/speech_worker_tests.py
                $<TARGET_FILE:crownless_audio_export>)
    endif()
endif()

add_test(
    NAME license_page_contract
    COMMAND ${CMAKE_COMMAND}
        -DCC_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/license_page_tests.cmake
)

if(APPLE)
    add_test(NAME macos_configuration_contract
        COMMAND ${CMAKE_COMMAND}
            -DCC_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DCC_TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/macos-contract
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/macos_configuration_tests.cmake)
    set_tests_properties(macos_configuration_contract PROPERTIES TIMEOUT 180)
endif()

if(APPLE AND CC_BUILD_CLIENT)
    add_test(
        NAME macos_deployment_target
        COMMAND ${CMAKE_COMMAND}
            -DCC_BINARY=$<TARGET_FILE:crownless_carriage>
            -DCC_EXPECTED_MINOS=${CMAKE_OSX_DEPLOYMENT_TARGET}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/macos_deployment_target_tests.cmake
    )
endif()

if(CC_BUILD_BENCHMARKS)
    add_test(NAME robotics_benchmark COMMAND crownless_robotics_benchmark)
    set_tests_properties(robotics_benchmark PROPERTIES RUN_SERIAL TRUE)
    set(CC_BENCHMARK_TEST_ARGUMENTS --quick)
    # The simulation budget is calibrated on the Linux CI runner. Keep
    # the benchmark smoke test on Apple hosts, but do not compare CPU-time
    # thresholds across different architectures and host schedulers.
    if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$" AND
       NOT APPLE)
        list(APPEND CC_BENCHMARK_TEST_ARGUMENTS --assert-budget)
    endif()
    add_test(NAME headless_performance_budget
             COMMAND crownless_benchmark ${CC_BENCHMARK_TEST_ARGUMENTS})
    set_tests_properties(headless_performance_budget PROPERTIES RUN_SERIAL TRUE)
    unset(CC_BENCHMARK_TEST_ARGUMENTS)
    add_test(NAME benchmark_rejects_day_overflow
             COMMAND crownless_benchmark --sim-years 5883517)
    set_tests_properties(benchmark_rejects_day_overflow
                         PROPERTIES WILL_FAIL TRUE)
endif()
