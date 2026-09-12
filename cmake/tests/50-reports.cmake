add_executable(client_policy_tests tests/client_policy_tests.c)
target_link_libraries(client_policy_tests PRIVATE crownless_client_policy)
cc_strict_warnings(client_policy_tests)
add_test(NAME daily_play_policy COMMAND client_policy_tests)

add_executable(animal_economy_tests tests/animal_economy_tests.c)
target_link_libraries(animal_economy_tests PRIVATE crownless_persistence)
cc_strict_warnings(animal_economy_tests)
add_test(NAME horse_cattle_and_flock_economy COMMAND animal_economy_tests)

add_executable(horse_stable_tests tests/horse_stable_tests.c)
target_link_libraries(horse_stable_tests PRIVATE crownless_persistence)
cc_strict_warnings(horse_stable_tests)
add_test(NAME horse_stable_breeding COMMAND horse_stable_tests)

add_executable(route_observation_tests tests/route_observation_tests.c)
target_link_libraries(route_observation_tests PRIVATE crownless_sim)
cc_strict_warnings(route_observation_tests)
add_test(NAME route_daily_observations COMMAND route_observation_tests)

add_executable(bandit_exposure_tests tests/bandit_exposure_tests.c)
target_link_libraries(bandit_exposure_tests PRIVATE crownless_sim)
cc_strict_warnings(bandit_exposure_tests)
add_test(NAME bandit_exposure_counts COMMAND bandit_exposure_tests)

add_executable(ritual_offering_plan_tests tests/ritual_offering_plan_tests.c)
target_link_libraries(ritual_offering_plan_tests PRIVATE crownless_sim)
cc_strict_warnings(ritual_offering_plan_tests)
add_test(NAME ritual_offering_plan COMMAND ritual_offering_plan_tests)

add_executable(campaign_launch_plan_tests tests/campaign_launch_plan_tests.c)
target_link_libraries(campaign_launch_plan_tests PRIVATE crownless_sim)
cc_strict_warnings(campaign_launch_plan_tests)
add_test(NAME campaign_launch_plan COMMAND campaign_launch_plan_tests)

add_executable(road_recovery_plan_tests tests/road_recovery_plan_tests.c)
target_link_libraries(road_recovery_plan_tests PRIVATE crownless_sim)
cc_strict_warnings(road_recovery_plan_tests)
add_test(NAME roadside_recovery_plan COMMAND road_recovery_plan_tests)

add_executable(agent_validation_tests tests/agent_validation_tests.c)
target_link_libraries(agent_validation_tests PRIVATE crownless_sim)
cc_strict_warnings(agent_validation_tests)
add_test(NAME player_agent_checkpoint_validation COMMAND agent_validation_tests)

add_executable(agent_report_tests tests/agent_report_tests.c)
target_link_libraries(agent_report_tests PRIVATE crownless_sim)
cc_strict_warnings(agent_report_tests)
add_test(NAME player_agent_hunger_report COMMAND agent_report_tests)

add_executable(hunger_metrics_tests tests/hunger_metrics_tests.c)
target_link_libraries(hunger_metrics_tests PRIVATE crownless_sim)
cc_strict_warnings(hunger_metrics_tests)
add_test(NAME inhabited_hunger_metrics COMMAND hunger_metrics_tests)
if(CC_PYTHON3_EXECUTABLE AND CC_BUILD_BENCHMARKS)
    add_executable(threat_snapshot_fixture tests/threat_snapshot_fixture.c)
    target_link_libraries(threat_snapshot_fixture PRIVATE crownless_persistence)
    cc_strict_warnings(threat_snapshot_fixture)
    add_test(NAME threat_group_snapshots
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/threat_snapshot_tests.py
                     $<TARGET_FILE:threat_snapshot_fixture>)
    add_executable(road_recovery_json_fixture tests/road_recovery_json_fixture.c)
    target_link_libraries(road_recovery_json_fixture PRIVATE crownless_persistence)
    cc_strict_warnings(road_recovery_json_fixture)
    add_test(NAME road_recovery_json
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/road_recovery_json_tests.py
                     $<TARGET_FILE:road_recovery_json_fixture>)
    add_executable(campaign_json_fixture tests/transition_json_fixture.c)
    target_link_libraries(campaign_json_fixture PRIVATE crownless_persistence)
    cc_strict_warnings(campaign_json_fixture)
    add_executable(ritual_json_fixture tests/transition_json_fixture.c)
    target_compile_definitions(ritual_json_fixture PRIVATE CC_RITUAL_JSON_FIXTURE)
    target_link_libraries(ritual_json_fixture PRIVATE crownless_persistence)
    cc_strict_warnings(ritual_json_fixture)
    add_test(NAME transition_plan_json
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/transition_json_tests.py
                     $<TARGET_FILE:campaign_json_fixture>
                     $<TARGET_FILE:ritual_json_fixture>)
    add_executable(archive_work_fixture tests/archive_work_fixture.c)
    target_link_libraries(archive_work_fixture PRIVATE crownless_persistence)
    cc_strict_warnings(archive_work_fixture)
    add_test(NAME archive_work_json
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/archive_work_tests.py
                     $<TARGET_FILE:archive_work_fixture>)
    add_executable(road_network_fixture tests/road_network_fixture.c)
    target_link_libraries(road_network_fixture PRIVATE crownless_persistence)
    cc_strict_warnings(road_network_fixture)
    add_test(NAME road_network_json
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/road_network_tests.py
                     $<TARGET_FILE:road_network_fixture>)
    add_executable(road_observation_tests tests/road_observation_tests.c)
    target_link_libraries(road_observation_tests PRIVATE crownless_sim)
    cc_strict_warnings(road_observation_tests)
    add_test(NAME road_observation COMMAND road_observation_tests)
    add_executable(road_context_fixture tests/road_context_fixture.c)
    target_link_libraries(road_context_fixture PRIVATE crownless_persistence)
    cc_strict_warnings(road_context_fixture)
    add_test(NAME road_context_json
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/road_context_tests.py
                     $<TARGET_FILE:road_context_fixture>)
    add_executable(retained_history_fixture tests/retained_history_fixture.c)
    target_link_libraries(retained_history_fixture PRIVATE crownless_persistence)
    cc_strict_warnings(retained_history_fixture)
    add_test(NAME retained_history_json
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/retained_history_tests.py
                     $<TARGET_FILE:retained_history_fixture>)
    add_test(NAME production_json_capture
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/production_json_tests.py
                     $<TARGET_FILE:crownless_sim_runner>)
    add_test(NAME route_observation_csv
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/route_observation_tests.py
                     $<TARGET_FILE:crownless_sim_metrics>)
    add_test(NAME sweep_input_boundaries
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/sweep_input_tests.py
                     $<TARGET_FILE:crownless_sim_runner>
                     $<TARGET_FILE:crownless_sim_metrics>)
    add_test(NAME hunger_report_agreement
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/hunger_report_tests.py
                     $<TARGET_FILE:crownless_sim_runner>
                     $<TARGET_FILE:crownless_sim_metrics>)
    add_test(NAME welfare_report_agreement
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/welfare_report_tests.py
                     $<TARGET_FILE:crownless_sim_metrics>)
    add_test(NAME welfare_plot_inputs
             COMMAND ${CC_PYTHON3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/welfare_plot_tests.py)
endif()

