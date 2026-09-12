add_executable(core_conversation_tests tests/core_conversation_tests.c)
target_link_libraries(core_conversation_tests PRIVATE crownless_story)
cc_strict_warnings(core_conversation_tests)
add_test(NAME core_conversation_world_and_history COMMAND core_conversation_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/language/core.ccv2)
if(CC_PYTHON3_EXECUTABLE)
    add_test(NAME core_model_tables_current COMMAND ${CC_PYTHON3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/language/compile_model.py --check)
    add_test(NAME core_model_native_parity COMMAND ${CC_PYTHON3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/core_model_tests.py $<TARGET_FILE:core_model_probe>)
endif()
