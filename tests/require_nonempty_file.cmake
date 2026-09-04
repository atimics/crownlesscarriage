if(NOT DEFINED CC_FILE OR NOT EXISTS "${CC_FILE}")
    message(FATAL_ERROR "Expected review artifact is missing: ${CC_FILE}")
endif()

file(SIZE "${CC_FILE}" CC_FILE_SIZE)
if(CC_FILE_SIZE LESS 1024)
    message(FATAL_ERROR
        "Expected review artifact is too small: ${CC_FILE_SIZE} bytes")
endif()
