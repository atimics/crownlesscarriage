if(CC_PYTHON3_EXECUTABLE)
    add_test(NAME research_artifact_budget
        COMMAND ${CC_PYTHON3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/research_artifact_tests.py)
endif()
