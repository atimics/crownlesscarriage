find_program(CC_NODE_EXECUTABLE node)
if(CC_NODE_EXECUTABLE)
    add_test(NAME web_speech_delivery COMMAND ${CC_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/web_voice_tests.cjs)
    add_test(NAME browser_persistence_contract
             COMMAND ${CC_NODE_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/web_persistence_tests.mjs)
    add_test(NAME browser_shared_carriage_contract
             COMMAND ${CC_NODE_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/web_coop_tests.mjs)
    add_test(NAME browser_music_download_and_cache
             COMMAND ${CC_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/web_music_tests.mjs)
endif()

find_program(CC_PYTHON3_EXECUTABLE python3)
if(CC_PYTHON3_EXECUTABLE)
    add_test(NAME hrakhor_model_pairs
        COMMAND ${CC_PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/hrakhor_pairs_tests.py $<TARGET_FILE:core_account_probe>)
    add_test(NAME core_account_rules_current
        COMMAND ${CC_PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/compile_core_accounts.py --check)
    add_test(NAME core_account_native_parity
        COMMAND ${CC_PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/core_account_tests.py $<TARGET_FILE:core_account_probe>)
    add_test(NAME core_diagnostic_contract
        COMMAND ${CC_PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/core_diagnostic_tests.py $<TARGET_FILE:core_account_probe>)
    if(CC_BUILD_BENCHMARKS)
        add_test(NAME species_sweep_integrity COMMAND ${CC_PYTHON3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/species_sweep_tests.py
            $<TARGET_FILE:crownless_species_metrics> $<TARGET_FILE:crownless_sim_metrics>)
    endif()
    add_test(NAME gossip_corpus_integrity
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/gossip_corpus_tests.py
                     $<TARGET_FILE:crownless_gossip_corpus>)
    add_test(NAME age_of_dragons_sweep_integrity
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/age_of_dragons_sweep_tests.py)
    add_test(NAME sweep_hunger_analysis
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/sweep_analysis_tests.py)
    if(CC_BUILD_CLIENT AND NOT EMSCRIPTEN)
        add_executable(native_coop_tests tests/native_coop_tests.c src/client/cc_coop_client.c src/client/cc_company.c)
        target_link_libraries(native_coop_tests PRIVATE crownless_persistence crownless_client_session raylib CURL::libcurl Threads::Threads)
        cc_strict_warnings(native_coop_tests)
        # Python loads the host bridge with ctypes, as in shared_carriage_server.
        if(NOT CMAKE_C_FLAGS MATCHES "sanitize")
            add_test(NAME native_shared_company COMMAND ${CC_PYTHON3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/native_coop_tests.py $<TARGET_FILE:native_coop_tests> $<TARGET_FILE:crownless_coop>)
            set_tests_properties(native_shared_company PROPERTIES TIMEOUT 60)
        endif()
    endif()
    add_test(NAME music_catalog_metadata
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tools/music_catalog.py --check)
    add_test(NAME bundled_music_and_host_catalog
             COMMAND ${CC_PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/music_host_tests.py)
    add_test(NAME webgl2_skinning_uniform_budget
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/shader_uniform_budget_tests.py)
    add_test(NAME simulation_runner_resume
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/sim_runner_tests.py
                     $<TARGET_FILE:crownless_sim_runner>)
    set_tests_properties(simulation_runner_resume PROPERTIES TIMEOUT 60)

    # crownless_letter_probe is only built under CC_BUILD_BENCHMARKS, so
    # the test has to be registered under the same condition. Otherwise
    # the generator expression below has no target to resolve and the
    # whole configure step fails, which is what the release container
    # hits when it builds with -DCC_BUILD_BENCHMARKS=OFF.
    if(CC_BUILD_BENCHMARKS)
        add_test(NAME letter_probe_contracts
                 COMMAND ${CC_PYTHON3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/letter_probe_tests.py
                         $<TARGET_FILE:crownless_letter_probe>)
        set_tests_properties(letter_probe_contracts PROPERTIES TIMEOUT 120)
    endif()
endif()

add_executable(interaction_tests tests/interaction_tests.c)
target_link_libraries(interaction_tests PRIVATE crownless_client_policy)
cc_strict_warnings(interaction_tests)
add_test(NAME interaction_planner COMMAND interaction_tests)
if(NOT EMSCRIPTEN)
    add_executable(coop_bridge_tests tests/coop_bridge_tests.c)
    target_link_libraries(coop_bridge_tests PRIVATE crownless_coop crownless_sim)
    cc_strict_warnings(coop_bridge_tests)
    add_test(NAME shared_carriage_bridge COMMAND coop_bridge_tests)
    if(CC_PYTHON3_EXECUTABLE AND NOT CMAKE_C_FLAGS MATCHES "sanitize")
        add_test(NAME shared_carriage_server
            COMMAND ${CC_PYTHON3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/coop_tests.py
                    $<TARGET_FILE:crownless_coop>)
    endif()
endif()

