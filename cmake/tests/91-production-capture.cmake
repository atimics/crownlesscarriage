if(CC_PYTHON3_EXECUTABLE)
    add_test(NAME production_capture_failure_records
        COMMAND ${CC_PYTHON3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/production_capture_tests.py)
endif()
