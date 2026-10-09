if(TARGET renderer_regression_tests)
    add_test(NAME building_condition_presentation
             COMMAND renderer_regression_tests --building-conditions)
endif()
