if(Python3_Interpreter_FOUND)
    add_test(NAME gossip_flow_integrity
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/gossip_flow_tests.py
                $<TARGET_FILE:crownless_gossip_flow>)
endif()
