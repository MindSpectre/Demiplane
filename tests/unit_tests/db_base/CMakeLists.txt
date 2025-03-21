
##############################################################################
add_executable(${UNIT_TESTING_TARGET}_DbBase_Field
        test_db_field.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}_DbBase_Field
        PRIVATE
        Demiplane::Database::Base
        ${TEST_NECESSARY_LIBS}

)
add_test(UnitTest_DbBase_Field ${UNIT_TESTING_TARGET}_DbBase_Field)
##############################################################################

##############################################################################
add_executable(${UNIT_TESTING_TARGET}_DbBase_ArrayField
        test_db_array_field.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}_DbBase_ArrayField
        PRIVATE
        Demiplane::Database::Base
        ${TEST_NECESSARY_LIBS}

)
add_test(UnitTest_DbBase_ArrayField ${UNIT_TESTING_TARGET}_DbBase_ArrayField)
##############################################################################

##############################################################################
add_executable(${UNIT_TESTING_TARGET}_DbBase_Uuid
        test_db_uuid.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}_DbBase_Uuid
        PRIVATE
        Demiplane::Database::Base
        ${TEST_NECESSARY_LIBS}

)
add_test(UnitTest_DbBase_Uuid ${UNIT_TESTING_TARGET}_DbBase_Uuid)
##############################################################################