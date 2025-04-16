##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.DbBase.Field
        test_db_field.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.DbBase.Field
        PRIVATE
        Demiplane::Database
        ${TEST_NECESSARY_LIBS}

)
##############################################################################

##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.DbBase.ArrayField
        test_db_array_field.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.DbBase.ArrayField
        PRIVATE
        Demiplane::Database
        ${TEST_NECESSARY_LIBS}

)
##############################################################################

##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.DbBase.Uuid
        test_db_uuid.cpp
)

target_link_libraries(${UNIT_TESTING_TARGET}.DbBase.Uuid
        PRIVATE
        Demiplane::Database
        ${TEST_NECESSARY_LIBS}

)
##############################################################################
