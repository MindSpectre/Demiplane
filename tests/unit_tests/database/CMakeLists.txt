##############################################################################
# Test regular DBField
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.DbBase.Field
        db_base/ test_db_field.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.DbBase.Field
        PRIVATE
        Demiplane::Database
        ${TEST_LIBS}

)
##############################################################################
# Test array DbField
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.DbBase.ArrayField
        db_base/ test_db_array_field.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.DbBase.ArrayField
        PRIVATE
        Demiplane::Database
        ${TEST_LIBS}

)
##############################################################################
# Test specific DbUuid  //TODO Do we need it?
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.DbBase.Uuid
        db_base/test_db_uuid.cpp
)

target_link_libraries(${UNIT_TESTING_TARGET}.DbBase.Uuid
        PRIVATE
        Demiplane::Database
        ${TEST_LIBS}

)
##############################################################################


##############################################################################
# Test Transaction Manager
##############################################################################

add_unit_test(${UNIT_TESTING_TARGET}.TransactionManager
        database/transaction_manager/test_transaction_manager.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.TransactionManager
        PRIVATE
        Demiplane::Database
        ${TEST_LIBS}

)

##############################################################################

##############################################################################
# Test DbInterface Pool
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.DbInterfacePool
        database/db_interface_pool/test_db_interface_pool.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.DbInterfacePool
        PRIVATE
        Demiplane::Database
        ${TEST_LIBS}
)

##############################################################################

##############################################################################
# Test Basic DB interface Mock
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.DbInterfaceBasicMock
        database/db_mocks/test_db_basic_mock.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.DbInterfaceBasicMock
        PRIVATE
        Demiplane::Database::Mock::Basic
        ${TEST_LIBS}

)
##############################################################################

##############################################################################
# Test Silent DB interface Mock
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.DbInterfaceSilentMock
        database/db_mocks/test_db_silent_mock.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.DbInterfaceSilentMock
        PRIVATE
        Demiplane::Database::Mock::Silent
        ${TEST_LIBS}

)
##############################################################################

##############################################################################
# Test Pqxx Query Engine
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.PqxxQueryEngine
        database/pqxx_query_engine/pqxx_query_engine.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.PqxxQueryEngine
        PRIVATE
        Demiplane::Database::Pqxx
        ${TEST_LIBS}

)
##############################################################################